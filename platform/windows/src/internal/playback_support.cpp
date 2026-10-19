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
                != source_.timebase().frame_domain_id()) {
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
    terminalOutputDomain_ = false;
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
    if (terminalOutputDomain_) return status_failure(
        core::ErrorCode::InvalidState,
        "Terminal output requires a separately qualified handoff path.");
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
    // A loop's numeric scheduling cursor wraps when the final block is
    // encoded, while OLD frames can still be queued or pending. At the
    // prefilled loop boundary there are no old frames left to crossfade.
    const bool atPrefilledLoopBoundary =
        loop_ && loopTraversalEligible_
        && handoffOutputFrame
            == source_to_output_frame(loop_->begin().value())
        && (output_->queued_bytes() > 0U || !pendingBytes_.empty());
    const std::int64_t xfadeOutputFrames = atPrefilledLoopBoundary
        ? 0
        : std::max<std::int64_t>(
            0, std::min(xfadeOutputFramesRequested, boundary - handoffOutputFrame));

    // Audible handoff time must follow the actual output backlog rather
    // than the already-wrapped loop-relative scheduling cursor. Count both
    // backend-queued bytes and not-yet-enqueued bytes of the old source.
    const auto currentProcessedFrame = std::max<std::int64_t>(
        0, output_->processed_frames());
    const auto queuedBytes = output_->queued_bytes();
    const auto pendingBytes = pendingBytes_.size() - pendingOffset_;
    if (queuedBytes > std::numeric_limits<std::size_t>::max() - pendingBytes) {
        return status_failure(
            core::ErrorCode::OutOfRange,
            "PCM handoff backlog size overflow.");
    }
    const auto bytesPerFrame =
        source.format().channel_count() * bytes_per_sample(*sampleFormat_);
    const auto backlogBytes = queuedBytes + pendingBytes;
    const auto backlogFrames = backlogBytes / bytesPerFrame
        + (backlogBytes % bytesPerFrame != 0U ? 1U : 0U);
    const auto maxFrame = std::numeric_limits<std::int64_t>::max();
    const auto framesUntilHandoff = static_cast<std::int64_t>(
        std::min<std::size_t>(
            backlogFrames, static_cast<std::size_t>(maxFrame)));
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

        // This queue can contain undrained OLD bytes when we append a mixed
        // OLD/NEW crossfade. A single pending-source stamp CANNOT distinguish
        // the boundary. Conservatively invalidate the provenance of *all*
        // as-yet-unaccepted pending bytes. Previously accepted OLD runs remain
        // exact; the current mixed container MUST NEVER inherit that identity.
        // A future bounded per-block ledger may recover exact boundaries.
        c2PendingOwner_.reset();
        c2PendingExactMapping_ = false;
        c2PendingHandoffPureNew_ = false;
        c2PendingGapReason_ = C2CoverageProbeReason::UNPROVEN_HANDOFF;
        c2AcceptedContainsUnprovenHandoff_ = true; // sticky in this generation

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
        generatedOutputFrames_ += xfadeOutputFrames;
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

    return install_pcm_candidate_accepted(source,std::move(output),
        sampleFormat,std::move(rateAdapter),std::move(lifetime),
        realizationId,false);
}

core::Status PlaybackEngine::install_terminal_output_pcm_candidate(
    audio::AudioBufferView source,
    std::unique_ptr<IPlaybackOutput> output,
    DeviceSampleFormat sampleFormat,
    std::shared_ptr<const void> lifetime,
    core::RealizationId identity,
    core::SampleRate actualDeviceRate)
{
    if (!output || !lifetime || identity.value==0
        || source.timebase().frame_domain_id()!=audio::FrameDomainId::OUTPUT_RATE
        || source.absolute_start_frame().value()!=0
        || source.frame_count().value()<=0
        || source.format().sample_rate()!=actualDeviceRate)
        return status_failure(core::ErrorCode::InvalidArgument,
            "M17 terminal PCM requires owned OUTPUT_RATE with matching device "
            "frequency and nonzero identity; no second SRC is permitted.");
    return install_pcm_candidate_accepted(source,std::move(output),
        sampleFormat,std::nullopt,std::move(lifetime),identity,true);
}

