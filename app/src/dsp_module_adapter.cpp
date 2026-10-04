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
    QString workflowContext,
    QObject* parent)
    : DspModuleAdapter(parent)
    , gainViewModel_(gainViewModel)
    , chainState_(chainState)
    , workflowContext_(std::move(workflowContext))
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
    return workflowContext_;
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
    return true;
}

bool InputGainModuleAdapter::can_undo() const noexcept
{
    if (gainViewModel_ != nullptr) {
        return gainViewModel_->can_undo();
    }
    return false;
}

bool InputGainModuleAdapter::can_redo() const noexcept
{
    if (gainViewModel_ != nullptr) {
        return gainViewModel_->can_redo();
    }
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

CompressorViewModel* InputGainModuleAdapter::compressor_view_model() const noexcept
{
    return nullptr;
}

QString InputGainModuleAdapter::family_accent() const
{
    return QStringLiteral("#6F9FB3");
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
    if (gainViewModel_ != nullptr) {
        gainViewModel_->undo();
    }
}

void InputGainModuleAdapter::redo()
{
    if (gainViewModel_ != nullptr) {
        gainViewModel_->redo();
    }
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
    QString workflowContext,
    QObject* parent)
    : DspModuleAdapter(parent)
    , eqViewModel_(eqViewModel)
    , chainState_(chainState)
    , workflowContext_(std::move(workflowContext))
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
    return workflowContext_;
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

CompressorViewModel* ParametricEqModuleAdapter::compressor_view_model() const noexcept
{
    return nullptr;
}

QString ParametricEqModuleAdapter::family_accent() const
{
    return QStringLiteral("#4E86C6");
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

// -----------------------------------------------------------------------------
// Compressor Module Adapter
// -----------------------------------------------------------------------------

CompressorModuleAdapter::CompressorModuleAdapter(
    CompressorViewModel* compressorViewModel,
    MasteringChainState* chainState,
    QString workflowContext,
    QObject* parent)
    : DspModuleAdapter(parent)
    , compressorViewModel_(compressorViewModel)
    , chainState_(chainState)
    , workflowContext_(std::move(workflowContext))
{
    if (compressorViewModel_ != nullptr) {
        connect(compressorViewModel_, &CompressorViewModel::changed, this, &DspModuleAdapter::changed);
    }
}

QString CompressorModuleAdapter::instance_id() const
{
    if (chainState_ != nullptr) {
        return QString::fromStdString(chainState_->compressor_instance_id().to_string());
    }
    return {};
}

QString CompressorModuleAdapter::type_id() const
{
    return QStringLiteral("rgsml.dsp.compressor");
}

QString CompressorModuleAdapter::display_name() const
{
    return QStringLiteral("Compressor");
}

QString CompressorModuleAdapter::workspace_context_label() const
{
    return workflowContext_;
}

QString CompressorModuleAdapter::configuration_state() const
{
    if (compressorViewModel_ == nullptr) {
        return QStringLiteral("Default");
    }
    return (compressorViewModel_->threshold_dbfs() == -24.0
            && compressorViewModel_->ratio() == 2.0
            && compressorViewModel_->knee_db() == 6.0
            && compressorViewModel_->attack_ms() == 30.0
            && compressorViewModel_->release_ms() == 200.0
            && compressorViewModel_->rms_time_constant_ms() == 50.0
            && compressorViewModel_->look_ahead_ms() == 5.0
            && compressorViewModel_->mix_percent() == 100.0
            && compressorViewModel_->makeup_gain_db() == 0.0)
        ? QStringLiteral("Default")
        : QStringLiteral("Manual");
}

bool CompressorModuleAdapter::bypass_supported() const noexcept
{
    return true;
}

bool CompressorModuleAdapter::bypass() const noexcept
{
    if (compressorViewModel_ != nullptr) {
        return compressorViewModel_->bypass();
    }
    return false;
}

bool CompressorModuleAdapter::preview_supported() const noexcept
{
    return true;
}

QString CompressorModuleAdapter::preview_status() const
{
    if (compressorViewModel_ != nullptr) {
        return compressorViewModel_->preview_status();
    }
    return QStringLiteral("IDLE");
}

QString CompressorModuleAdapter::preview_error() const
{
    if (compressorViewModel_ != nullptr) {
        return compressorViewModel_->preview_error();
    }
    return {};
}

bool CompressorModuleAdapter::history_supported() const noexcept
{
    return true;
}

bool CompressorModuleAdapter::can_undo() const noexcept
{
    if (compressorViewModel_ != nullptr) {
        return compressorViewModel_->can_undo();
    }
    return false;
}

bool CompressorModuleAdapter::can_redo() const noexcept
{
    if (compressorViewModel_ != nullptr) {
        return compressorViewModel_->can_redo();
    }
    return false;
}

bool CompressorModuleAdapter::reset_supported() const noexcept
{
    return true;
}

QString CompressorModuleAdapter::editor_content_key() const
{
    return QStringLiteral("COMPRESSOR");
}

QString CompressorModuleAdapter::live_change_policy() const
{
    return QStringLiteral("PREPARED_REALIZATION_HOT_SWAP");
}

GainViewModel* CompressorModuleAdapter::gain_view_model() const noexcept
{
    return nullptr;
}

EqViewModel* CompressorModuleAdapter::eq_view_model() const noexcept
{
    return nullptr;
}

CompressorViewModel* CompressorModuleAdapter::compressor_view_model() const noexcept
{
    return compressorViewModel_;
}

QString CompressorModuleAdapter::family_accent() const
{
    return QStringLiteral("#C4774A");
}

QString CompressorModuleAdapter::state_text() const
{
    if (compressorViewModel_ == nullptr) {
        return QStringLiteral("Default");
    }
    return (compressorViewModel_->threshold_dbfs() == -24.0
            && compressorViewModel_->ratio() == 2.0
            && compressorViewModel_->knee_db() == 6.0
            && compressorViewModel_->attack_ms() == 30.0
            && compressorViewModel_->release_ms() == 200.0
            && compressorViewModel_->rms_time_constant_ms() == 50.0
            && compressorViewModel_->look_ahead_ms() == 5.0
            && compressorViewModel_->mix_percent() == 100.0
            && compressorViewModel_->makeup_gain_db() == 0.0)
        ? QStringLiteral("Default")
        : QStringLiteral("Manual Edit");
}

bool CompressorModuleAdapter::has_error() const noexcept
{
    if (compressorViewModel_ != nullptr) {
        return compressorViewModel_->preview_status() == QStringLiteral("ERROR");
    }
    return false;
}

void CompressorModuleAdapter::setBypass(bool bypass)
{
    if (compressorViewModel_ != nullptr) {
        compressorViewModel_->setBypass(bypass);
    }
}

void CompressorModuleAdapter::undo()
{
    if (compressorViewModel_ != nullptr) {
        compressorViewModel_->undo();
    }
}

void CompressorModuleAdapter::redo()
{
    if (compressorViewModel_ != nullptr) {
        compressorViewModel_->redo();
    }
}

void CompressorModuleAdapter::resetToDefault()
{
    if (compressorViewModel_ != nullptr) {
        compressorViewModel_->resetToDefault();
    }
}

}  // namespace rgsml::app
