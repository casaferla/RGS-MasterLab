#include "playback_support.hpp"

#include <rgsml/audio/audio_buffer.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <new>
#include <string>
#include <utility>

namespace rgsml::platform::windows::internal {
namespace {

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

[[nodiscard]] std::size_t bytes_per_sample(DeviceSampleFormat format) noexcept
{
    return format == DeviceSampleFormat::IEEE_F32 ? 4U : 2U;
}

void append_u16_le(std::vector<std::byte>& bytes, std::uint16_t value)
{
    bytes.push_back(static_cast<std::byte>(value & 0xffU));
    bytes.push_back(static_cast<std::byte>((value >> 8U) & 0xffU));
}

void append_u32_le(std::vector<std::byte>& bytes, std::uint32_t value)
{
    for (unsigned shift = 0U; shift < 32U; shift += 8U) {
        bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
    }
}

[[nodiscard]] std::int64_t round_ties_to_even(double value) noexcept
{
    const double lower = std::floor(value);
    const double fraction = value - lower;
    if (fraction < 0.5) {
        return static_cast<std::int64_t>(lower);
    }
    if (fraction > 0.5) {
        return static_cast<std::int64_t>(lower + 1.0);
    }
    const auto lowerInteger = static_cast<std::int64_t>(lower);
    return (lowerInteger % 2) == 0 ? lowerInteger : lowerInteger + 1;
}

}  // namespace

core::Result<DeviceFormat> select_device_format(
    const audio::AudioFormat& sourceFormat,
    bool exactFloat32Supported,
    bool exactPcm16Supported,
    bool pairedFloat32Supported,
    bool pairedPcm16Supported)
{
    const auto rate = sourceFormat.sample_rate().value();
    if (rate <= 0 || rate > std::numeric_limits<int>::max()) {
        return failure<DeviceFormat>(
            core::ErrorCode::OutOfRange,
            "Source sample rate cannot be represented by the audio device format.");
    }
    const auto channels = sourceFormat.channel_count();
    if (channels != 1U && channels != 2U) {
        return failure<DeviceFormat>(
            core::ErrorCode::UnsupportedAudioLayout,
            "Source channel layout is unsupported by the playback adapter.");
    }
    if (exactFloat32Supported) {
        return core::Result<DeviceFormat>::success(DeviceFormat{
            static_cast<int>(rate),
            channels,
            DeviceSampleFormat::IEEE_F32,
            false});
    }
    if (exactPcm16Supported) {
        return core::Result<DeviceFormat>::success(DeviceFormat{
            static_cast<int>(rate),
            channels,
            DeviceSampleFormat::PCM_S16,
            false});
    }
    const std::int64_t pairedRate = rate == 44'100
        ? 48'000
        : (rate == 48'000 ? 44'100 : 0);
    if (pairedRate != 0 && pairedFloat32Supported) {
        return core::Result<DeviceFormat>::success(DeviceFormat{
            static_cast<int>(pairedRate),
            channels,
            DeviceSampleFormat::IEEE_F32,
            true});
    }
    if (pairedRate != 0 && pairedPcm16Supported) {
        return core::Result<DeviceFormat>::success(DeviceFormat{
            static_cast<int>(pairedRate),
            channels,
            DeviceSampleFormat::PCM_S16,
            true});
    }
    return failure<DeviceFormat>(
        core::ErrorCode::UnsupportedOperation,
        "Default output supports neither the exact nor authorized paired Source rate.");
}

core::Result<std::vector<std::byte>> encode_device_block(
    audio::AudioBufferView source,
    DeviceSampleFormat destinationFormat)
{
    const auto frames = source.frame_count().value();
    const auto channels = source.format().channel_count();
    if (frames < 0 || (channels != 1U && channels != 2U)) {
        return failure<std::vector<std::byte>>(
            core::ErrorCode::InvalidArgument,
            "Canonical audio block metadata is invalid for playback.");
    }

    const auto frameCount = static_cast<std::uint64_t>(frames);
    const auto sampleBytes = bytes_per_sample(destinationFormat);
    if (frameCount != 0U
        && channels > std::numeric_limits<std::size_t>::max() / frameCount) {
        return failure<std::vector<std::byte>>(
            core::ErrorCode::IntegerOverflow,
            "Playback sample count overflowed.");
    }
    const auto sampleCount = static_cast<std::size_t>(frameCount) * channels;
    if (sampleCount != 0U
        && sampleBytes > std::numeric_limits<std::size_t>::max() / sampleCount) {
        return failure<std::vector<std::byte>>(
            core::ErrorCode::IntegerOverflow,
            "Playback byte count overflowed.");
    }

    std::array<std::span<const double>, 2> planes{};
    for (std::size_t channel = 0; channel < channels; ++channel) {
        auto plane = source.channel(channel);
        if (!plane) {
            return core::Result<std::vector<std::byte>>::failure(*plane.error());
        }
        planes[channel] = *plane.value();
    }

    std::vector<std::byte> result;
    try {
        result.reserve(sampleCount * sampleBytes);
    } catch (const std::bad_alloc&) {
        return failure<std::vector<std::byte>>(
            core::ErrorCode::IoFailure,
            "Unable to allocate bounded playback conversion storage.");
    }

    for (std::size_t frame = 0; frame < static_cast<std::size_t>(frameCount); ++frame) {
        for (std::size_t channel = 0; channel < channels; ++channel) {
            const double sample = planes[channel][frame];
            if (!std::isfinite(sample)) {
                return failure<std::vector<std::byte>>(
                    core::ErrorCode::InvalidAudioSample,
                    "Non-finite canonical sample reached the playback bridge.");
            }
            if (destinationFormat == DeviceSampleFormat::IEEE_F32) {
                const float converted = static_cast<float>(sample);
                if (!std::isfinite(converted)) {
                    return failure<std::vector<std::byte>>(
                        core::ErrorCode::InvalidAudioSample,
                        "Canonical sample is not representable as finite binary32.");
                }
                append_u32_le(result, std::bit_cast<std::uint32_t>(converted));
                continue;
            }

            if (sample < -1.0 || sample > 1.0) {
                return failure<std::vector<std::byte>>(
                    core::ErrorCode::InvalidAudioSample,
                    "Canonical sample is outside the PCM16 audition range.");
            }
            const auto rounded = round_ties_to_even(sample * 32768.0);
            const auto clamped = std::clamp<std::int64_t>(rounded, -32768, 32767);
            const auto signedCode = static_cast<std::int16_t>(clamped);
            append_u16_le(result, std::bit_cast<std::uint16_t>(signedCode));
        }
    }
    return core::Result<std::vector<std::byte>>::success(std::move(result));
}

PlaybackEngine::~PlaybackEngine() noexcept
{
    if (output_) {
        static_cast<void>(output_->stop());
    }
    if (reader_) {
        static_cast<void>(reader_->close());
    }
}

core::Status PlaybackEngine::install_candidate(
    std::unique_ptr<audio::WavReader> reader,
    std::unique_ptr<IPlaybackOutput> output,
    DeviceSampleFormat sampleFormat,
    std::optional<audio::PlaybackSampleRateAdapter> rateAdapter)
{
    if (!reader || !output) {
        return status_failure(
            core::ErrorCode::InvalidArgument,
            "Playback candidate requires a decoder and output.");
    }

    if (output_) {
        auto stopped = output_->stop();
        if (!stopped) {
            return stopped;
        }
    }
    if (reader_) {
        static_cast<void>(reader_->close());
    }

    duration_ = reader->info().frame_count();
    outputDuration_ = rateAdapter
        ? rateAdapter->output_frame_count()
        : reader->info().frame_count();
    reader_ = std::move(reader);
    output_ = std::move(output);
    sampleFormat_ = sampleFormat;
    rateAdapter_ = std::move(rateAdapter);
    state_ = core::PlaybackState::STOPPED;
    position_ = core::FrameIndex{0};
    loop_.reset();
    runtimeError_.reset();
    reset_queue_state(0);
    return core::Status::success();
}

core::Status PlaybackEngine::clear()
{
    if (output_) {
        auto stopped = output_->stop();
        if (!stopped) {
            return stopped;
        }
    }
    if (reader_) {
        auto closed = reader_->close();
        if (!closed) {
            return closed;
        }
    }
    reader_.reset();
    output_.reset();
    sampleFormat_.reset();
    rateAdapter_.reset();
    duration_.reset();
    outputDuration_.reset();
    loop_.reset();
    runtimeError_.reset();
    state_ = core::PlaybackState::NO_SOURCE;
    position_ = core::FrameIndex{0};
    reset_queue_state(0);
    return core::Status::success();
}

core::Status PlaybackEngine::play()
{
    if (!reader_ || !output_ || !duration_ || !outputDuration_ || !sampleFormat_) {
        return status_failure(
            core::ErrorCode::InvalidState,
            "Playback requires a prepared Source.");
    }
    if (runtimeError_) {
        return core::Status::failure(*runtimeError_);
    }
    if (state_ == core::PlaybackState::PLAYING) {
        return core::Status::success();
    }
    if (state_ == core::PlaybackState::PAUSED
        && output_->state() == OutputState::SUSPENDED) {
        auto resumed = output_->resume();
        if (!resumed) {
            return resumed;
        }
        state_ = core::PlaybackState::PLAYING;
        return core::Status::success();
    }

    if (position_.value() == duration_->value()) {
        position_ = core::FrameIndex{0};
    }
    auto stopped = output_->stop();
    if (!stopped) {
        return stopped;
    }
    output_->clear_queue();
    reset_queue_state(position_.value());
    auto filled = prefill();
    if (!filled) {
        output_->clear_queue();
        reset_queue_state(position_.value());
        return filled;
    }
    if (output_->queued_bytes() == 0U && eofScheduled_) {
        position_ = core::FrameIndex{duration_->value()};
        state_ = core::PlaybackState::STOPPED;
        return core::Status::success();
    }
    auto started = output_->start();
    if (!started) {
        output_->clear_queue();
        reset_queue_state(position_.value());
        return started;
    }
    playbackStartOutputFrame_ = source_to_output_frame(position_.value());
    state_ = core::PlaybackState::PLAYING;
    return core::Status::success();
}

core::Status PlaybackEngine::pause()
{
    if (state_ != core::PlaybackState::PLAYING || !output_) {
        return status_failure(
            core::ErrorCode::InvalidState,
            "Pause is valid only while playing.");
    }
    update_position();
    auto suspended = output_->suspend();
    if (!suspended) {
        return suspended;
    }
    state_ = core::PlaybackState::PAUSED;
    return core::Status::success();
}

core::Status PlaybackEngine::stop()
{
    if (!reader_ || !output_) {
        return status_failure(
            core::ErrorCode::InvalidState,
            "Stop requires a prepared Source.");
    }
    auto stopped = output_->stop();
    if (!stopped) {
        return stopped;
    }
    output_->clear_queue();
    state_ = core::PlaybackState::STOPPED;
    position_ = core::FrameIndex{0};
    runtimeError_.reset();
    reset_queue_state(0);
    return core::Status::success();
}

core::Status PlaybackEngine::seek(core::FrameIndex position)
{
    if (!reader_ || !output_ || !duration_ || !outputDuration_) {
        return status_failure(
            core::ErrorCode::InvalidState,
            "Seek requires a prepared Source.");
    }
    if (position.value() < 0 || position.value() > duration_->value()) {
        return status_failure(
            core::ErrorCode::OutOfRange,
            "Playback seek position is out of range.");
    }

    const auto previousState = state_;
    auto stopped = output_->stop();
    if (!stopped) {
        return stopped;
    }
    output_->clear_queue();
    position_ = position;
    runtimeError_.reset();
    reset_queue_state(position.value());
    if (previousState == core::PlaybackState::PLAYING) {
        state_ = core::PlaybackState::STOPPED;
        auto restarted = play();
        if (!restarted) {
            state_ = core::PlaybackState::STOPPED;
            position_ = position;
            return restarted;
        }
    } else {
        state_ = previousState;
    }
    return core::Status::success();
}

core::Status PlaybackEngine::set_loop(std::optional<core::FrameRange> loop)
{
    if (!reader_ || !duration_ || !outputDuration_) {
        return status_failure(
            core::ErrorCode::InvalidState,
            "Loop configuration requires a prepared Source.");
    }
    if (loop) {
        if (loop->begin().value() < 0
            || loop->end().value() <= loop->begin().value()
            || loop->end().value() > duration_->value()) {
            return status_failure(
                core::ErrorCode::InvalidFrameRange,
                "Playback loop must be a non-empty range within the Source.");
        }
    }
    loop_ = loop;
    if (state_ != core::PlaybackState::PLAYING || !output_) {
        return core::Status::success();
    }

    update_position();
    auto stopped = output_->stop();
    if (!stopped) {
        return stopped;
    }
    output_->clear_queue();
    if (loop_ && position_.value() >= loop_->end().value()) {
        position_ = loop_->begin();
    }
    reset_queue_state(position_.value());
    state_ = core::PlaybackState::STOPPED;
    return play();
}

core::Result<core::PlaybackSnapshot> PlaybackEngine::snapshot() const
{
    if (runtimeError_) {
        return core::Result<core::PlaybackSnapshot>::failure(*runtimeError_);
    }
    return core::Result<core::PlaybackSnapshot>::success(core::PlaybackSnapshot{
        state_, position_, duration_, loop_});
}

void PlaybackEngine::tick()
{
    if (state_ != core::PlaybackState::PLAYING || !output_) {
        return;
    }
    if (auto backendError = output_->error()) {
        update_position();
        record_runtime_error(std::move(*backendError));
        return;
    }

    update_position();
    for (std::size_t iteration = 0;
         iteration < kMaximumPumpIterations && output_->writable_bytes() > 0U;
         ++iteration) {
        auto pumped = pump_once();
        if (!pumped) {
            record_runtime_error(*pumped.error());
            return;
        }
        if ((pendingBytes_.empty() && eofScheduled_)
            || output_->writable_bytes() == 0U) {
            break;
        }
    }

    if (eofScheduled_ && pendingBytes_.empty() && output_->queued_bytes() == 0U
        && (output_->state() == OutputState::IDLE
            || output_->state() == OutputState::STOPPED)) {
        static_cast<void>(output_->stop());
        state_ = core::PlaybackState::STOPPED;
        position_ = core::FrameIndex{duration_->value()};
        reset_queue_state(duration_->value());
    }
}

core::Status PlaybackEngine::prefill()
{
    for (std::size_t iteration = 0;
         iteration < kMaximumPumpIterations && output_->writable_bytes() > 0U;
         ++iteration) {
        auto pumped = pump_once();
        if (!pumped) {
            return pumped;
        }
        if ((pendingBytes_.empty() && eofScheduled_)
            || output_->writable_bytes() == 0U) {
            break;
        }
    }
    return core::Status::success();
}

core::Status PlaybackEngine::pump_once()
{
    if (!reader_ || !output_ || !duration_ || !outputDuration_ || !sampleFormat_) {
        return status_failure(
            core::ErrorCode::InvalidState,
            "Playback pump has no prepared session.");
    }

    if (!pendingBytes_.empty()) {
        const auto remaining = std::span<const std::byte>{pendingBytes_}.subspan(
            pendingOffset_);
        const auto offered = remaining.first(
            std::min(remaining.size(), output_->writable_bytes()));
        if (offered.empty()) {
            return core::Status::success();
        }
        auto written = output_->enqueue(offered);
        if (!written) {
            return core::Status::failure(*written.error());
        }
        if (*written.value() > offered.size()) {
            return status_failure(
                core::ErrorCode::IoFailure,
                "Playback output accepted an invalid byte count.");
        }
        pendingOffset_ += *written.value();
        if (pendingOffset_ == pendingBytes_.size()) {
            pendingBytes_.clear();
            pendingOffset_ = 0U;
        }
        return core::Status::success();
    }

    if (eofScheduled_) {
        return core::Status::success();
    }

    const std::int64_t boundary = output_boundary();
    if (scheduledOutputFrame_ >= boundary) {
        if (loop_) {
            scheduledOutputFrame_ =
                source_to_output_frame(loop_->begin().value());
        } else {
            eofScheduled_ = true;
            return core::Status::success();
        }
    }
    const auto frames = std::min(
        kDecodeBlockFrames, boundary - scheduledOutputFrame_);
    auto blockFrameCount = core::FrameCount::create(frames);
    if (!blockFrameCount) {
        return core::Status::failure(*blockFrameCount.error());
    }
    auto blockFormat = reader_->info().audio_format();
    auto blockDomain = audio::FrameDomainId::SOURCE_PROCESSING_RATE;
    if (rateAdapter_) {
        auto outputFormat = audio::AudioFormat::create(
            rateAdapter_->output_rate(),
            reader_->info().audio_format().channel_layout());
        if (!outputFormat) {
            return core::Status::failure(*outputFormat.error());
        }
        blockFormat = *outputFormat.value();
        blockDomain = audio::FrameDomainId::OUTPUT_RATE;
    }
    auto block = audio::AudioBuffer::create(
        blockFormat,
        blockDomain,
        core::FrameIndex{scheduledOutputFrame_},
        *blockFrameCount.value());
    if (!block) {
        return core::Status::failure(*block.error());
    }
    auto decoded = rateAdapter_
        ? rateAdapter_->read_frames(
            *reader_,
            core::FrameIndex{scheduledOutputFrame_},
            block.value()->mutable_view())
        : reader_->read_frames(
            core::FrameIndex{scheduledOutputFrame_},
            block.value()->mutable_view());
    if (!decoded) {
        return core::Status::failure(*decoded.error());
    }
    if (decoded.value()->value() != frames) {
        return status_failure(
            core::ErrorCode::TruncatedAudioData,
            "Playback decoder returned an unexpected short block.");
    }
    auto encoded = encode_device_block(block.value()->view(), *sampleFormat_);
    if (!encoded) {
        return core::Status::failure(*encoded.error());
    }
    pendingBytes_ = std::move(*encoded.value());
    pendingOffset_ = 0U;
    scheduledOutputFrame_ += frames;
    if (!loop_ && scheduledOutputFrame_ == outputDuration_->value()) {
        eofScheduled_ = true;
    } else if (loop_ && scheduledOutputFrame_ == output_boundary()) {
        scheduledOutputFrame_ =
            source_to_output_frame(loop_->begin().value());
    }
    return pump_once();
}

void PlaybackEngine::update_position() noexcept
{
    if (!output_ || !duration_ || state_ == core::PlaybackState::NO_SOURCE) {
        return;
    }
    const auto processed = std::max<std::int64_t>(0, output_->processed_frames());
    std::int64_t candidateOutput = playbackStartOutputFrame_;
    if (processed <= std::numeric_limits<std::int64_t>::max() - candidateOutput) {
        candidateOutput += processed;
    } else {
        candidateOutput = outputDuration_->value();
    }
    if (loop_) {
        const auto loopBeginOutput =
            source_to_output_frame(loop_->begin().value());
        const auto loopEndOutput =
            source_to_output_frame(loop_->end().value());
        if (candidateOutput >= loopEndOutput) {
            const auto loopLength = loopEndOutput - loopBeginOutput;
            candidateOutput = loopBeginOutput
                + ((candidateOutput - loopEndOutput) % loopLength);
        }
    } else {
        candidateOutput = std::min(candidateOutput, outputDuration_->value());
    }
    position_ = core::FrameIndex{std::max<std::int64_t>(
        0, output_to_source_frame(candidateOutput))};
}

void PlaybackEngine::record_runtime_error(core::Error error) noexcept
{
    if (output_) {
        static_cast<void>(output_->stop());
        output_->clear_queue();
    }
    state_ = core::PlaybackState::STOPPED;
    pendingBytes_.clear();
    pendingOffset_ = 0U;
    runtimeError_ = std::move(error);
}

void PlaybackEngine::reset_queue_state(std::int64_t frame) noexcept
{
    scheduledOutputFrame_ = source_to_output_frame(frame);
    playbackStartOutputFrame_ = scheduledOutputFrame_;
    pendingBytes_.clear();
    pendingOffset_ = 0U;
    eofScheduled_ = false;
}

std::int64_t PlaybackEngine::source_to_output_frame(
    std::int64_t sourceFrame) const noexcept
{
    if (!rateAdapter_) {
        return sourceFrame;
    }
    auto mapped = rateAdapter_->map_input_frame_to_output(
        core::FrameIndex{sourceFrame});
    return mapped ? mapped.value()->value() : outputDuration_->value();
}

std::int64_t PlaybackEngine::output_to_source_frame(
    std::int64_t outputFrame) const noexcept
{
    if (!rateAdapter_) {
        return outputFrame;
    }
    auto mapped = rateAdapter_->map_output_frame_to_input_cursor(
        core::FrameIndex{outputFrame});
    return mapped ? mapped.value()->value() : duration_->value();
}

std::int64_t PlaybackEngine::output_boundary() const noexcept
{
    return loop_
        ? source_to_output_frame(loop_->end().value())
        : outputDuration_->value();
}

}  // namespace rgsml::platform::windows::internal
