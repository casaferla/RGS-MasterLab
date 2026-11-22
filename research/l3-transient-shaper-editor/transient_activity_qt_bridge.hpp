#pragma once
// Qt translation of already-checked authentic PlaybackActivityProjection.
// Does not own playback time, DSP calculations, or a synthetic clock.
#include "transient_activity_playback_projection.hpp"
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <cstdint>
#include <cmath>

namespace rgsml::research::transient_ui {
class TransientActivityQtBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList activityHistory READ activityHistory NOTIFY changed)
    Q_PROPERTY(bool activityTelemetryAvailable READ activityTelemetryAvailable NOTIFY changed)
    Q_PROPERTY(QString telemetryRealizationId READ telemetryRealizationId NOTIFY changed)
    Q_PROPERTY(QString audibleRealizationId READ audibleRealizationId NOTIFY changed)
    Q_PROPERTY(double audibleFrame READ audibleFrame NOTIFY changed)
    Q_PROPERTY(int activitySampleRateHz READ activitySampleRateHz NOTIFY changed)
    Q_PROPERTY(bool auditioningProcessed READ auditioningProcessed NOTIFY changed)
    Q_PROPERTY(bool shaperBypassed READ shaperBypassed NOTIFY changed)
    Q_PROPERTY(QString telemetryStatus READ telemetryStatus NOTIFY changed)
public:
    explicit TransientActivityQtBridge(QObject* parent=nullptr):QObject(parent){}
    [[nodiscard]] QVariantList activityHistory() const {return history_;}
    [[nodiscard]] bool activityTelemetryAvailable() const noexcept {return valid_;}
    [[nodiscard]] QString telemetryRealizationId() const {return identity_;}
    [[nodiscard]] QString audibleRealizationId() const {return identity_;}
    [[nodiscard]] double audibleFrame() const noexcept {return audible_frame_;}
    [[nodiscard]] int activitySampleRateHz() const noexcept {return sample_rate_;}
    [[nodiscard]] bool auditioningProcessed() const noexcept {return valid_;}
    [[nodiscard]] bool shaperBypassed() const noexcept {return false;}
    [[nodiscard]] QString telemetryStatus() const {return status_;}
    void apply(const PlaybackActivityProjection& p) {
        constexpr std::uint64_t safe=9007199254740991ULL;
        if(!p.available() || !p.realization_id || p.realization_id->value==0
           || p.sample_rate_hz==0 || p.sample_rate_hz>192000
           || p.audible_frame>safe || p.history.empty()
           || p.history.size()>4096U) {
            clear(p.status);
            return;
        }
        QVariantList candidate;
        candidate.reserve(static_cast<qsizetype>(p.history.size()));
        bool first=true;
        std::uint64_t last_end=0;
        // Reject malformed sidecars before notifying QML of availability.
        // Do not rely on the scene graph to validate or sanitize audio data.
        const validTriple=[](double minimum,double last,double maximum) {
            constexpr double bound=12.000001;
            return std::isfinite(minimum) && std::isfinite(last)
                && std::isfinite(maximum)
                && minimum>=-bound && maximum<=bound
                && minimum<=last && last<=maximum;
        };
        for(const auto& b:p.history) {
            if(!validTriple(b.attack_min_db,b.attack_last_db,b.attack_max_db)
               ||!validTriple(b.sustain_min_db,b.sustain_last_db,b.sustain_max_db)) {
                clear(PlaybackActivityStatus::INCOMPLETE_TELEMETRY);
                return;
            }
            if(b.frame_begin>safe || b.frame_end_exclusive>safe
               || b.frame_end_exclusive<=b.frame_begin
               || (!first && last_end!=b.frame_begin)
               || b.frame_end_exclusive>p.audible_frame) {
                clear(PlaybackActivityStatus::INCOMPLETE_TELEMETRY);
                return;
            }
            // Force exact integers within JS safe range into JS Number.
            candidate.append(QVariantMap{
                {QStringLiteral("frameBegin"),double(b.frame_begin)},
                {QStringLiteral("frameEndExclusive"),double(b.frame_end_exclusive)},
                {QStringLiteral("attackMinDb"),b.attack_min_db},
                {QStringLiteral("attackMaxDb"),b.attack_max_db},
                {QStringLiteral("attackLastDb"),b.attack_last_db},
                {QStringLiteral("sustainMinDb"),b.sustain_min_db},
                {QStringLiteral("sustainMaxDb"),b.sustain_max_db},
                {QStringLiteral("sustainLastDb"),b.sustain_last_db}});
            last_end=b.frame_end_exclusive;
            first=false;
        }
        history_=std::move(candidate);
        valid_=true;
        audible_frame_=double(p.audible_frame);
        sample_rate_=int(p.sample_rate_hz);
        identity_=QString::number(p.realization_id->value);
        status_=p.status==PlaybackActivityStatus::PAUSED
            ?QStringLiteral("PAUSED"):QStringLiteral("ACTIVE");
        emit changed();
    }
    void clear(PlaybackActivityStatus s=PlaybackActivityStatus::UNAVAILABLE) {
        history_.clear(); valid_=false; audible_frame_=-1;
        sample_rate_=0; identity_.clear();
        status_=QString::number(int(s));
        emit changed();
    }
signals:
    void changed();
private:
    QVariantList history_;
    bool valid_{false};
    double audible_frame_{-1.0};
    int sample_rate_{0};
    QString identity_;
    QString status_{QStringLiteral("UNAVAILABLE")};
};
}
