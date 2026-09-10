#include "waveform_geometry.hpp"

#include <algorithm>
#include <cmath>

namespace rgsml::ui::internal {
namespace {

[[nodiscard]] double bucket_boundary(
    double logicalWidth,
    double devicePixelRatio,
    std::size_t physicalWidth,
    std::size_t bucketCount,
    std::size_t boundaryIndex) noexcept
{
    if (boundaryIndex == 0U) {
        return 0.0;
    }
    if (boundaryIndex >= bucketCount) {
        return logicalWidth;
    }

    const auto physicalBoundary = std::floor(
        static_cast<long double>(boundaryIndex)
        * static_cast<long double>(physicalWidth)
        / static_cast<long double>(bucketCount));
    return static_cast<double>(physicalBoundary) / devicePixelRatio;
}

}  // namespace

std::size_t waveform_target_range_count(
    double logicalWidth,
    double devicePixelRatio) noexcept
{
    if (!std::isfinite(logicalWidth) || !std::isfinite(devicePixelRatio)
        || logicalWidth <= 0.0 || devicePixelRatio <= 0.0) {
        return 1U;
    }
    const auto physicalWidth = std::floor(logicalWidth * devicePixelRatio);
    return static_cast<std::size_t>(std::clamp(
        physicalWidth,
        1.0,
        static_cast<double>(audio::WaveformSummary::kMaximumUiRangesPerChannel)));
}

std::size_t select_waveform_level(
    const audio::WaveformSummary& summary,
    std::size_t targetRangeCount) noexcept
{
    if (summary.level_count() == 0U) {
        return 0U;
    }
    const auto target = std::clamp<std::size_t>(
        targetRangeCount,
        1U,
        audio::WaveformSummary::kMaximumUiRangesPerChannel);
    for (std::size_t levelIndex = 0U;
         levelIndex < summary.level_count();
         ++levelIndex) {
        const auto level = summary.level(levelIndex);
        if (level
            && static_cast<std::size_t>(level.value()->bucket_count().value()) <= target) {
            return levelIndex;
        }
    }
    return summary.level_count() - 1U;
}

WaveformBucketSpan waveform_bucket_span(
    double logicalWidth,
    double devicePixelRatio,
    std::size_t bucketCount,
    std::size_t bucketIndex) noexcept
{
    if (!std::isfinite(logicalWidth) || !std::isfinite(devicePixelRatio)
        || logicalWidth <= 0.0 || devicePixelRatio <= 0.0
        || bucketCount == 0U || bucketIndex >= bucketCount) {
        return {};
    }

    const auto physicalWidth = static_cast<std::size_t>(std::max(
        1.0,
        std::floor(logicalWidth * devicePixelRatio)));
    return WaveformBucketSpan{
        bucket_boundary(
            logicalWidth,
            devicePixelRatio,
            physicalWidth,
            bucketCount,
            bucketIndex),
        bucket_boundary(
            logicalWidth,
            devicePixelRatio,
            physicalWidth,
            bucketCount,
            bucketIndex + 1U),
    };
}

}  // namespace rgsml::ui::internal
