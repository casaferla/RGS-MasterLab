#include <rgsml/audio/waveform_summary.hpp>

#include "internal/waveform_summary_builder.hpp"

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/wav_reader.hpp>
#include <rgsml/core/checked_integer.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::audio {
namespace {

template <typename T>
[[nodiscard]] core::Result<T> failure(core::ErrorCode code, std::string message)
{
    return core::Result<T>::failure(core::Error{code, std::move(message)});
}

[[nodiscard]] core::Error cancelled_error()
{
    return core::Error{
        core::ErrorCode::InvalidState,
        "Waveform summary construction was cancelled.",
    };
}

[[nodiscard]] double minimum_sample(double left, double right) noexcept
{
    if (left < right) {
        return left;
    }
    if (right < left) {
        return right;
    }
    if (left == 0.0 && right == 0.0) {
        return std::signbit(left) || std::signbit(right) ? -0.0 : 0.0;
    }
    return left;
}

[[nodiscard]] double maximum_sample(double left, double right) noexcept
{
    if (left > right) {
        return left;
    }
    if (right > left) {
        return right;
    }
    if (left == 0.0 && right == 0.0) {
        return !std::signbit(left) || !std::signbit(right) ? 0.0 : -0.0;
    }
    return left;
}

[[nodiscard]] core::Result<std::int64_t> next_power_of_two(std::int64_t value)
{
    if (value <= 0) {
        return failure<std::int64_t>(
            core::ErrorCode::InvalidArgument,
            "Waveform bucket size must be positive.");
    }
    const auto unsignedValue = static_cast<std::uint64_t>(value);
    if (unsignedValue > (std::uint64_t{1} << 62U)) {
        return failure<std::int64_t>(
            core::ErrorCode::IntegerOverflow,
            "Waveform bucket size overflowed.");
    }
    const auto result = std::bit_ceil(unsignedValue);
    if (result > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return failure<std::int64_t>(
            core::ErrorCode::IntegerOverflow,
            "Waveform bucket size overflowed.");
    }
    return core::Result<std::int64_t>::success(static_cast<std::int64_t>(result));
}

[[nodiscard]] core::Result<std::size_t> checked_size(std::int64_t value)
{
    if (value < 0
        || static_cast<std::uint64_t>(value)
            > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return failure<std::size_t>(
            core::ErrorCode::IntegerOverflow,
            "Waveform size cannot be represented on this platform.");
    }
    return core::Result<std::size_t>::success(static_cast<std::size_t>(value));
}

}  // namespace

WaveformSummary::LevelView::LevelView(
    const LevelData* data,
    std::size_t channelCount) noexcept
    : data_(data)
    , channelCount_(channelCount)
{
}

core::FrameCount WaveformSummary::LevelView::frames_per_bucket() const noexcept
{
    return data_->framesPerBucket;
}

core::FrameCount WaveformSummary::LevelView::bucket_count() const noexcept
{
    return data_->bucketCount;
}

std::size_t WaveformSummary::LevelView::channel_count() const noexcept
{
    return channelCount_;
}

core::Result<WaveformSummary::PeakRange> WaveformSummary::LevelView::peak(
    std::size_t channelIndex,
    core::FrameIndex bucketIndex) const
{
    if (channelIndex >= channelCount_
        || bucketIndex.value() < 0
        || bucketIndex.value() >= data_->bucketCount.value()) {
        return failure<PeakRange>(
            core::ErrorCode::OutOfRange,
            "Waveform peak index is out of range.");
    }
    const auto bucketCount = static_cast<std::size_t>(data_->bucketCount.value());
    const auto offset = channelIndex * bucketCount
        + static_cast<std::size_t>(bucketIndex.value());
    return core::Result<PeakRange>::success(data_->peaks[offset]);
}

WaveformSummary::WaveformSummary(
    core::SampleRate sampleRate,
    std::size_t channelCount,
    core::FrameCount sourceFrameCount,
    std::vector<LevelData> levels,
    std::size_t payloadBytes) noexcept
    : sampleRate_(sampleRate)
    , channelCount_(channelCount)
    , sourceFrameCount_(sourceFrameCount)
    , levels_(std::move(levels))
    , payloadBytes_(payloadBytes)
{
}

