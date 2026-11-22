#pragma once
// Research-only Qt carrier for already validated C01 Activity projection.
// It does not create a new DSP detector, clock, SRC map or RealizationId.
#include "dynamic_eq_activity_playback_projection.hpp"
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <cmath>
#include <cstdint>
#include <utility>

namespace rgsml::research::dynamic_ui {
class C01ActivityQtBridge final:public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QVariantList buckets READ buckets NOTIFY changed)
    Q_PROPERTY(int bandCount READ bandCount NOTIFY changed)
    Q_PROPERTY(double audibleFrame READ audibleFrame NOTIFY changed)
    Q_PROPERTY(int sampleRateHz READ sampleRateHz NOTIFY changed)
    Q_PROPERTY(QString realizationId READ realizationId NOTIFY changed)
    Q_PROPERTY(QString stateText READ stateText NOTIFY changed)
public:
    explicit C01ActivityQtBridge(QObject* parent=nullptr):QObject(parent){}
    [[nodiscard]] bool available() const noexcept {return valid_;}
    [[nodiscard]] QVariantList buckets() const {return buckets_;}
    [[nodiscard]] int bandCount() const noexcept {return band_count_;}
    [[nodiscard]] double audibleFrame() const noexcept {return audible_frame_;}
    [[nodiscard]] int sampleRateHz() const noexcept {return fs_;}
    [[nodiscard]] QString realizationId() const {return identity_;}
    [[nodiscard]] QString stateText() const {return state_text_;}

    void apply(const C01ActivityProjection& p) {
        constexpr auto kJsSafe=std::uint64_t{9007199254740991ULL};
        if(!p.available()||!p.realization_id
           ||p.realization_id->value==0 ||p.audible_frame>kJsSafe
           ||p.sample_rate_hz==0 ||p.sample_rate_hz>192000U
           ||p.history.empty()||p.history.size()>4096) {
            clear();return;
        }
        const auto n=p.history.front().band_count;
        if(n==0 || n>6){clear();return;}
        QVariantList mapped;
        mapped.reserve(static_cast<qsizetype>(p.history.size()));
        std::uint64_t previous_end=0;
        bool first=true;
        const auto first_bucket=p.history.front();
        for(const auto& frame:p.history) {
            if(frame.frame_begin>kJsSafe
               ||frame.frame_end_exclusive>kJsSafe
               ||frame.frame_end_exclusive<=frame.frame_begin
               ||frame.frame_end_exclusive>p.audible_frame
               ||(!first && frame.frame_begin!=previous_end)
               ||frame.band_count!=n) {
                clear();return;
            }
            QVariantList bands;
            bands.reserve(static_cast<qsizetype>(n));
            for(std::size_t i=0;i<n;++i) {
                const auto& b=frame.bands[i];
                const auto& reference=first_bucket.bands[i];
                if(b.band_id!=reference.band_id
                   ||b.routing!=reference.routing
                   ||!std::isfinite(b.gain_min_db)
                   ||!std::isfinite(b.gain_max_db)
                   ||!std::isfinite(b.gain_last_db)
                   ||!std::isfinite(b.effective_gain_last_db)
                   ||!std::isfinite(b.signed_target_last_db)
                   ||!std::isfinite(b.detector_power_max)
                   ||b.detector_power_max<0.0
                   ||std::abs(b.effective_gain_last_db)>18.000001
                   ||std::abs(b.signed_target_last_db)>18.000001
                   ||b.gain_min_db>b.gain_last_db
                   ||b.gain_last_db>b.gain_max_db
                   ||std::abs(b.gain_min_db)>18.000001
                   ||std::abs(b.gain_max_db)>18.000001) {
                    clear();return;
                }
                bands.append(QVariantMap{
                    {QStringLiteral("bandId"),QString::fromStdString(b.band_id.to_string())},
                    {QStringLiteral("gainMinDb"),b.gain_min_db},
                    {QStringLiteral("gainMaxDb"),b.gain_max_db},
                    {QStringLiteral("gainLastDb"),b.gain_last_db},
                    {QStringLiteral("effectiveGainLastDb"),b.effective_gain_last_db},
                    {QStringLiteral("signedTargetLastDb"),b.signed_target_last_db}});
            }
            mapped.append(QVariantMap{
                {QStringLiteral("frameBegin"),double(frame.frame_begin)},
                {QStringLiteral("frameEndExclusive"),double(frame.frame_end_exclusive)},
                {QStringLiteral("bands"),bands}});
            previous_end=frame.frame_end_exclusive;first=false;
        }
        buckets_=std::move(mapped);
        valid_=true;
        band_count_=static_cast<int>(n);
        audible_frame_=double(p.audible_frame);
        fs_=static_cast<int>(p.sample_rate_hz);
        identity_=QString::number(p.realization_id->value);
        state_text_=p.status==C01ProjectionStatus::PAUSED
            ?QStringLiteral("PAUSED"):QStringLiteral("PLAYBACK SYNCED");
        emit changed();
    }
    void clear() {
        buckets_.clear();valid_=false;band_count_=0;
        audible_frame_=-1.0;fs_=0;identity_.clear();
        state_text_=QStringLiteral("TELEMETRY UNAVAILABLE");
        emit changed();
    }
signals:
    void changed();
private:
    bool valid_{false};
    QVariantList buckets_;
    int band_count_{0};
    double audible_frame_{-1.0};
    int fs_{0};
    QString identity_;
    QString state_text_{QStringLiteral("TELEMETRY UNAVAILABLE")};
};
} // namespace rgsml::research::dynamic_ui
