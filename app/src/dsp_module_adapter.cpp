#include "dsp_module_adapter.hpp"

namespace rgsml::app {

DspModuleAdapter::DspModuleAdapter(QObject* parent)
    : QObject(parent)
{
}

// -----------------------------------------------------------------------------
// Input Gain Module Adapter
// -----------------------------------------------------------------------------

InputGainModuleAdapter::InputGainModuleAdapter(
    GainViewModel* gainViewModel,
    MasteringChainState* chainState,
    QObject* parent)
    : DspModuleAdapter(parent)
    , gainViewModel_(gainViewModel)
    , chainState_(chainState)
{
    if (gainViewModel_ != nullptr) {
        connect(gainViewModel_, &GainViewModel::changed, this, &DspModuleAdapter::changed);
    }
}

QString InputGainModuleAdapter::instance_id() const
{
    if (chainState_ != nullptr) {
        return QString::fromStdString(chainState_->gain_instance_id().to_string());
    }
    return {};
}

QString InputGainModuleAdapter::type_id() const
{
    return QStringLiteral("rgsml.dsp.gain");
}

QString InputGainModuleAdapter::display_name() const
{
    return QStringLiteral("Input Gain");
}

QString InputGainModuleAdapter::workspace_context_label() const
{
    return QStringLiteral("Gain Staging / Manual Mastering");
}

QString InputGainModuleAdapter::configuration_state() const
{
    if (gainViewModel_ == nullptr) {
        return QStringLiteral("Default");
    }
    return (gainViewModel_->gain_db() == 0.0)
        ? QStringLiteral("Default")
        : QStringLiteral("Manual");
}

bool InputGainModuleAdapter::bypass_supported() const noexcept
{
    return true;
}

bool InputGainModuleAdapter::bypass() const noexcept
{
    if (gainViewModel_ != nullptr) {
        return gainViewModel_->bypass();
    }
    return false;
}

bool InputGainModuleAdapter::preview_supported() const noexcept
{
    return true;
}

QString InputGainModuleAdapter::preview_status() const
{
    if (gainViewModel_ != nullptr) {
        return gainViewModel_->preview_status();
    }
    return QStringLiteral("IDLE");
}

QString InputGainModuleAdapter::preview_error() const
{
    if (gainViewModel_ != nullptr) {
        return gainViewModel_->preview_error();
    }
    return {};
}

bool InputGainModuleAdapter::history_supported() const noexcept
{
    return false;
}

bool InputGainModuleAdapter::can_undo() const noexcept
{
    return false;
}

bool InputGainModuleAdapter::can_redo() const noexcept
{
    return false;
}

bool InputGainModuleAdapter::reset_supported() const noexcept
{
    return true;
}

QString InputGainModuleAdapter::editor_content_key() const
{
    return QStringLiteral("INPUT_GAIN");
}

QString InputGainModuleAdapter::live_change_policy() const
{
    return QStringLiteral("PREPARED_REALIZATION_HOT_SWAP");
}

GainViewModel* InputGainModuleAdapter::gain_view_model() const noexcept
{
    return gainViewModel_;
}

EqViewModel* InputGainModuleAdapter::eq_view_model() const noexcept
{
    return nullptr;
}

QString InputGainModuleAdapter::state_text() const
{
    if (gainViewModel_ == nullptr) {
        return QStringLiteral("0.0 dB Default");
    }
    if (gainViewModel_->gain_db() == 0.0) {
        return QStringLiteral("0.0 dB Default");
    }
    const double val = gainViewModel_->gain_db();
    const QString prefix = val > 0.0 ? QStringLiteral("+") : QString{};
    return prefix + gainViewModel_->gain_db_text() + QStringLiteral(" dB Manual");
}

bool InputGainModuleAdapter::has_error() const noexcept
{
    if (gainViewModel_ != nullptr) {
        return gainViewModel_->preview_status() == QStringLiteral("ERROR");
    }
    return false;
}

void InputGainModuleAdapter::setBypass(bool bypass)
{
    if (gainViewModel_ != nullptr) {
        gainViewModel_->setBypass(bypass);
    }
}

void InputGainModuleAdapter::undo()
{
    // No history for Input Gain
}

void InputGainModuleAdapter::redo()
{
    // No history for Input Gain
}

void InputGainModuleAdapter::resetToDefault()
{
    if (gainViewModel_ != nullptr) {
        gainViewModel_->resetToDefault();
    }
}

// -----------------------------------------------------------------------------
// Parametric EQ Module Adapter
// -----------------------------------------------------------------------------

ParametricEqModuleAdapter::ParametricEqModuleAdapter(
    EqViewModel* eqViewModel,
    MasteringChainState* chainState,
    QObject* parent)
    : DspModuleAdapter(parent)
    , eqViewModel_(eqViewModel)
    , chainState_(chainState)
{
    if (eqViewModel_ != nullptr) {
        connect(eqViewModel_, &EqViewModel::changed, this, &DspModuleAdapter::changed);
    }
}

QString ParametricEqModuleAdapter::instance_id() const
{
    if (chainState_ != nullptr) {
        return QString::fromStdString(chainState_->eq_instance_id().to_string());
    }
    return {};
}

QString ParametricEqModuleAdapter::type_id() const
{
    return QStringLiteral("rgsml.dsp.parametric-eq");
}

QString ParametricEqModuleAdapter::display_name() const
{
    return QStringLiteral("Parametric EQ");
}

QString ParametricEqModuleAdapter::workspace_context_label() const
{
    return QStringLiteral("Manual Mastering");
}

QString ParametricEqModuleAdapter::configuration_state() const
{
    if (eqViewModel_ == nullptr) {
        return QStringLiteral("Default");
    }
    return eqViewModel_->is_default()
        ? QStringLiteral("Default")
        : QStringLiteral("Manual");
}

bool ParametricEqModuleAdapter::bypass_supported() const noexcept
{
    return true;
}

bool ParametricEqModuleAdapter::bypass() const noexcept
{
    if (eqViewModel_ != nullptr) {
        return eqViewModel_->bypass();
    }
    return false;
}

bool ParametricEqModuleAdapter::preview_supported() const noexcept
{
    return true;
}

QString ParametricEqModuleAdapter::preview_status() const
{
    if (eqViewModel_ != nullptr) {
        return eqViewModel_->preview_status();
    }
    return QStringLiteral("IDLE");
}

QString ParametricEqModuleAdapter::preview_error() const
{
    if (eqViewModel_ != nullptr) {
        return eqViewModel_->preview_error();
    }
    return {};
}

bool ParametricEqModuleAdapter::history_supported() const noexcept
{
    return true;
}

bool ParametricEqModuleAdapter::can_undo() const noexcept
{
    if (eqViewModel_ != nullptr) {
        return eqViewModel_->can_undo();
    }
    return false;
}

bool ParametricEqModuleAdapter::can_redo() const noexcept
{
    if (eqViewModel_ != nullptr) {
        return eqViewModel_->can_redo();
    }
    return false;
}

bool ParametricEqModuleAdapter::reset_supported() const noexcept
{
    return true;
}

QString ParametricEqModuleAdapter::editor_content_key() const
{
    return QStringLiteral("PARAMETRIC_EQ");
}

QString ParametricEqModuleAdapter::live_change_policy() const
{
    return QStringLiteral("PREPARED_REALIZATION_HOT_SWAP");
}

GainViewModel* ParametricEqModuleAdapter::gain_view_model() const noexcept
{
    return nullptr;
}

EqViewModel* ParametricEqModuleAdapter::eq_view_model() const noexcept
{
    return eqViewModel_;
}

QString ParametricEqModuleAdapter::state_text() const
{
    if (eqViewModel_ == nullptr) {
        return QStringLiteral("Flat Default");
    }
    return eqViewModel_->is_default()
        ? QStringLiteral("Flat Default")
        : QStringLiteral("Manual Edit");
}

bool ParametricEqModuleAdapter::has_error() const noexcept
{
    if (eqViewModel_ != nullptr) {
        return eqViewModel_->preview_status() == QStringLiteral("ERROR");
    }
    return false;
}

void ParametricEqModuleAdapter::setBypass(bool bypass)
{
    if (eqViewModel_ != nullptr) {
        eqViewModel_->setBypass(bypass);
    }
}

void ParametricEqModuleAdapter::undo()
{
    if (eqViewModel_ != nullptr) {
        eqViewModel_->undo();
    }
}

void ParametricEqModuleAdapter::redo()
{
    if (eqViewModel_ != nullptr) {
        eqViewModel_->redo();
    }
}

void ParametricEqModuleAdapter::resetToDefault()
{
    if (eqViewModel_ != nullptr) {
        eqViewModel_->resetToFlat();
    }
}

}  // namespace rgsml::app
