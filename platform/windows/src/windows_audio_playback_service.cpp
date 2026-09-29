#include <rgsml/platform/windows/windows_audio_playback_service.hpp>

#include "internal/playback_support.hpp"

#include <rgsml/analysis/live_spectrum_analyzer.hpp>
#include <rgsml/audio/wav_reader.hpp>
#include <rgsml/platform/windows/windows_resource_reader.hpp>

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QByteArray>
#include <QIODevice>
#include <QMediaDevices>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace rgsml::platform::windows {
namespace {

using internal::DeviceSampleFormat;
using internal::IPlaybackOutput;
using internal::OutputState;

template <typename T>
[[nodiscard]] core::Result<T> failure(
    core::ErrorCode code,
    const char* message)
{
    return core::Result<T>::failure(core::Error{code, message});
}

[[nodiscard]] core::Status status_failure(
    core::ErrorCode code,
    const char* message)
{
    return core::Status::failure(core::Error{code, message});
}

class BufferedAudioDevice final : public QIODevice {
public:
    explicit BufferedAudioDevice(qsizetype capacity)
        : capacity_(capacity)
        , buffer_(static_cast<std::size_t>(capacity), std::byte{0})
    {
        open(QIODevice::ReadOnly);
    }

    [[nodiscard]] std::size_t writable_bytes() const noexcept
    {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto tail = tail_.load(std::memory_order_acquire);
        const auto count = (head >= tail) ? (head - tail) : (capacity_ - (tail - head));
        return static_cast<std::size_t>(capacity_ - 1 - count);
    }

    [[nodiscard]] std::size_t queued_bytes() const noexcept
    {
        const auto head = head_.load(std::memory_order_acquire);
        const auto tail = tail_.load(std::memory_order_relaxed);
        return static_cast<std::size_t>((head >= tail) ? (head - tail) : (capacity_ - (tail - head)));
    }

    [[nodiscard]] std::size_t append(std::span<const std::byte> bytes)
    {
        const auto available = writable_bytes();
        const auto accepted = std::min(available, bytes.size());
        if (accepted == 0) {
            return 0;
        }

        auto head = head_.load(std::memory_order_relaxed);
        for (std::size_t i = 0; i < accepted; ++i) {
            buffer_[head] = bytes[i];
            head = (head + 1) % capacity_;
        }
        head_.store(head, std::memory_order_release);
        emit readyRead();
        return accepted;
    }

    void clear() noexcept
    {
        tail_.store(head_.load(std::memory_order_relaxed), std::memory_order_release);
    }

    void attach_analyzer(
        rgsml::analysis::LiveSpectrumAnalyzer* analyzer,
        std::uint32_t sampleRate,
        std::uint8_t channels,
        rgsml::analysis::SampleEncoding encoding,
        std::uint64_t gen,
        std::uint64_t epoch)
    {
        sampleRate_ = sampleRate;
        channelCount_ = channels;
        encoding_ = encoding;
        streamGeneration_.store(gen, std::memory_order_relaxed);
        analysisEpoch_.store(epoch, std::memory_order_relaxed);
        analyzer_.store(analyzer, std::memory_order_release);
    }

    [[nodiscard]] bool isSequential() const override
    {
        return true;
    }

    [[nodiscard]] qint64 bytesAvailable() const override
    {
        return static_cast<qint64>(queued_bytes()) + QIODevice::bytesAvailable();
    }

protected:
    qint64 readData(char* destination, qint64 maximumBytes) override
    {
        if (maximumBytes <= 0) {
            return 0;
        }
        const auto available = queued_bytes();
        const auto count = std::min(static_cast<std::size_t>(maximumBytes), available);
        if (count == 0) {
            return 0;
        }

        auto tail = tail_.load(std::memory_order_relaxed);
        for (std::size_t i = 0; i < count; ++i) {
            destination[i] = static_cast<char>(buffer_[tail]);
            tail = (tail + 1) % capacity_;
        }
        tail_.store(tail, std::memory_order_release);

        auto* analyzer = analyzer_.load(std::memory_order_acquire);
        if (analyzer != nullptr) {
            analyzer->push_audio_bytes(
                destination,
                count,
                sampleRate_,
                channelCount_,
                encoding_,
                streamGeneration_.load(std::memory_order_relaxed),
                analysisEpoch_.load(std::memory_order_relaxed));
        }

        return static_cast<qint64>(count);
    }

