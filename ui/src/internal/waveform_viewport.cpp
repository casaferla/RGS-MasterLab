#include "waveform_viewport.hpp"

#include <rgsml/core/checked_integer.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace rgsml::ui::internal {
namespace {

[[nodiscard]] core::Result<std::int64_t> invalid_math(const char* message)
{
    return core::Result<std::int64_t>::failure(
        core::Error{core::ErrorCode::InvalidArgument, message});
}

[[nodiscard]] core::Result<std::int64_t> overflow_math()
{
    return core::Result<std::int64_t>::failure(core::Error{
        core::ErrorCode::IntegerOverflow,
        "Waveform viewport arithmetic exceeds signed 64-bit range."});
}

[[nodiscard]] core::Result<std::int64_t> mul_div_floor_and_remainder(
    std::int64_t left,
    std::int64_t right,
    std::int64_t divisor,
    std::int64_t& remainder)
{
    if (left < 0 || right < 0 || divisor <= 0) {
        return invalid_math("mul_div_ties_even requires non-negative operands and a positive divisor.");
    }

    // Binary long division of left*right/divisor.  The quotient and remainder
    // are advanced without ever materializing the potentially overflowing
    // product.  Each intermediate remainder stays in [0, divisor).
    std::int64_t quotient = 0;
    remainder = 0;
    const auto rightQuotient = right / divisor;
    const auto rightRemainder = right % divisor;
    bool started = false;
    for (int bit = 62; bit >= 0; --bit) {
        const bool set = ((static_cast<std::uint64_t>(left)
                           >> static_cast<unsigned int>(bit))
                          & 1U) != 0U;
        if (!started && !set) {
            continue;
        }
        started = true;

        auto doubled = core::checked_multiply(quotient, 2);
        if (!doubled) {
            return overflow_math();
        }
        quotient = *doubled.value();

        const bool remainderCarry = remainder >= divisor - remainder;
        remainder = remainderCarry
            ? remainder - (divisor - remainder)
            : remainder + remainder;
        if (remainderCarry) {
            auto incremented = core::checked_increment(quotient);
            if (!incremented) {
                return overflow_math();
            }
            quotient = *incremented.value();
        }

        if (!set) {
            continue;
        }
        auto withWhole = core::checked_add(quotient, rightQuotient);
        if (!withWhole) {
            return overflow_math();
        }
        quotient = *withWhole.value();
        if (rightRemainder != 0) {
            const bool carry = remainder >= divisor - rightRemainder;
            remainder = carry
                ? remainder - (divisor - rightRemainder)
                : remainder + rightRemainder;
            if (carry) {
                auto incremented = core::checked_increment(quotient);
                if (!incremented) {
                    return overflow_math();
                }
                quotient = *incremented.value();
            }
        }
    }
    return core::Result<std::int64_t>::success(quotient);
}

[[nodiscard]] std::int64_t ceil_ratio(
    std::int64_t value,
    std::int64_t numerator,
    std::int64_t denominator) noexcept
{
    const auto whole = value / denominator;
    const auto remainder = value % denominator;
    const auto wholeProduct = core::checked_multiply(whole, numerator);
    const auto remainderProduct = core::checked_multiply(remainder, numerator);
    if (!wholeProduct || !remainderProduct) {
        return std::numeric_limits<std::int64_t>::max();
    }
    auto tail = *remainderProduct.value() / denominator;
    if (*remainderProduct.value() % denominator != 0) {
        ++tail;
    }
    const auto result = core::checked_add(*wholeProduct.value(), tail);
    return result ? *result.value() : std::numeric_limits<std::int64_t>::max();
}

[[nodiscard]] std::int64_t clamp_boundary(
    std::int64_t value,
    std::int64_t maximum) noexcept
{
    return std::clamp<std::int64_t>(value, 0, std::max<std::int64_t>(0, maximum));
}

}  // namespace