core::Status PlaybackEngine::install_pcm_candidate_accepted(
    audio::AudioBufferView source,
    std::unique_ptr<IPlaybackOutput> output,
    DeviceSampleFormat sampleFormat,
    std::optional<audio::PlaybackSampleRateAdapter> rateAdapter,
    std::shared_ptr<const void> lifetime,
    std::optional<core::RealizationId> realizationId,
    bool terminalOutputDomain)
{
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
    terminalOutputDomain_ = terminalOutputDomain;
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
    terminalOutputDomain_ = false;
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
    const bool wasPlaying = state_ == core::PlaybackState::PLAYING;
    const bool wasPaused = state_ == core::PlaybackState::PAUSED;
    if (!wasPlaying && !wasPaused) {
        loop_ = loop;
        loopTraversalEligible_ = loop_
            && position_.value() < loop_->end().value();
        return core::Status::success();
    }

    // All device bytes already generated (including partially enqueued pending
    // bytes and bytes acquired internally by QAudioSink) are immutable. The
    // switch becomes audible only when those frames have actually played.
    // Neither stop(), clear_queue(), nor reset_queue_state() is allowed here.
    if (wasPlaying) {
        update_position();
    }
    const LoopEpoch oldEpoch = scheduledLoopEpochs_.empty()
        ? initialLoopEpoch_ : scheduledLoopEpochs_.back();
    const auto elapsedInOldEpoch = std::max<std::int64_t>(
        0, generatedOutputFrames_ - oldEpoch.firstGeneratedFrame);
    const auto wrapsBefore = oldEpoch.wrapsBefore
        + wraps_in_loop_epoch(oldEpoch, elapsedInOldEpoch);

    // The next sample must follow the last already generated sample. If
    // enabling a region whose end has already been passed by the ahead-of-
    // playback decoder, enter at the region's begin on the FUTURE boundary.
    const bool replacingPendingPolicy = !scheduledLoopEpochs_.empty()
        && scheduledLoopEpochs_.back().firstGeneratedFrame
            == generatedOutputFrames_;
    const auto uncommittedCursor = replacingPendingPolicy
        ? scheduledLoopEpochs_.back().cursorBeforeUncommittedSwitch
        : scheduledOutputFrame_;
    auto nextOutputFrame = uncommittedCursor;
    auto eligible = loop
        && output_to_source_frame(nextOutputFrame) < loop->end().value();
    if (loop && !eligible) {
        nextOutputFrame = source_to_output_frame(loop->begin().value());
        eligible = true;
    }
    const LoopEpoch nextEpoch{
        generatedOutputFrames_, nextOutputFrame, loop, eligible, wrapsBefore,
        uncommittedCursor};

    try {
        if (!scheduledLoopEpochs_.empty()
            && scheduledLoopEpochs_.back().firstGeneratedFrame
                == generatedOutputFrames_) {
            // Rapid toggles with no intervening decoded frames collapse to
            // one future policy; already committed PCM remains unchanged.
            scheduledLoopEpochs_.back() = nextEpoch;
        } else {
            scheduledLoopEpochs_.push_back(nextEpoch);
        }
    } catch (const std::bad_alloc&) {
        return status_failure(
            core::ErrorCode::IoFailure,
            "Unable to schedule continuous Loop Region reconfiguration.");
    }

    loop_ = loop;
    loopTraversalEligible_ = eligible;
    scheduledOutputFrame_ = nextOutputFrame;
    eofScheduled_ = !eligible
        && scheduledOutputFrame_ >= outputDuration_->value();

    // Keep pendingHandoff_: earlier buffered PCM may still belong to the old
    // audible realization. Clearing it here would falsify GR provenance.
    // PAUSED remains suspended and PLAYING keeps its existing QAudioSink.
    return core::Status::success();
}

PlaybackEngine::LoopEpoch PlaybackEngine::audible_loop_epoch(
    std::int64_t processedFrame) const noexcept
{
    auto epoch = initialLoopEpoch_;
    for (const auto& scheduled : scheduledLoopEpochs_) {
        if (scheduled.firstGeneratedFrame > processedFrame) {
            break;
        }
        epoch = scheduled;
    }
    return epoch;
}

std::uint64_t PlaybackEngine::wraps_in_loop_epoch(
    const LoopEpoch& epoch, std::int64_t elapsedFrames) const noexcept
{
    if (!epoch.loop || !epoch.eligible || elapsedFrames <= 0) {
        return 0;
    }
    const auto begin = source_to_output_frame(epoch.loop->begin().value());
    const auto end = source_to_output_frame(epoch.loop->end().value());
    const auto length = end - begin;
    if (length <= 0) {
        return 0;
    }
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const auto candidate = elapsedFrames > maximum - epoch.firstOutputFrame
        ? maximum : epoch.firstOutputFrame + elapsedFrames;
    if (candidate < end) {
        return 0;
    }
    return static_cast<std::uint64_t>((candidate - begin) / length);
}

