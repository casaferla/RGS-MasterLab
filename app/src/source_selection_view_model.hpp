#pragma once

#include <rgsml/audio/source_resource.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>

#include <QObject>
#include <QThread>
#include <QString>
#include <QUrl>

#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace rgsml::app {

class PlaybackTransportViewModel;
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
    using SourceCommittedHandler = std::function<void(
        const core::ResourceReference&)>;
    // An M18 physical SOURCE batch MUST use a status-bearing downstream
    // Product dispatcher. A legacy void observer cannot report BUSY or
    // an invalid M17/Audition handoff; legacy M17 remains unchanged.
    using M18CheckedSourceCommittedHandler = std::function<
        core::Status(const core::ResourceReference&)>;

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
    [[nodiscard]] const audio::SourceResource* source_resource() const noexcept;
    // Source replacement is a commit-like mutation, even when invoked by
    // Project open rather than by the native SOURCE file picker.
    using M18SourceMutationAdmission=std::function<core::Status(const char*)>;
    void set_private_m18_source_mutation_admission(
        M18SourceMutationAdmission gate)
    {
        // A synchronous Product callback or Qt publication listener
        // cannot silently unregister its own shared mutation authority.
        if(QThread::currentThread()!=thread())return;
        if(m18SourceAdmitting_||m18SourcePublishing_)return;
        m18SourceMutationAdmission_=std::move(gate);
    }
    // Real Product SOURCE commits need a physical writer lease spanning
    // the synchronous committed-handler + transport + Qt signals. Mutually
    // exclusive with the older one-shot status admission when installed.
    using M18SourceBatchBegin=std::function<
        core::Result<std::shared_ptr<const void>>()>;
    void set_private_m18_source_batch_begin(M18SourceBatchBegin begin) {
        if(QThread::currentThread()!=thread())return;
        if(m18SourceAdmitting_||m18SourcePublishing_)return;
        m18SourceBatchBegin_=std::move(begin);
    }
    [[nodiscard]] core::Status adopt_probed_source(audio::SourceResource candidate);

    void set_playback_transport(PlaybackTransportViewModel* playback) noexcept;
    void set_source_committed_handler(SourceCommittedHandler handler);
    void set_private_m18_checked_source_committed_handler(
        M18CheckedSourceCommittedHandler handler);
    Q_INVOKABLE void selectSource(const QUrl& selectedFile);
    Q_INVOKABLE void cancelSourceSelection() noexcept;

signals:
    void sourceChanged();
    void sourceStateChanged();
    void errorChanged();

private:
    [[nodiscard]] core::Status admit_m18_source_mutation(
        const char* operation) const;
    void publish_error(const QString& message);
    void publish_error(const core::Error& error);
    void fail_closed_checked_source_commit(const core::Error& error);

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
    PlaybackTransportViewModel* playbackTransport_{nullptr};
    M18SourceMutationAdmission m18SourceMutationAdmission_;
    M18SourceBatchBegin m18SourceBatchBegin_;
    // Scoped across the Product admission and the SOURCE committed
    // handler, audio transport preparation and Qt NOTIFY publication.
    // Inert until a shared M18 owner gate is explicitly installed.
    mutable bool m18SourceAdmitting_{false};
    bool m18SourcePublishing_{false};
    SourceCommittedHandler sourceCommittedHandler_;
    M18CheckedSourceCommittedHandler m18CheckedSourceCommittedHandler_;
};

}  // namespace rgsml::app