core::Result<std::int64_t> round_div_ties_even(
    std::int64_t dividend,
    std::int64_t divisor)
{
    auto quotient = core::floor_div_signed(dividend, divisor);
    auto remainder = core::floor_mod(dividend, divisor);
    if (!quotient || !remainder) {
        return invalid_math("round_div_ties_even requires a positive divisor.");
    }
    const auto opposite = divisor - *remainder.value();
    const bool roundUp = *remainder.value() > opposite
        || (*remainder.value() == opposite && (*quotient.value() % 2 != 0));
    if (!roundUp) {
        return quotient;
    }
    return core::checked_increment(*quotient.value());
}

core::Result<std::int64_t> mul_div_ties_even(
    std::int64_t left,
    std::int64_t right,
    std::int64_t divisor)
{
    std::int64_t remainder = 0;
    auto quotient = mul_div_floor_and_remainder(left, right, divisor, remainder);
    if (!quotient) {
        return quotient;
    }
    const auto opposite = divisor - remainder;
    const bool roundUp = remainder > opposite
        || (remainder == opposite && (*quotient.value() % 2 != 0));
    if (!roundUp) {
        return quotient;
    }
    return core::checked_increment(*quotient.value());
}

std::int64_t waveform_physical_width(
    double logicalWidth,
    double devicePixelRatio) noexcept
{
    if (!std::isfinite(logicalWidth) || !std::isfinite(devicePixelRatio)
        || logicalWidth <= 0.0 || devicePixelRatio <= 0.0) {
        return 1;
    }
    const auto product = std::floor(
        static_cast<long double>(logicalWidth)
        * static_cast<long double>(devicePixelRatio));
    if (product >= static_cast<long double>(std::numeric_limits<std::int64_t>::max())) {
        return std::numeric_limits<std::int64_t>::max();
    }
    return std::max<std::int64_t>(1, static_cast<std::int64_t>(product));
}

void WaveformViewport::clear() noexcept
{
    sourceFrames_ = 0;
    start_ = 0;
    end_ = 0;
    minimumSpan_ = 0;
}

bool WaveformViewport::reset(
    core::FrameCount sourceFrameCount,
    core::FrameCount baseFramesPerBucket) noexcept
{
    clear();
    const auto frames = sourceFrameCount.value();
    const auto base = baseFramesPerBucket.value();
    if (frames <= 0 || base <= 0) {
        return false;
    }
    sourceFrames_ = frames;
    start_ = 0;
    end_ = frames;
    constexpr std::int64_t minimumBucketCount = 64;
    minimumSpan_ = base > frames / minimumBucketCount
        ? frames
        : std::max<std::int64_t>(1, base * minimumBucketCount);
    return true;
}

bool WaveformViewport::enabled() const noexcept { return sourceFrames_ > 0; }
core::FrameRange WaveformViewport::visible_range() const noexcept
{
    return *core::FrameRange::create(
        core::FrameIndex{start_}, core::FrameIndex{end_}).value();
}
core::FrameCount WaveformViewport::visible_span() const noexcept
{
    return *core::FrameCount::create(end_ - start_).value();
}
core::FrameCount WaveformViewport::minimum_span() const noexcept
{
    return *core::FrameCount::create(minimumSpan_).value();
}
core::FrameCount WaveformViewport::source_frame_count() const noexcept
{
    return *core::FrameCount::create(sourceFrames_).value();
}
bool WaveformViewport::is_full_fit() const noexcept
{
    return enabled() && start_ == 0 && end_ == sourceFrames_;
}

double WaveformViewport::zoom_position() const noexcept
{
    if (!enabled() || minimumSpan_ >= sourceFrames_) {
        return 0.0;
    }
    const auto available = static_cast<long double>(sourceFrames_ - minimumSpan_);
    const auto hidden = static_cast<long double>(sourceFrames_ - (end_ - start_));
    return static_cast<double>(std::clamp(hidden / available, 0.0L, 1.0L));
}

