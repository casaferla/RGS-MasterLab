#pragma once

#include <rgsml/audio/waveform_summary.hpp>

#include <cstddef>

namespace rgsml::ui::internal {

inline constexpr float kWaveformPlayheadWidth = 2.0F;
inline constexpr float kWaveformPlayheadMarkerSize = 8.0F;
inline constexpr float kWaveformRegionBoundaryWidth = 2.0F;
inline constexpr float kWaveformRegionHandleWidth = 6.0F;
inline constexpr float kWaveformRegionHandleHeight = 16.0F;

struct WaveformBucketSpan final {
    double left{0.0};
    double right{0.0};
};

[[nodiscard]] std::size_t waveform_target_range_count(
    double logicalWidth,
    double devicePixelRatio) noexcept;

[[nodiscard]] std::size_t select_waveform_level(
    const audio::WaveformSummary& summary,
    std::size_t targetRangeCount) noexcept;

[[nodiscard]] WaveformBucketSpan waveform_bucket_span(
    double logicalWidth,
    double devicePixelRatio,
    std::size_t bucketCount,
    std::size_t bucketIndex) noexcept;

}  // namespace rgsml::ui::internal
