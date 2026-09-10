#pragma once

#include <rgsml/audio/waveform_summary.hpp>

#include <QObject>
#include <QString>

#include <memory>

namespace rgsml::ui {

class WaveformPresentation final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state_token NOTIFY changed)
    Q_PROPERTY(QString statusText READ status_text NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool hasOverrange READ has_overrange NOTIFY changed)
    Q_PROPERTY(int channelCount READ channel_count NOTIFY changed)
    Q_PROPERTY(qint64 sourceFrameCount READ source_frame_count NOTIFY changed)
    Q_PROPERTY(qint64 baseFramesPerBucket READ base_frames_per_bucket NOTIFY changed)
    Q_PROPERTY(qint64 baseBucketCount READ base_bucket_count NOTIFY changed)
    Q_PROPERTY(qint64 levelCount READ level_count NOTIFY changed)
    Q_PROPERTY(qint64 payloadBytes READ payload_bytes NOTIFY changed)

public:
    enum class State {
        Empty,
        Building,
        Ready,
        Failed,
    };
    Q_ENUM(State)

    explicit WaveformPresentation(QObject* parent = nullptr);

    [[nodiscard]] QString state_token() const;
    [[nodiscard]] QString status_text() const;
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] bool has_overrange() const noexcept;
    [[nodiscard]] int channel_count() const noexcept;
    [[nodiscard]] qint64 source_frame_count() const noexcept;
    [[nodiscard]] qint64 base_frames_per_bucket() const noexcept;
    [[nodiscard]] qint64 base_bucket_count() const noexcept;
    [[nodiscard]] qint64 level_count() const noexcept;
    [[nodiscard]] qint64 payload_bytes() const noexcept;
    [[nodiscard]] std::shared_ptr<const audio::WaveformSummary> summary() const noexcept;

    void publish_empty();
    void publish_building();
    void publish_ready(std::shared_ptr<const audio::WaveformSummary> summary);
    void publish_failed(QString message);

    Q_INVOKABLE void requestRetry();

signals:
    void changed();
    void retryRequested();

private:
    State state_{State::Empty};
    QString failureMessage_;
    std::shared_ptr<const audio::WaveformSummary> summary_;
    bool hasOverrange_{false};
};

}  // namespace rgsml::ui
