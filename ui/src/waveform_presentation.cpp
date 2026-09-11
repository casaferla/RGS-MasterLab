#include "waveform_presentation.hpp"

#include "internal/waveform_viewport.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace rgsml::ui {

WaveformPresentation::WaveformPresentation(QObject* parent)
    : QObject(parent)
    , viewport_(std::make_unique<internal::WaveformViewport>())
{
}

WaveformPresentation::~WaveformPresentation() = default;

QString WaveformPresentation::state_token() const
{
    switch (state_) {
    case State::Empty:
        return QStringLiteral("EMPTY");
    case State::Building:
        return QStringLiteral("BUILDING");
    case State::Ready:
        return QStringLiteral("READY");
    case State::Failed:
        return QStringLiteral("FAILED");
    }
    return QStringLiteral("FAILED");
}

QString WaveformPresentation::status_text() const
{
    switch (state_) {
    case State::Empty:
        return QStringLiteral("Select a Source WAV to build its overview.");
    case State::Building:
        return QStringLiteral("Building bounded Source overview…");
    case State::Ready:
        return summary_ && summary_->source_frame_count().value() == 0
            ? QStringLiteral("The Source contains no audio frames.")
            : QStringLiteral("Full Source overview");
    case State::Failed:
        return failureMessage_.isEmpty()
            ? QStringLiteral("Waveform analysis failed. The Source is unchanged.")
            : failureMessage_;
    }
    return {};
}

bool WaveformPresentation::ready() const noexcept
{
    return state_ == State::Ready && static_cast<bool>(summary_);
}

bool WaveformPresentation::has_overrange() const noexcept { return hasOverrange_; }
int WaveformPresentation::channel_count() const noexcept
{
    return summary_ ? static_cast<int>(summary_->channel_count()) : 0;
}
qint64 WaveformPresentation::source_frame_count() const noexcept
{
    return summary_ ? summary_->source_frame_count().value() : 0;
}
qint64 WaveformPresentation::base_frames_per_bucket() const noexcept
{
    if (!summary_ || summary_->level_count() == 0U) {
        return 0;
    }
    const auto level = summary_->level(0U);
    return level ? level.value()->frames_per_bucket().value() : 0;
}
qint64 WaveformPresentation::base_bucket_count() const noexcept
{
    if (!summary_ || summary_->level_count() == 0U) {
        return 0;
    }
    const auto level = summary_->level(0U);
    return level ? level.value()->bucket_count().value() : 0;
}
qint64 WaveformPresentation::level_count() const noexcept
{
    return summary_ ? static_cast<qint64>(summary_->level_count()) : 0;
}
qint64 WaveformPresentation::payload_bytes() const noexcept
{
    return summary_ ? static_cast<qint64>(summary_->payload_bytes()) : 0;
}

bool WaveformPresentation::can_navigate() const noexcept
{
    return ready() && viewport_->enabled();
}

bool WaveformPresentation::full_fit() const noexcept
{
    return viewport_->is_full_fit();
}

QString WaveformPresentation::viewport_start_text() const
{
    return can_navigate()
        ? format_frame_boundary(viewport_->visible_range().begin().value())
        : QStringLiteral("—");
}

QString WaveformPresentation::viewport_end_text() const
{
    return can_navigate()
        ? format_frame_boundary(viewport_->visible_range().end().value())
        : QStringLiteral("—");
}

QString WaveformPresentation::viewport_duration_text() const
{
    return can_navigate()
        ? format_frame_boundary(viewport_->visible_span().value())
        : QStringLiteral("—");
}

qint64 WaveformPresentation::visible_range_count() const noexcept
{
    if (!summary_ || !viewport_->enabled()) {
        return 0;
    }
    const auto selected = internal::select_visible_waveform_window(
        *summary_, viewport_->visible_range());
    return selected ? selected->count() : 0;
}

std::shared_ptr<const audio::WaveformSummary> WaveformPresentation::summary() const noexcept
{
    return summary_;
}

core::FrameRange WaveformPresentation::visible_range() const noexcept
{
    return viewport_->visible_range();
}

std::optional<core::FrameRange> WaveformPresentation::displayed_region() const noexcept
{
    return displayedRegion_;
}