    qint64 writeData(const char*, qint64) override
    {
        return -1;
    }

private:
    std::size_t capacity_;
    std::vector<std::byte> buffer_;
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};

    std::atomic<rgsml::analysis::LiveSpectrumAnalyzer*> analyzer_{nullptr};
    std::uint32_t sampleRate_{44100};
    std::uint8_t channelCount_{2};
    rgsml::analysis::SampleEncoding encoding_{rgsml::analysis::SampleEncoding::IEEE_FLOAT32};
    std::atomic<std::uint64_t> streamGeneration_{0};
    std::atomic<std::uint64_t> analysisEpoch_{0};
};

class QtPlaybackOutput final : public IPlaybackOutput {
public:
    static constexpr qsizetype kQueueCapacityBytes = 64 * 1024;

    QtPlaybackOutput(QAudioDevice device, QAudioFormat format)
        : device_(std::move(device))
        , format_(format)
        , queue_(kQueueCapacityBytes)
    {
    }

    ~QtPlaybackOutput() noexcept override
    {
        static_cast<void>(stop());
    }

    [[nodiscard]] std::size_t writable_bytes() const noexcept override
    {
        return queue_.writable_bytes();
    }

    [[nodiscard]] std::size_t queued_bytes() const noexcept override
    {
        return queue_.queued_bytes();
    }

    [[nodiscard]] core::Result<std::size_t> enqueue(
        std::span<const std::byte> bytes) override
    {
        return core::Result<std::size_t>::success(queue_.append(bytes));
    }

    void clear_queue() noexcept override
    {
        queue_.clear();
    }

    void attach_analyzer(
        rgsml::analysis::LiveSpectrumAnalyzer* analyzer,
        std::uint64_t gen,
        std::uint64_t epoch) override
    {
        const auto sampleRate = static_cast<std::uint32_t>(format_.sampleRate());
        const auto channels = static_cast<std::uint8_t>(format_.channelCount());
        const auto encoding = (format_.sampleFormat() == QAudioFormat::Float)
            ? rgsml::analysis::SampleEncoding::IEEE_FLOAT32
            : rgsml::analysis::SampleEncoding::PCM16_LE;
        queue_.attach_analyzer(analyzer, sampleRate, channels, encoding, gen, epoch);
    }

    [[nodiscard]] core::Status start() override
    {
        const auto currentDefault = QMediaDevices::defaultAudioOutput();
        if (currentDefault.isNull()
            || currentDefault.mode() != QAudioDevice::Output
            || !currentDefault.isFormatSupported(format_)) {
            return status_failure(
                core::ErrorCode::UnsupportedOperation,
                "The current default output no longer supports the prepared format.");
        }
        device_ = currentDefault;
        try {
            sink_ = std::make_unique<QAudioSink>(device_, format_);
        } catch (const std::bad_alloc&) {
            return status_failure(
                core::ErrorCode::IoFailure,
                "Unable to allocate the Windows audio sink.");
        }
        if (sink_->isNull()) {
            sink_.reset();
            return status_failure(
                core::ErrorCode::UnsupportedOperation,
                "The default Windows audio sink could not be created.");
        }
        sink_->setBufferSize(static_cast<qsizetype>(32 * 1024));
        sink_->start(&queue_);
        if (sink_->error() != QtAudio::NoError) {
            const auto mapped = error();
            sink_->stop();
            sink_.reset();
            return core::Status::failure(mapped.value_or(core::Error{
                core::ErrorCode::IoFailure,
                "The Windows audio sink failed to start."}));
        }
        return core::Status::success();
    }