core::FrameIndex WaveformViewport::frame_boundary(
    std::int64_t physicalBoundary,
    std::int64_t physicalWidth) const noexcept
{
    if (!enabled() || physicalWidth <= 0) {
        return core::FrameIndex{0};
    }
    const auto pixel = clamp_boundary(physicalBoundary, physicalWidth);
    const auto mapped = mul_div_ties_even(pixel, end_ - start_, physicalWidth);
    if (!mapped) {
        return core::FrameIndex{start_};
    }
    const auto frame = core::checked_add(start_, *mapped.value());
    return core::FrameIndex{frame ? std::clamp(*frame.value(), start_, end_) : start_};
}

std::int64_t WaveformViewport::pixel_boundary(
    core::FrameIndex frame,
    std::int64_t physicalWidth) const noexcept
{
    if (!enabled() || physicalWidth <= 0) {
        return 0;
    }
    const auto bounded = std::clamp(frame.value(), start_, end_);
    const auto mapped = mul_div_ties_even(
        bounded - start_, physicalWidth, end_ - start_);
    return mapped ? std::clamp(*mapped.value(), std::int64_t{0}, physicalWidth) : 0;
}

core::FrameIndex WaveformViewport::seek_frame(
    std::int64_t physicalBoundary,
    std::int64_t physicalWidth) const noexcept
{
    if (!enabled()) {
        return core::FrameIndex{0};
    }
    return core::FrameIndex{std::clamp(
        frame_boundary(physicalBoundary, physicalWidth).value(),
        std::int64_t{0},
        sourceFrames_ - 1)};
}

bool WaveformViewport::set_start_and_span(
    std::int64_t start,
    std::int64_t span) noexcept
{
    if (!enabled() || span < minimumSpan_ || span > sourceFrames_) {
        return false;
    }
    const auto boundedStart = std::clamp(
        start, std::int64_t{0}, sourceFrames_ - span);
    const auto candidateEnd = boundedStart + span;
    const bool changed = start_ != boundedStart || end_ != candidateEnd;
    start_ = boundedStart;
    end_ = candidateEnd;
    return changed;
}

bool WaveformViewport::set_span_anchored(
    std::int64_t candidateSpan,
    std::int64_t anchorPhysicalBoundary,
    std::int64_t physicalWidth) noexcept
{
    if (!enabled() || physicalWidth <= 0) {
        return false;
    }
    const auto pixel = clamp_boundary(anchorPhysicalBoundary, physicalWidth);
    const auto anchorFrame = frame_boundary(pixel, physicalWidth).value();
    const auto offset = mul_div_ties_even(pixel, candidateSpan, physicalWidth);
    if (!offset) {
        return false;
    }
    const auto raw = core::checked_subtract(anchorFrame, *offset.value());
    return set_start_and_span(raw ? *raw.value() : 0, candidateSpan);
}

bool WaveformViewport::zoom(
    bool zoomIn,
    std::int64_t anchorPhysicalBoundary,
    std::int64_t physicalWidth) noexcept
{
    if (!enabled()) {
        return false;
    }
    const auto span = end_ - start_;
    const auto candidate = zoomIn
        ? std::max(minimumSpan_, ceil_ratio(span, 4, 5))
        : std::min(sourceFrames_, ceil_ratio(span, 5, 4));
    return set_span_anchored(candidate, anchorPhysicalBoundary, physicalWidth);
}

bool WaveformViewport::set_zoom_position(
    double position,
    std::int64_t anchorPhysicalBoundary,
    std::int64_t physicalWidth) noexcept
{
    if (!enabled() || !std::isfinite(position)) {
        return false;
    }
    const auto bounded = std::clamp(position, 0.0, 1.0);
    const auto available = static_cast<long double>(sourceFrames_ - minimumSpan_);
    const auto requested = static_cast<long double>(minimumSpan_)
        + (1.0L - static_cast<long double>(bounded)) * available;
    std::int64_t candidate = minimumSpan_;
    if (requested >= static_cast<long double>(sourceFrames_)) {
        candidate = sourceFrames_;
    } else if (requested > static_cast<long double>(minimumSpan_)) {
        candidate = static_cast<std::int64_t>(std::floor(requested + 0.5L));
    }
    return set_span_anchored(candidate, anchorPhysicalBoundary, physicalWidth);
}

