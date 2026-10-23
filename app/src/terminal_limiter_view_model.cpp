#include "terminal_limiter_view_model.hpp"

#include <rgsml/dsp/true_peak_limiter_parameters.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace rgsml::app {
namespace {
const QString kField=QStringLiteral("ceilingDbTp");
const QString kInvalidRange=QStringLiteral(
    "CEILING must be within -12.0 to 0.0 dBTP");
}

TerminalLimiterViewModel::TerminalLimiterViewModel(
    TerminalLimiterState* state, AcceptedStateChange accepted, QObject* parent)
    : QObject(parent),state_(state),stateChanged_(std::move(accepted))
{
    if(state_)draftCeilingDbTp_=state_->parameters().ceiling_db_tp();
}

double TerminalLimiterViewModel::ceiling_db_tp() const noexcept
{
    return state_?state_->parameters().ceiling_db_tp():-1.0;
}
QString TerminalLimiterViewModel::ceiling_db_tp_text() const
{
    return QString::number(draft_ceiling_db_tp(),'f',2);
}
double TerminalLimiterViewModel::draft_ceiling_db_tp() const noexcept
{
    return draftCeilingDbTp_.value_or(ceiling_db_tp());
}
bool TerminalLimiterViewModel::bypass() const noexcept
{
    return !state_||state_->bypassed();
}
void TerminalLimiterViewModel::set_validation(const QString& message)
{
    validationField_=kField;
    validationMessage_=message;
    emit changed();
}
void TerminalLimiterViewModel::clear_validation()
{
    validationField_.clear();
    validationMessage_.clear();
}

bool TerminalLimiterViewModel::setDraftFieldValue(
    const QString& field,double value)
{
    if(field!=kField||!state_)return false;
    auto valid=dsp::TruePeakLimiterParameters::create(value);
    if(!valid) {
        set_validation(kInvalidRange);
        return false;
    }
    draftCeilingDbTp_=valid.value()->ceiling_db_tp();
    clear_validation();
    emit changed();
    return true;
}

bool TerminalLimiterViewModel::setDraftFieldText(
    const QString& field,const QString& text)
{
    if(field!=kField||!state_)return false;
    const auto normalized=text.trimmed();
    bool parsed=false;
    const double value=normalized.isEmpty()?0.0:normalized.toDouble(&parsed);
    if(!parsed) {
        set_validation(QStringLiteral("CEILING must be a finite number"));
        return false;
    }
    return setDraftFieldValue(field,value);
}

bool TerminalLimiterViewModel::commitDraft()
{
    if(!state_||!draftCeilingDbTp_.has_value())return false;
    if(!validationMessage_.isEmpty())return false;
    auto valid=dsp::TruePeakLimiterParameters::create(*draftCeilingDbTp_);
    if(!valid) {
        set_validation(kInvalidRange);
        return false;
    }
    const double newValue=valid.value()->ceiling_db_tp();
    if(newValue==state_->parameters().ceiling_db_tp()) {
        clear_validation();
        emit changed();
        return true;
    }
    auto committed=state_->set_ceiling_db_tp(newValue);
    if(!committed) {
        set_validation(kInvalidRange);
        return false;
    }
    clear_validation();
    emit changed();
    // Sole producer of product-owned rerender invalidation. A missing
    // callback does not imply that processing is audible or available.
    if(stateChanged_)stateChanged_();
    emit acceptedStateChanged();
    return true;
}

void TerminalLimiterViewModel::cancelDraft()
{
    if(state_)draftCeilingDbTp_=state_->parameters().ceiling_db_tp();
    else draftCeilingDbTp_.reset();
    clear_validation();
    emit changed();
}

void TerminalLimiterViewModel::resetToDefault()
{
    if(!state_)return;
    // Use the same DSP constructor/authority as ordinary accepted edits.
    const auto defaults=dsp::TruePeakLimiterParameters::create_default();
    if(!defaults) {
        set_validation(QStringLiteral("True Peak Limiter defaults unavailable"));
        return;
    }
    const double ceiling=defaults.value()->ceiling_db_tp();
    if(state_->parameters().ceiling_db_tp()==ceiling) {
        // Reset also cancels any invalid, uncommitted edit.
        draftCeilingDbTp_=ceiling;
        clear_validation();
        emit changed();
        return;
    }
    auto applied=state_->set_ceiling_db_tp(ceiling);
    if(!applied) {
        set_validation(QStringLiteral("True Peak Limiter reset failed"));
        return;
    }
    draftCeilingDbTp_=ceiling;
    clear_validation();
    emit changed();
    // Preserve BYPASS/ON. An ON reset rerenders through the same accepted
    // coordinator path as a normal ceiling commit; no-op never rerenders.
    if(stateChanged_)stateChanged_();
    emit acceptedStateChanged();
}

