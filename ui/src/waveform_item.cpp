#include "waveform_item.hpp"

#include "internal/waveform_geometry.hpp"
#include "internal/waveform_viewport.hpp"

#include <rgsml/core/checked_integer.hpp>

#include <QFocusEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQuickWindow>
#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QStyleHints>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rgsml::ui {
namespace {

struct Vertex final {
    float x;
    float y;
};

[[nodiscard]] QSGGeometryNode* make_lines(
    const std::vector<Vertex>& vertices,
    const QColor& color)
{
    auto* geometry = new QSGGeometry(
        QSGGeometry::defaultAttributes_Point2D(),
        static_cast<int>(vertices.size()));
    geometry->setDrawingMode(QSGGeometry::DrawLines);
    auto* target = geometry->vertexDataAsPoint2D();
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        target[index].set(vertices[index].x, vertices[index].y);
    }

    auto* material = new QSGFlatColorMaterial;
    material->setColor(color);
    auto* node = new QSGGeometryNode;
    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

[[nodiscard]] QSGGeometryNode* make_triangles(
    const std::vector<Vertex>& vertices,
    const QColor& color)
{
    auto* geometry = new QSGGeometry(
        QSGGeometry::defaultAttributes_Point2D(),
        static_cast<int>(vertices.size()));
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* target = geometry->vertexDataAsPoint2D();
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        target[index].set(vertices[index].x, vertices[index].y);
    }

    auto* material = new QSGFlatColorMaterial;
    material->setColor(color);
    auto* node = new QSGGeometryNode;
    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

[[nodiscard]] double clipped_sample(double value) noexcept
{
    return std::clamp(value, -1.0, 1.0);
}

}  // namespace

WaveformItem::WaveformItem(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
    setActiveFocusOnTab(true);
    singleClickTimer_.setSingleShot(true);
    connect(
        &singleClickTimer_,
        &QTimer::timeout,
        this,
        &WaveformItem::commit_pending_seek);
}

WaveformPresentation* WaveformItem::presentation() const noexcept
{
    return presentation_;
}

void WaveformItem::set_presentation(WaveformPresentation* presentation)
{
    if (presentation_ == presentation) {
        return;
    }
    cancel_pending_seek();
    QObject::disconnect(presentationConnection_);
    presentation_ = presentation;
    if (presentation_) {
        presentationConnection_ = connect(
            presentation_,
            &WaveformPresentation::changed,
            this,
            &WaveformItem::synchronize_presentation);
    }
    synchronize_presentation();
    emit presentationChanged();
}

qint64 WaveformItem::position_frames() const noexcept { return positionFrames_; }
void WaveformItem::set_position_frames(qint64 frames)
{
    const auto bounded = std::max<qint64>(0, frames);
    if (positionFrames_ == bounded) {
        return;
    }
    positionFrames_ = bounded;
    if (presentation_) {
        presentation_->set_playhead_frame(core::FrameIndex{positionFrames_});
    }
    update();
    emit timelineChanged();
}
qint64 WaveformItem::duration_frames() const noexcept { return durationFrames_; }
void WaveformItem::set_duration_frames(qint64 frames)
{
    const auto bounded = std::max<qint64>(0, frames);
    if (durationFrames_ == bounded) {
        return;
    }
    durationFrames_ = bounded;
    update();
    emit timelineChanged();
}

QColor WaveformItem::waveform_color() const { return waveformColor_; }
void WaveformItem::set_waveform_color(const QColor& color)
{
    if (waveformColor_ == color) {
        return;
    }
    waveformColor_ = color;
    update();
    emit colorsChanged();
}
QColor WaveformItem::zero_line_color() const { return zeroLineColor_; }
void WaveformItem::set_zero_line_color(const QColor& color)
{
    if (zeroLineColor_ == color) {
        return;
    }
    zeroLineColor_ = color;
    update();
    emit colorsChanged();
}
QColor WaveformItem::playhead_color() const { return playheadColor_; }
void WaveformItem::set_playhead_color(const QColor& color)
{
    if (playheadColor_ == color) {
        return;
    }
    playheadColor_ = color;
    update();
    emit colorsChanged();
}
QColor WaveformItem::overrange_color() const { return overrangeColor_; }
void WaveformItem::set_overrange_color(const QColor& color)
{
    if (overrangeColor_ == color) {
        return;
    }
    overrangeColor_ = color;
    update();
    emit colorsChanged();
}