    [[nodiscard]] core::Status suspend() override
    {
        if (!sink_ || sink_->state() != QtAudio::ActiveState) {
            return status_failure(
                core::ErrorCode::InvalidState,
                "The Windows audio sink is not active.");
        }
        sink_->suspend();
        return core::Status::success();
    }

    [[nodiscard]] core::Status resume() override
    {
        if (!sink_ || sink_->state() != QtAudio::SuspendedState) {
            return status_failure(
                core::ErrorCode::InvalidState,
                "The Windows audio sink is not suspended.");
        }
        sink_->resume();
        return core::Status::success();
    }

    [[nodiscard]] core::Status stop() override
    {
        if (sink_) {
            sink_->stop();
            sink_.reset();
        }
        queue_.clear();
        return core::Status::success();
    }

    [[nodiscard]] std::int64_t processed_frames() const noexcept override
    {
        if (!sink_) {
            return 0;
        }
        const auto microseconds = std::max<qint64>(0, sink_->processedUSecs());
        const long double frames = static_cast<long double>(microseconds)
            * static_cast<long double>(format_.sampleRate()) / 1'000'000.0L;
        if (frames >= static_cast<long double>(
                std::numeric_limits<std::int64_t>::max())) {
            return std::numeric_limits<std::int64_t>::max();
        }
        return static_cast<std::int64_t>(frames);
    }

    [[nodiscard]] OutputState state() const noexcept override
    {
        if (!sink_) {
            return OutputState::STOPPED;
        }
        if (sink_->error() != QtAudio::NoError) {
            return OutputState::ERROR;
        }
        switch (sink_->state()) {
        case QtAudio::ActiveState:
            return OutputState::ACTIVE;
        case QtAudio::SuspendedState:
            return OutputState::SUSPENDED;
        case QtAudio::IdleState:
            return OutputState::IDLE;
        case QtAudio::StoppedState:
            return OutputState::STOPPED;
        }
        return OutputState::ERROR;
    }

    [[nodiscard]] std::optional<core::Error> error() const override
    {
        if (!sink_ || sink_->error() == QtAudio::NoError) {
            return std::nullopt;
        }
        switch (sink_->error()) {
        case QtAudio::OpenError:
            return core::Error{
                core::ErrorCode::AccessDenied,
                "The default Windows audio output could not be opened."};
        case QtAudio::UnderrunError:
            return core::Error{
                core::ErrorCode::IoFailure,
                "The Windows audio output reported an underrun."};
        case QtAudio::IOError:
            return core::Error{
                core::ErrorCode::IoFailure,
                "The Windows audio output reported an I/O failure."};
        case QtAudio::FatalError:
            return core::Error{
                core::ErrorCode::IoFailure,
                "The Windows audio output reported a fatal backend failure."};
        case QtAudio::NoError:
            break;
        }
        return std::nullopt;
    }

private:
    QAudioDevice device_;
    QAudioFormat format_;
    BufferedAudioDevice queue_;
    std::unique_ptr<QAudioSink> sink_;
};

struct OutputCandidate final {
    std::unique_ptr<IPlaybackOutput> output;
    DeviceSampleFormat sampleFormat;
    std::optional<audio::PlaybackSampleRateAdapter> rateAdapter;
};

[[nodiscard]] QAudioFormat make_format(
    const audio::AudioFormat& source,
    int sampleRate,
    QAudioFormat::SampleFormat sampleFormat)
{
    QAudioFormat format;
    format.setSampleRate(sampleRate);
    if (source.channel_layout() == audio::ChannelLayout::MONO_C) {
        format.setChannelConfig(QAudioFormat::ChannelConfigMono);
    } else {
        format.setChannelConfig(QAudioFormat::ChannelConfigStereo);
    }
    format.setSampleFormat(sampleFormat);
    return format;
}

