#include "source_waveform_view_model.hpp"

#include <rgsml/audio/wav_reader.hpp>
#include <rgsml/platform/windows/windows_resource_reader.hpp>

#include <QMetaObject>

#include <limits>
#include <new>
#include <string>
#include <utility>

namespace rgsml::app {
namespace {

[[nodiscard]] core::Result<audio::WaveformSummary> build_failure(
    core::ErrorCode code,
    std::string message)
{
    return core::Result<audio::WaveformSummary>::failure(
        core::Error{code, std::move(message)});
}

[[nodiscard]] QString waveform_user_message(core::ErrorCode code)
{
    switch (code) {
    case core::ErrorCode::ResourceNotFound:
        return QStringLiteral("Waveform Source is no longer available. Select it again.");
    case core::ErrorCode::AccessDenied:
        return QStringLiteral("Waveform Source cannot be read. Check access and retry.");
    case core::ErrorCode::InvalidAudioSample:
    case core::ErrorCode::MalformedAudioContainer:
    case core::ErrorCode::TruncatedAudioData:
        return QStringLiteral("Waveform analysis found invalid or incomplete Source audio.");
    default:
        return QStringLiteral("Waveform analysis failed. The Source and playback are unchanged.");
    }
}

}  // namespace

SourceWaveformViewModel::SourceWaveformViewModel(
    ui::WaveformPresentation* presentation,
    QObject* parent)
    : SourceWaveformViewModel(
          presentation,
          &SourceWaveformViewModel::build_from_windows_source,
          parent)
{
}

SourceWaveformViewModel::SourceWaveformViewModel(
    ui::WaveformPresentation* presentation,
    BuildFunction buildFunction,
    QObject* parent)
    : QObject(parent)
    , presentation_(presentation)
    , buildFunction_(std::move(buildFunction))
    , worker_([this] { worker_loop(); })
{
    if (presentation_) {
        connect(
            presentation_,
            &ui::WaveformPresentation::retryRequested,
            this,
            &SourceWaveformViewModel::retry);
        presentation_->publish_empty();
    }
}

SourceWaveformViewModel::~SourceWaveformViewModel()
{
    {
        const std::scoped_lock lock{mutex_};
        stopping_ = true;
        pending_.reset();
        activeStop_.request_stop();
    }
    condition_.notify_one();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void SourceWaveformViewModel::source_committed(
    const core::ResourceReference& source)
{
    currentReference_ = source;
    enqueue(source);
}

void SourceWaveformViewModel::retry()
{
    if (currentReference_) {
        enqueue(*currentReference_);
    }
}

std::uint64_t SourceWaveformViewModel::generation() const noexcept
{
    return generation_;
}

std::uint64_t SourceWaveformViewModel::source_replacement_cancellations() const noexcept
{
    return sourceReplacementCancellations_;
}

std::uint64_t SourceWaveformViewModel::stale_results_discarded() const noexcept
{
    return staleResultsDiscarded_;
}

core::Result<audio::WaveformSummary>
SourceWaveformViewModel::build_from_windows_source(
    const core::ResourceReference& source,
    std::stop_token stopToken)
{
    auto resource = platform::windows::WindowsResourceReader::open_read_only(source);
    if (!resource) {
        return core::Result<audio::WaveformSummary>::failure(*resource.error());
    }
    auto wav = audio::WavReader::open(std::move(*resource.value()));
    if (!wav) {
        return core::Result<audio::WaveformSummary>::failure(*wav.error());
    }
    auto summary = audio::build_waveform_summary(**wav.value(), stopToken);
    const auto close = (*wav.value())->close();
    if (!summary) {
        return summary;
    }
    if (!close) {
        return core::Result<audio::WaveformSummary>::failure(*close.error());
    }
    return summary;
}

void SourceWaveformViewModel::enqueue(const core::ResourceReference& source)
{
    if (generation_ == std::numeric_limits<std::uint64_t>::max()) {
        currentSummary_.reset();
        if (presentation_) {
            presentation_->publish_failed(
                QStringLiteral("Waveform generation counter is exhausted."));
        }
        return;
    }
    ++generation_;
    currentSummary_.reset();
    if (presentation_) {
        presentation_->publish_building();
    }
    {
        const std::scoped_lock lock{mutex_};
        if (workerActive_ || pending_) {
            ++sourceReplacementCancellations_;
        }
        activeStop_.request_stop();
        pending_ = Job{generation_, source};
    }
    condition_.notify_one();
}

void SourceWaveformViewModel::worker_loop()
{
    while (true) {
        std::optional<Job> job;
        std::stop_token activeToken;
        {
            std::unique_lock lock{mutex_};
            condition_.wait(lock, [this] { return stopping_ || pending_.has_value(); });
            if (stopping_) {
                return;
            }
            job = std::move(pending_);
            pending_.reset();
            activeStop_ = std::stop_source{};
            activeToken = activeStop_.get_token();
            workerActive_ = true;
        }

        auto result = [&]() -> core::Result<audio::WaveformSummary> {
            try {
                if (!buildFunction_) {
                    return build_failure(
                        core::ErrorCode::InvalidState,
                        "Waveform build function is unavailable.");
                }
                return buildFunction_(job->reference, activeToken);
            } catch (const std::bad_alloc&) {
                return build_failure(
                    core::ErrorCode::IoFailure,
                    "Waveform worker could not allocate bounded state.");
            } catch (...) {
                return build_failure(
                    core::ErrorCode::InvalidState,
                    "Waveform worker failed unexpectedly.");
            }
        }();
        const bool cancelled = activeToken.stop_requested();
        auto outcome = std::make_shared<core::Result<audio::WaveformSummary>>(
            std::move(result));
        {
            const std::scoped_lock lock{mutex_};
            workerActive_ = false;
        }
        static_cast<void>(QMetaObject::invokeMethod(
            this,
            [this, generation = job->generation, outcome, cancelled] {
                publish_result(generation, outcome, cancelled);
            },
            Qt::QueuedConnection));
    }
}

void SourceWaveformViewModel::publish_result(
    std::uint64_t generation,
    std::shared_ptr<core::Result<audio::WaveformSummary>> outcome,
    bool cancelled)
{
    if (generation != generation_) {
        ++staleResultsDiscarded_;
        return;
    }
    if (cancelled) {
        return;
    }
    if (!outcome || !*outcome) {
        currentSummary_.reset();
        if (presentation_) {
            const auto code = outcome && outcome->error()
                ? outcome->error()->code()
                : core::ErrorCode::InvalidState;
            presentation_->publish_failed(waveform_user_message(code));
        }
        return;
    }
    currentSummary_ = std::make_shared<audio::WaveformSummary>(
        std::move(*outcome->value()));
    if (presentation_) {
        presentation_->publish_ready(currentSummary_);
    }
}

}  // namespace rgsml::app
