#pragma once

#include "waveform_presentation.hpp"

#include <QColor>
#include <QMetaObject>
#include <QQuickItem>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <optional>

namespace rgsml::ui {

class WaveformItem : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(WaveformPresentation* presentation READ presentation WRITE set_presentation NOTIFY presentationChanged)
    Q_PROPERTY(qint64 positionFrames READ position_frames WRITE set_position_frames NOTIFY timelineChanged)
    Q_PROPERTY(qint64 durationFrames READ duration_frames WRITE set_duration_frames NOTIFY timelineChanged)
    Q_PROPERTY(QColor waveformColor READ waveform_color WRITE set_waveform_color NOTIFY colorsChanged)
    Q_PROPERTY(QColor zeroLineColor READ zero_line_color WRITE set_zero_line_color NOTIFY colorsChanged)
    Q_PROPERTY(QColor playheadColor READ playhead_color WRITE set_playhead_color NOTIFY colorsChanged)
    Q_PROPERTY(QColor overrangeColor READ overrange_color WRITE set_overrange_color NOTIFY colorsChanged)
    Q_PROPERTY(QColor regionColor READ region_color WRITE set_region_color NOTIFY colorsChanged)
    Q_PROPERTY(QColor regionHandleColor READ region_handle_color WRITE set_region_handle_color NOTIFY colorsChanged)
    Q_PROPERTY(qulonglong discardedWheelSteps READ discarded_wheel_steps NOTIFY diagnosticsChanged)

public:
    explicit WaveformItem(QQuickItem* parent = nullptr);
    ~WaveformItem() override = default;

    [[nodiscard]] WaveformPresentation* presentation() const noexcept;
    void set_presentation(WaveformPresentation* presentation);
    [[nodiscard]] qint64 position_frames() const noexcept;
    void set_position_frames(qint64 frames);
    [[nodiscard]] qint64 duration_frames() const noexcept;
    void set_duration_frames(qint64 frames);
    [[nodiscard]] QColor waveform_color() const;
    void set_waveform_color(const QColor& color);
    [[nodiscard]] QColor zero_line_color() const;
    void set_zero_line_color(const QColor& color);
    [[nodiscard]] QColor playhead_color() const;
    void set_playhead_color(const QColor& color);
    [[nodiscard]] QColor overrange_color() const;
    void set_overrange_color(const QColor& color);
    [[nodiscard]] QColor region_color() const;
    void set_region_color(const QColor& color);
    [[nodiscard]] QColor region_handle_color() const;
    void set_region_handle_color(const QColor& color);
    [[nodiscard]] qulonglong discarded_wheel_steps() const noexcept;

signals:
    void presentationChanged();
    void timelineChanged();
    void colorsChanged();
    void diagnosticsChanged();

protected:
    QSGNode* updatePaintNode(
        QSGNode* oldNode,
        UpdatePaintNodeData* updatePaintNodeData) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseUngrabEvent() override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    void commit_pending_seek();

private:
    enum class Gesture {
        None,
        Background,
        RegionBody,
        CreateRegion,
        StartHandle,
        EndHandle,
    };

    void synchronize_presentation();
    [[nodiscard]] std::int64_t physical_width() const noexcept;
    [[nodiscard]] std::int64_t physical_boundary(double logicalX) const noexcept;
    [[nodiscard]] Gesture hit_test(std::int64_t physicalBoundary, bool shift) const noexcept;
    void update_candidate(std::int64_t physicalBoundary);
    void finish_gesture(std::int64_t physicalBoundary);
    void cancel_gesture();
    void schedule_pending_seek(core::FrameIndex position);
    void cancel_pending_seek();

    WaveformPresentation* presentation_{nullptr};
    QMetaObject::Connection presentationConnection_;
    std::shared_ptr<const audio::WaveformSummary> summary_;
    qint64 positionFrames_{0};
    qint64 durationFrames_{0};
    QColor waveformColor_{QStringLiteral("#73d6ff")};
    QColor zeroLineColor_{QStringLiteral("#505862")};
    QColor playheadColor_{QStringLiteral("#f5c96a")};
    QColor overrangeColor_{QStringLiteral("#ff7d8a")};
    QColor regionColor_{113, 214, 255, 72};
    QColor regionHandleColor_{QStringLiteral("#f1f3f5")};
    bool hasOverrange_{false};
    Gesture gesture_{Gesture::None};
    std::int64_t pressPhysicalBoundary_{0};
    std::int64_t currentPhysicalBoundary_{0};
    std::int64_t gesturePhysicalWidth_{1};
    std::optional<core::FrameRange> dragStartViewport_;
    std::optional<core::FrameRange> dragStartRegion_;
    bool dragThresholdReached_{false};
    QTimer singleClickTimer_;
    std::optional<core::FrameIndex> pendingSeek_;
    int zoomWheelAngleRemainder_{0};
    int panWheelAngleRemainder_{0};
    qulonglong discardedWheelSteps_{0U};
};

}  // namespace rgsml::ui
