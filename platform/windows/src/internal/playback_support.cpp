#include "playback_support.hpp"

#include <rgsml/audio/audio_buffer.hpp>

#include "playback_src_input.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <new>
#include <string>
#include <utility>

namespace rgsml::platform::windows::internal {
class IPlaybackSource : public audio::internal::PlaybackSrcInput {
public:
    virtual ~IPlaybackSource() noexcept = default;
    [[nodiscard]] virtual core::FrameIndex absolute_begin() const noexcept = 0;
    [[nodiscard]] virtual core::Status close() = 0;
};

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

class WavPlaybackSource final : public IPlaybackSource {
public:
    explicit WavPlaybackSource(std::unique_ptr<audio::WavReader> reader) noexcept
        : reader_(std::move(reader))
    {
    }
    [[nodiscard]] const audio::AudioFormat& format() const noexcept override
    {
        return reader_->info().audio_format();
    }
    [[nodiscard]] core::FrameIndex absolute_begin() const noexcept override
    {
        return core::FrameIndex{0};
    }
    [[nodiscard]] core::FrameCount frame_count() const noexcept override
    {
        return reader_->info().frame_count();
    }
    [[nodiscard]] core::Result<core::FrameCount> read_frames(
        core::FrameIndex localStart,
        audio::MutableAudioBufferView destination) override
    {
        return reader_->read_frames(localStart, destination);
    }
    [[nodiscard]] core::Status close() override { return reader_->close(); }

private:
    std::unique_ptr<audio::WavReader> reader_;
};

class PcmPlaybackSource final : public IPlaybackSource {
public:
    explicit PcmPlaybackSource(
        audio::AudioBufferView source,
        std::shared_ptr<const void> lifetime = nullptr) noexcept
        : source_(source)
        , lifetime_(std::move(lifetime))
    {
    }
    [[nodiscard]] const audio::AudioFormat& format() const noexcept override
    {
        return source_.format();
    }
    [[nodiscard]] core::FrameIndex absolute_begin() const noexcept override
    {
        return source_.absolute_start_frame();
    }
    [[nodiscard]] core::FrameCount frame_count() const noexcept override
    {
        return source_.frame_count();
    }
    [[nodiscard]] core::Result<core::FrameCount> read_frames(
        core::FrameIndex localStart,
        audio::MutableAudioBufferView destination) override
    {
        const auto absoluteStart = core::FrameIndex{
            source_.absolute_start_frame().value() + localStart.value()};
        auto input = source_.subview(absoluteStart, destination.frame_count());
        if (!input) {
            return core::Result<core::FrameCount>::failure(*input.error());
        }
        if (input.value()->format() != destination.format()
            || destination.timebase().frame_domain_id()
                != audio::FrameDomainId::SOURCE_PROCESSING_RATE) {
            return failure<core::FrameCount>(
                core::ErrorCode::InvalidArgument,
                "PCM playback block metadata is incoherent.");
        }
        for (std::size_t channel = 0;
             channel < input.value()->format().channel_count();
             ++channel) {
            auto sourceChannel = input.value()->channel(channel);
            auto destinationChannel = destination.channel(channel);
            if (!sourceChannel || !destinationChannel) {
                return core::Result<core::FrameCount>::failure(
                    *(sourceChannel ? destinationChannel.error() : sourceChannel.error()));
            }
            std::copy(
                sourceChannel.value()->begin(), sourceChannel.value()->end(),
                destinationChannel.value()->begin());
        }
        return core::Result<core::FrameCount>::success(destination.frame_count());
    }
    [[nodiscard]] core::Status close() override
    {
        return core::Status::success();
    }

private:
    audio::AudioBufferView source_;
    std::shared_ptr<const void> lifetime_;
};

}  // namespace

