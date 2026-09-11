#pragma once

#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>

#include <QObject>
#include <QString>

#include <optional>

namespace rgsml::app {

class PlaybackTransportViewModel;

class AuditionRegionViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hasRegion READ has_region NOTIFY changed)
    Q_PROPERTY(bool loopEnabled READ loop_enabled NOTIFY changed)
    Q_PROPERTY(bool controlsEnabled READ controls_enabled NOTIFY changed)
    Q_PROPERTY(bool canLoop READ can_loop NOTIFY changed)
    Q_PROPERTY(QString startHours READ start_hours NOTIFY changed)
    Q_PROPERTY(QString startMinutes READ start_minutes NOTIFY changed)
    Q_PROPERTY(QString startSeconds READ start_seconds NOTIFY changed)
    Q_PROPERTY(QString startFraction READ start_fraction NOTIFY changed)
    Q_PROPERTY(QString endHours READ end_hours NOTIFY changed)
    Q_PROPERTY(QString endMinutes READ end_minutes NOTIFY changed)
    Q_PROPERTY(QString endSeconds READ end_seconds NOTIFY changed)
    Q_PROPERTY(QString endFraction READ end_fraction NOTIFY changed)
    Q_PROPERTY(QString durationText READ duration_text NOTIFY changed)
    Q_PROPERTY(QString startFrameText READ start_frame_text NOTIFY changed)
    Q_PROPERTY(QString endFrameText READ end_frame_text NOTIFY changed)
    Q_PROPERTY(QString errorMessage READ error_message NOTIFY changed)

public:
    explicit AuditionRegionViewModel(
        PlaybackTransportViewModel* playback,
        QObject* parent = nullptr);

    [[nodiscard]] bool has_region() const noexcept;
    [[nodiscard]] std::optional<core::FrameRange> region() const noexcept;
    [[nodiscard]] bool loop_enabled() const noexcept;
    [[nodiscard]] bool controls_enabled() const noexcept;
    [[nodiscard]] bool can_loop() const noexcept;
    [[nodiscard]] QString start_hours() const;
    [[nodiscard]] QString start_minutes() const;
    [[nodiscard]] QString start_seconds() const;
    [[nodiscard]] QString start_fraction() const;
    [[nodiscard]] QString end_hours() const;
    [[nodiscard]] QString end_minutes() const;
    [[nodiscard]] QString end_seconds() const;
    [[nodiscard]] QString end_fraction() const;
    [[nodiscard]] QString duration_text() const;
    [[nodiscard]] QString start_frame_text() const;
    [[nodiscard]] QString end_frame_text() const;
    [[nodiscard]] QString error_message() const;

    [[nodiscard]] core::Status set_region(core::FrameRange candidate);
    [[nodiscard]] core::Status set_start(core::FrameIndex start);
    [[nodiscard]] core::Status set_end_exclusive(core::FrameIndex end);
    [[nodiscard]] core::Status clear_region();
    [[nodiscard]] core::Status set_loop_enabled(bool enabled);
    [[nodiscard]] core::Status seek(core::FrameIndex position);

    void source_committed(core::FrameCount frameCount, core::SampleRate sampleRate);
    void set_waveform_ready(bool ready);
    void synchronize_playback();

    Q_INVOKABLE void commitStartSegments(
        const QString& hours,
        const QString& minutes,
        const QString& seconds,
        const QString& fraction);
    Q_INVOKABLE void commitEndSegments(
        const QString& hours,
        const QString& minutes,
        const QString& seconds,
        const QString& fraction);
    Q_INVOKABLE void requestLoopEnabled(bool enabled);
    Q_INVOKABLE void requestClearRegion();
    Q_INVOKABLE void nudgeStartBackward();
    Q_INVOKABLE void nudgeStartForward();
    Q_INVOKABLE void nudgeEndBackward();
    Q_INVOKABLE void nudgeEndForward();
    Q_INVOKABLE void clearError();

signals:
    void changed();

private:
    struct TimeSegments final {
        QString hours;
        QString minutes;
        QString seconds;
        QString fraction;
    };

    [[nodiscard]] core::Status validate(core::FrameRange candidate) const;
    [[nodiscard]] core::Status reposition_if_outside(
        core::FrameRange activeLoop,
        core::FrameIndex positionBeforeLoopUpdate);
    [[nodiscard]] core::Result<core::FrameIndex> parse_segmented_time(
        const QString& hours,
        const QString& minutes,
        const QString& seconds,
        const QString& fraction) const;
    [[nodiscard]] std::optional<TimeSegments> format_segments(
        std::int64_t frame) const;
    [[nodiscard]] QString format_frame(std::int64_t frame) const;
    void publish_region(std::optional<core::FrameRange> region);
    void publish_error(const core::Error& error, bool partial = false);
    void publish_error(QString message);

    PlaybackTransportViewModel* playback_{nullptr};
    std::optional<core::FrameRange> region_;
    std::int64_t sourceFrames_{0};
    std::int64_t sampleRate_{0};
    bool waveformReady_{false};
    bool loopEnabled_{false};
    QString errorMessage_;
};

}  // namespace rgsml::app