core::SampleRate WaveformSummary::sample_rate() const noexcept { return sampleRate_; }
std::size_t WaveformSummary::channel_count() const noexcept { return channelCount_; }
core::FrameCount WaveformSummary::source_frame_count() const noexcept
{
    return sourceFrameCount_;
}
std::size_t WaveformSummary::level_count() const noexcept { return levels_.size(); }

core::Result<WaveformSummary::LevelView> WaveformSummary::level(
    std::size_t levelIndex) const
{
    if (levelIndex >= levels_.size()) {
        return failure<LevelView>(
            core::ErrorCode::OutOfRange,
            "Waveform level index is out of range.");
    }
    return core::Result<LevelView>::success(
        LevelView{&levels_[levelIndex], channelCount_});
}

std::size_t WaveformSummary::payload_bytes() const noexcept { return payloadBytes_; }

core::Result<WaveformSummary> build_waveform_summary(
    WavReader& reader,
    std::stop_token stopToken)
{
    return build_waveform_summary_with_block_limit(
        reader,
        WaveformSummary::kMaximumDecodeBlockFrames,
        stopToken);
}

core::Result<WaveformSummary> build_waveform_summary_with_block_limit(
    WavReader& reader,
    std::size_t blockFrameLimit,
    std::stop_token stopToken)
{
    if (blockFrameLimit == 0U
        || blockFrameLimit > WaveformSummary::kMaximumDecodeBlockFrames) {
        return failure<WaveformSummary>(
            core::ErrorCode::InvalidArgument,
            "Waveform decode block limit is outside the bounded contract.");
    }
    if (stopToken.stop_requested()) {
        return core::Result<WaveformSummary>::failure(cancelled_error());
    }

    const auto& info = reader.info();
    const auto frameCount = info.frame_count();
    const auto frames = frameCount.value();
    const auto channelCount = info.audio_format().channel_count();
    if (channelCount == 0U || channelCount > 2U) {
        return failure<WaveformSummary>(
            core::ErrorCode::UnsupportedAudioLayout,
            "Waveform summary requires mono or stereo canonical audio.");
    }

    std::vector<WaveformSummary::LevelData> levels;
    std::size_t payloadBytes = 0U;
    if (frames == 0) {
        return core::Result<WaveformSummary>::success(WaveformSummary{
            info.audio_format().sample_rate(),
            channelCount,
            frameCount,
            std::move(levels),
            payloadBytes,
        });
    }

    auto requestedFramesPerBucket = core::ceil_div_signed(
        frames,
        static_cast<std::int64_t>(WaveformSummary::kMaximumBaseBucketsPerChannel));
    if (!requestedFramesPerBucket) {
        return core::Result<WaveformSummary>::failure(
            *requestedFramesPerBucket.error());
    }
    auto framesPerBucket = next_power_of_two(
        std::max<std::int64_t>(1, *requestedFramesPerBucket.value()));
    if (!framesPerBucket) {
        return core::Result<WaveformSummary>::failure(*framesPerBucket.error());
    }
    auto baseBucketCount = core::ceil_div_signed(frames, *framesPerBucket.value());
    if (!baseBucketCount) {
        return core::Result<WaveformSummary>::failure(*baseBucketCount.error());
    }
    if (*baseBucketCount.value()
        > static_cast<std::int64_t>(WaveformSummary::kMaximumBaseBucketsPerChannel)) {
        return failure<WaveformSummary>(
            core::ErrorCode::IntegerOverflow,
            "Waveform base bucket count exceeds its fixed bound.");
    }

    auto baseSize = checked_size(*baseBucketCount.value());
    if (!baseSize) {
        return core::Result<WaveformSummary>::failure(*baseSize.error());
    }
    auto peakCountValue = core::checked_multiply(
        *baseBucketCount.value(),
        static_cast<std::int64_t>(channelCount));
    if (!peakCountValue) {
        return core::Result<WaveformSummary>::failure(*peakCountValue.error());
    }
    auto peakCount = checked_size(*peakCountValue.value());
    if (!peakCount) {
        return core::Result<WaveformSummary>::failure(*peakCount.error());
    }

    auto framesPerBucketCount = core::FrameCount::create(*framesPerBucket.value());
    auto baseBucketFrameCount = core::FrameCount::create(*baseBucketCount.value());
    if (!framesPerBucketCount || !baseBucketFrameCount) {
        return core::Result<WaveformSummary>::failure(
            !framesPerBucketCount
                ? *framesPerBucketCount.error()
                : *baseBucketFrameCount.error());
    }
    WaveformSummary::LevelData base{
        *framesPerBucketCount.value(),
        *baseBucketFrameCount.value(),
        {},
    };
    try {
        base.peaks.resize(*peakCount.value(), WaveformSummary::PeakRange{0.0, 0.0});
    } catch (const std::bad_alloc&) {
        return failure<WaveformSummary>(
            core::ErrorCode::IoFailure,
            "Unable to allocate bounded waveform peak storage.");
    }

    std::vector<bool> initialized;
    try {
        initialized.resize(*peakCount.value(), false);
    } catch (const std::bad_alloc&) {
        return failure<WaveformSummary>(
            core::ErrorCode::IoFailure,
            "Unable to allocate bounded waveform construction state.");
    }

    std::int64_t absoluteFrame = 0;
    while (absoluteFrame < frames) {
        if (stopToken.stop_requested()) {
            return core::Result<WaveformSummary>::failure(cancelled_error());
        }
        const auto remaining = frames - absoluteFrame;
        const auto blockFrames = std::min<std::int64_t>(
            remaining,
            static_cast<std::int64_t>(blockFrameLimit));
        auto blockCount = core::FrameCount::create(blockFrames);
        if (!blockCount) {
            return core::Result<WaveformSummary>::failure(*blockCount.error());
        }
        auto block = AudioBuffer::create(
            info.audio_format(),
            FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{absoluteFrame},
            *blockCount.value());
        if (!block) {
            return core::Result<WaveformSummary>::failure(*block.error());
        }
        auto decoded = reader.read_frames(
            core::FrameIndex{absoluteFrame},
            block.value()->mutable_view());
        if (!decoded) {
            return core::Result<WaveformSummary>::failure(*decoded.error());
        }
        if (decoded.value()->value() != blockFrames) {
            return failure<WaveformSummary>(
                core::ErrorCode::TruncatedAudioData,
                "Waveform decode returned fewer frames than the Source metadata.");
        }

        const auto view = block.value()->view();
        for (std::size_t channel = 0; channel < channelCount; ++channel) {
            auto plane = view.channel(channel);
            if (!plane) {
                return core::Result<WaveformSummary>::failure(*plane.error());
            }
            for (std::int64_t local = 0; local < blockFrames; ++local) {
                const double sample = (*plane.value())[static_cast<std::size_t>(local)];
                if (!std::isfinite(sample)) {
                    return failure<WaveformSummary>(
                        core::ErrorCode::InvalidAudioSample,
                        "Waveform input contains a non-finite canonical sample.");
                }
                const auto bucket = static_cast<std::size_t>(
                    (absoluteFrame + local) / *framesPerBucket.value());
                const auto offset = channel * *baseSize.value() + bucket;
                auto& peak = base.peaks[offset];
                if (!initialized[offset]) {
                    peak = WaveformSummary::PeakRange{sample, sample};
                    initialized[offset] = true;
                } else {
                    peak.minimum = minimum_sample(peak.minimum, sample);
                    peak.maximum = maximum_sample(peak.maximum, sample);
                }
            }
        }
        absoluteFrame += blockFrames;
    }

    try {
        levels.push_back(std::move(base));
        std::size_t totalBucketsPerChannel = *baseSize.value();
        while (levels.back().bucketCount.value() > 1) {
            if (stopToken.stop_requested()) {
                return core::Result<WaveformSummary>::failure(cancelled_error());
            }
            const auto& previous = levels.back();
            auto nextBucketCount = core::ceil_div_signed(
                previous.bucketCount.value(), 2);
            auto nextFramesPerBucket = core::checked_multiply(
                previous.framesPerBucket.value(), 2);
            if (!nextBucketCount || !nextFramesPerBucket) {
                return core::Result<WaveformSummary>::failure(
                    !nextBucketCount ? *nextBucketCount.error() : *nextFramesPerBucket.error());
            }
            auto nextSize = checked_size(*nextBucketCount.value());
            if (!nextSize) {
                return core::Result<WaveformSummary>::failure(*nextSize.error());
            }
            auto nextPeakCountValue = core::checked_multiply(
                *nextBucketCount.value(), static_cast<std::int64_t>(channelCount));
            if (!nextPeakCountValue) {
                return core::Result<WaveformSummary>::failure(*nextPeakCountValue.error());
            }
            auto nextPeakCount = checked_size(*nextPeakCountValue.value());
            if (!nextPeakCount) {
                return core::Result<WaveformSummary>::failure(*nextPeakCount.error());
            }
            auto nextFramesCount = core::FrameCount::create(*nextFramesPerBucket.value());
            auto nextBucketsCount = core::FrameCount::create(*nextBucketCount.value());
            if (!nextFramesCount || !nextBucketsCount) {
                return core::Result<WaveformSummary>::failure(
                    !nextFramesCount ? *nextFramesCount.error() : *nextBucketsCount.error());
            }
            WaveformSummary::LevelData next{
                *nextFramesCount.value(),
                *nextBucketsCount.value(),
                {},
            };
            next.peaks.resize(*nextPeakCount.value());
            const auto previousSize = static_cast<std::size_t>(
                previous.bucketCount.value());
            for (std::size_t channel = 0; channel < channelCount; ++channel) {
                for (std::size_t bucket = 0; bucket < *nextSize.value(); ++bucket) {
                    const auto first = previous.peaks[channel * previousSize + bucket * 2U];
                    auto combined = first;
                    const auto secondIndex = bucket * 2U + 1U;
                    if (secondIndex < previousSize) {
                        const auto second = previous.peaks[
                            channel * previousSize + secondIndex];
                        combined.minimum = minimum_sample(first.minimum, second.minimum);
                        combined.maximum = maximum_sample(first.maximum, second.maximum);
                    }
                    next.peaks[channel * *nextSize.value() + bucket] = combined;
                }
            }
            totalBucketsPerChannel += *nextSize.value();
            if (totalBucketsPerChannel
                > WaveformSummary::kMaximumTotalBucketsPerChannel) {
                return failure<WaveformSummary>(
                    core::ErrorCode::IntegerOverflow,
                    "Waveform pyramid exceeds its fixed total-bucket bound.");
            }
            levels.push_back(std::move(next));
        }

        auto payloadValue = core::checked_multiply(
            static_cast<std::int64_t>(totalBucketsPerChannel),
            static_cast<std::int64_t>(channelCount));
        if (!payloadValue) {
            return core::Result<WaveformSummary>::failure(*payloadValue.error());
        }
        payloadValue = core::checked_multiply(
            *payloadValue.value(),
            static_cast<std::int64_t>(sizeof(WaveformSummary::PeakRange)));
        if (!payloadValue) {
            return core::Result<WaveformSummary>::failure(*payloadValue.error());
        }
        auto payloadSize = checked_size(*payloadValue.value());
        if (!payloadSize) {
            return core::Result<WaveformSummary>::failure(*payloadSize.error());
        }
        payloadBytes = *payloadSize.value();
    } catch (const std::bad_alloc&) {
        return failure<WaveformSummary>(
            core::ErrorCode::IoFailure,
            "Unable to allocate bounded waveform pyramid storage.");
    }

    if (stopToken.stop_requested()) {
        return core::Result<WaveformSummary>::failure(cancelled_error());
    }
    return core::Result<WaveformSummary>::success(WaveformSummary{
        info.audio_format().sample_rate(),
        channelCount,
        frameCount,
        std::move(levels),
        payloadBytes,
    });
}

}  // namespace rgsml::audio