[[nodiscard]] core::Result<OutputCandidate> make_output_candidate(
    const audio::AudioFormat& sourceFormat,
    core::FrameCount sourceFrameCount)
{
    const auto defaultOutput = QMediaDevices::defaultAudioOutput();
    if (defaultOutput.isNull() || defaultOutput.mode() != QAudioDevice::Output) {
        return failure<OutputCandidate>(
            core::ErrorCode::UnsupportedOperation,
            "No default Windows audio output is available.");
    }

    const auto sourceRate = static_cast<int>(sourceFormat.sample_rate().value());
    const auto pairedRate = sourceRate == 44'100
        ? 48'000
        : (sourceRate == 48'000 ? 44'100 : 0);
    const auto exactFloatFormat = make_format(
        sourceFormat, sourceRate, QAudioFormat::Float);
    const auto exactPcm16Format = make_format(
        sourceFormat, sourceRate, QAudioFormat::Int16);
    const auto pairedFloatFormat = make_format(
        sourceFormat, pairedRate, QAudioFormat::Float);
    const auto pairedPcm16Format = make_format(
        sourceFormat, pairedRate, QAudioFormat::Int16);
    auto selected = internal::select_device_format(
        sourceFormat,
        defaultOutput.isFormatSupported(exactFloatFormat),
        defaultOutput.isFormatSupported(exactPcm16Format),
        pairedRate != 0
            && defaultOutput.isFormatSupported(pairedFloatFormat),
        pairedRate != 0
            && defaultOutput.isFormatSupported(pairedPcm16Format));
    if (!selected) {
        return core::Result<OutputCandidate>::failure(*selected.error());
    }
    const auto qtFormat = selected.value()->sampleRateHz == sourceRate
        ? (selected.value()->sampleFormat == DeviceSampleFormat::IEEE_F32
            ? exactFloatFormat
            : exactPcm16Format)
        : (selected.value()->sampleFormat
            == DeviceSampleFormat::IEEE_F32
            ? pairedFloatFormat
            : pairedPcm16Format);

    std::optional<audio::PlaybackSampleRateAdapter> rateAdapter;
    if (selected.value()->srcApplied) {
        auto outputRate = core::SampleRate::create(selected.value()->sampleRateHz);
        if (!outputRate) {
            return core::Result<OutputCandidate>::failure(*outputRate.error());
        }
        auto adapter = audio::PlaybackSampleRateAdapter::create(
            audio::PlaybackRateSpec{
                sourceFormat.sample_rate(),
                *outputRate.value(),
                sourceFormat.channel_layout(),
                sourceFrameCount,
            });
        if (!adapter) {
            return core::Result<OutputCandidate>::failure(*adapter.error());
        }
        rateAdapter.emplace(std::move(*adapter.value()));
    }
    try {
        return core::Result<OutputCandidate>::success(OutputCandidate{
            std::make_unique<QtPlaybackOutput>(defaultOutput, qtFormat),
            selected.value()->sampleFormat,
            std::move(rateAdapter),
        });
    } catch (const std::bad_alloc&) {
        return failure<OutputCandidate>(
            core::ErrorCode::IoFailure,
            "Unable to allocate the Windows playback output.");
    }
}

class PlaybackWorker final : public QObject {
public:
    [[nodiscard]] core::Status start()
    {
        if (timer_ != nullptr) {
            return core::Status::success();
        }
        try {
            timer_ = new QTimer(this);
        } catch (const std::bad_alloc&) {
            return status_failure(
                core::ErrorCode::IoFailure,
                "Unable to allocate the Windows playback scheduler.");
        }
        timer_->setInterval(10);
        timer_->setTimerType(Qt::PreciseTimer);
        QObject::connect(timer_, &QTimer::timeout, this, [this] {
            static_cast<void>(engine_.tick());
        });
        timer_->start();
        return core::Status::success();
    }

    [[nodiscard]] core::Status shutdown()
    {
        if (timer_ != nullptr) {
            timer_->stop();
        }
        return clear();
    }