PlaybackEngine::PlaybackEngine() = default;

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
    if (source_) {
        static_cast<void>(source_->close());
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
    if (source_) {
        static_cast<void>(source_->close());
    }

    sourceBegin_ = core::FrameIndex{0};
    duration_ = reader->info().frame_count();
    outputDuration_ = rateAdapter
        ? rateAdapter->output_frame_count()
        : reader->info().frame_count();
    try {
        source_ = std::make_unique<WavPlaybackSource>(std::move(reader));
    } catch (const std::bad_alloc&) {
        return status_failure(
            core::ErrorCode::IoFailure,
            "Unable to allocate the WAV playback input source.");
    }
    output_ = std::move(output);
    sampleFormat_ = sampleFormat;
    rateAdapter_ = std::move(rateAdapter);
    state_ = core::PlaybackState::STOPPED;
    position_ = core::FrameIndex{0};
    loop_.reset();
    loopTraversalEligible_ = false;
    runtimeError_.reset();
    activeRealizationId_.reset();
    pendingHandoff_.reset();
    baseLoopWrapCount_ = 0;
    reset_queue_state(0);
    return core::Status::success();
}

core::Status PlaybackEngine::handoff_pcm(
    audio::AudioBufferView source,
    std::shared_ptr<const void> lifetime,
    std::optional<core::RealizationId> realizationId)
{
    if (!has_source() || !output_ || !duration_ || !outputDuration_ || !sampleFormat_) {
        return status_failure(
            core::ErrorCode::InvalidState,
            "Playback handoff requires an active prepared session.");
    }
    if (source.timebase().frame_domain_id()
            != audio::FrameDomainId::SOURCE_PROCESSING_RATE
        || source.frame_count().value() <= 0) {
        return status_failure(
            core::ErrorCode::InvalidArgument,
            "PCM handoff requires a non-empty Source-domain canonical view.");
    }
    if (source.format() != source_format()) {
        return status_failure(
            core::ErrorCode::InvalidArgument,
            "PCM handoff candidate format does not match active playback format.");
    }
    if (rateAdapter_
        && (rateAdapter_->input_rate() != source.format().sample_rate()
            || rateAdapter_->channel_layout()
                != source.format().channel_layout()
            || rateAdapter_->input_frame_count() != source.frame_count())) {
        return status_failure(
            core::ErrorCode::InvalidArgument,
            "PCM handoff SRC metadata is incoherent with candidate view.");
    }

    const auto newBegin = source.absolute_start_frame();
    const auto newEnd = source.absolute_end_frame();
    const auto previousRealizationId = activeRealizationId_;
    if (state_ == core::PlaybackState::PLAYING) {
        update_position();
    }

    auto candidatePosition = position_;
    if (candidatePosition == newEnd) {
        candidatePosition = newBegin;
    } else if (candidatePosition < newBegin || candidatePosition > newEnd) {
        return status_failure(
            core::ErrorCode::OutOfRange,
            "Current playback cue is outside candidate handoff realization range.");
    }

    if (loop_) {
        if (loop_->begin() < newBegin || loop_->end() > newEnd) {
            return status_failure(
                core::ErrorCode::InvalidFrameRange,
                "Armed loop range is outside candidate realization range.");
        }
    }

    auto newDuration = core::FrameCount::create(newEnd.value());
    if (!newDuration) {
        return core::Status::failure(*newDuration.error());
    }

    std::unique_ptr<IPlaybackSource> newSource;
    try {
        newSource = std::make_unique<PcmPlaybackSource>(source, std::move(lifetime));
    } catch (const std::bad_alloc&) {
        return status_failure(
            core::ErrorCode::IoFailure,
            "Unable to allocate PCM playback source during handoff.");
    }

    if (state_ != core::PlaybackState::PLAYING) {
        if (state_ == core::PlaybackState::PAUSED) {
            auto stopped = output_->stop();
            if (!stopped) {
                return stopped;
            }
            output_->clear_queue();
        }
        source_ = std::move(newSource);
        sourceBegin_ = newBegin;
        duration_ = *newDuration.value();
        outputDuration_ = rateAdapter_
            ? rateAdapter_->output_frame_count()
            : source.frame_count();
        position_ = candidatePosition;
        activeRealizationId_ = realizationId;
        pendingHandoff_.reset();
        reset_queue_state(position_.value());
        return core::Status::success();
    }

    // PLAYING state: determine deterministic future handoff boundary on continuous output timeline
    const std::int64_t scheduledHandoffOutputFrame = scheduledOutputFrame_;
    const std::int64_t handoffSourceFrameValue = output_to_source_frame(scheduledHandoffOutputFrame);
    core::FrameIndex handoffSourceFrame{handoffSourceFrameValue};
    std::int64_t handoffOutputFrame = scheduledHandoffOutputFrame;

    if (handoffSourceFrame == newEnd) {
        if (loop_ && loopTraversalEligible_) {
            handoffSourceFrame = loop_->begin();
            handoffOutputFrame =
                source_to_output_frame(loop_->begin().value());
        } else {
            // All remaining old realization audio is already committed through
            // natural EOF. Preserve that queued material without manufacturing
            // a frame-zero replay; the new realization becomes authoritative
            // for the next explicit playback.
            source_ = std::move(newSource);
            sourceBegin_ = newBegin;
            duration_ = *newDuration.value();
            position_ = candidatePosition;
            outputDuration_ = rateAdapter_
                ? rateAdapter_->output_frame_count()
                : source.frame_count();
            scheduledOutputFrame_ = outputDuration_->value();
            eofScheduled_ = true;
            activeRealizationId_ = realizationId;
            pendingHandoff_ = PendingHandoff{
                previousRealizationId,
                realizationId,
                0,
                0,
                std::nullopt,
                true};
            return core::Status::success();
        }
    } else if (handoffSourceFrame < newBegin || handoffSourceFrame > newEnd) {
        return status_failure(
            core::ErrorCode::OutOfRange,
            "Scheduled handoff position is outside candidate realization range.");
    }

    const double outputRateHz = rateAdapter_
        ? static_cast<double>(rateAdapter_->output_rate().value())
        : static_cast<double>(source.format().sample_rate().value());
    const auto xfadeOutputFramesRequested = static_cast<std::int64_t>(
        std::floor(0.015 * outputRateHz + 0.5));
    const std::int64_t boundary = output_boundary();
    const std::int64_t xfadeOutputFrames = std::max<std::int64_t>(
        0, std::min(xfadeOutputFramesRequested, boundary - handoffOutputFrame));

    const auto currentProcessedFrame = std::max<std::int64_t>(
        0, output_->processed_frames());
    const auto currentOutputFrame = current_output_frame();
    std::int64_t framesUntilHandoff = 0;
    if (loop_ && loopTraversalEligible_
        && handoffOutputFrame < currentOutputFrame) {
        const auto loopBeginOutput =
            source_to_output_frame(loop_->begin().value());
        framesUntilHandoff = std::max<std::int64_t>(
            0,
            (boundary - currentOutputFrame)
                + (handoffOutputFrame - loopBeginOutput));
    } else {
        framesUntilHandoff = std::max<std::int64_t>(
            0, handoffOutputFrame - currentOutputFrame);
    }
    const auto maxFrame = std::numeric_limits<std::int64_t>::max();
    const auto handoffProcessedFrame =
        framesUntilHandoff > maxFrame - currentProcessedFrame
        ? maxFrame
        : currentProcessedFrame + framesUntilHandoff;
    const auto handoffEndProcessedFrame =
        xfadeOutputFrames > maxFrame - handoffProcessedFrame
        ? maxFrame
        : handoffProcessedFrame + xfadeOutputFrames;

    if (xfadeOutputFrames > 0) {
        auto xfadeCount = core::FrameCount::create(xfadeOutputFrames);
        if (!xfadeCount) {
            return core::Status::failure(*xfadeCount.error());
        }

        auto xfadeFormat = source.format();
        auto xfadeDomain = audio::FrameDomainId::SOURCE_PROCESSING_RATE;
        if (rateAdapter_) {
            auto outputFormat = audio::AudioFormat::create(
                rateAdapter_->output_rate(),
                source.format().channel_layout());
            if (!outputFormat) {
                return core::Status::failure(*outputFormat.error());
            }
            xfadeFormat = *outputFormat.value();
            xfadeDomain = audio::FrameDomainId::OUTPUT_RATE;
        }

        const auto xfadeAbsoluteStart = rateAdapter_
            ? core::FrameIndex{handoffOutputFrame}
            : core::FrameIndex{sourceBegin_.value() + handoffOutputFrame};

        auto oldBuffer = audio::AudioBuffer::create(
            xfadeFormat, xfadeDomain, xfadeAbsoluteStart, *xfadeCount.value());
        if (!oldBuffer) {
            return core::Status::failure(*oldBuffer.error());
        }
        auto readOld = rateAdapter_
            ? audio::internal::read_playback_src_frames(
                *rateAdapter_, *source_, core::FrameIndex{handoffOutputFrame}, oldBuffer.value()->mutable_view())
            : read_source_frames(xfadeAbsoluteStart, oldBuffer.value()->mutable_view());
        if (!readOld || readOld.value()->value() != xfadeOutputFrames) {
            return status_failure(
                core::ErrorCode::TruncatedAudioData,
                "Failed to read old source frames for crossfade.");
        }

        auto newBuffer = audio::AudioBuffer::create(
            xfadeFormat, xfadeDomain, xfadeAbsoluteStart, *xfadeCount.value());
        if (!newBuffer) {
            return core::Status::failure(*newBuffer.error());
        }
        auto readNew = rateAdapter_
            ? audio::internal::read_playback_src_frames(
                *rateAdapter_, *newSource, core::FrameIndex{handoffOutputFrame}, newBuffer.value()->mutable_view())
            : newSource->read_frames(
                core::FrameIndex{handoffSourceFrame.value() - newBegin.value()}, newBuffer.value()->mutable_view());
        if (!readNew || readNew.value()->value() != xfadeOutputFrames) {
            return status_failure(
                core::ErrorCode::TruncatedAudioData,
                "Failed to read new source frames for crossfade.");
        }

        auto blendedBuffer = audio::AudioBuffer::create(
            xfadeFormat, xfadeDomain, xfadeAbsoluteStart, *xfadeCount.value());
        if (!blendedBuffer) {
            return core::Status::failure(*blendedBuffer.error());
        }

        const auto channels = xfadeFormat.channel_count();
        const double N = static_cast<double>(xfadeOutputFrames);
        auto blendedView = blendedBuffer.value()->mutable_view();
        for (std::size_t ch = 0; ch < channels; ++ch) {
            auto oldPlane = oldBuffer.value()->view().channel(ch);
            auto newPlane = newBuffer.value()->view().channel(ch);
            auto blendPlane = blendedView.channel(ch);
            if (!oldPlane || !newPlane || !blendPlane) {
                return status_failure(
                    core::ErrorCode::InvalidArgument,
                    "Incoherent channel access during crossfade.");
            }
            for (std::int64_t i = 0; i < xfadeOutputFrames; ++i) {
                const double alpha = static_cast<double>(i) / N;
                const double oldSample = (*oldPlane.value())[static_cast<std::size_t>(i)];
                const double newSample = (*newPlane.value())[static_cast<std::size_t>(i)];
                (*blendPlane.value())[static_cast<std::size_t>(i)] =
                    (1.0 - alpha) * oldSample + alpha * newSample;
            }
        }

        auto encodedCrossfade = encode_device_block(
            blendedBuffer.value()->view(), *sampleFormat_);
        if (!encodedCrossfade) {
            return core::Status::failure(*encodedCrossfade.error());
        }

        // Seamlessly append crossfade block to pendingBytes_ WITHOUT clear_queue() or reset_queue_state()
        if (pendingBytes_.empty()) {
            pendingBytes_ = std::move(*encodedCrossfade.value());
            pendingOffset_ = 0U;
        } else {
            pendingBytes_.insert(
                pendingBytes_.end(),
                encodedCrossfade.value()->begin(),
                encodedCrossfade.value()->end());
        }
        scheduledOutputFrame_ = handoffOutputFrame + xfadeOutputFrames;
    } else {
        scheduledOutputFrame_ = handoffOutputFrame;
    }

    auto previousSource = std::move(source_);
    const auto previousSourceBegin = sourceBegin_;
    const auto previousDuration = duration_;
    const auto previousOutputDuration = outputDuration_;

    source_ = std::move(newSource);
    sourceBegin_ = newBegin;
    duration_ = *newDuration.value();
    outputDuration_ = rateAdapter_
        ? rateAdapter_->output_frame_count()
        : source.frame_count();
    position_ = candidatePosition;

    if (!loop_ && scheduledOutputFrame_ == outputDuration_->value()) {
        eofScheduled_ = true;
    } else if (loop_ && loopTraversalEligible_
        && scheduledOutputFrame_ == output_boundary()) {
        scheduledOutputFrame_ = source_to_output_frame(loop_->begin().value());
    }

    // The first pure-NEW numeric Source frame must be derived from the
    // already-resolved output cursor, not from the device's absolute processed
    // frame counter. scheduledOutputFrame_ already includes the crossfade and,
    // when it lands on loop.end, has been wrapped to loop.begin above.
    const auto newNumericStartSourceFrame =
        output_to_source_frame(scheduledOutputFrame_);

    // Publish the candidate identity before any newly prepared bytes can reach
    // the output queue. If prefill fails after the commit point, fail closed:
    // stop/clear the backend and restore the previously accepted source/ID.
    activeRealizationId_ = realizationId;
    pendingHandoff_ = PendingHandoff{
        previousRealizationId,
        realizationId,
        handoffProcessedFrame,
        handoffEndProcessedFrame,
        newNumericStartSourceFrame,
        false};

    auto filled = prefill();
    if (!filled) {
        static_cast<void>(output_->stop());
        output_->clear_queue();
        source_ = std::move(previousSource);
        sourceBegin_ = previousSourceBegin;
        duration_ = previousDuration;
        outputDuration_ = previousOutputDuration;
        activeRealizationId_ = previousRealizationId;
        pendingHandoff_.reset();
        state_ = core::PlaybackState::STOPPED;
        runtimeError_.reset();
        reset_queue_state(position_.value());
        return filled;
    }
    return core::Status::success();
}