QColor WaveformItem::region_color() const { return regionColor_; }
void WaveformItem::set_region_color(const QColor& color)
{
    if (regionColor_ == color) {
        return;
    }
    regionColor_ = color;
    update();
    emit colorsChanged();
}

QColor WaveformItem::region_handle_color() const { return regionHandleColor_; }
void WaveformItem::set_region_handle_color(const QColor& color)
{
    if (regionHandleColor_ == color) {
        return;
    }
    regionHandleColor_ = color;
    update();
    emit colorsChanged();
}

qulonglong WaveformItem::discarded_wheel_steps() const noexcept
{
    return discardedWheelSteps_;
}

void WaveformItem::synchronize_presentation()
{
    summary_ = presentation_ ? presentation_->summary() : nullptr;
    hasOverrange_ = presentation_ && presentation_->has_overrange();
    if (presentation_) {
        presentation_->set_playhead_frame(core::FrameIndex{positionFrames_});
    }
    if (!presentation_ || !presentation_->can_navigate()) {
        cancel_pending_seek();
        cancel_gesture();
    }
    update();
}

std::int64_t WaveformItem::physical_width() const noexcept
{
    return internal::waveform_physical_width(
        width(), window() ? window()->devicePixelRatio() : 1.0);
}

std::int64_t WaveformItem::physical_boundary(double logicalX) const noexcept
{
    const auto dpr = window() ? window()->devicePixelRatio() : 1.0;
    const auto physicalWidth = physical_width();
    const auto bounded = std::clamp(logicalX, 0.0, std::max(0.0, width()));
    if (bounded >= width()) {
        return physicalWidth;
    }
    return std::clamp<std::int64_t>(
        static_cast<std::int64_t>(std::floor(bounded * dpr)),
        0,
        physicalWidth);
}

WaveformItem::Gesture WaveformItem::hit_test(
    std::int64_t physicalBoundary,
    bool shift) const noexcept
{
    if (shift) {
        return Gesture::CreateRegion;
    }
    if (!presentation_) {
        return Gesture::Background;
    }
    const auto region = presentation_->displayed_region();
    if (!region) {
        return Gesture::Background;
    }
    constexpr std::int64_t handleRadius = 6;
    const auto visible = presentation_->visible_range();
    const bool startVisible = region->begin().value() >= visible.begin().value()
        && region->begin().value() <= visible.end().value();
    const bool endVisible = region->end().value() >= visible.begin().value()
        && region->end().value() <= visible.end().value();
    const auto startPixel = presentation_->physical_pixel_boundary(
        region->begin().value(), physical_width());
    const auto endPixel = presentation_->physical_pixel_boundary(
        region->end().value(), physical_width());
    if (startVisible && std::abs(physicalBoundary - startPixel) <= handleRadius) {
        return Gesture::StartHandle;
    }
    if (endVisible && std::abs(physicalBoundary - endPixel) <= handleRadius) {
        return Gesture::EndHandle;
    }
    const auto frame = presentation_->physical_frame_boundary(
        physicalBoundary, physical_width());
    if (frame >= region->begin().value() && frame < region->end().value()) {
        return Gesture::RegionBody;
    }
    return Gesture::Background;
}