    [[nodiscard]] core::Status prepare(const core::ResourceReference& source)
    {
        if (analyzer_ != nullptr) {
            streamGeneration_++;
            analysisEpoch_++;
            analyzer_->set_stream_generation(streamGeneration_);
            analyzer_->invalidate_and_clear();
        }
        auto resource = WindowsResourceReader::open_read_only(source);
        if (!resource) {
            return core::Status::failure(*resource.error());
        }
        auto reader = audio::WavReader::open(std::move(*resource.value()));
        if (!reader) {
            return core::Status::failure(*reader.error());
        }
        auto output = make_output_candidate(
            (*reader.value())->info().audio_format(),
            (*reader.value())->info().frame_count());
        if (!output) {
            return core::Status::failure(*output.error());
        }
        if (analyzer_ != nullptr) {
            output.value()->output->attach_analyzer(analyzer_, streamGeneration_, analysisEpoch_);
        }
        auto installed = engine_.install_candidate(
            std::move(*reader.value()),
            std::move(output.value()->output),
            output.value()->sampleFormat,
            std::move(output.value()->rateAdapter));
        if (!installed) {
            return installed;
        }
        preparedSource_ = source;
        preparedPcm_.reset();
        return core::Status::success();
    }

    [[nodiscard]] core::Status prepare_pcm(
        audio::AudioBufferView source,
        std::shared_ptr<const void> lifetime = nullptr)
    {
        if (analyzer_ != nullptr) {
            streamGeneration_++;
            analysisEpoch_++;
            analyzer_->set_stream_generation(streamGeneration_);
            analyzer_->invalidate_and_clear();
        }
        auto output = make_output_candidate(
            source.format(), source.frame_count());
        if (!output) {
            return core::Status::failure(*output.error());
        }
        if (analyzer_ != nullptr) {
            output.value()->output->attach_analyzer(analyzer_, streamGeneration_, analysisEpoch_);
        }
        auto installed = engine_.install_pcm_candidate(
            source,
            std::move(output.value()->output),
            output.value()->sampleFormat,
            std::move(output.value()->rateAdapter),
            std::move(lifetime));
        if (!installed) {
            return installed;
        }
        preparedSource_.reset();
        preparedPcm_ = source;
        return core::Status::success();
    }

    [[nodiscard]] core::Status handoff_pcm(
        audio::AudioBufferView source,
        std::shared_ptr<const void> lifetime = nullptr)
    {
        auto handedOff = engine_.handoff_pcm(source, lifetime);
        if (!handedOff) {
            return handedOff;
        }
        preparedSource_.reset();
        preparedPcm_ = source;
        return core::Status::success();
    }

    [[nodiscard]] core::Status clear()
    {
        auto cleared = engine_.clear();
        if (cleared) {
            preparedSource_.reset();
            preparedPcm_.reset();
        }
        return cleared;
    }

    [[nodiscard]] core::Status play()
    {
        auto snapshot = engine_.snapshot();
        if (!snapshot) {
            return core::Status::failure(*snapshot.error());
        }
        if (snapshot.value()->state == core::PlaybackState::STOPPED
            && (preparedSource_ || preparedPcm_)) {
            const auto preservedPosition = snapshot.value()->position;
            const auto preservedLoop = snapshot.value()->loop;
            const auto source = preparedSource_;
            const auto pcm = preparedPcm_;
            auto reprepared = source
                ? prepare(*source)
                : prepare_pcm(*pcm);
            if (!reprepared) {
                return reprepared;
            }
            auto sought = engine_.seek(preservedPosition);
            if (!sought) {
                return sought;
            }
            auto looped = engine_.set_loop(preservedLoop);
            if (!looped) {
                return looped;
            }
        }
        return engine_.play();
    }

    [[nodiscard]] core::Status pause()
    {
        if (analyzer_ != nullptr) {
            analyzer_->invalidate_and_clear();
        }
        return engine_.pause();
    }

    [[nodiscard]] core::Status stop()
    {
        if (analyzer_ != nullptr) {
            analyzer_->invalidate_and_clear();
        }
        return engine_.stop();
    }

