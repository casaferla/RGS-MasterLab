#pragma once

#include <rgsml/audio/waveform_summary.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace rgsml::ui::internal {

struct VisibleWaveformWindow final {
    std::size_t levelIndex{0U};
    std::int64_t firstBucket{0};
    std::int64_t endBucket{0};

    [[nodiscard]] std::int64_t count() const noexcept
    {
        return endBucket - firstBucket;
    }
};

[[nodiscard]] core::Result<std::int64_t> round_div_ties_even(
    std::int64_t dividend,
    std::int64_t divisor);

[[nodiscard]] core::Result<std::int64_t> mul_div_ties_even(
    std::int64_t left,
    std::int64_t right,
    std::int64_t divisor);

[[nodiscard]] std::int64_t waveform_physical_width(
    double logicalWidth,
    double devicePixelRatio) noexcept;

class WaveformViewport final {
public:
    void clear() noexcept;
    [[nodiscard]] bool reset(
        core::FrameCount sourceFrameCount,
        core::FrameCount baseFramesPerBucket) noexcept;

    [[nodiscard]] bool enabled() const noexcept;
    [[nodiscard]] core::FrameRange visible_range() const noexcept;
    [[nodiscard]] core::FrameCount visible_span() const noexcept;
    [[nodiscard]] core::FrameCount minimum_span() const noexcept;
    [[nodiscard]] core::FrameCount source_frame_count() const noexcept;
    [[nodiscard]] bool is_full_fit() const noexcept;
    [[nodiscard]] double zoom_position() const noexcept;

    [[nodiscard]] core::FrameIndex frame_boundary(
        std::int64_t physicalBoundary,
        std::int64_t physicalWidth) const noexcept;
    [[nodiscard]] std::int64_t pixel_boundary(
        core::FrameIndex frame,
        std::int64_t physicalWidth) const noexcept;
    [[nodiscard]] core::FrameIndex seek_frame(
        std::int64_t physicalBoundary,
        std::int64_t physicalWidth) const noexcept;

    [[nodiscard]] bool zoom(
        bool zoomIn,
        std::int64_t anchorPhysicalBoundary,
        std::int64_t physicalWidth) noexcept;
    [[nodiscard]] bool set_zoom_position(
        double position,
        std::int64_t anchorPhysicalBoundary,
        std::int64_t physicalWidth) noexcept;
    [[nodiscard]] bool pan_from_snapshot(
        std::int64_t pressPhysicalBoundary,
        std::int64_t currentPhysicalBoundary,
        std::int64_t physicalWidth,
        core::FrameRange dragStartViewport) noexcept;
    [[nodiscard]] bool pan_step(
        bool towardRight,
        bool fine,
        std::int64_t physicalWidth) noexcept;
    [[nodiscard]] bool fit_source() noexcept;
    [[nodiscard]] bool fit_region(core::FrameRange region) noexcept;

private:
    [[nodiscard]] bool set_span_anchored(
        std::int64_t candidateSpan,
        std::int64_t anchorPhysicalBoundary,
        std::int64_t physicalWidth) noexcept;
    [[nodiscard]] bool set_start_and_span(
        std::int64_t start,
        std::int64_t span) noexcept;

    std::int64_t sourceFrames_{0};
    std::int64_t start_{0};
    std::int64_t end_{0};
    std::int64_t minimumSpan_{0};
};

[[nodiscard]] std::optional<VisibleWaveformWindow> select_visible_waveform_window(
    const audio::WaveformSummary& summary,
    core::FrameRange visibleRange) noexcept;

}  // namespace rgsml::ui::internal
