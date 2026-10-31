#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtGlobal>
#include <array>
#include <functional>
#include <utility>
namespace rgsml::app {
// Phase A Project/Render preference only: NOT the DEVICE rate, and NOT YET
// the published/rendered terminal PCM rate. Never certify SRC from selection.
class MasterOutputRateState final : public QObject {
    Q_OBJECT
    Q_PROPERTY(qint64 sourceRateHz READ source_rate_hz NOTIFY changed)
    Q_PROPERTY(qint64 masterRateHz READ master_rate_hz NOTIFY changed)
    Q_PROPERTY(bool hasSupportedMasterRate READ has_supported_master_rate NOTIFY changed)
    Q_PROPERTY(bool canEditTarget READ can_edit_target NOTIFY selectionAvailabilityChanged)
    Q_PROPERTY(quint64 rateRevision READ rate_revision NOTIFY changed)
    // All six Final SRC admission results and the selected MASTER gate are
    // observable. QML must not call opaque methods without a NOTIFY-bound
    // property dependency, or SOURCE changes leave stale blocked rows.
    Q_PROPERTY(QVariantList candidateRateAdmissions READ candidate_rate_admissions NOTIFY selectionAvailabilityChanged)
    Q_PROPERTY(bool currentMasterRateAdmissible READ current_master_rate_admissible NOTIFY selectionAvailabilityChanged)
public:
    explicit MasterOutputRateState(QObject* parent=nullptr):QObject(parent){}
    [[nodiscard]] static constexpr bool supported(qint64 hz) noexcept {
        constexpr std::array<qint64,6> rates{44100,48000,88200,96000,176400,192000};
        for(const auto candidate:rates)if(candidate==hz)return true;
        return false;
    }
    // GUI and rate-picker use the very same original-Q0 product admission
    // as M17 worker and six-rate memory panel; not a DEVICE-derived choice.
    void set_original_q0_family_ready(bool ready) noexcept {
        if(originalQ0Ready_==ready)return;
        originalQ0Ready_=ready;
        emit selectionAvailabilityChanged();
    }
    [[nodiscard]] qint64 source_rate_hz()const noexcept{return sourceRateHz_;}
    [[nodiscard]] qint64 master_rate_hz()const noexcept{return masterRateHz_;}
    [[nodiscard]] QVariantList candidate_rate_admissions() const {
        constexpr std::array<qint64,6> rates{
            44100,48000,88200,96000,176400,192000};
        QVariantList admission;
        admission.reserve(static_cast<qsizetype>(rates.size()));
        for(const auto hz:rates)
            admission.append(m17_candidate_rate_admissible(hz));
        return admission;
    }
    [[nodiscard]] bool current_master_rate_admissible() const noexcept {
        return m17_candidate_rate_admissible(masterRateHz_);
    }
    [[nodiscard]] bool has_supported_master_rate()const noexcept {
        return supported(sourceRateHz_)&&supported(masterRateHz_);
    }
    [[nodiscard]] quint64 rate_revision()const noexcept{return rateRevision_;}
    // Product-owned guard: a PRIVATE DEVICE monitor keeps its exact
    // immutable MASTER rate until Return. SOURCE epoch changes remain
    // separate authoritative events, never subject to this UI guard.
    using TargetSelectionGuard=std::function<bool()>;
    void set_target_selection_guard(TargetSelectionGuard guard) {
        targetSelectionGuard_=std::move(guard);
        emit selectionAvailabilityChanged();
    }
    [[nodiscard]] bool can_edit_target() const noexcept {
        return has_supported_master_rate()
            &&(!targetSelectionGuard_||targetSelectionGuard_());
    }
    // Availability may change when the private monitoring lease changes,
    // without changing the Project MASTER rate/revision.
    void refresh_selection_availability() {emit selectionAvailabilityChanged();}
    void source_committed(qint64 sourceRateHz) noexcept;
    void source_cleared() noexcept;
    Q_INVOKABLE bool choose_master_rate(qint64 targetHz) noexcept;
    Q_INVOKABLE bool m17_candidate_rate_admissible(qint64 targetHz) const noexcept;
    Q_INVOKABLE QString final_src_path_status(qint64 targetHz) const;
signals:
    // Only changed() represents a genuine rate/epoch revision. Separate
    // monitor admission availability MUST NEVER invalidate M17 MASTER PCM.
    void changed();
    void selectionAvailabilityChanged();
private:
    qint64 sourceRateHz_{0};
    qint64 masterRateHz_{0};
    quint64 rateRevision_{0};
    bool originalQ0Ready_{false};
    TargetSelectionGuard targetSelectionGuard_;
};
} // namespace rgsml::app