    [[nodiscard]] core::Status seek(core::FrameIndex position)
    {
        if (analyzer_ != nullptr) {
            analyzer_->invalidate_and_clear();
        }
        return engine_.seek(position);
    }

    [[nodiscard]] core::Status set_loop(
        std::optional<core::FrameRange> loop)
    {
        return engine_.set_loop(loop);
    }

    [[nodiscard]] core::Result<core::PlaybackSnapshot> snapshot() const
    {
        return engine_.snapshot();
    }

    void attach_analyzer(rgsml::analysis::LiveSpectrumAnalyzer* analyzer)
    {
        analyzer_ = analyzer;
    }

private:
    internal::PlaybackEngine engine_;
    rgsml::analysis::LiveSpectrumAnalyzer* analyzer_{nullptr};
    std::uint64_t streamGeneration_{1};
    std::uint64_t analysisEpoch_{1};
    QTimer* timer_{nullptr};
    std::optional<core::ResourceReference> preparedSource_;
    std::optional<audio::AudioBufferView> preparedPcm_;
};

}  // namespace

class WindowsAudioPlaybackService::Impl final {
public:
    Impl()
    {
        worker_ = new PlaybackWorker;
        worker_->moveToThread(&thread_);
        QObject::connect(
            &thread_, &QThread::finished, worker_, &QObject::deleteLater);
        thread_.setObjectName(QStringLiteral("RGSML playback worker"));
        thread_.start();

        std::optional<core::Status> started;
        const bool invoked = QMetaObject::invokeMethod(
            worker_,
            [&] { started.emplace(worker_->start()); },
            Qt::BlockingQueuedConnection);
        ready_ = invoked && started.has_value() && static_cast<bool>(*started);
    }

    ~Impl() noexcept
    {
        if (worker_ != nullptr && thread_.isRunning()) {
            static_cast<void>(QMetaObject::invokeMethod(
                worker_,
                [&] { static_cast<void>(worker_->shutdown()); },
                Qt::BlockingQueuedConnection));
        }
        thread_.quit();
        static_cast<void>(thread_.wait());
        worker_ = nullptr;
    }

    [[nodiscard]] core::Status prepare(const core::ResourceReference& source)
    {
        return invoke_status([source](PlaybackWorker& worker) {
            return worker.prepare(source);
        });
    }

    [[nodiscard]] core::Status prepare_pcm(
        audio::AudioBufferView source,
        std::shared_ptr<const void> lifetime = nullptr)
    {
        return invoke_status([source, lifetime = std::move(lifetime)](PlaybackWorker& worker) mutable {
            return worker.prepare_pcm(source, std::move(lifetime));
        });
    }

    [[nodiscard]] core::Status handoff_pcm(
        audio::AudioBufferView source,
        std::shared_ptr<const void> lifetime = nullptr)
    {
        return invoke_status([source, lifetime = std::move(lifetime)](PlaybackWorker& worker) mutable {
            return worker.handoff_pcm(source, std::move(lifetime));
        });
    }

    [[nodiscard]] core::Status clear()
    {
        return invoke_status([](PlaybackWorker& worker) {
            return worker.clear();
        });
    }

    [[nodiscard]] core::Status play()
    {
        return invoke_status([](PlaybackWorker& worker) {
            return worker.play();
        });
    }

    [[nodiscard]] core::Status pause()
    {
        return invoke_status([](PlaybackWorker& worker) {
            return worker.pause();
        });
    }

    [[nodiscard]] core::Status stop()
    {
        return invoke_status([](PlaybackWorker& worker) {
            return worker.stop();
        });
    }

    [[nodiscard]] core::Status seek(core::FrameIndex position)
    {
        return invoke_status([position](PlaybackWorker& worker) {
            return worker.seek(position);
        });
    }

    [[nodiscard]] core::Status set_loop(
        std::optional<core::FrameRange> loop)
    {
        return invoke_status([loop](PlaybackWorker& worker) {
            return worker.set_loop(loop);
        });
    }