void TerminalLimiterViewModel::setBypass(bool value)
{
    if(!state_||state_->bypassed()==value)return;
    auto status=state_->set_user_bypass(value);
    if(!status) {
        set_validation(QStringLiteral(
            "True Peak Limiter bypass could not be updated"));
        return;
    }
    clear_validation();
    emit changed();
    if(stateChanged_)stateChanged_();
    emit acceptedStateChanged();
}

void TerminalLimiterViewModel::requestPrepare()
{
    if(state_ && !state_->bypassed()
        && audioStatus_.startsWith(QStringLiteral("NEEDS PREPARATION")))
        emit prepareRequested();
}

void TerminalLimiterViewModel::cancelPreparation()
{
    if(state_ && !state_->bypassed()
        && audioStatus_.startsWith(QStringLiteral("Preparing")))
        emit cancelRequested();
}

void TerminalLimiterViewModel::previewPrepared()
{
    if(state_ && !state_->bypassed()
        && audioStatus_.startsWith(QStringLiteral("READY")))
        emit previewRequested();
}

void TerminalLimiterViewModel::setNonconformingObservation(
    double measuredLinearPeak,double ceilingLinear,double targetDbTp)
{
    // M16's actual OUTPUT_RATE linear peak, not rounded dB readouts,
    // determines even a one-ULP Ceiling exceedance.
    if(!std::isfinite(measuredLinearPeak)||!std::isfinite(ceilingLinear)
       ||!std::isfinite(targetDbTp)||ceilingLinear<=0.0
       ||measuredLinearPeak<=ceilingLinear) {
        nonconformingPeakText_.clear();
        nonconformingTargetText_.clear();
        nonconformingExcessText_.clear();
        emit changed();
        return;
    }
    const double excessDb=20.0/std::log(10.0)
        *std::log1p((measuredLinearPeak-ceilingLinear)/ceilingLinear);
    if(!std::isfinite(excessDb)||excessDb<=0.0) {
        nonconformingPeakText_.clear();
        nonconformingTargetText_.clear();
        nonconformingExcessText_.clear();
        emit changed();
        return;
    }
    nonconformingPeakText_=QString::number(
        20.0*std::log10(measuredLinearPeak),'f',2)+QStringLiteral(" dBTP");
    nonconformingTargetText_=QString::number(targetDbTp,'f',2)
        +QStringLiteral(" dBTP");
    nonconformingExcessText_=excessDb<0.01
        ?QStringLiteral("<0.01 dB")
        :QStringLiteral("+%1 dB").arg(QString::number(excessDb,'f',2));
    emit changed();
}

void TerminalLimiterViewModel::previewNonconforming()
{
    // A distinct affirmative action is mandatory for failed Ceiling.
    // No auto playback or treating it as verified READY.
    if(state_ && !state_->bypassed()
       && audioStatus_.startsWith(QStringLiteral("NON-CONFORMING")))
        emit previewNonconformingRequested();
}

void TerminalLimiterViewModel::setPreparationProgress(
    const QString& stage,double fraction,bool determinate,const QString& eta)
{
    // Only trusted coordinator code can supply progress; arbitrary QML
    // cannot fabricate a readiness percentage.
    const double bounded=std::clamp(fraction,0.0,1.0);
    if(stage==preparationStage_&&bounded==preparationFraction_
       &&determinate==preparationDeterminate_&&eta==preparationEta_)return;
    preparationStage_=stage;
    preparationFraction_=bounded;
    preparationDeterminate_=determinate;
    preparationEta_=eta;
    emit changed();
}

void TerminalLimiterViewModel::clearPreparationProgress()
{
    setPreparationProgress({},0.0,false);
}

void TerminalLimiterViewModel::setAudioStatus(const QString& value)
{
    if(audioStatus_==value)return;
    audioStatus_=value;
    emit changed();
}

void TerminalLimiterViewModel::reportAudioError(const QString& message)
{
    validationField_=QStringLiteral("audio");
    validationMessage_=message;
    emit changed();
}
void TerminalLimiterViewModel::clearAudioError()
{
    if(validationField_!=QStringLiteral("audio"))return;
    clear_validation();
    emit changed();
}

void TerminalLimiterViewModel::refreshFromAuthority()
{
    cancelDraft();
}
} // namespace rgsml::app
