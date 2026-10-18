#include "terminal_limiter_view_model.hpp"

#include <rgsml/dsp/true_peak_limiter_parameters.hpp>

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
    return true;
}

void TerminalLimiterViewModel::cancelDraft()
{
    if(state_)draftCeilingDbTp_=state_->parameters().ceiling_db_tp();
    else draftCeilingDbTp_.reset();
    clear_validation();
    emit changed();
}

void TerminalLimiterViewModel::setBypass(bool value)
{
    if(!state_||state_->bypassed()==value)return;
    auto changed=state_->set_user_bypass(value);
    if(!changed) {
        set_validation(QStringLiteral(
            "True Peak Limiter bypass could not be updated"));
        return;
    }
    clear_validation();
    emit changed();
    if(stateChanged_)stateChanged_();
}

void TerminalLimiterViewModel::refreshFromAuthority()
{
    cancelDraft();
}
} // namespace rgsml::app
