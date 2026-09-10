#include "waveform_item.hpp"

#include "internal/waveform_geometry.hpp"

#include <QQuickWindow>
#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>

#include <algorithm>
#include <cmath>
#include <cstddef>
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
    setAcceptedMouseButtons(Qt::NoButton);
    setAcceptHoverEvents(false);
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

void WaveformItem::synchronize_presentation()
{
    summary_ = presentation_ ? presentation_->summary() : nullptr;
    hasOverrange_ = presentation_ && presentation_->has_overrange();
    update();
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

    const double devicePixelRatio = window() ? window()->devicePixelRatio() : 1.0;
    const auto targetRanges = internal::waveform_target_range_count(
        width(), devicePixelRatio);
    const auto levelIndex = internal::select_waveform_level(*summary_, targetRanges);
    const auto levelResult = summary_->level(levelIndex);
    if (!levelResult) {
        return root;
    }
    const auto level = *levelResult.value();
    const auto bucketCount = static_cast<std::size_t>(level.bucket_count().value());
    const auto channelCount = summary_->channel_count();
    if (bucketCount == 0U || bucketCount > targetRanges
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
        for (std::size_t bucket = 0; bucket < bucketCount; ++bucket) {
            const auto peakResult = level.peak(
                channel,
                core::FrameIndex{static_cast<std::int64_t>(bucket)});
            if (!peakResult) {
                continue;
            }
            const auto peak = *peakResult.value();
            const auto span = internal::waveform_bucket_span(
                width(), devicePixelRatio, bucketCount, bucket);
            const double top = center - clipped_sample(peak.maximum) * amplitude;
            const double bottom = center - clipped_sample(peak.minimum) * amplitude;
            const auto left = static_cast<float>(span.left);
            const auto right = static_cast<float>(span.right);
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

    if (durationFrames_ > 0) {
        const double fraction = std::clamp(
            static_cast<double>(positionFrames_) / static_cast<double>(durationFrames_),
            0.0,
            1.0);
        const float x = static_cast<float>(fraction * width());
        root->appendChildNode(make_lines(
            std::vector<Vertex>{{x, 0.0F}, {x, static_cast<float>(height())}},
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