std::uint64_t PlaybackEngine::current_loop_wrap_count() const noexcept
{
    if (!output_) {
        return baseLoopWrapCount_;
    }
    const auto processed = std::max<std::int64_t>(
        0, output_->processed_frames() - processedFrameBaseline_);
    const auto epoch = audible_loop_epoch(processed);
    return epoch.wrapsBefore + wraps_in_loop_epoch(
        epoch, std::max<std::int64_t>(
            0, processed - epoch.firstGeneratedFrame));
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

C2CoverageProbe PlaybackEngine::c2_processed_coverage_probe(
    std::optional<std::uint64_t> expectedGeneration) const noexcept
{
    C2CoverageProbe probe{};
    probe.generation = c2CoverageGeneration_;
    probe.generatedOutputFrames = generatedOutputFrames_;
    probe.retainedOutputBegin = c2RetainedOutputBegin_;
    probe.acceptedOutputFrames = c2AcceptedOutputFrames_;
    probe.acceptedRunCount = c2AcceptedRuns_.size();
    probe.acceptedLedgerOverflow = c2AcceptedLedgerOverflow_;
    if (expectedGeneration && *expectedGeneration != probe.generation) {
        probe.reason = C2CoverageProbeReason::STALE_GENERATION;
        return probe;
    }
    if (c2CoverageGenerationExhausted_) {
        probe.reason = C2CoverageProbeReason::GENERATION_EXHAUSTED;
        return probe;
    }
    if (terminalOutputDomain_) {
        probe.reason=C2CoverageProbeReason::MISSING_CHRONOLOGICAL_LEDGER;
        return probe;
    }
    if (!output_ || !source_
        || (state_ != core::PlaybackState::PLAYING
            && state_ != core::PlaybackState::PAUSED)) {
        return probe; // NOT_MONITORED, never a synthetic zero-valued TP.
    }

    const auto processed = output_->processed_frames();
    if (processedFrameBaseline_ < 0 || processed < processedFrameBaseline_) {
        probe.reason = C2CoverageProbeReason::CLOCK_BEFORE_BASELINE;
        return probe;
    }
    // Both values non-negative, so subtraction cannot overflow int64.
    probe.processedWatermarkExclusive = processed - processedFrameBaseline_;
    if (probe.processedWatermarkExclusive > generatedOutputFrames_) {
        probe.reason = C2CoverageProbeReason::CLOCK_AHEAD_OF_GENERATED;
    } else if (probe.processedWatermarkExclusive > c2AcceptedOutputFrames_) {
        probe.reason = C2CoverageProbeReason::CLOCK_AHEAD_OF_ACCEPTED;
    } else if (c2AcceptedLedgerOverflow_) {
        probe.reason = C2CoverageProbeReason::ACCEPTED_LEDGER_OVERFLOW;
    } else if (!activeRealizationId_) {
        probe.reason = C2CoverageProbeReason::NOT_MONITORED;
    } else if (pendingHandoff_ || c2AcceptedContainsUnprovenHandoff_) {
        probe.reason = C2CoverageProbeReason::UNPROVEN_HANDOFF;
    } else if (rateAdapter_) {
        probe.reason = C2CoverageProbeReason::UNPROVEN_SRC;
    } else if (loop_ || initialLoopEpoch_.loop || !scheduledLoopEpochs_.empty()) {
        probe.reason = C2CoverageProbeReason::UNPROVEN_LOOP;
    } else {
        // Even direct 1:1 playback lacks a retained owner-stamped interval
        // history. The sink clock alone cannot prove every consumed segment.
        probe.reason = C2CoverageProbeReason::MISSING_CHRONOLOGICAL_LEDGER;
    }
    // Do not claim complete coverage until a separate, falsified C2D ledger
    // materializes every owner/phase/seek/loop boundary through the watermark.
    return probe;
}

C2SrcRetirementObservation PlaybackEngine::c2_src_retirement_observation(
    std::optional<std::uint64_t> expectedGeneration) const noexcept
{
    C2SrcRetirementObservation result{};
    const auto probe = c2_processed_coverage_probe(expectedGeneration);
    result.generation = probe.generation;
    result.consumedOutputExclusive = probe.processedWatermarkExclusive;
    result.reason = probe.reason;
    // C2D18: a looped SRC path cannot use the whole-track source prefix
    // as if sink OUTPUT time never wrapped. Instead retain a separate
    // contiguous SRC OUTPUT_RATE traversal from the exact enqueue ledger.
    // This is a sink-clock-limited observation; the OUTPUT ledger stays GAP.
    if ((loop_ || !scheduledLoopEpochs_.empty())
        && rateAdapter_ && activeRealizationId_ && outputDuration_
        && probe.reason == C2CoverageProbeReason::UNPROVEN_SRC
        && probe.processedWatermarkExclusive > 0
        && !pendingHandoff_ && !c2AcceptedContainsUnprovenHandoff_
        && !c2AcceptedLedgerOverflow_ && !c2CoverageGenerationExhausted_
        && c2RetainedOutputBegin_ >= 0
        && c2RetainedOutputBegin_ < probe.processedWatermarkExclusive) {
        const auto epoch = audible_loop_epoch(
            probe.processedWatermarkExclusive);
        // The region may remain ARMED while a deliberate seek after its
        // end plays normal whole-track PCM. This is not a loop traversal:
        // preserve the source-owned SRC proof, but never label it as a wrap.
        if (loop_ ? (!epoch.loop || *epoch.loop != *loop_)
                  : epoch.loop.has_value())
            return result;
        // Only the sink-AUDIBLE traversal is eligible: a future generated
        // loop block, the selected checkbox and the position cursor are not.
        const auto audibleWrap = current_loop_wrap_count();
        std::int64_t cursor = c2RetainedOutputBegin_;
        std::int64_t srcAbsoluteFirst = -1;
        std::int64_t srcAbsoluteAfter = -1;
        std::int64_t epochOutputBegin = cursor;
        std::uint64_t observedWrap = 0;
        for (const auto& run : c2AcceptedRuns_) {
            if (cursor == result.consumedOutputExclusive) break;
            if (run.generation != result.generation
                || run.outputBegin != cursor || run.outputEnd <= cursor
                || run.realizationId != activeRealizationId_
                || run.exactOneToOne || run.sourceBegin || run.sourceEnd
                || run.gapReason != C2CoverageProbeReason::UNPROVEN_SRC
                || !run.srcOutputBegin || !run.srcOutputEnd
                || *run.srcOutputBegin < 0
                || *run.srcOutputEnd - *run.srcOutputBegin
                    != run.outputEnd - run.outputBegin
                || run.loopTraversalProven || run.handoffPureNewProven)
                return result;
            const auto upper = std::min(
                run.outputEnd, result.consumedOutputExclusive);
            const auto absoluteBegin =
                *run.srcOutputBegin + (cursor - run.outputBegin);
            const auto absoluteAfter =
                *run.srcOutputBegin + (upper - run.outputBegin);
            if (srcAbsoluteAfter != absoluteBegin
                || (srcAbsoluteFirst >= 0 && observedWrap != run.loopWrapCount)) {
                // Never join different repetitions, policy epochs or SRC
                // discontinuities. Discard all old FIR/RECENT/HOLD history.
                epochOutputBegin = cursor;
                srcAbsoluteFirst = absoluteBegin;
            }
            observedWrap = run.loopWrapCount;
            srcAbsoluteAfter = absoluteAfter;
            cursor = upper;
        }
        if (cursor != result.consumedOutputExclusive
            || observedWrap != audibleWrap
            || srcAbsoluteFirst < 0
            || srcAbsoluteAfter <= srcAbsoluteFirst
            || srcAbsoluteAfter > outputDuration_->value()
            || epochOutputBegin >= result.consumedOutputExclusive)
            return result;
        const bool audibleTraversal = loop_ && epoch.eligible;
        const auto regionOutputEnd = audibleTraversal
            ? source_to_output_frame(loop_->end().value())
            : outputDuration_->value();
        if (srcAbsoluteAfter > regionOutputEnd)
            return result;
        auto first = rateAdapter_->map_output_frame_to_input_cursor(
            core::FrameIndex{srcAbsoluteFirst});
        // Whole-track no-future FIR dependency is conservative for the
        // remainder of this ONE SOURCE traversal. It must never combine
        // independent wraps, even if the current source position repeats.
        auto retired = rateAdapter_->no_future_dependency_source_prefix(
            core::FrameIndex{srcAbsoluteAfter});
        const auto retiredSourceEnd = retired
            ? std::min(retired.value()->value(),
                  audibleTraversal ? loop_->end().value()
                                     : duration_->value()) : 0;
        if (!first || !retired
            || retiredSourceEnd <= first.value()->value())
            return result;
        result.consumedOutputBegin = epochOutputBegin;
        result.loopWrapCount = observedWrap;
        result.loopTraversalProven = audibleTraversal;
        result.firstSourceFrame = first.value()->value();
        result.noFutureSourcePrefixExclusive = retiredSourceEnd;
        result.realizationId = activeRealizationId_;
        result.sourceRateHz = rateAdapter_->input_rate().value();
        result.outputRateHz = rateAdapter_->output_rate().value();
        return result;
    }

    // A source sample absent from *future* SRC output is not necessarily
    // audible. We require one owner-stamped, actually consumed OUTPUT epoch,
    // including after a seek; never reclassify the underlying GAP as VALID.
    // The absolute SRC output origin was retained at queue reset/seek, NOT
    // guessed from a GUI cursor or the current transport position.
    if (probe.reason != C2CoverageProbeReason::UNPROVEN_SRC
        || probe.processedWatermarkExclusive <= 0
        || !rateAdapter_ || !activeRealizationId_ || !outputDuration_
        || sourceBegin_.value() != 0 || playbackStartOutputFrame_ < 0
        || playbackStartOutputFrame_ >= outputDuration_->value()
        || probe.processedWatermarkExclusive
            > outputDuration_->value() - playbackStartOutputFrame_
        || c2RetainedOutputBegin_ != 0
        || c2AcceptedLedgerOverflow_ || c2CoverageGenerationExhausted_
        || pendingHandoff_ || c2AcceptedContainsUnprovenHandoff_
        || loop_ || initialLoopEpoch_.loop
        || !scheduledLoopEpochs_.empty()) {
        return result;
    }
    // Preserve the ENTIRE accepted chronology [0, sink watermark) of the NEW
    // post-seek OUTPUT epoch. Pre-seek accepted history is never reused.
    std::int64_t cursor = 0;
    for (const auto& run : c2AcceptedRuns_) {
        if (cursor == result.consumedOutputExclusive) break;
        if (run.generation != result.generation
            || run.outputBegin != cursor || run.outputEnd <= cursor
            || run.realizationId != activeRealizationId_
            || run.exactOneToOne || run.sourceBegin || run.sourceEnd
            || run.gapReason != C2CoverageProbeReason::UNPROVEN_SRC
            || run.loopTraversalProven || run.loopWrapCount != 0
            || run.handoffPureNewProven) {
            return result;
        }
        cursor = std::min(run.outputEnd, result.consumedOutputExclusive);
    }
    if (cursor != result.consumedOutputExclusive) return result;

    // Map the relative sink-consumed output into the absolute SRC timeline.
    // The original FIR kernel/even-reflection oracle can now safely retire
    // SOURCE frames from a nonzero seek origin, never from fake frame zero.
    const auto absoluteConsumedOutput =
        playbackStartOutputFrame_ + result.consumedOutputExclusive;
    auto start = rateAdapter_->map_output_frame_to_input_cursor(
        core::FrameIndex{playbackStartOutputFrame_});
    auto retired = rateAdapter_->no_future_dependency_source_prefix(
        core::FrameIndex{absoluteConsumedOutput});
    if (!start || !retired
        || start.value()->value() < 0
        || retired.value()->value() <= start.value()->value()
        || retired.value()->value() > rateAdapter_->input_frame_count().value()) {
        return result;
    }
    result.firstSourceFrame = start.value()->value();
    result.noFutureSourcePrefixExclusive = retired.value()->value();
    result.realizationId = activeRealizationId_;
    result.sourceRateHz = rateAdapter_->input_rate().value();
    result.outputRateHz = rateAdapter_->output_rate().value();
    return result;
}

C2ConsumedPrefix PlaybackEngine::c2_consumed_prefix(
    std::optional<std::uint64_t> expectedGeneration) const noexcept
{
    C2ConsumedPrefix result{};
    const auto probe = c2_processed_coverage_probe(expectedGeneration);
    result.generation = probe.generation;
    result.reason = probe.reason;
    result.retainedOutputBegin = c2RetainedOutputBegin_;
    // A clock discontinuity, stale token or absent output gives NO prefix,
    // rather than a fabricated empty-silence or VALID coverage claim.
    if (probe.reason == C2CoverageProbeReason::STALE_GENERATION
        || probe.reason == C2CoverageProbeReason::GENERATION_EXHAUSTED
        || probe.reason == C2CoverageProbeReason::CLOCK_BEFORE_BASELINE
        || probe.reason == C2CoverageProbeReason::CLOCK_AHEAD_OF_GENERATED
        || probe.reason == C2CoverageProbeReason::CLOCK_AHEAD_OF_ACCEPTED
        || probe.reason == C2CoverageProbeReason::NOT_MONITORED) {
        return result;
    }
    const auto watermark = probe.processedWatermarkExclusive;
    result.processedWatermarkExclusive = watermark;
    if (watermark == 0) {
        return result; // an empty prefix does not prove a peak of zero
    }

    // Only the retained SUFFIX is represented: omitted frames have already
    // passed the sink clock and are never reclassified as silent or valid.
    if (result.retainedOutputBegin < 0
        || result.retainedOutputBegin >= watermark) return result;
    std::int64_t cursor = result.retainedOutputBegin;
    for (const auto& accepted : c2AcceptedRuns_) {
        if (cursor >= watermark) {
            break;
        }
        // Source ledger corruption or a silent non-contiguous interval is
        // explicitly represented as GAP, never folded into adjacent owner.
        if (accepted.generation != result.generation
            || accepted.outputBegin != cursor
            || accepted.outputEnd <= accepted.outputBegin) {
            break;
        }
        const auto upper = std::min(watermark, accepted.outputEnd);
        if (result.size >= C2ConsumedPrefix::kMaximumSlices) {
            break;
        }
        auto& slice = result.slices[result.size++];
        slice.generation = result.generation;
        slice.outputBegin = cursor;
        slice.outputEnd = upper;
        slice.reason = probe.reason;
        // This classification is explicitly *candidate*, NOT VALID coverage.
        // A SOURCE_CANDIDATE still needs immutable PROCESSED ownership,
        // chain revision, playback phase and canonical FIR support evidence.
        if (accepted.exactOneToOne && accepted.realizationId
            && accepted.sourceBegin && accepted.sourceEnd
            && *accepted.sourceEnd - *accepted.sourceBegin
                == accepted.outputEnd - accepted.outputBegin) {
            slice.kind = C2ConsumedSliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED;
            slice.realizationId = accepted.realizationId;
            const auto offset = cursor - accepted.outputBegin;
            slice.sourceBegin = *accepted.sourceBegin + offset;
            slice.sourceEnd = *slice.sourceBegin + (upper - cursor);
            slice.loopTraversalProven = accepted.loopTraversalProven;
            slice.loopWrapCount = accepted.loopWrapCount;
            slice.handoffPureNewProven = accepted.handoffPureNewProven;
        } else {
            slice.kind = C2ConsumedSliceKind::GAP_UNPROVEN;
            // Retain generation-time GAP reason: later handoffs may not
            // relabel an earlier loop gap as handoff evidence.
            slice.reason = accepted.gapReason;
        }
        cursor = upper;
    }
    if (cursor < watermark) {
        // A bounded producer that stops recording after overflow may still
        // advance the processed counter. Its missing tail is *not* silence.
        if (result.size < C2ConsumedPrefix::kMaximumSlices) {
            auto& lost = result.slices[result.size++];
            lost.generation = result.generation;
            lost.outputBegin = cursor;
            lost.outputEnd = watermark;
            lost.kind = C2ConsumedSliceKind::GAP_LOST_PROVENANCE;
            lost.reason = C2CoverageProbeReason::ACCEPTED_LEDGER_OVERFLOW;
            cursor = watermark;
        }
    }
    result.partitionRepresented = cursor == watermark;
    // Crucial: even complete *representation* with source candidates does
    // not establish the immutable PROCESSED target or true-peak FIR support.
    result.certifiedForLiveTruePeak = false;
    return result;
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
        const auto bytesPerFrame =
            source_format().channel_count() * bytes_per_sample(*sampleFormat_);
        const auto completedBefore = pendingOffset_ / bytesPerFrame;
        pendingOffset_ += *written.value();
        const auto completedAfter = pendingOffset_ / bytesPerFrame;
        if (completedAfter > completedBefore) {
            c2_record_accepted_frames(completedBefore, completedAfter);
        }
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
    if (frames > std::numeric_limits<std::int64_t>::max()
            - generatedOutputFrames_) {
        return status_failure(
            core::ErrorCode::OutOfRange,
            "Playback device frame timeline overflow.");
    }
    auto blockFrameCount = core::FrameCount::create(frames);
    if (!blockFrameCount) {
        return core::Status::failure(*blockFrameCount.error());
    }
    auto blockFormat = source_format();
    auto blockDomain = terminalOutputDomain_
        ? audio::FrameDomainId::OUTPUT_RATE
        : audio::FrameDomainId::SOURCE_PROCESSING_RATE;
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
    // Capture owner/domain at *generation* time. A future handoff can change
    // activeRealizationId_ while old bytes are still queued or pending.
    c2PendingOwner_ = activeRealizationId_;
    c2PendingSourceBegin_ = blockAbsoluteStart.value();
    // A past loop policy does not taint NEW, precisely generated direct PCM.
    // The chronological ledger still retains previous unproven loop runs.
    // Old accepted/pending bytes preserve their original generation-time tags;
    // do not clear, restart or rewrite the audio output on loop OFF.
    // Crossfade and queued bytes remain GAP_UNPROVEN. Only a separately
    // decoded, pure NEW block after the handoff's numeric boundary may
    // regain source identity; the consumed sink watermark gates publication.
    // NEW-only continuity is established in absolute OUTPUT time. Comparing
    // numeric Source frames would incorrectly reject pure NEW loop wraps
    // that naturally return below the initial handoff Source frame.
    const bool outputClockMapped = processedFrameBaseline_ >= 0
        && generatedOutputFrames_
            <= std::numeric_limits<std::int64_t>::max() - processedFrameBaseline_;
    const bool pureNewAfterHandoff = pendingHandoff_
        && !pendingHandoff_->awaitingReplay
        && pendingHandoff_->newNumericStartSourceFrame
        && c2PendingOwner_ == pendingHandoff_->newRealizationId
        && outputClockMapped
        && processedFrameBaseline_ + generatedOutputFrames_
            >= pendingHandoff_->endProcessedFrame;
    // A decoder block is guaranteed not to cross the loop boundary: pump
    // limits frames to output_boundary(). Record a pure, source-mapped
    // per-traversal candidate ONLY with exact non-SRC owner identity and a
    // region long enough for at least a 100-ms window (FIR startup still
    // requires its own proven history). Tiny loops remain UNPROVEN.
    const bool loopCandidate = loop_ && loopTraversalEligible_
        && loop_->end().value() - loop_->begin().value()
            >= source_format().sample_rate().value() / 10;
    // When an armed loop has been bypassed by a SOURCE seek beyond its
    // end, this output block is genuinely non-looped and source-mapped.
    const bool nonTraversingOutsideLoop = loop_ && !loopTraversalEligible_
        && scheduledOutputFrame_
            >= source_to_output_frame(loop_->end().value());
    c2PendingExactMapping_ = !rateAdapter_ && !terminalOutputDomain_
        && (!loop_ || loopCandidate || nonTraversingOutsideLoop)
        && c2PendingOwner_.has_value()
        && (!pendingHandoff_ || pureNewAfterHandoff);
    c2PendingLoopTraversalProven_ = c2PendingExactMapping_
        && loopCandidate;
    c2PendingHandoffPureNew_ = c2PendingExactMapping_
        && pureNewAfterHandoff;
    c2PendingGapReason_ = pendingHandoff_
        ? C2CoverageProbeReason::UNPROVEN_HANDOFF
        : rateAdapter_ ? C2CoverageProbeReason::UNPROVEN_SRC
        : loop_ ? C2CoverageProbeReason::UNPROVEN_LOOP
        : C2CoverageProbeReason::MISSING_CHRONOLOGICAL_LEDGER;
    const auto epoch = audible_loop_epoch(generatedOutputFrames_);
    c2PendingLoopWrapCount_ = epoch.wrapsBefore
        + wraps_in_loop_epoch(epoch,
            std::max<std::int64_t>(0,
                generatedOutputFrames_ - epoch.firstGeneratedFrame));
    generatedOutputFrames_ += frames;
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

void PlaybackEngine::c2_record_accepted_frames(
    std::size_t oldComplete, std::size_t newComplete) noexcept
{
    // Observer bookkeeping is CONTROL PLANE only. Never fail an audio write
    // or alter PCM when a diagnostic allocation/retention bound is exceeded.
    if (newComplete <= oldComplete) {
        return;
    }
    const auto accepted = newComplete - oldComplete;
    if (accepted > static_cast<std::size_t>(
            std::numeric_limits<std::int64_t>::max() - c2AcceptedOutputFrames_)) {
        c2AcceptedLedgerOverflow_ = true;
        return;
    }
    const auto outputBegin = c2AcceptedOutputFrames_;
    c2AcceptedOutputFrames_ += static_cast<std::int64_t>(accepted);
    if (c2AcceptedLedgerOverflow_ || c2CoverageGenerationExhausted_) {
        return;
    }

    C2AcceptedOutputRun run{};
    run.generation = c2CoverageGeneration_;
    run.outputBegin = outputBegin;
    run.outputEnd = c2AcceptedOutputFrames_;
    run.realizationId = c2PendingOwner_;
    run.exactOneToOne = c2PendingExactMapping_;
    run.loopTraversalProven = c2PendingLoopTraversalProven_;
    run.loopWrapCount = c2PendingLoopWrapCount_;
    run.handoffPureNewProven = c2PendingHandoffPureNew_;
    run.gapReason = c2PendingGapReason_;
    if (run.exactOneToOne) {
        // The decoded block was already checked to lie inside its finite
        // source resource. Retain *generation-time* mapping, not UI cursor.
        run.sourceBegin = c2PendingSourceBegin_
            + static_cast<std::int64_t>(oldComplete);
        run.sourceEnd = c2PendingSourceBegin_
            + static_cast<std::int64_t>(newComplete);
    } else if (rateAdapter_ && !pendingHandoff_
               && c2PendingGapReason_ == C2CoverageProbeReason::UNPROVEN_SRC) {
        // Retain the OUTPUT_RATE SRC decoder origin for this accepted block.
        // This cannot be inferred from the GUI playhead or converted to
        // canonical SOURCE without a separate consumed-clock/FIR proof.
        run.srcOutputBegin = c2PendingSourceBegin_
            + static_cast<std::int64_t>(oldComplete);
        run.srcOutputEnd = c2PendingSourceBegin_
            + static_cast<std::int64_t>(newComplete);
    }
    if (!c2AcceptedRuns_.empty()) {
        auto& previous = c2AcceptedRuns_.back();
        if (run.exactOneToOne && previous.exactOneToOne
            && previous.generation == run.generation
            && previous.outputEnd == run.outputBegin
            && previous.sourceEnd == run.sourceBegin
            && previous.realizationId == run.realizationId
            && previous.loopWrapCount == run.loopWrapCount
            && previous.loopTraversalProven == run.loopTraversalProven
            && previous.handoffPureNewProven == run.handoffPureNewProven) {
            previous.outputEnd = run.outputEnd;
            previous.sourceEnd = run.sourceEnd;
            return;
        }
        // Merge only adjacent UNSAFE runs into one explicit GAP. The gap
        // NEVER becomes source-mapped or LIVE. Retain its generation-time
        // owner *solely* for single-owner SRC negative dependency diagnostics
        // when both halves demonstrably share that owner and traversal.
        // Mixed/unknown owners and every other gap remain ownerless.
        if (!run.exactOneToOne && !previous.exactOneToOne
            && previous.generation == run.generation
            && previous.outputEnd == run.outputBegin
            && previous.gapReason == run.gapReason
            && (run.gapReason != C2CoverageProbeReason::UNPROVEN_SRC
                || (previous.realizationId.has_value()
                    && previous.realizationId == run.realizationId
                    && previous.srcOutputEnd && run.srcOutputBegin
                    && previous.srcOutputEnd == run.srcOutputBegin
                    && previous.loopWrapCount == run.loopWrapCount
                    && !previous.handoffPureNewProven
                    && !run.handoffPureNewProven))) {
            const bool sameUninterruptedSrcOwner =
                run.gapReason == C2CoverageProbeReason::UNPROVEN_SRC
                && previous.realizationId.has_value()
                && previous.realizationId == run.realizationId
                && previous.srcOutputEnd && run.srcOutputBegin
                && previous.srcOutputEnd == run.srcOutputBegin
                && !previous.sourceBegin && !previous.sourceEnd
                && !run.sourceBegin && !run.sourceEnd
                && !previous.loopTraversalProven && !run.loopTraversalProven
                && previous.loopWrapCount == run.loopWrapCount
                && !previous.handoffPureNewProven
                && !run.handoffPureNewProven;
            previous.outputEnd = run.outputEnd;
            if (sameUninterruptedSrcOwner) {
                previous.srcOutputEnd = run.srcOutputEnd;
            } else {
                previous.realizationId.reset();
                previous.srcOutputBegin.reset();
                previous.srcOutputEnd.reset();
            }
            previous.sourceBegin.reset();
            previous.sourceEnd.reset();
            return;
        }
    }
    if (c2AcceptedRuns_.size() >= kC2MaximumAcceptedRuns) {
        // Sliding OUTPUT_CONSUMED evidence: free only complete segments
        // *behind* the actual sink clock. Unconsumed runs are immutable.
        // Keep at least the last run to preserve its source/owner boundary;
        // deleting history NEVER upgrades candidate or GAP classification.
        const auto processed = output_
            ? output_->processed_frames() : std::int64_t{-1};
        // Reject inconsistent device clocks before pruning historical
        // proof. The sink may never consume beyond accepted output.
        if (processedFrameBaseline_ >= 0
            && processed >= processedFrameBaseline_
            && processed - processedFrameBaseline_ <= c2AcceptedOutputFrames_
            && processed - processedFrameBaseline_ <= generatedOutputFrames_) {
            const auto watermark = processed - processedFrameBaseline_;
            std::size_t prune = 0;
            while (prune + 1U < c2AcceptedRuns_.size()
                && c2AcceptedRuns_[prune].outputEnd <= watermark)
                ++prune;
            if (prune > 0U) {
                c2RetainedOutputBegin_ = c2AcceptedRuns_[prune].outputBegin;
                c2AcceptedRuns_.erase(c2AcceptedRuns_.begin(),
                    c2AcceptedRuns_.begin()
                        + static_cast<std::ptrdiff_t>(prune));
            }
        }
        if (c2AcceptedRuns_.size() >= kC2MaximumAcceptedRuns) {
            c2AcceptedLedgerOverflow_ = true; // unprunable, fail closed
            return;
        }
    }
    try {
        c2AcceptedRuns_.push_back(std::move(run));
    } catch (const std::bad_alloc&) {
        c2AcceptedLedgerOverflow_ = true;
    }
}

void PlaybackEngine::reset_queue_state(std::int64_t frame) noexcept
{
    // Reset is the authoritative point for a new processed-output clock epoch.
    // Never reuse token identities after int64 counters/device clocks restart.
    if (c2CoverageGeneration_ == std::numeric_limits<std::uint64_t>::max()) {
        c2CoverageGenerationExhausted_ = true;
    } else {
        ++c2CoverageGeneration_;
    }
    c2AcceptedRuns_.clear();
    c2AcceptedOutputFrames_ = 0;
    c2RetainedOutputBegin_ = 0;
    c2AcceptedLedgerOverflow_ = false;
    c2AcceptedContainsUnprovenHandoff_ = false;
    c2PendingSourceBegin_ = 0;
    c2PendingOwner_.reset();
    c2PendingExactMapping_ = false;
    c2PendingLoopTraversalProven_ = false;
    c2PendingHandoffPureNew_ = false;
    c2PendingGapReason_ = C2CoverageProbeReason::MISSING_CHRONOLOGICAL_LEDGER;
    c2PendingLoopWrapCount_ = 0;
    scheduledOutputFrame_ = source_to_output_frame(frame);
    playbackStartOutputFrame_ = scheduledOutputFrame_;
    generatedOutputFrames_ = 0;
    initialLoopEpoch_ = LoopEpoch{
        0, scheduledOutputFrame_, loop_, loopTraversalEligible_,
        baseLoopWrapCount_, scheduledOutputFrame_};
    scheduledLoopEpochs_.clear();
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
    const auto processed = std::max<std::int64_t>(
        0, totalProcessed - processedFrameBaseline_);
    const auto epoch = audible_loop_epoch(processed);
    const auto elapsed = std::max<std::int64_t>(
        0, processed - epoch.firstGeneratedFrame);
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    auto candidate = elapsed > maximum - epoch.firstOutputFrame
        ? maximum : epoch.firstOutputFrame + elapsed;
    if (epoch.loop && epoch.eligible) {
        const auto begin = source_to_output_frame(epoch.loop->begin().value());
        const auto end = source_to_output_frame(epoch.loop->end().value());
        if (elapsed > 0 && candidate >= end) {
            const auto length = end - begin;
            if (length > 0) {
                candidate = begin + ((candidate - end) % length);
            }
        }
    } else {
        candidate = std::min(candidate, outputDuration_->value());
    }
    return candidate;
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