core::Status PlaybackEngine::install_pcm_candidate(
    audio::AudioBufferView source,
    std::unique_ptr<IPlaybackOutput> output,
    DeviceSampleFormat sampleFormat,
    std::optional<audio::PlaybackSampleRateAdapter> rateAdapter,
    std::shared_ptr<const void> lifetime,
    std::optional<core::RealizationId> realizationId)
{
    if (!output
        || source.timebase().frame_domain_id()
            != audio::FrameDomainId::SOURCE_PROCESSING_RATE
        || source.frame_count().value() <= 0) {
        return status_failure(
            core::ErrorCode::InvalidArgument,
            "PCM playback requires a non-empty Source-domain canonical view and output.");
    }
    if (rateAdapter
        && (rateAdapter->input_rate() != source.format().sample_rate()
            || rateAdapter->channel_layout()
                != source.format().channel_layout()
            || rateAdapter->input_frame_count() != source.frame_count())) {
        return status_failure(
            core::ErrorCode::InvalidArgument,
            "PCM playback SRC metadata is incoherent with the immutable Source view.");
    }

    if (output_) {
        auto stopped = output_->stop();
        if (!stopped) {
            return stopped;
        }
    }
    if (source_) {
        static_cast<void>(source_->close());
    }

    auto duration = core::FrameCount::create(source.absolute_end_frame().value());
    if (!duration) {
        return core::Status::failure(*duration.error());
    }
    sourceBegin_ = source.absolute_start_frame();
    duration_ = *duration.value();
    outputDuration_ = rateAdapter
        ? rateAdapter->output_frame_count()
        : source.frame_count();
    try {
        source_ = std::make_unique<PcmPlaybackSource>(source, std::move(lifetime));
    } catch (const std::bad_alloc&) {
        return status_failure(
            core::ErrorCode::IoFailure,
            "Unable to allocate the PCM playback input source.");
    }
    output_ = std::move(output);
    sampleFormat_ = sampleFormat;
    rateAdapter_ = std::move(rateAdapter);
    state_ = core::PlaybackState::STOPPED;
    position_ = sourceBegin_;
    loop_.reset();
    loopTraversalEligible_ = false;
    runtimeError_.reset();
    activeRealizationId_ = realizationId;
    pendingHandoff_.reset();
    reset_queue_state(sourceBegin_.value());
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
    if (source_) {
        auto closed = source_->close();
        if (!closed) {
            return closed;
        }
    }
    source_.reset();
    output_.reset();
    sampleFormat_.reset();
    rateAdapter_.reset();
    duration_.reset();
    outputDuration_.reset();
    loop_.reset();
    loopTraversalEligible_ = false;
    runtimeError_.reset();
    state_ = core::PlaybackState::NO_SOURCE;
    position_ = core::FrameIndex{0};
    sourceBegin_ = core::FrameIndex{0};
    activeRealizationId_.reset();
    pendingHandoff_.reset();
    reset_queue_state(0);
    return core::Status::success();
}

