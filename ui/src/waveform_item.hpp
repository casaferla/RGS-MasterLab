#pragma once

#include "waveform_presentation.hpp"

#include <QColor>
#include <QMetaObject>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

#include <memory>

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

signals:
    void presentationChanged();
    void timelineChanged();
    void colorsChanged();

protected:
    QSGNode* updatePaintNode(
        QSGNode* oldNode,
        UpdatePaintNodeData* updatePaintNodeData) override;

private:
    void synchronize_presentation();

    WaveformPresentation* presentation_{nullptr};
    QMetaObject::Connection presentationConnection_;
    std::shared_ptr<const audio::WaveformSummary> summary_;
    qint64 positionFrames_{0};
    qint64 durationFrames_{0};
    QColor waveformColor_{QStringLiteral("#73d6ff")};
    QColor zeroLineColor_{QStringLiteral("#505862")};
    QColor playheadColor_{QStringLiteral("#f5c96a")};
    QColor overrangeColor_{QStringLiteral("#ff7d8a")};
    bool hasOverrange_{false};
};

}  // namespace rgsml::ui