void WaveformItem::update_candidate(std::int64_t physicalBoundary)
{
    if (!presentation_
        || ((gesture_ == Gesture::StartHandle || gesture_ == Gesture::EndHandle)
            && !dragStartRegion_)) {
        return;
    }
    currentPhysicalBoundary_ = std::clamp(
        physicalBoundary, std::int64_t{0}, gesturePhysicalWidth_);
    if (std::abs(currentPhysicalBoundary_ - pressPhysicalBoundary_) >= 4) {
        dragThresholdReached_ = true;
    }
    if (!dragThresholdReached_) {
        return;
    }

    if (gesture_ == Gesture::Background) {
        if (dragStartViewport_) {
            static_cast<void>(presentation_->pan_from_snapshot(
                pressPhysicalBoundary_,
                currentPhysicalBoundary_,
                gesturePhysicalWidth_,
                *dragStartViewport_));
        }
        return;
    }

    const auto mapped = presentation_->physical_frame_boundary(
        currentPhysicalBoundary_, gesturePhysicalWidth_);
    std::int64_t start = mapped;
    std::int64_t end = mapped;
    if (gesture_ == Gesture::CreateRegion) {
        const auto anchor = presentation_->physical_frame_boundary(
            pressPhysicalBoundary_, gesturePhysicalWidth_);
        start = std::min(anchor, mapped);
        end = std::max(anchor, mapped);
    } else if (gesture_ == Gesture::StartHandle) {
        start = std::clamp(
            mapped,
            std::int64_t{0},
            dragStartRegion_->end().value() - 1);
        end = dragStartRegion_->end().value();
    } else if (gesture_ == Gesture::EndHandle) {
        start = dragStartRegion_->begin().value();
        end = std::clamp(
            mapped,
            start + 1,
            summary_ ? summary_->source_frame_count().value() : start + 1);
    }
    if (start < end) {
        auto candidate = core::FrameRange::create(
            core::FrameIndex{start}, core::FrameIndex{end});
        if (candidate) {
            presentation_->set_candidate_region(*candidate.value());
        }
    }
}

void WaveformItem::finish_gesture(std::int64_t physicalBoundary)
{
    if (!presentation_ || gesture_ == Gesture::None) {
        cancel_gesture();
        return;
    }
    update_candidate(physicalBoundary);
    if ((gesture_ == Gesture::Background || gesture_ == Gesture::RegionBody)
        && !dragThresholdReached_) {
        schedule_pending_seek(presentation_->physical_seek_frame(
            currentPhysicalBoundary_, gesturePhysicalWidth_));
    } else if (gesture_ != Gesture::Background && dragThresholdReached_) {
        const auto candidate = presentation_->candidate_region();
        if (candidate) {
            static_cast<void>(presentation_->commit_candidate(*candidate));
        }
    }
    cancel_gesture();
}

void WaveformItem::schedule_pending_seek(core::FrameIndex position)
{
    if (pendingSeek_) {
        commit_pending_seek();
    }
    pendingSeek_ = position;
    singleClickTimer_.start(
        QGuiApplication::styleHints()->mouseDoubleClickInterval());
}

void WaveformItem::cancel_pending_seek()
{
    singleClickTimer_.stop();
    pendingSeek_.reset();
}

void WaveformItem::commit_pending_seek()
{
    singleClickTimer_.stop();
    const auto position = pendingSeek_;
    pendingSeek_.reset();
    if (position && presentation_ && presentation_->can_navigate()) {
        static_cast<void>(presentation_->seek_frame(*position));
    }
}

void WaveformItem::cancel_gesture()
{
    if (presentation_) {
        presentation_->cancel_candidate();
    }
    gesture_ = Gesture::None;
    dragStartViewport_.reset();
    dragStartRegion_.reset();
    dragThresholdReached_ = false;
    ungrabMouse();
}