std::optional<core::FrameRange> WaveformPresentation::candidate_region() const noexcept
{
    return candidateRegion_;
}

std::int64_t WaveformPresentation::physical_frame_boundary(
    std::int64_t physicalBoundary,
    std::int64_t physicalWidth) const noexcept
{
    return viewport_->frame_boundary(physicalBoundary, physicalWidth).value();
}

core::FrameIndex WaveformPresentation::physical_seek_frame(
    std::int64_t physicalBoundary,
    std::int64_t physicalWidth) const noexcept
{
    return viewport_->seek_frame(physicalBoundary, physicalWidth);
}

std::int64_t WaveformPresentation::physical_pixel_boundary(
    std::int64_t sourceFrameBoundary,
    std::int64_t physicalWidth) const noexcept
{
    return viewport_->pixel_boundary(
        core::FrameIndex{sourceFrameBoundary}, physicalWidth);
}

void WaveformPresentation::publish_empty()
{
    state_ = State::Empty;
    failureMessage_.clear();
    summary_.reset();
    viewport_->clear();
    candidateRegion_.reset();
    hasOverrange_ = false;
    emit changed();
}

void WaveformPresentation::publish_building()
{
    state_ = State::Building;
    failureMessage_.clear();
    summary_.reset();
    viewport_->clear();
    candidateRegion_.reset();
    hasOverrange_ = false;
    emit changed();
}

void WaveformPresentation::publish_ready(
    std::shared_ptr<const audio::WaveformSummary> summary)
{
    if (!summary) {
        publish_failed(QStringLiteral("Waveform analysis returned no summary."));
        return;
    }
    bool overrange = false;
    if (summary->level_count() > 0U) {
        const auto coarsest = summary->level(summary->level_count() - 1U);
        if (coarsest) {
            for (std::size_t channel = 0; channel < summary->channel_count(); ++channel) {
                const auto peak = coarsest.value()->peak(channel, core::FrameIndex{0});
                if (peak && (peak.value()->minimum < -1.0 || peak.value()->maximum > 1.0)) {
                    overrange = true;
                    break;
                }
            }
        }
    }
    summary_ = std::move(summary);
    if (summary_->source_frame_count().value() == 0) {
        viewport_->clear();
    } else {
        const auto levelZero = summary_->level(0U);
        if (!levelZero
            || !viewport_->reset(
                summary_->source_frame_count(),
                levelZero.value()->frames_per_bucket())) {
            publish_failed(QStringLiteral("Waveform viewport could not initialize."));
            return;
        }
    }
    hasOverrange_ = overrange;
    failureMessage_.clear();
    state_ = State::Ready;
    emit changed();
}

void WaveformPresentation::publish_failed(QString message)
{
    state_ = State::Failed;
    summary_.reset();
    viewport_->clear();
    candidateRegion_.reset();
    hasOverrange_ = false;
    failureMessage_ = std::move(message);
    emit changed();
}

void WaveformPresentation::set_seek_handler(SeekHandler handler)
{
    seekHandler_ = std::move(handler);
}

void WaveformPresentation::set_region_commit_handler(RegionCommitHandler handler)
{
    regionCommitHandler_ = std::move(handler);
}

void WaveformPresentation::set_displayed_region(
    std::optional<core::FrameRange> region)
{
    if (displayedRegion_ == region) {
        return;
    }
    displayedRegion_ = std::move(region);
    emit changed();
}

void WaveformPresentation::set_playhead_frame(core::FrameIndex frame)
{
    playheadFrame_ = frame;
}

void WaveformPresentation::set_candidate_region(
    std::optional<core::FrameRange> region)
{
    if (candidateRegion_ == region) {
        return;
    }
    candidateRegion_ = std::move(region);
    emit changed();
}

void WaveformPresentation::cancel_candidate()
{
    set_candidate_region(std::nullopt);
}

bool WaveformPresentation::seek_at(
    std::int64_t physicalBoundary,
    std::int64_t physicalWidth)
{
    if (!can_navigate()) {
        return false;
    }
    return seek_frame(physical_seek_frame(physicalBoundary, physicalWidth));
}

