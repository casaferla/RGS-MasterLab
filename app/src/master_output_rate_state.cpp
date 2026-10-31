#include "master_output_rate_state.hpp"
#include "final_src_candidate_policy.hpp"
namespace rgsml::app {
bool MasterOutputRateState::m17_candidate_rate_admissible(qint64 hz) const noexcept {
    return m17_final_src_candidate_admissible(sourceRateHz_,hz,originalQ0Ready_);
}
QString MasterOutputRateState::final_src_path_status(qint64 hz) const {
    switch(final_src_candidate_path(sourceRateHz_,hz)) {
    case FinalSrcCandidatePath::IDENTITY:
        return QStringLiteral("IDENTITY · no Final SRC");
    case FinalSrcCandidatePath::EXISTING_44K_48K_FIR:
        return QStringLiteral("44.1/48 FIR candidate · Final SRC release pending");
    case FinalSrcCandidatePath::EXPERIMENTAL_UNQUALIFIED:
        return originalQ0Ready_
            ? QStringLiteral("Original SHA-pinned Q0 FIR · M17 FQ pending")
            : QStringLiteral("Final SRC UNQUALIFIED · Prepare unavailable");
    default:
        return QStringLiteral("SOURCE / target rate unavailable");
    }
}

void MasterOutputRateState::source_committed(qint64 hz) noexcept {
    sourceRateHz_=hz>0?hz:0;
    masterRateHz_=supported(sourceRateHz_)?sourceRateHz_:0;
    ++rateRevision_; // different SOURCE identity invalidates even at same Fs
    emit changed();
    emit selectionAvailabilityChanged();
}
void MasterOutputRateState::source_cleared() noexcept {
    if(sourceRateHz_==0&&masterRateHz_==0)return;
    sourceRateHz_=0;masterRateHz_=0;++rateRevision_;
    emit changed();
    emit selectionAvailabilityChanged();
}
bool MasterOutputRateState::choose_master_rate(qint64 hz) noexcept {
    if(!supported(sourceRateHz_)||!supported(hz))return false;
    if(hz==masterRateHz_)return true;
    // A requested *change* during an exact DEVICE monitor is refused
    // without invalidating either the Project or the M17 render epoch.
    if(!can_edit_target())return false;
    masterRateHz_=hz;++rateRevision_;
    emit changed();
    // The current MASTER admission and observable six-rate list share the
    // same binding contract; update even when the SOURCE itself is stable.
    emit selectionAvailabilityChanged();
    return true;
}
} // namespace rgsml::app