    [[nodiscard]] core::Result<core::PlaybackSnapshot> snapshot() const
    {
        if (!ready_ || worker_ == nullptr) {
            return failure<core::PlaybackSnapshot>(
                core::ErrorCode::IoFailure,
                "The Windows playback worker is unavailable.");
        }
        if (QThread::currentThread() == worker_->thread()) {
            return worker_->snapshot();
        }
        std::optional<core::Result<core::PlaybackSnapshot>> result;
        const bool invoked = QMetaObject::invokeMethod(
            worker_,
            [&] { result.emplace(worker_->snapshot()); },
            Qt::BlockingQueuedConnection);
        if (!invoked || !result.has_value()) {
            return failure<core::PlaybackSnapshot>(
                core::ErrorCode::IoFailure,
                "Unable to query the Windows playback worker.");
        }
        return std::move(*result);
    }

    void attach_analyzer(analysis::LiveSpectrumAnalyzer* analyzer)
    {
        if (!ready_ || worker_ == nullptr) {
            return;
        }
        static_cast<void>(QMetaObject::invokeMethod(
            worker_,
            [this, analyzer] { worker_->attach_analyzer(analyzer); },
            Qt::BlockingQueuedConnection));
    }

private:
    template <typename Operation>
    [[nodiscard]] core::Status invoke_status(Operation&& operation)
    {
        if (!ready_ || worker_ == nullptr) {
            return status_failure(
                core::ErrorCode::IoFailure,
                "The Windows playback worker is unavailable.");
        }
        if (QThread::currentThread() == worker_->thread()) {
            return operation(*worker_);
        }
        std::optional<core::Status> result;
        const bool invoked = QMetaObject::invokeMethod(
            worker_,
            [&] { result.emplace(operation(*worker_)); },
            Qt::BlockingQueuedConnection);
        if (!invoked || !result.has_value()) {
            return status_failure(
                core::ErrorCode::IoFailure,
                "Unable to invoke the Windows playback worker.");
        }
        return std::move(*result);
    }

    QThread thread_;
    PlaybackWorker* worker_{nullptr};
    bool ready_{false};
};

WindowsAudioPlaybackService::WindowsAudioPlaybackService()
    : impl_(std::make_unique<Impl>())
{
}

WindowsAudioPlaybackService::~WindowsAudioPlaybackService() noexcept = default;

core::Status WindowsAudioPlaybackService::prepare(
    const core::ResourceReference& source)
{
    return impl_->prepare(source);
}

core::Status WindowsAudioPlaybackService::prepare_pcm(
    audio::AudioBufferView source)
{
    return impl_->prepare_pcm(source, nullptr);
}

core::Status WindowsAudioPlaybackService::prepare_pcm(
    audio::AudioBufferView source,
    std::shared_ptr<const void> lifetime)
{
    return impl_->prepare_pcm(source, std::move(lifetime));
}

core::Status WindowsAudioPlaybackService::handoff_pcm(
    audio::AudioBufferView source,
    std::shared_ptr<const void> lifetime)
{
    return impl_->handoff_pcm(source, std::move(lifetime));
}

core::Status WindowsAudioPlaybackService::clear()
{
    return impl_->clear();
}

core::Status WindowsAudioPlaybackService::play()
{
    return impl_->play();
}

core::Status WindowsAudioPlaybackService::pause()
{
    return impl_->pause();
}

core::Status WindowsAudioPlaybackService::stop()
{
    return impl_->stop();
}

core::Status WindowsAudioPlaybackService::seek(core::FrameIndex position)
{
    return impl_->seek(position);
}

core::Status WindowsAudioPlaybackService::set_loop(
    std::optional<core::FrameRange> loop)
{
    return impl_->set_loop(loop);
}

core::Result<core::PlaybackSnapshot>
WindowsAudioPlaybackService::snapshot() const
{
    return impl_->snapshot();
}

void WindowsAudioPlaybackService::attach_analyzer(
    analysis::LiveSpectrumAnalyzer* analyzer)
{
    impl_->attach_analyzer(analyzer);
}

}  // namespace rgsml::platform::windows