bool WaveformPresentation::seek_frame(core::FrameIndex position)
{
    if (!can_navigate() || !seekHandler_) {
        return false;
    }
    return static_cast<bool>(seekHandler_(position));
}

bool WaveformPresentation::commit_candidate(core::FrameRange candidate)
{
    if (!can_navigate() || !regionCommitHandler_) {
        cancel_candidate();
        return false;
    }
    const bool accepted = static_cast<bool>(regionCommitHandler_(candidate));
    cancel_candidate();
    return accepted;
}

bool WaveformPresentation::zoom_at(
    bool zoomIn,
    std::int64_t anchorPhysicalBoundary,
    std::int64_t physicalWidth)
{
    if (!can_navigate()
        || !viewport_->zoom(zoomIn, anchorPhysicalBoundary, physicalWidth)) {
        return false;
    }
    emit changed();
    return true;
}

bool WaveformPresentation::zoom_keyboard(
    bool zoomIn,
    std::int64_t physicalWidth)
{
    return zoom_at(
        zoomIn, keyboard_anchor(physicalWidth), physicalWidth);
}

bool WaveformPresentation::pan_from_snapshot(
    std::int64_t pressPhysicalBoundary,
    std::int64_t currentPhysicalBoundary,
    std::int64_t physicalWidth,
    core::FrameRange dragStartViewport)
{
    if (!can_navigate()
        || !viewport_->pan_from_snapshot(
            pressPhysicalBoundary,
            currentPhysicalBoundary,
            physicalWidth,
            dragStartViewport)) {
        return false;
    }
    emit changed();
    return true;
}

bool WaveformPresentation::pan_step(
    bool towardRight,
    bool fine,
    std::int64_t physicalWidth)
{
    if (!can_navigate()
        || !viewport_->pan_step(towardRight, fine, physicalWidth)) {
        return false;
    }
    emit changed();
    return true;
}

void WaveformPresentation::requestRetry()
{
    if (state_ == State::Failed) {
        emit retryRequested();
    }
}

std::int64_t WaveformPresentation::keyboard_anchor(
    std::int64_t physicalWidth) const noexcept
{
    if (physicalWidth <= 0 || !viewport_->enabled()) {
        return 0;
    }
    const auto visible = viewport_->visible_range();
    if (playheadFrame_.value() >= visible.begin().value()
        && playheadFrame_.value() <= visible.end().value()) {
        return viewport_->pixel_boundary(playheadFrame_, physicalWidth);
    }
    return physicalWidth / 2;
}

void WaveformPresentation::zoomIn()
{
    constexpr std::int64_t virtualWidth = 1'000;
    static_cast<void>(zoom_keyboard(true, virtualWidth));
}

void WaveformPresentation::zoomOut()
{
    constexpr std::int64_t virtualWidth = 1'000;
    static_cast<void>(zoom_keyboard(false, virtualWidth));
}

void WaveformPresentation::fitSource()
{
    if (can_navigate() && viewport_->fit_source()) {
        emit changed();
    }
}

void WaveformPresentation::fitRegion()
{
    if (can_navigate() && displayedRegion_ && viewport_->fit_region(*displayedRegion_)) {
        emit changed();
    }
}

QString WaveformPresentation::format_frame_boundary(std::int64_t frame) const
{
    if (!summary_ || frame < 0) {
        return QStringLiteral("—");
    }
    const auto rate = summary_->sample_rate().value();
    const auto seconds = frame / rate;
    const auto remainder = frame % rate;
    const auto microsResult = internal::mul_div_ties_even(remainder, 1'000'000, rate);
    auto micros = microsResult ? *microsResult.value() : 0;
    auto adjustedSeconds = seconds;
    if (micros == 1'000'000) {
        ++adjustedSeconds;
        micros = 0;
    }
    const auto hours = adjustedSeconds / 3'600;
    const auto minutes = (adjustedSeconds / 60) % 60;
    const auto trailingSeconds = adjustedSeconds % 60;
    return QStringLiteral("%1:%2:%3.%4")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(trailingSeconds, 2, 10, QLatin1Char('0'))
        .arg(micros, 6, 10, QLatin1Char('0'));
}

}  // namespace rgsml::ui