bool WaveformViewport::pan_from_snapshot(
    std::int64_t pressPhysicalBoundary,
    std::int64_t currentPhysicalBoundary,
    std::int64_t physicalWidth,
    core::FrameRange dragStartViewport) noexcept
{
    if (!enabled() || physicalWidth <= 0) {
        return false;
    }
    const auto dragStart = dragStartViewport.begin().value();
    const auto dragEnd = dragStartViewport.end().value();
    const auto span = dragEnd - dragStart;
    if (span < minimumSpan_ || span > sourceFrames_) {
        return false;
    }
    const auto p0 = clamp_boundary(pressPhysicalBoundary, physicalWidth);
    const auto p1 = clamp_boundary(currentPhysicalBoundary, physicalWidth);
    const auto magnitude = mul_div_ties_even(
        p0 >= p1 ? p0 - p1 : p1 - p0,
        span,
        physicalWidth);
    if (!magnitude) {
        return false;
    }
    const auto signedDelta = p0 >= p1
        ? core::Result<std::int64_t>::success(*magnitude.value())
        : core::checked_negate(*magnitude.value());
    if (!signedDelta) {
        return false;
    }
    const auto raw = core::checked_add(dragStart, *signedDelta.value());
    return set_start_and_span(raw ? *raw.value() : 0, span);
}

bool WaveformViewport::pan_step(
    bool towardRight,
    bool fine,
    std::int64_t physicalWidth) noexcept
{
    if (!enabled() || is_full_fit()) {
        return false;
    }
    const auto span = end_ - start_;
    std::int64_t step = 1;
    if (fine) {
        const auto rounded = round_div_ties_even(span, std::max<std::int64_t>(1, physicalWidth));
        step = rounded ? std::max<std::int64_t>(1, *rounded.value()) : 1;
    } else {
        step = std::max<std::int64_t>(1, span / 10 + (span % 10 != 0 ? 1 : 0));
    }
    const auto delta = towardRight ? step : -step;
    const auto raw = core::checked_add(start_, delta);
    return set_start_and_span(raw ? *raw.value() : (towardRight ? sourceFrames_ : 0), span);
}

bool WaveformViewport::fit_source() noexcept
{
    return enabled() && set_start_and_span(0, sourceFrames_);
}

bool WaveformViewport::fit_region(core::FrameRange region) noexcept
{
    if (!enabled()) {
        return false;
    }
    const auto start = std::clamp(region.begin().value(), std::int64_t{0}, sourceFrames_);
    const auto end = std::clamp(region.end().value(), std::int64_t{0}, sourceFrames_);
    if (start >= end) {
        return false;
    }
    const auto length = end - start;
    if (length >= minimumSpan_) {
        return set_start_and_span(start, length);
    }
    const auto left = minimumSpan_ - length;
    const auto candidateStart = start - left / 2;
    return set_start_and_span(candidateStart, minimumSpan_);
}

std::optional<VisibleWaveformWindow> select_visible_waveform_window(
    const audio::WaveformSummary& summary,
    core::FrameRange visibleRange) noexcept
{
    if (visibleRange.begin().value() < 0
        || visibleRange.begin().value() >= visibleRange.end().value()
        || visibleRange.end().value() > summary.source_frame_count().value()) {
        return std::nullopt;
    }
    for (std::size_t levelIndex = 0; levelIndex < summary.level_count(); ++levelIndex) {
        const auto levelResult = summary.level(levelIndex);
        if (!levelResult) {
            continue;
        }
        const auto level = *levelResult.value();
        const auto bucketSize = level.frames_per_bucket().value();
        const auto bucketCount = level.bucket_count().value();
        const auto first = visibleRange.begin().value() / bucketSize;
        const auto end = std::min(
            bucketCount,
            visibleRange.end().value() / bucketSize
                + (visibleRange.end().value() % bucketSize != 0 ? 1 : 0));
        if (end - first <= static_cast<std::int64_t>(
                audio::WaveformSummary::kMaximumUiRangesPerChannel)) {
            return VisibleWaveformWindow{levelIndex, first, end};
        }
    }
    return std::nullopt;
}

}  // namespace rgsml::ui::internal