core::Status PlaybackEngine::play()
{
    if (state_ == core::PlaybackState::PAUSED
        && output_ && output_->state() == OutputState::SUSPENDED) {
        auto resumed = output_->resume();
        if (!resumed) {
            return resumed;
        }
        state_ = core::PlaybackState::PLAYING;
        return core::Status::success();
    }
    return play_internal(true);
}

core::Status PlaybackEngine::play_internal(bool isNewTraversal)
{
    if (!has_source() || !output_ || !duration_ || !outputDuration_ || !sampleFormat_) {
        return status_failure(
            core::ErrorCode::InvalidState,
            "Playback requires a prepared Source.");
    }
    if (runtimeError_) {
        return core::Status::failure(*runtimeError_);
    }
    if (state_ == core::PlaybackState::PLAYING && !isNewTraversal) {
        return core::Status::success();
    }

    if (position_.value() == duration_->value()) {
        position_ = sourceBegin_;
        loopTraversalEligible_ = loop_.has_value();
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
    pendingHandoff_.reset();
    if (isNewTraversal) {
        baseLoopWrapCount_ = 0;
        ++traversalSerial_;
    }
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
    if (!has_source() || !output_) {
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
    position_ = sourceBegin_;
    loopTraversalEligible_ = loop_.has_value();
    runtimeError_.reset();
    pendingHandoff_.reset();
    reset_queue_state(sourceBegin_.value());
    return core::Status::success();
}

core::Status PlaybackEngine::seek(core::FrameIndex position)
{
    if (!has_source() || !output_ || !duration_ || !outputDuration_) {
        return status_failure(
            core::ErrorCode::InvalidState,
            "Seek requires a prepared Source.");
    }
    if (position < sourceBegin_ || position.value() > duration_->value()) {
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
    loopTraversalEligible_ = loop_
        && position.value() < loop_->end().value();
    runtimeError_.reset();
    pendingHandoff_.reset();
    baseLoopWrapCount_ = 0;
    reset_queue_state(position.value());
    if (previousState == core::PlaybackState::PLAYING) {
        state_ = core::PlaybackState::STOPPED;
        if (position.value() == duration_->value()) {
            ++seekSerial_;
            return core::Status::success();
        }
        auto restarted = play_internal(false);
        if (!restarted) {
            state_ = core::PlaybackState::STOPPED;
            position_ = position;
            return restarted;
        }
    } else {
        state_ = previousState;
    }
    ++seekSerial_;
    return core::Status::success();
}

core::Status PlaybackEngine::set_loop(std::optional<core::FrameRange> loop)
{
    if (!has_source() || !duration_ || !outputDuration_) {
        return status_failure(
            core::ErrorCode::InvalidState,
            "Loop configuration requires a prepared Source.");
    }
    if (loop_ == loop && (state_ == core::PlaybackState::PLAYING || state_ == core::PlaybackState::PAUSED)) {
        return core::Status::success();
    }
    if (loop) {
        if (loop->begin() < sourceBegin_
            || loop->end().value() <= loop->begin().value()
            || loop->end().value() > duration_->value()) {
            return status_failure(
                core::ErrorCode::InvalidFrameRange,
                "Playback loop must be a non-empty range within the Source.");
        }
    }
    const auto previousPosition = position_;
    const auto previousLoop = loop_;
    const auto previousEligibility = loopTraversalEligible_;
    const bool wasPlaying = state_ == core::PlaybackState::PLAYING;
    const bool wasPaused = state_ == core::PlaybackState::PAUSED;
    if (wasPlaying) {
        update_position();
    }
    if ((wasPlaying || wasPaused) && output_) {
        auto stopped = output_->stop();
        if (!stopped) {
            position_ = previousPosition;
            loop_ = previousLoop;
            loopTraversalEligible_ = previousEligibility;
            return stopped;
        }
        output_->clear_queue();
    }

    loop_ = loop;
    loopTraversalEligible_ = loop_
        && position_.value() < loop_->end().value();
    if (wasPlaying || wasPaused) {
        pendingHandoff_.reset();
    }
    if (!wasPlaying && !wasPaused) {
        return core::Status::success();
    }
    reset_queue_state(position_.value());
    if (wasPaused) {
        state_ = core::PlaybackState::PAUSED;
        return core::Status::success();
    }
    state_ = core::PlaybackState::STOPPED;
    return play_internal(false);
}

std::uint64_t PlaybackEngine::current_loop_wrap_count() const noexcept
{
    if (!output_ || !outputDuration_ || !loop_ || !loopTraversalEligible_) {
        return baseLoopWrapCount_;
    }
    const auto totalProcessed = std::max<std::int64_t>(0, output_->processed_frames());
    const auto relativeProcessed = std::max<std::int64_t>(0, totalProcessed - processedFrameBaseline_);
    const auto loopBeginOutput = source_to_output_frame(loop_->begin().value());
    const auto loopEndOutput = source_to_output_frame(loop_->end().value());
    const auto loopLength = loopEndOutput - loopBeginOutput;
    std::int64_t candidateOutput = playbackStartOutputFrame_ + relativeProcessed;
    if (relativeProcessed > 0 && candidateOutput >= loopEndOutput && loopLength > 0) {
        const auto wraps = static_cast<std::uint64_t>((candidateOutput - loopBeginOutput) / loopLength);
        return baseLoopWrapCount_ + wraps;
    }
    return baseLoopWrapCount_;
}

core::Result<core::PlaybackSnapshot> PlaybackEngine::snapshot() const
{
    if (runtimeError_) {
        return core::Result<core::PlaybackSnapshot>::failure(*runtimeError_);
    }
    return core::Result<core::PlaybackSnapshot>::success(core::PlaybackSnapshot{
        state_, position_, duration_, loop_, audible_realization_state(),
        traversalSerial_, seekSerial_, current_loop_wrap_count()});
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
    if (!has_source() || !output_ || !duration_ || !outputDuration_ || !sampleFormat_) {
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
        if (loop_ && loopTraversalEligible_) {
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
    auto blockFormat = source_format();
    auto blockDomain = audio::FrameDomainId::SOURCE_PROCESSING_RATE;
    if (rateAdapter_) {
        auto outputFormat = audio::AudioFormat::create(
            rateAdapter_->output_rate(),
            source_format().channel_layout());
        if (!outputFormat) {
            return core::Status::failure(*outputFormat.error());
        }
        blockFormat = *outputFormat.value();
        blockDomain = audio::FrameDomainId::OUTPUT_RATE;
    }
    const auto blockAbsoluteStart = rateAdapter_
        ? core::FrameIndex{scheduledOutputFrame_}
        : core::FrameIndex{sourceBegin_.value() + scheduledOutputFrame_};
    auto block = audio::AudioBuffer::create(
        blockFormat,
        blockDomain,
        blockAbsoluteStart,
        *blockFrameCount.value());
    if (!block) {
        return core::Status::failure(*block.error());
    }
    auto decoded = rateAdapter_
        ? audio::internal::read_playback_src_frames(
            *rateAdapter_,
            *source_,
            core::FrameIndex{scheduledOutputFrame_},
            block.value()->mutable_view())
        : read_source_frames(blockAbsoluteStart, block.value()->mutable_view());
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
    } else if (loop_ && loopTraversalEligible_
        && scheduledOutputFrame_ == output_boundary()) {
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
    position_ = core::FrameIndex{std::max<std::int64_t>(
        sourceBegin_.value(), output_to_source_frame(current_output_frame()))};
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
    pendingHandoff_.reset();
    runtimeError_ = std::move(error);
}

void PlaybackEngine::reset_queue_state(std::int64_t frame) noexcept
{
    scheduledOutputFrame_ = source_to_output_frame(frame);
    playbackStartOutputFrame_ = scheduledOutputFrame_;
    pendingBytes_.clear();
    pendingOffset_ = 0U;
    eofScheduled_ = false;
    if (output_) {
        processedFrameBaseline_ = output_->processed_frames();
    } else {
        processedFrameBaseline_ = 0;
    }
}

std::int64_t PlaybackEngine::source_to_output_frame(
    std::int64_t sourceFrame) const noexcept
{
    const auto localFrame = sourceFrame - sourceBegin_.value();
    if (!rateAdapter_) {
        return localFrame;
    }
    auto mapped = rateAdapter_->map_input_frame_to_output(
        core::FrameIndex{localFrame});
    return mapped ? mapped.value()->value() : outputDuration_->value();
}

std::int64_t PlaybackEngine::output_to_source_frame(
    std::int64_t outputFrame) const noexcept
{
    if (!rateAdapter_) {
        return sourceBegin_.value() + outputFrame;
    }
    auto mapped = rateAdapter_->map_output_frame_to_input_cursor(
        core::FrameIndex{outputFrame});
    return mapped
        ? sourceBegin_.value() + mapped.value()->value()
        : duration_->value();
}

std::int64_t PlaybackEngine::output_boundary() const noexcept
{
    return loop_ && loopTraversalEligible_
        ? source_to_output_frame(loop_->end().value())
        : outputDuration_->value();
}

std::int64_t PlaybackEngine::current_output_frame() const noexcept
{
    if (!output_ || !outputDuration_) {
        return 0;
    }
    const auto totalProcessed = std::max<std::int64_t>(
        0, output_->processed_frames());
    const auto relativeProcessed = std::max<std::int64_t>(
        0, totalProcessed - processedFrameBaseline_);
    std::int64_t candidateOutput = playbackStartOutputFrame_;
    if (relativeProcessed
        <= std::numeric_limits<std::int64_t>::max() - candidateOutput) {
        candidateOutput += relativeProcessed;
    } else {
        candidateOutput = outputDuration_->value();
    }
    if (loop_ && loopTraversalEligible_) {
        const auto loopBeginOutput =
            source_to_output_frame(loop_->begin().value());
        const auto loopEndOutput =
            source_to_output_frame(loop_->end().value());
        if (relativeProcessed > 0 && candidateOutput >= loopEndOutput) {
            const auto loopLength = loopEndOutput - loopBeginOutput;
            if (loopLength > 0) {
                candidateOutput = loopBeginOutput
                    + ((candidateOutput - loopEndOutput) % loopLength);
            }
        }
    } else {
        candidateOutput = std::min(candidateOutput, outputDuration_->value());
    }
    return candidateOutput;
}

core::AudibleRealizationState PlaybackEngine::audible_realization_state() const noexcept
{
    const auto unavailable = [] {
        return core::AudibleRealizationState{
            core::AudibleHandoffPhase::UNAVAILABLE, std::nullopt, std::nullopt};
    };
    const auto exclusive = [](std::optional<core::RealizationId> id,
                              core::AudibleHandoffPhase phase,
                              std::optional<std::int64_t> endFrame = std::nullopt) {
        return id
            ? core::AudibleRealizationState{phase, id, endFrame}
            : core::AudibleRealizationState{
                core::AudibleHandoffPhase::UNAVAILABLE, std::nullopt, std::nullopt};
    };

    if (!pendingHandoff_) {
        return exclusive(
            activeRealizationId_, core::AudibleHandoffPhase::NEW);
    }
    if (pendingHandoff_->awaitingReplay) {
        return exclusive(
            pendingHandoff_->oldRealizationId,
            core::AudibleHandoffPhase::OLD);
    }
    if (!output_) {
        return unavailable();
    }

    const auto processedFrame = std::max<std::int64_t>(
        0, output_->processed_frames());
    if (processedFrame < pendingHandoff_->startProcessedFrame) {
        return exclusive(
            pendingHandoff_->oldRealizationId,
            core::AudibleHandoffPhase::OLD);
    }
    if (processedFrame < pendingHandoff_->endProcessedFrame) {
        return core::AudibleRealizationState{
            core::AudibleHandoffPhase::TRANSITION, std::nullopt, std::nullopt};
    }

    if (!pendingHandoff_->newNumericStartSourceFrame.has_value()) {
        return unavailable();
    }
    return exclusive(
        pendingHandoff_->newRealizationId,
        core::AudibleHandoffPhase::NEW,
        pendingHandoff_->newNumericStartSourceFrame);
}

bool PlaybackEngine::has_source() const noexcept
{
    return source_ != nullptr;
}

const audio::AudioFormat& PlaybackEngine::source_format() const noexcept
{
    return source_->format();
}

core::Result<core::FrameCount> PlaybackEngine::read_source_frames(
    core::FrameIndex absoluteStart,
    audio::MutableAudioBufferView destination)
{
    if (!source_) {
        return failure<core::FrameCount>(
            core::ErrorCode::InvalidState,
            "Playback has no prepared input source.");
    }
    return source_->read_frames(
        core::FrameIndex{absoluteStart.value() - sourceBegin_.value()},
        destination);
}

}  // namespace rgsml::platform::windows::internal