void WaveformItem::mousePressEvent(QMouseEvent* event)
{
    if (!presentation_ || !presentation_->can_navigate()
        || event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    forceActiveFocus(Qt::MouseFocusReason);
    gesturePhysicalWidth_ = physical_width();
    pressPhysicalBoundary_ = physical_boundary(event->position().x());
    currentPhysicalBoundary_ = pressPhysicalBoundary_;
    gesture_ = hit_test(
        pressPhysicalBoundary_,
        event->modifiers().testFlag(Qt::ShiftModifier));
    dragStartViewport_ = presentation_->visible_range();
    dragStartRegion_ = presentation_->displayed_region();
    dragThresholdReached_ = false;
    grabMouse();
    event->accept();
}

void WaveformItem::mouseMoveEvent(QMouseEvent* event)
{
    if (gesture_ == Gesture::None) {
        event->ignore();
        return;
    }
    update_candidate(physical_boundary(event->position().x()));
    event->accept();
}

void WaveformItem::mouseReleaseEvent(QMouseEvent* event)
{
    if (gesture_ == Gesture::None || event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    finish_gesture(physical_boundary(event->position().x()));
    event->accept();
}

void WaveformItem::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (!presentation_ || !presentation_->can_navigate()
        || event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    cancel_pending_seek();
    cancel_gesture();
    const auto target = hit_test(
        physical_boundary(event->position().x()), false);
    if (target == Gesture::RegionBody
        || target == Gesture::StartHandle
        || target == Gesture::EndHandle) {
        presentation_->fitRegion();
    } else {
        presentation_->fitSource();
    }
    event->accept();
}

void WaveformItem::mouseUngrabEvent()
{
    cancel_gesture();
}

void WaveformItem::wheelEvent(QWheelEvent* event)
{
    if (!presentation_ || !presentation_->can_navigate()) {
        event->ignore();
        return;
    }
    const auto modifiers = event->modifiers();
    if (!modifiers.testFlag(Qt::ControlModifier)
        && !modifiers.testFlag(Qt::ShiftModifier)) {
        event->ignore();
        return;
    }
    auto& remainder = modifiers.testFlag(Qt::ControlModifier)
        ? zoomWheelAngleRemainder_
        : panWheelAngleRemainder_;
    remainder += event->angleDelta().y();
    int steps = remainder / 120;
    remainder %= 120;
    if (std::abs(steps) > 8) {
        discardedWheelSteps_ += static_cast<qulonglong>(std::abs(steps) - 8);
        steps = std::clamp(steps, -8, 8);
        remainder = 0;
        emit diagnosticsChanged();
    }
    const auto count = std::abs(steps);
    for (int index = 0; index < count; ++index) {
        if (modifiers.testFlag(Qt::ControlModifier)) {
            static_cast<void>(presentation_->zoom_at(
                steps > 0,
                physical_boundary(event->position().x()),
                physical_width()));
        } else {
            static_cast<void>(presentation_->pan_step(
                steps < 0, false, physical_width()));
        }
    }
    event->accept();
}

void WaveformItem::keyPressEvent(QKeyEvent* event)
{
    if (!presentation_ || !presentation_->can_navigate()) {
        event->ignore();
        return;
    }
    const auto modifiers = event->modifiers();
    if (modifiers.testFlag(Qt::ControlModifier)
        && (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal)) {
        static_cast<void>(presentation_->zoom_keyboard(true, physical_width()));
    } else if (modifiers.testFlag(Qt::ControlModifier)
               && event->key() == Qt::Key_Minus) {
        static_cast<void>(presentation_->zoom_keyboard(false, physical_width()));
    } else if (modifiers.testFlag(Qt::AltModifier)
               && (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right)) {
        static_cast<void>(presentation_->pan_step(
            event->key() == Qt::Key_Right,
            modifiers.testFlag(Qt::ShiftModifier),
            physical_width()));
    } else if (event->key() == Qt::Key_Home) {
        presentation_->fitSource();
    } else if (event->key() == Qt::Key_Escape) {
        cancel_gesture();
    } else if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
               && presentation_->candidate_region()) {
        static_cast<void>(presentation_->commit_candidate(
            *presentation_->candidate_region()));
        cancel_gesture();
    } else {
        event->ignore();
        return;
    }
    event->accept();
}

void WaveformItem::focusOutEvent(QFocusEvent* event)
{
    cancel_pending_seek();
    cancel_gesture();
    QQuickItem::focusOutEvent(event);
}

void WaveformItem::geometryChange(
    const QRectF& newGeometry,
    const QRectF& oldGeometry)
{
    if (newGeometry.size() != oldGeometry.size()) {
        cancel_pending_seek();
        cancel_gesture();
    }
    QQuickItem::geometryChange(newGeometry, oldGeometry);
}

QSGNode* WaveformItem::updatePaintNode(
    QSGNode* oldNode,
    UpdatePaintNodeData*)
{
    delete oldNode;
    auto* root = new QSGNode;
    if (!summary_ || summary_->level_count() == 0U
        || width() <= 0.0 || height() <= 0.0) {
        return root;
    }

    if (!presentation_ || !presentation_->can_navigate()) {
        return root;
    }
    const double devicePixelRatio = window() ? window()->devicePixelRatio() : 1.0;
    const auto physicalWidth = physical_width();
    const auto visibleRange = presentation_->visible_range();
    const auto windowSelection = internal::select_visible_waveform_window(
        *summary_, visibleRange);
    if (!windowSelection) {
        return root;
    }
    const auto levelResult = summary_->level(windowSelection->levelIndex);
    if (!levelResult) {
        return root;
    }
    const auto level = *levelResult.value();
    const auto bucketCount = static_cast<std::size_t>(windowSelection->count());
    const auto channelCount = summary_->channel_count();
    if (bucketCount == 0U
        || bucketCount > audio::WaveformSummary::kMaximumUiRangesPerChannel) {
        return root;
    }

    std::vector<Vertex> axes;
    std::vector<Vertex> peaks;
    axes.reserve(channelCount * 2U);
    peaks.reserve(channelCount * bucketCount * 6U);
    const double laneHeight = height() / static_cast<double>(channelCount);
    for (std::size_t channel = 0; channel < channelCount; ++channel) {
        const double laneTop = laneHeight * static_cast<double>(channel);
        const double center = laneTop + laneHeight * 0.5;
        const double amplitude = std::max(1.0, laneHeight * 0.5 - 8.0);
        axes.push_back(Vertex{0.0F, static_cast<float>(center)});
        axes.push_back(Vertex{static_cast<float>(width()), static_cast<float>(center)});
        for (std::size_t offset = 0; offset < bucketCount; ++offset) {
            const auto bucket = windowSelection->firstBucket
                + static_cast<std::int64_t>(offset);
            const auto peakResult = level.peak(
                channel,
                core::FrameIndex{bucket});
            if (!peakResult) {
                continue;
            }
            const auto peak = *peakResult.value();
            const auto bucketStartProduct = core::checked_multiply(
                bucket, level.frames_per_bucket().value());
            const auto bucketEndProduct = core::checked_multiply(
                bucket + 1, level.frames_per_bucket().value());
            if (!bucketStartProduct || !bucketEndProduct) {
                continue;
            }
            const auto clippedStart = std::max(
                visibleRange.begin().value(), *bucketStartProduct.value());
            const auto clippedEnd = std::min({
                visibleRange.end().value(),
                summary_->source_frame_count().value(),
                *bucketEndProduct.value()});
            if (clippedStart >= clippedEnd) {
                continue;
            }
            const auto leftPhysical = presentation_->physical_pixel_boundary(
                clippedStart, physicalWidth);
            const auto rightPhysical = presentation_->physical_pixel_boundary(
                clippedEnd, physicalWidth);
            const auto logicalBoundary = [this, devicePixelRatio, physicalWidth](
                std::int64_t value) {
                return value >= physicalWidth
                    ? width()
                    : static_cast<double>(value) / devicePixelRatio;
            };
            const auto leftLogical = logicalBoundary(leftPhysical);
            const auto rightLogical = logicalBoundary(rightPhysical);
            if (rightLogical <= leftLogical) {
                continue;
            }
            const double top = center - clipped_sample(peak.maximum) * amplitude;
            const double bottom = center - clipped_sample(peak.minimum) * amplitude;
            const auto left = static_cast<float>(leftLogical);
            const auto right = static_cast<float>(rightLogical);
            const auto topVertex = static_cast<float>(top);
            const auto bottomVertex = static_cast<float>(bottom);
            peaks.insert(peaks.end(), {
                Vertex{left, topVertex},
                Vertex{left, bottomVertex},
                Vertex{right, bottomVertex},
                Vertex{left, topVertex},
                Vertex{right, bottomVertex},
                Vertex{right, topVertex},
            });
        }
    }
    root->appendChildNode(make_lines(axes, zeroLineColor_));
    root->appendChildNode(make_triangles(peaks, waveformColor_));

    const auto overlayRegion = presentation_->candidate_region()
        ? presentation_->candidate_region()
        : presentation_->displayed_region();
    if (overlayRegion
        && overlayRegion->end().value() > visibleRange.begin().value()
        && overlayRegion->begin().value() < visibleRange.end().value()) {
        const auto start = std::max(
            overlayRegion->begin().value(), visibleRange.begin().value());
        const auto end = std::min(
            overlayRegion->end().value(), visibleRange.end().value());
        const auto logicalBoundary = [this, devicePixelRatio, physicalWidth](
            std::int64_t value) {
            return value >= physicalWidth
                ? width()
                : static_cast<double>(value) / devicePixelRatio;
        };
        const auto left = static_cast<float>(logicalBoundary(
            presentation_->physical_pixel_boundary(start, physicalWidth)));
        const auto right = static_cast<float>(logicalBoundary(
            presentation_->physical_pixel_boundary(end, physicalWidth)));
        const auto bottom = static_cast<float>(height());
        root->appendChildNode(make_triangles(
            std::vector<Vertex>{
                {left, 0.0F}, {left, bottom}, {right, bottom},
                {left, 0.0F}, {right, bottom}, {right, 0.0F},
            },
            regionColor_));
        std::vector<Vertex> boundaries;
        std::vector<Vertex> handles;
        const auto append_rectangle = [](std::vector<Vertex>& vertices,
                                          float left,
                                          float top,
                                          float right,
                                          float bottomEdge) {
            vertices.insert(vertices.end(), {
                {left, top}, {left, bottomEdge}, {right, bottomEdge},
                {left, top}, {right, bottomEdge}, {right, top},
            });
        };
        const auto append_boundary = [&](std::int64_t frame) {
            const auto x = static_cast<float>(logicalBoundary(
                presentation_->physical_pixel_boundary(frame, physicalWidth)));
            const auto boundaryLeft = std::clamp(
                x - internal::kWaveformRegionBoundaryWidth * 0.5F, 0.0F,
                std::max(0.0F, static_cast<float>(width())
                    - internal::kWaveformRegionBoundaryWidth));
            append_rectangle(boundaries, boundaryLeft, 0.0F, boundaryLeft + internal::kWaveformRegionBoundaryWidth, bottom);
            const auto handleLeft = std::clamp(
                x - internal::kWaveformRegionHandleWidth * 0.5F, 0.0F,
                std::max(0.0F, static_cast<float>(width())
                    - internal::kWaveformRegionHandleWidth));
            append_rectangle(handles, handleLeft, 0.0F, handleLeft + internal::kWaveformRegionHandleWidth,
                internal::kWaveformRegionHandleHeight);
        };
        if (overlayRegion->begin().value() >= visibleRange.begin().value()
            && overlayRegion->begin().value() <= visibleRange.end().value()) {
            append_boundary(overlayRegion->begin().value());
        }
        if (overlayRegion->end().value() >= visibleRange.begin().value()
            && overlayRegion->end().value() <= visibleRange.end().value()) {
            append_boundary(overlayRegion->end().value());
        }
        if (!boundaries.empty()) {
            root->appendChildNode(make_triangles(boundaries, regionHandleColor_));
            root->appendChildNode(make_triangles(handles, regionHandleColor_));
        }
    }

    if (positionFrames_ >= visibleRange.begin().value()
        && positionFrames_ <= visibleRange.end().value()) {
        const auto xPhysical = presentation_->physical_pixel_boundary(
            positionFrames_, physicalWidth);
        const float x = static_cast<float>(xPhysical >= physicalWidth
            ? width()
            : static_cast<double>(xPhysical) / devicePixelRatio);
        const auto playheadLeft = std::clamp(
            x - internal::kWaveformPlayheadWidth * 0.5F, 0.0F,
            std::max(0.0F, static_cast<float>(width())
                - internal::kWaveformPlayheadWidth));
        const auto playheadRight = playheadLeft + internal::kWaveformPlayheadWidth;
        const auto markerLeft = std::clamp(
            x - internal::kWaveformPlayheadMarkerSize * 0.5F, 0.0F,
            std::max(0.0F, static_cast<float>(width())
                - internal::kWaveformPlayheadMarkerSize));
        root->appendChildNode(make_triangles(
            std::vector<Vertex>{
                {playheadLeft, 0.0F}, {playheadLeft, static_cast<float>(height())},
                {playheadRight, static_cast<float>(height())},
                {playheadLeft, 0.0F}, {playheadRight, static_cast<float>(height())},
                {playheadRight, 0.0F},
                {markerLeft, 0.0F}, {markerLeft + internal::kWaveformPlayheadMarkerSize, 0.0F},
                {x, internal::kWaveformPlayheadMarkerSize},
            },
            playheadColor_));
    }

    if (hasOverrange_) {
        const float right = static_cast<float>(width());
        root->appendChildNode(make_lines(
            std::vector<Vertex>{
                {0.0F, 1.0F}, {right, 1.0F},
                {0.0F, static_cast<float>(height() - 1.0)},
                {right, static_cast<float>(height() - 1.0)},
            },
            overrangeColor_));
    }
    return root;
}

}  // namespace rgsml::ui
