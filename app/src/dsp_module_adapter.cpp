#include "dsp_module_adapter.hpp"
#include "internal/manual_dynamic_eq_edit.hpp"
#include "internal/l3_linked_editor_parameter_projection.hpp"
#include <QRegularExpression>
#include <QUuid>

namespace rgsml::app {

DspModuleAdapter::DspModuleAdapter(QObject* parent)
    : QObject(parent)
{
}

bool DspModuleAdapter::linked_match_type_active(
    const MasteringPreviewController* preview,std::string_view type)
{
    if(!preview)return false;
    if(const auto transfer=preview->private_manual_transfer_preview_ticket()) {
        const auto nodes=transfer->capture().native.linked_after.instances();
        return std::any_of(nodes.begin(),nodes.end(),[&](const auto& node){
            return node.active()&&node.module_type_id()==type;
        });
    }
    const auto ticket=preview->private_m12_product_preview_ticket();
    if(!ticket||!ticket->replay)return false;
    const auto nodes=ticket->replay->reference_linked.instances();
    return std::any_of(nodes.begin(),nodes.end(),[&](const auto& node){
        return node.active()&&node.module_type_id()==type;
    });
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

// Stereo/M-S uses the established DSP host contract and Sea Green identity.
StereoMsModuleAdapter::StereoMsModuleAdapter(StereoMsViewModel* vm,
    MasteringChainState* state, QString context, QObject* parent)
    : DspModuleAdapter(parent), vm_(vm), state_(state),
      context_(std::move(context))
{
    if (vm_) connect(vm_, &StereoMsViewModel::changed, this,
                     &DspModuleAdapter::changed);
}
QString StereoMsModuleAdapter::instance_id() const
{
    return state_ && state_->stereo_ms_instance_id()
        ? QString::fromStdString(state_->stereo_ms_instance_id()->to_string()) : QString{};
}
QString StereoMsModuleAdapter::type_id() const { return QStringLiteral("rgsml.dsp.stereo-ms"); }
QString StereoMsModuleAdapter::display_name() const { return QStringLiteral("Stereo Imager"); }
QString StereoMsModuleAdapter::workspace_context_label() const { return context_; }
QString StereoMsModuleAdapter::configuration_state() const
{
    if (!vm_ || !vm_->available()) return QStringLiteral("Default");
    const auto defaults = dsp::StereoMsParameters::create_default();
    const auto& actual = state_->stereo_ms_parameters();
    return defaults && actual && *actual == *defaults.value()
        ? QStringLiteral("Default") : QStringLiteral("Manual");
}
bool StereoMsModuleAdapter::bypass_supported() const noexcept { return true; }
bool StereoMsModuleAdapter::bypass() const noexcept { return vm_ && vm_->bypass(); }
bool StereoMsModuleAdapter::preview_supported() const noexcept { return true; }
QString StereoMsModuleAdapter::preview_status() const
{ return vm_ ? vm_->preview_status() : QStringLiteral("IDLE"); }
QString StereoMsModuleAdapter::preview_error() const
{ return vm_ ? vm_->preview_error() : QString{}; }
bool StereoMsModuleAdapter::history_supported() const noexcept { return false; }
bool StereoMsModuleAdapter::can_undo() const noexcept { return false; }
bool StereoMsModuleAdapter::can_redo() const noexcept { return false; }
bool StereoMsModuleAdapter::reset_supported() const noexcept { return true; }
QString StereoMsModuleAdapter::editor_content_key() const { return QStringLiteral("STEREO_MS"); }
QString StereoMsModuleAdapter::live_change_policy() const
{ return QStringLiteral("PREPARED_REALIZATION_HOT_SWAP"); }
GainViewModel* StereoMsModuleAdapter::gain_view_model() const noexcept { return nullptr; }
EqViewModel* StereoMsModuleAdapter::eq_view_model() const noexcept { return nullptr; }
CompressorViewModel* StereoMsModuleAdapter::compressor_view_model() const noexcept { return nullptr; }
StereoMsViewModel* StereoMsModuleAdapter::stereo_ms_view_model() const noexcept { return vm_; }
QString StereoMsModuleAdapter::family_accent() const { return QStringLiteral("#4A9A88"); }
QString StereoMsModuleAdapter::state_text() const
{
    return configuration_state() == QStringLiteral("Default")
        ? QStringLiteral("Width 100% Default") : QStringLiteral("Manual Edit");
}
bool StereoMsModuleAdapter::has_error() const noexcept
{
    return vm_ && (!vm_->validation_message().isEmpty()
        || vm_->preview_status() == QStringLiteral("ERROR"));
}
void StereoMsModuleAdapter::setBypass(bool bypass) { if (vm_) vm_->setBypass(bypass); }
void StereoMsModuleAdapter::undo() {}
void StereoMsModuleAdapter::redo() {}
void StereoMsModuleAdapter::resetToDefault() { if (vm_) vm_->resetToDefault(); }

namespace de=manual_dynamic_eq;
ManualDynamicEqModuleAdapter::ManualDynamicEqModuleAdapter(
    MasteringChainState* state,MasteringPreviewController* preview,QString context,QObject* parent)
    :DspModuleAdapter(parent),state_(state),preview_(preview),context_(std::move(context)),
     id_(state?state->dynamic_eq_instance_id():std::nullopt),
     observed_(state?state->dynamic_eq_parameters():std::nullopt)
{
    if(preview_)connect(preview_,&MasteringPreviewController::changed,this,&ManualDynamicEqModuleAdapter::refresh);
}
const ManualDynamicEqModuleAdapter::Params* ManualDynamicEqModuleAdapter::params() const noexcept {
    if(!state_||!id_||state_->dynamic_eq_instance_id()!=id_||!state_->dynamic_eq_parameters())return nullptr;
    return &*state_->dynamic_eq_parameters();
}
bool ManualDynamicEqModuleAdapter::history_current() const noexcept {
    return params()&&observed_&&*observed_==*params();
}
std::shared_ptr<const render::RenderResult> ManualDynamicEqModuleAdapter::prepared() const {
    return preview_&&preview_->snapshot_provider()?preview_->snapshot_provider()():nullptr;
}
bool ManualDynamicEqModuleAdapter::route_available() const {
    const auto audio=prepared();
    return audio&&audio->view().format().channel_count()==2;
}
QString ManualDynamicEqModuleAdapter::configuration_state() const {
    if(!params())return QStringLiteral("Unavailable");
    for(const auto& b:params()->bands())
        if(b.spec().control!=dsp::DynamicEqControl{})return QStringLiteral("Manual");
    return QStringLiteral("Default");
}
bool ManualDynamicEqModuleAdapter::bypass() const noexcept {
    if(!params())return true;
    const auto value=state_->is_bypassed(*id_);
    return !value||*value.value();
}
QString ManualDynamicEqModuleAdapter::state_text() const {
    if(!params())return QStringLiteral("Unavailable");
    if(has_error())return QStringLiteral("Error");
    if(bypass())return QStringLiteral("Bypass");
    return preview_status()==QStringLiteral("RENDERING")?QStringLiteral("Rendering"):QStringLiteral("Manual");
}
bool ManualDynamicEqModuleAdapter::has_error() const noexcept {
    return !validationMessage_.isEmpty()||!operationError_.isEmpty()||
        (preview_&&preview_->preview_status()==QStringLiteral("ERROR"));
}
QVariantMap ManualDynamicEqModuleAdapter::linked_parameters() const {
    if(!params())return {};
    QVariantList bands;
    for(const auto& b:params()->bands())
        bands.append(l3_automatch::project_linked_dynamic_band(b.spec()));
    const auto audio=prepared();
    const double maximum=audio?std::min(20000.0,0.45*audio->view().format().sample_rate().value()):20000.0;
    return {{"instanceId",instance_id()},{"manualEditable",true},
        {"activityAuthorized",false},{"bands",bands},{"maximumFrequencyHz",maximum}};
}
QVariantList ManualDynamicEqModuleAdapter::linkedBandResponse(int index) const {
    const auto audio=prepared();
    if(!params()||!audio)return {};
    const auto format=audio->view().format();
    return l3_automatch::project_dynamic_response(*params(),index,
        format.sample_rate().value(),static_cast<std::uint32_t>(format.channel_count()));
}
bool ManualDynamicEqModuleAdapter::reject(const QString& field,const QString& message) {
    validationField_=field;validationMessage_=message;emit changed();return false;
}
void ManualDynamicEqModuleAdapter::clearValidation() {
    if(validationField_.isEmpty()&&validationMessage_.isEmpty())return;
    validationField_.clear();validationMessage_.clear();emit changed();
}
void ManualDynamicEqModuleAdapter::refresh() {
    if(!history_current()) {
        undo_.clear();redo_.clear();
        observed_=params()?std::optional<Params>(*params()):std::nullopt;
        validationField_.clear();validationMessage_.clear();
    }
    emit changed();
}
void ManualDynamicEqModuleAdapter::publish() {
    if(preview_&&!preview_->private_manual_transfer_preview_ticket())preview_->request_preview();
    emit changed();
}
bool ManualDynamicEqModuleAdapter::write(const Params& p) {
    const auto audio=prepared();
    if(audio) {
        const auto fmt=audio->view().format();
        const auto valid=p.validate_for_audio(fmt.sample_rate().value(),
            static_cast<std::uint32_t>(fmt.channel_count()));
        if(!valid)return reject(QStringLiteral("audio"),QString::fromStdString(valid.error()->message()));
    }
    const auto result=preview_&&preview_->private_manual_transfer_preview_ticket()
        ?preview_->edit_private_transferred_parameter(*id_,p)
        :state_->set_dynamic_eq_parameters(p);
    if(!result){operationError_=QString::fromStdString(result.error()->message());emit changed();return false;}
    operationError_.clear();observed_=p;
    validationField_.clear();validationMessage_.clear();return true;
}
bool ManualDynamicEqModuleAdapter::accept(Result result,const QString& field) {
    if(!params())return false;
    if(!result)return reject(field,QString::fromStdString(result.error()->message()));
    if(!history_current())refresh();
    if(*params()==*result.value()){operationError_.clear();clearValidation();return true;}
    const auto previous=*params();
    if(!write(*result.value()))return false;
    undo_.push_back(previous);redo_.clear();publish();return true;
}
bool ManualDynamicEqModuleAdapter::editBandNumber(const QString& id,const QString& field,double value) {
    if(!params())return false;
    const auto uuid=core::Uuid::parse(id.toStdString());
    if(!uuid)return reject(field,QStringLiteral("Band identity is invalid."));
    return accept(de::numeric(*params(),*uuid.value(),field,value),field);
}
bool ManualDynamicEqModuleAdapter::editBandText(const QString& id,const QString& field,const QString& value) {
    static const QRegularExpression decimal(QStringLiteral(R"(^[+-]?(?:[0-9]+(?:[.,][0-9]*)?|[.,][0-9]+)$)"));
    auto text=value.trimmed();
    if(!decimal.match(text).hasMatch())return reject(field,QStringLiteral("Enter a valid decimal number."));
    bool ok=false;
    const double number=text.replace(',','.').toDouble(&ok);
    return ok?editBandNumber(id,field,number):reject(field,QStringLiteral("Invalid number."));
}
bool ManualDynamicEqModuleAdapter::editBandFilter(const QString& id,const QString& filter) {
    if(!params())return false;
    const auto uuid=core::Uuid::parse(id.toStdString());
    if(!uuid)return reject("filter","Band identity is invalid.");
    using F=dsp::DynamicEqFilterType;
    if(filter=="BELL")return accept(de::filter(*params(),*uuid.value(),F::BELL),"filter");
    if(filter=="LOW_SHELF")return accept(de::filter(*params(),*uuid.value(),F::LOW_SHELF),"filter");
    if(filter=="HIGH_SHELF")return accept(de::filter(*params(),*uuid.value(),F::HIGH_SHELF),"filter");
    return reject("filter","Unsupported filter.");
}
bool ManualDynamicEqModuleAdapter::editBandRouting(const QString& id,const QString& routing) {
    if(!params())return false;
    const auto uuid=core::Uuid::parse(id.toStdString());
    if(!uuid)return reject("routing","Band identity is invalid.");
    using R=dsp::EqRouting;
    if(routing=="STEREO")return accept(de::routing(*params(),*uuid.value(),R::STEREO),"routing");
    if(!route_available())return reject("routing","Stereo source required.");
    if(routing=="MID")return accept(de::routing(*params(),*uuid.value(),R::MID),"routing");
    if(routing=="SIDE")return accept(de::routing(*params(),*uuid.value(),R::SIDE),"routing");
    if(routing=="LEFT")return accept(de::routing(*params(),*uuid.value(),R::LEFT),"routing");
    if(routing=="RIGHT")return accept(de::routing(*params(),*uuid.value(),R::RIGHT),"routing");
    return reject("routing","Unsupported routing.");
}
bool ManualDynamicEqModuleAdapter::editBandEnabled(const QString& id,bool value) {
    if(!params())return false;
    const auto uuid=core::Uuid::parse(id.toStdString());
    return uuid?accept(de::enabled(*params(),*uuid.value(),value),"enabled"):
        reject("enabled","Band identity is invalid.");
}
bool ManualDynamicEqModuleAdapter::editBandGraph(const QString& id,double hz,double gain) {
    if(!params())return false;
    const auto uuid=core::Uuid::parse(id.toStdString());
    return uuid?accept(de::graph(*params(),*uuid.value(),hz,gain),"graph"):
        reject("graph","Band identity is invalid.");
}
bool ManualDynamicEqModuleAdapter::appendBand() {
    if(!params())return false;
    const auto uuid=core::Uuid::parse(QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString());
    return uuid?accept(de::add(*params(),*uuid.value()),"bandCount"):false;
}
bool ManualDynamicEqModuleAdapter::deleteBand(const QString& id) {
    if(!params())return false;
    const auto uuid=core::Uuid::parse(id.toStdString());
    return uuid?accept(de::remove(*params(),*uuid.value()),"bandCount"):
        reject("bandCount","Band identity is invalid.");
}
void ManualDynamicEqModuleAdapter::setBypass(bool value) {
    if(!params()||bypass()==value)return;
    if(!value&&linked_match_type_active(preview_,"rgsml.dsp.dynamic-eq")) {
        operationError_=QStringLiteral("AutoMatch owns the active Dynamic EQ. Transfer or suspend the match first.");
        emit changed();return;
    }
    if(!value) {
        const auto audio=prepared();
        if(audio) {
            const auto format=audio->view().format();
            const auto checked=params()->validate_for_audio(format.sample_rate().value(),
                static_cast<std::uint32_t>(format.channel_count()));
            if(!checked){reject("audio",QString::fromStdString(checked.error()->message()));return;}
        }
    }
    const auto status=state_->set_user_bypass(*id_,value);
    if(!status){operationError_=QString::fromStdString(status.error()->message());emit changed();return;}
    operationError_.clear();publish();
}
void ManualDynamicEqModuleAdapter::resetToDefault() {
    if(params())accept(de::reset_flat(*params()),"reset");
}
void ManualDynamicEqModuleAdapter::move_history(std::vector<Params>& from,std::vector<Params>& to) {
    if(!history_current()||from.empty())return;
    const auto previous=*params(),next=from.back();
    if(!write(next))return;
    from.pop_back();to.push_back(previous);publish();
}

}  // namespace rgsml::app
