#pragma once

#include <rgsml/core/audio_playback_service.hpp>
#include <rgsml/core/resource_reference.hpp>

#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>

namespace rgsml::app {

class PlaybackTransportViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool playbackAvailable READ playback_available NOTIFY playbackChanged)
    Q_PROPERTY(QString stateLabel READ state_label NOTIFY playbackChanged)
    Q_PROPERTY(bool isPlaying READ is_playing NOTIFY playbackChanged)
    Q_PROPERTY(bool isPaused READ is_paused NOTIFY playbackChanged)
    Q_PROPERTY(bool canPlay READ can_play NOTIFY playbackChanged)
    Q_PROPERTY(bool canPause READ can_pause NOTIFY playbackChanged)
    Q_PROPERTY(bool canStop READ can_stop NOTIFY playbackChanged)
    Q_PROPERTY(qint64 positionFrames READ position_frames NOTIFY playbackChanged)
    Q_PROPERTY(qint64 durationFrames READ duration_frames NOTIFY playbackChanged)
    Q_PROPERTY(QString positionLabel READ position_label NOTIFY playbackChanged)
    Q_PROPERTY(QString durationLabel READ duration_label NOTIFY playbackChanged)
    Q_PROPERTY(QString errorMessage READ error_message NOTIFY playbackChanged)

public:
    explicit PlaybackTransportViewModel(
        std::unique_ptr<core::IAudioPlaybackService> service,
        QObject* parent = nullptr);

    [[nodiscard]] bool playback_available() const noexcept;
    [[nodiscard]] QString state_label() const;
    [[nodiscard]] bool is_playing() const noexcept;
    [[nodiscard]] bool is_paused() const noexcept;
    [[nodiscard]] bool can_play() const noexcept;
    [[nodiscard]] bool can_pause() const noexcept;
    [[nodiscard]] bool can_stop() const noexcept;
    [[nodiscard]] qint64 position_frames() const noexcept;
    [[nodiscard]] qint64 duration_frames() const noexcept;
    [[nodiscard]] QString position_label() const;
    [[nodiscard]] QString duration_label() const;
    [[nodiscard]] QString error_message() const;

    void prepare_source(
        const core::ResourceReference& source,
        qint64 sampleRateHz);

    Q_INVOKABLE void playOrResume();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();

signals:
    void playbackChanged();

private:
    void refresh();
    void publish_failure(const core::Error& error);
    [[nodiscard]] QString format_frames(qint64 frames) const;

    std::unique_ptr<core::IAudioPlaybackService> service_;
    QTimer refreshTimer_;
    core::PlaybackState state_{core::PlaybackState::NO_SOURCE};
    qint64 positionFrames_{0};
    qint64 durationFrames_{0};
    qint64 sampleRateHz_{0};
    bool playbackAvailable_{false};
    QString errorMessage_;
};

}  // namespace rgsml::app
