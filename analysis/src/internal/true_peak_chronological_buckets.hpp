#pragma once

#include "true_peak_itu4x_channel_stream.hpp"

#include <rgsml/core/result.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace rgsml::analysis::internal {

// C2D5 numerical transport primitive only. A caller must independently
// establish an immutable PROCESSED owner, source-domain support and consumed
// watermark. These values are NOT LIVE, not Gate-G-qualified and not a QML DTO.
// All peak math delegates to the frozen M16 4x FIR stream, never a second FIR.
struct ChronologicalTruePeakWindow final {
    std::int64_t consumedSourceFrames{0};
    std::int64_t lastCompleteBucketEnd{0};
    bool recent100msComplete{false};
    bool shortHold1sComplete{false};
    std::array<double, 2> recentLinearPeak{};
    std::array<double, 2> shortHoldLinearPeak{};
};

class ChronologicalTruePeakBuckets final {
public:
    [[nodiscard]] static core::Result<ChronologicalTruePeakBuckets> create(
        std::int64_t sampleRateHz, std::size_t channels,
        std::int64_t startSourceFrame = 0);

    // Serialized worker-only input, never the device callback. 'proven' is
    // local source support evidence, not full immutable PROCESSED provenance.
    // A false value inserts an explicit unknown frame and resets FIR support.
    // NaN/Inf and malformed frame sizes fail atomically, without consuming.
    [[nodiscard]] core::Status consume_frame(
        std::span<const double> samples, bool sourceSupportProven);
    [[nodiscard]] core::Status consume_gap();
    [[nodiscard]] ChronologicalTruePeakWindow snapshot() const noexcept;

    [[nodiscard]] std::int64_t consumed_source_frames() const noexcept {
        return nextSourceFrame_;
    }
    [[nodiscard]] std::int64_t frames_per_bucket() const noexcept {
        return framesPerBucket_;
    }

private:
    struct CompletedBucket final {
        std::int64_t index{-1};
        bool fullyProven{false};
        std::array<double, 2> linearPeak{};
    };
    static constexpr std::size_t kHoldBuckets = 100U;
    static constexpr std::size_t kRecentBuckets = 10U;
    std::array<TruePeakItu4xChannelStream, 2> fir_{};
    std::array<CompletedBucket, kHoldBuckets> ring_{};
    std::array<double, 2> openPeak_{};
    std::int64_t framesPerBucket_{0};
    std::int64_t nextSourceFrame_{0};
    std::int64_t nextBucketEnd_{0};
    std::int64_t lastClosedIndex_{-1};
    std::size_t channelCount_{0};
    std::size_t unknownHistoryFrames_{0};
    bool openFullyProven_{false};

    void advance_one_frame() noexcept;
};

} // namespace rgsml::analysis::internal
