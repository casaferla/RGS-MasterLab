#include "internal/true_peak_chronological_buckets.hpp"

#include <rgsml/core/error.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace rgsml::analysis::internal {
namespace {
[[nodiscard]] bool is_supported_rate(std::int64_t hz) noexcept
{
    switch (hz) {
    case 44100: case 48000: case 88200:
    case 96000: case 176400: case 192000: return true;
    default: return false;
    }
}
} // namespace

core::Result<ChronologicalTruePeakBuckets>
ChronologicalTruePeakBuckets::create(
    std::int64_t sampleRateHz, std::size_t channels,
    std::int64_t startSourceFrame)
{
    if (!is_supported_rate(sampleRateHz) || (channels != 1 && channels != 2)
        || startSourceFrame < 0) {
        return core::Result<ChronologicalTruePeakBuckets>::failure(
            core::Error{core::ErrorCode::InvalidArgument,
                "C2D5 requires one of six M16 rates, mono/stereo, nonnegative source start."});
    }
    const auto bucketFrames = sampleRateHz / 100; // six rates divide exactly
    if (startSourceFrame >= std::numeric_limits<std::int64_t>::max()
            - bucketFrames) {
        return core::Result<ChronologicalTruePeakBuckets>::failure(
            core::Error{core::ErrorCode::IntegerOverflow,
                "C2D5 source bucket cursor would overflow."});
    }
    ChronologicalTruePeakBuckets instance{};
    instance.framesPerBucket_ = bucketFrames;
    instance.channelCount_ = channels;
    instance.nextSourceFrame_ = startSourceFrame;
    instance.nextBucketEnd_ =
        (startSourceFrame / bucketFrames + 1) * bucketFrames;
    // A seek into an existing resource does NOT grant invented zero history.
    instance.openFullyProven_ = startSourceFrame == 0;
    instance.unknownHistoryFrames_ = startSourceFrame == 0 ? 0U : 11U;
    return core::Result<ChronologicalTruePeakBuckets>::success(std::move(instance));
}

void ChronologicalTruePeakBuckets::advance_one_frame() noexcept
{
    const auto frame = nextSourceFrame_++;
    if (nextSourceFrame_ == nextBucketEnd_) {
        const auto index = frame / framesPerBucket_;
        ring_[static_cast<std::size_t>(index % static_cast<std::int64_t>(kHoldBuckets))] =
            CompletedBucket{index, openFullyProven_, openPeak_};
        lastClosedIndex_ = index;
        openPeak_ = {};
        openFullyProven_ = true;
        // Reaching max is guarded at the start of every consume operation;
        // skip forward if even the next bucket endpoint would overflow.
        if (nextBucketEnd_ <= std::numeric_limits<std::int64_t>::max()
                - framesPerBucket_) {
            nextBucketEnd_ += framesPerBucket_;
        }
    }
}

core::Status ChronologicalTruePeakBuckets::consume_gap() noexcept
{
    if (nextSourceFrame_ == std::numeric_limits<std::int64_t>::max()
        || nextSourceFrame_ >= nextBucketEnd_) {
        return core::Status::failure(core::Error{
            core::ErrorCode::IntegerOverflow,
            "C2D5 cannot advance a saturated chronological source timeline."});
    }
    openFullyProven_ = false;
    unknownHistoryFrames_ = 11U;
    for (auto& stream : fir_) stream.reset();
    advance_one_frame();
    return core::Status::success();
}

core::Status ChronologicalTruePeakBuckets::consume_frame(
    std::span<const double> samples, bool sourceSupportProven)
{
    if (!sourceSupportProven) return consume_gap();
    if (samples.size() != channelCount_) {
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidArgument,
            "C2D5 canonical frame has an invalid mono/stereo channel count."});
    }
    if (nextSourceFrame_ == std::numeric_limits<std::int64_t>::max()
        || nextSourceFrame_ >= nextBucketEnd_) {
        return core::Status::failure(core::Error{
            core::ErrorCode::IntegerOverflow,
            "C2D5 cannot advance a saturated chronological source timeline."});
    }
    // Stage both FIR channels before committing either to ensure a late
    // right-channel NaN cannot mutate left history or the temporal cursor.
    auto staged = fir_;
    std::array<double, 2> peaks{};
    for (std::size_t ch = 0; ch < channelCount_; ++ch) {
        const auto phases = staged[ch].push(samples[ch]);
        if (!phases) return core::Status::failure(*phases.error());
        for (const double phase : phases.value()->signed_phases) {
            peaks[ch] = std::max(peaks[ch], std::abs(phase));
        }
    }
    fir_ = staged;
    if (unknownHistoryFrames_ > 0) {
        --unknownHistoryFrames_;
        openFullyProven_ = false;
    } else {
        for (std::size_t ch = 0; ch < channelCount_; ++ch)
            openPeak_[ch] = std::max(openPeak_[ch], peaks[ch]);
    }
    advance_one_frame();
    return core::Status::success();
}

ChronologicalTruePeakWindow
ChronologicalTruePeakBuckets::snapshot() const noexcept
{
    ChronologicalTruePeakWindow out{};
    out.consumedSourceFrames = nextSourceFrame_;
    if (lastClosedIndex_ < 0) return out;
    out.lastCompleteBucketEnd = (lastClosedIndex_ + 1) * framesPerBucket_;
    const auto collect = [&](std::size_t count, std::array<double, 2>& peak) {
        if (lastClosedIndex_ < static_cast<std::int64_t>(count - 1))
            return false;
        for (std::size_t n = 0; n < count; ++n) {
            const auto wanted = lastClosedIndex_ - static_cast<std::int64_t>(n);
            const auto& b = ring_[static_cast<std::size_t>(
                wanted % static_cast<std::int64_t>(kHoldBuckets))];
            if (!b.fullyProven || b.index != wanted) return false;
            for (std::size_t ch = 0; ch < channelCount_; ++ch)
                peak[ch] = std::max(peak[ch], b.linearPeak[ch]);
        }
        return true;
    };
    out.recent100msComplete = collect(kRecentBuckets, out.recentLinearPeak);
    out.shortHold1sComplete = collect(kHoldBuckets, out.shortHoldLinearPeak);
    if (!out.recent100msComplete) out.recentLinearPeak = {};
    if (!out.shortHold1sComplete) out.shortHoldLinearPeak = {};
    return out;
}

} // namespace rgsml::analysis::internal
