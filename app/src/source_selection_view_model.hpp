#pragma once

#include <rgsml/audio/source_resource.hpp>
#include <rgsml/core/error.hpp>

#include <QObject>
#include <QString>
#include <QUrl>

#include <optional>

namespace rgsml::app {

class SourceSelectionViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hasSource READ has_source NOTIFY sourceChanged)
    Q_PROPERTY(QString sourceState READ source_state NOTIFY sourceStateChanged)
    Q_PROPERTY(QString displayName READ display_name NOTIFY sourceChanged)
    Q_PROPERTY(QString containerLabel READ container_label NOTIFY sourceChanged)
    Q_PROPERTY(QString sampleFormatLabel READ sample_format_label NOTIFY sourceChanged)
    Q_PROPERTY(qint64 sampleRateHz READ sample_rate_hz NOTIFY sourceChanged)
    Q_PROPERTY(QString channelLayoutLabel READ channel_layout_label NOTIFY sourceChanged)
    Q_PROPERTY(int channelCount READ channel_count NOTIFY sourceChanged)
    Q_PROPERTY(qint64 frameCount READ frame_count NOTIFY sourceChanged)
    Q_PROPERTY(QString durationLabel READ duration_label NOTIFY sourceChanged)
    Q_PROPERTY(bool readOnly READ read_only NOTIFY sourceChanged)
    Q_PROPERTY(QString errorMessage READ error_message NOTIFY errorChanged)

public:
    explicit SourceSelectionViewModel(QObject* parent = nullptr);

    [[nodiscard]] bool has_source() const noexcept;
    [[nodiscard]] QString source_state() const;
    [[nodiscard]] QString display_name() const;
    [[nodiscard]] QString container_label() const;
    [[nodiscard]] QString sample_format_label() const;
    [[nodiscard]] qint64 sample_rate_hz() const noexcept;
    [[nodiscard]] QString channel_layout_label() const;
    [[nodiscard]] int channel_count() const noexcept;
    [[nodiscard]] qint64 frame_count() const noexcept;
    [[nodiscard]] QString duration_label() const;
    [[nodiscard]] bool read_only() const noexcept;
    [[nodiscard]] QString error_message() const;

    Q_INVOKABLE void selectSource(const QUrl& selectedFile);
    Q_INVOKABLE void cancelSourceSelection() noexcept;

signals:
    void sourceChanged();
    void sourceStateChanged();
    void errorChanged();

private:
    void publish_error(const QString& message);
    void publish_error(const core::Error& error);

    std::optional<audio::SourceResource> source_;
    QString displayName_;
    QString containerLabel_;
    QString sampleFormatLabel_;
    QString channelLayoutLabel_;
    QString durationLabel_;
    QString errorMessage_;
    qint64 sampleRateHz_{0};
    qint64 frameCount_{0};
    int channelCount_{0};
};

}  // namespace rgsml::app
