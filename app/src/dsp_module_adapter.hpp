#pragma once

#include "compressor_view_model.hpp"
#include "eq_view_model.hpp"
#include "gain_view_model.hpp"
#include "mastering_chain_state.hpp"
#include "mastering_preview_controller.hpp"
#include "stereo_ms_view_model.hpp"
#include "terminal_limiter_state.hpp"
#include "terminal_limiter_view_model.hpp"

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVariantList>

#include <memory>
#include <string_view>
#include <utility>

namespace rgsml::app {

class DspModuleAdapter : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString instanceId READ instance_id NOTIFY changed)
    Q_PROPERTY(QString typeId READ type_id NOTIFY changed)
    Q_PROPERTY(QString displayName READ display_name NOTIFY changed)
    Q_PROPERTY(QString workspaceContextLabel READ workspace_context_label NOTIFY changed)
    Q_PROPERTY(QString configurationState READ configuration_state NOTIFY changed)
    Q_PROPERTY(bool bypassSupported READ bypass_supported NOTIFY changed)
    Q_PROPERTY(bool nonProcessingService READ non_processing_service CONSTANT)
    Q_PROPERTY(bool bypass READ bypass WRITE setBypass NOTIFY changed)
    Q_PROPERTY(bool previewSupported READ preview_supported NOTIFY changed)
    Q_PROPERTY(QString previewStatus READ preview_status NOTIFY changed)
    Q_PROPERTY(QString previewError READ preview_error NOTIFY changed)
    Q_PROPERTY(bool historySupported READ history_supported NOTIFY changed)
    Q_PROPERTY(bool canUndo READ can_undo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ can_redo NOTIFY changed)
    Q_PROPERTY(bool resetSupported READ reset_supported NOTIFY changed)
    Q_PROPERTY(QString editorContentKey READ editor_content_key NOTIFY changed)
    Q_PROPERTY(QString liveChangePolicy READ live_change_policy NOTIFY changed)
    Q_PROPERTY(GainViewModel* gainViewModel READ gain_view_model NOTIFY changed)
    Q_PROPERTY(EqViewModel* eqViewModel READ eq_view_model NOTIFY changed)
    Q_PROPERTY(CompressorViewModel* compressorViewModel READ compressor_view_model NOTIFY changed)
    Q_PROPERTY(StereoMsViewModel* stereoMsViewModel READ stereo_ms_view_model NOTIFY changed)
    Q_PROPERTY(QString familyAccent READ family_accent NOTIFY changed)
    Q_PROPERTY(QString stateText READ state_text NOTIFY changed)
    Q_PROPERTY(bool hasError READ has_error NOTIFY changed)
    Q_PROPERTY(bool linkedReadOnly READ linked_read_only NOTIFY changed)
    Q_PROPERTY(QVariantMap linkedParameters READ linked_parameters NOTIFY changed)
    Q_PROPERTY(QVariantMap linkedActivity READ linked_activity NOTIFY activityChanged)

public:
    explicit DspModuleAdapter(QObject* parent = nullptr);
    // Consult the current immutable replay, never UI row visibility.
    [[nodiscard]] static bool linked_match_type_active(
        const MasteringPreviewController* preview,std::string_view type);
    ~DspModuleAdapter() override = default;

    [[nodiscard]] virtual QString instance_id() const = 0;
    [[nodiscard]] virtual QString type_id() const = 0;
    [[nodiscard]] virtual QString display_name() const = 0;
    [[nodiscard]] virtual QString workspace_context_label() const = 0;
    [[nodiscard]] virtual QString configuration_state() const = 0;
    [[nodiscard]] virtual bool bypass_supported() const noexcept = 0;
    // DSPs default to processing; explicit observers/utilities opt out.
    [[nodiscard]] virtual bool non_processing_service() const noexcept { return false; }
    [[nodiscard]] virtual bool bypass() const noexcept = 0;
    [[nodiscard]] virtual bool preview_supported() const noexcept = 0;
    [[nodiscard]] virtual QString preview_status() const = 0;
    [[nodiscard]] virtual QString preview_error() const = 0;
    [[nodiscard]] virtual bool history_supported() const noexcept = 0;
    [[nodiscard]] virtual bool can_undo() const noexcept = 0;
    [[nodiscard]] virtual bool can_redo() const noexcept = 0;
    [[nodiscard]] virtual bool reset_supported() const noexcept = 0;
    [[nodiscard]] virtual QString editor_content_key() const = 0;
    [[nodiscard]] virtual QString live_change_policy() const = 0;
    [[nodiscard]] virtual GainViewModel* gain_view_model() const noexcept = 0;
    [[nodiscard]] virtual EqViewModel* eq_view_model() const noexcept = 0;
    [[nodiscard]] virtual CompressorViewModel* compressor_view_model() const noexcept = 0;
    // Optional extension: prior adapters continue returning null.
    [[nodiscard]] virtual StereoMsViewModel* stereo_ms_view_model() const noexcept { return nullptr; }
    [[nodiscard]] virtual QString family_accent() const = 0;
    [[nodiscard]] virtual QString state_text() const = 0;
    [[nodiscard]] virtual bool has_error() const noexcept = 0;
    // Optional read-only projection for Product-owned REF_LINKED DSPs.
    // Existing MANUAL modules and nonprocessing services remain unchanged.
    [[nodiscard]] virtual bool linked_read_only() const noexcept {return false;}
    [[nodiscard]] virtual QVariantMap linked_parameters() const {return {};}
    Q_INVOKABLE virtual QVariantList linkedBandResponse(int) const {return {};}
    [[nodiscard]] virtual QVariantMap linked_activity() const {return {};}

    Q_INVOKABLE virtual void setBypass(bool bypass) = 0;
    Q_INVOKABLE virtual void undo() = 0;
    Q_INVOKABLE virtual void redo() = 0;
    Q_INVOKABLE virtual void resetToDefault() = 0;

signals:
    void activityChanged();
    void changed();
};

class InputGainModuleAdapter final : public DspModuleAdapter {
    Q_OBJECT

public:
    InputGainModuleAdapter(
        GainViewModel* gainViewModel,
        MasteringChainState* chainState,
        QString workflowContext = QStringLiteral("Mastering"),
        QObject* parent = nullptr);
    ~InputGainModuleAdapter() override = default;

    [[nodiscard]] QString instance_id() const override;
    [[nodiscard]] QString type_id() const override;
    [[nodiscard]] QString display_name() const override;
    [[nodiscard]] QString workspace_context_label() const override;
    [[nodiscard]] QString configuration_state() const override;
    [[nodiscard]] bool bypass_supported() const noexcept override;
    [[nodiscard]] bool bypass() const noexcept override;
    [[nodiscard]] bool preview_supported() const noexcept override;
    [[nodiscard]] QString preview_status() const override;
    [[nodiscard]] QString preview_error() const override;
    [[nodiscard]] bool history_supported() const noexcept override;
    [[nodiscard]] bool can_undo() const noexcept override;
    [[nodiscard]] bool can_redo() const noexcept override;
    [[nodiscard]] bool reset_supported() const noexcept override;
    [[nodiscard]] QString editor_content_key() const override;
    [[nodiscard]] QString live_change_policy() const override;
    [[nodiscard]] GainViewModel* gain_view_model() const noexcept override;
    [[nodiscard]] EqViewModel* eq_view_model() const noexcept override;
    [[nodiscard]] CompressorViewModel* compressor_view_model() const noexcept override;
    [[nodiscard]] QString family_accent() const override;
    [[nodiscard]] QString state_text() const override;
    [[nodiscard]] bool has_error() const noexcept override;

    void setBypass(bool bypass) override;
    void undo() override;
    void redo() override;
    void resetToDefault() override;

private:
    GainViewModel* gainViewModel_{nullptr};
    MasteringChainState* chainState_{nullptr};
    QString workflowContext_;
};

class ParametricEqModuleAdapter final : public DspModuleAdapter {
    Q_OBJECT

public:
    ParametricEqModuleAdapter(
        EqViewModel* eqViewModel,
        MasteringChainState* chainState,
        QString workflowContext = QStringLiteral("Mastering"),
        QObject* parent = nullptr);
    ~ParametricEqModuleAdapter() override = default;

    [[nodiscard]] QString instance_id() const override;
    [[nodiscard]] QString type_id() const override;
    [[nodiscard]] QString display_name() const override;
    [[nodiscard]] QString workspace_context_label() const override;
    [[nodiscard]] QString configuration_state() const override;
    [[nodiscard]] bool bypass_supported() const noexcept override;
    [[nodiscard]] bool bypass() const noexcept override;
    [[nodiscard]] bool preview_supported() const noexcept override;
    [[nodiscard]] QString preview_status() const override;
    [[nodiscard]] QString preview_error() const override;
    [[nodiscard]] bool history_supported() const noexcept override;
    [[nodiscard]] bool can_undo() const noexcept override;
    [[nodiscard]] bool can_redo() const noexcept override;
    [[nodiscard]] bool reset_supported() const noexcept override;
    [[nodiscard]] QString editor_content_key() const override;
    [[nodiscard]] QString live_change_policy() const override;
    [[nodiscard]] GainViewModel* gain_view_model() const noexcept override;
    [[nodiscard]] EqViewModel* eq_view_model() const noexcept override;
    [[nodiscard]] CompressorViewModel* compressor_view_model() const noexcept override;
    [[nodiscard]] QString family_accent() const override;
    [[nodiscard]] QString state_text() const override;
    [[nodiscard]] bool has_error() const noexcept override;

    void setBypass(bool bypass) override;
    void undo() override;
    void redo() override;
    void resetToDefault() override;

private:
    EqViewModel* eqViewModel_{nullptr};
    MasteringChainState* chainState_{nullptr};
    QString workflowContext_;
};

class CompressorModuleAdapter final : public DspModuleAdapter {
    Q_OBJECT

public:
    CompressorModuleAdapter(
        CompressorViewModel* compressorViewModel,
        MasteringChainState* chainState,
        QString workflowContext = QStringLiteral("Mastering"),
        QObject* parent = nullptr);
    ~CompressorModuleAdapter() override = default;

    [[nodiscard]] QString instance_id() const override;
    [[nodiscard]] QString type_id() const override;
    [[nodiscard]] QString display_name() const override;
    [[nodiscard]] QString workspace_context_label() const override;
    [[nodiscard]] QString configuration_state() const override;
    [[nodiscard]] bool bypass_supported() const noexcept override;
    [[nodiscard]] bool bypass() const noexcept override;
    [[nodiscard]] bool preview_supported() const noexcept override;
    [[nodiscard]] QString preview_status() const override;
    [[nodiscard]] QString preview_error() const override;
    [[nodiscard]] bool history_supported() const noexcept override;
    [[nodiscard]] bool can_undo() const noexcept override;
    [[nodiscard]] bool can_redo() const noexcept override;
    [[nodiscard]] bool reset_supported() const noexcept override;
    [[nodiscard]] QString editor_content_key() const override;
    [[nodiscard]] QString live_change_policy() const override;
    [[nodiscard]] GainViewModel* gain_view_model() const noexcept override;
    [[nodiscard]] EqViewModel* eq_view_model() const noexcept override;
    [[nodiscard]] CompressorViewModel* compressor_view_model() const noexcept override;
    [[nodiscard]] QString family_accent() const override;
    [[nodiscard]] QString state_text() const override;
    [[nodiscard]] bool has_error() const noexcept override;

    void setBypass(bool bypass) override;
    void undo() override;
    void redo() override;
    void resetToDefault() override;

private:
    CompressorViewModel* compressorViewModel_{nullptr};
    MasteringChainState* chainState_{nullptr};
    QString workflowContext_;
};

// An M15 adapter never creates a module: it mirrors only an opt-in chain
// instance. Undo/redo is not advertised until a real history exists.
class StereoMsModuleAdapter final : public DspModuleAdapter {
    Q_OBJECT
public:
    StereoMsModuleAdapter(StereoMsViewModel* vm, MasteringChainState* state,
                          QString context = QStringLiteral("Mastering"), QObject* parent = nullptr);
    [[nodiscard]] QString instance_id() const override;
    [[nodiscard]] QString type_id() const override;
    [[nodiscard]] QString display_name() const override;
    [[nodiscard]] QString workspace_context_label() const override;
    [[nodiscard]] QString configuration_state() const override;
    [[nodiscard]] bool bypass_supported() const noexcept override;
    [[nodiscard]] bool bypass() const noexcept override;
    [[nodiscard]] bool preview_supported() const noexcept override;
    [[nodiscard]] QString preview_status() const override;
    [[nodiscard]] QString preview_error() const override;
    [[nodiscard]] bool history_supported() const noexcept override;
    [[nodiscard]] bool can_undo() const noexcept override;
    [[nodiscard]] bool can_redo() const noexcept override;
    [[nodiscard]] bool reset_supported() const noexcept override;
    [[nodiscard]] QString editor_content_key() const override;
    [[nodiscard]] QString live_change_policy() const override;
    [[nodiscard]] GainViewModel* gain_view_model() const noexcept override;
    [[nodiscard]] EqViewModel* eq_view_model() const noexcept override;
    [[nodiscard]] CompressorViewModel* compressor_view_model() const noexcept override;
    [[nodiscard]] StereoMsViewModel* stereo_ms_view_model() const noexcept override;
    [[nodiscard]] QString family_accent() const override;
    [[nodiscard]] QString state_text() const override;
    [[nodiscard]] bool has_error() const noexcept override;
    void setBypass(bool bypass) override;
    void undo() override;
    void redo() override;
    void resetToDefault() override;
private:
    StereoMsViewModel* vm_{nullptr};
    MasteringChainState* state_{nullptr};
    QString context_;
};



// Native MANUAL C00 adapter. Shipping enumeration remains disabled until
// the C01 editor and REF_LINKED ownership transition are qualified.
class ManualTransientShaperModuleAdapter final : public DspModuleAdapter {
    Q_OBJECT
    Q_PROPERTY(bool manualEditable READ manual_editable NOTIFY changed)
    Q_PROPERTY(double attackAmountDb READ attack_amount_db NOTIFY changed)
    Q_PROPERTY(double sustainAmountDb READ sustain_amount_db NOTIFY changed)
    Q_PROPERTY(QString attackValidationMessage READ attack_validation_message NOTIFY changed)
    Q_PROPERTY(QString sustainValidationMessage READ sustain_validation_message NOTIFY changed)
public:
    ManualTransientShaperModuleAdapter(MasteringChainState* state,
        MasteringPreviewController* preview, QString context, QObject* parent=nullptr)
        :DspModuleAdapter(parent),state_(state),preview_(preview),context_(std::move(context)),
         id_(state?state->transient_shaper_instance_id():std::nullopt),
         observed_(state?state->transient_shaper_parameters():std::nullopt) {
        if(preview_)connect(preview_,&MasteringPreviewController::changed,
            this,&ManualTransientShaperModuleAdapter::refresh);
    }
    [[nodiscard]] bool manual_editable() const noexcept {
        return params()!=nullptr&&(!preview_||!preview_->private_manual_transfer_preview_ticket()||
            (id_&&preview_->can_edit_private_transferred_parameter(*id_)));
    }
    [[nodiscard]] double attack_amount_db() const noexcept {return params()?params()->attack_amount_db():0.0;}
    [[nodiscard]] double sustain_amount_db() const noexcept {return params()?params()->sustain_amount_db():0.0;}
    [[nodiscard]] QString attack_validation_message() const {return attackError_;}
    [[nodiscard]] QString sustain_validation_message() const {return sustainError_;}
    [[nodiscard]] QString instance_id() const override {return id_?QString::fromStdString(id_->to_string()):QString{};}
    [[nodiscard]] QString type_id() const override {return QStringLiteral("rgsml.dsp.transient-shaper");}
    [[nodiscard]] QString display_name() const override {return QStringLiteral("Transient Shaper");}
    [[nodiscard]] QString workspace_context_label() const override {return context_;}
    [[nodiscard]] QString configuration_state() const override {
        return params() && params()->neutral()?QStringLiteral("Default"):QStringLiteral("Manual");
    }
    [[nodiscard]] bool bypass_supported() const noexcept override {
        return manual_editable()&&(!preview_||!preview_->private_manual_transfer_preview_ticket());
    }
    [[nodiscard]] bool bypass() const noexcept override {
        if(!params()||!id_)return true;
        const auto value=state_->is_bypassed(*id_);
        return !value||*value.value();
    }
    [[nodiscard]] bool preview_supported() const noexcept override {return preview_!=nullptr;}
    [[nodiscard]] QString preview_status() const override {return preview_?preview_->preview_status():QStringLiteral("IDLE");}
    [[nodiscard]] QString preview_error() const override {
        return !operationError_.isEmpty()?operationError_:(preview_?preview_->preview_error():QString{});
    }
    [[nodiscard]] bool history_supported() const noexcept override {return manual_editable();}
    [[nodiscard]] bool can_undo() const noexcept override {
        if(preview_&&preview_->private_manual_transfer_preview_ticket())
            return id_&&preview_->can_change_private_transferred_edit_history(*id_,false);
        return manual_editable()&&history_current()&&!undo_.empty();
    }
    [[nodiscard]] bool can_redo() const noexcept override {
        if(preview_&&preview_->private_manual_transfer_preview_ticket())
            return id_&&preview_->can_change_private_transferred_edit_history(*id_,true);
        return manual_editable()&&history_current()&&!redo_.empty();
    }
    [[nodiscard]] bool reset_supported() const noexcept override {return manual_editable();}
    [[nodiscard]] QString editor_content_key() const override {return QStringLiteral("TRANSIENT_SHAPER");}
    [[nodiscard]] QString live_change_policy() const override {return QStringLiteral("MANUAL_COMMIT_REBUILDS_PREVIEW");}
    [[nodiscard]] GainViewModel* gain_view_model() const noexcept override {return nullptr;}
    [[nodiscard]] EqViewModel* eq_view_model() const noexcept override {return nullptr;}
    [[nodiscard]] CompressorViewModel* compressor_view_model() const noexcept override {return nullptr;}
    [[nodiscard]] QString family_accent() const override {return QStringLiteral("#C4774A");}
    [[nodiscard]] QString state_text() const override {
        if(!params())return QStringLiteral("Unavailable");
        if(has_error())return QStringLiteral("Error");
        if(bypass())return QStringLiteral("Bypass");
        return preview_status()==QStringLiteral("RENDERING")?QStringLiteral("Rendering"):QStringLiteral("Manual");
    }
    [[nodiscard]] bool has_error() const noexcept override {
        return !attackError_.isEmpty()||!sustainError_.isEmpty()||!operationError_.isEmpty()
            ||(preview_&&preview_->preview_status()==QStringLiteral("ERROR"));
    }
    Q_INVOKABLE bool setAttackAmountDb(double value) {return edit(value,sustain_amount_db(),true);}
    Q_INVOKABLE bool setSustainAmountDb(double value) {return edit(attack_amount_db(),value,false);}
    Q_INVOKABLE void reportNumericValidation(const QString& field,const QString& message) {
        if(!params())return;
        QString* target=field==QStringLiteral("Attack")?&attackError_:
            field==QStringLiteral("Sustain")?&sustainError_:nullptr;
        if(!target||*target==message)return;
        *target=message;emit changed();
    }
    void setBypass(bool value) override {
        if(!params()||bypass()==value)return;
        if(!value&&linked_match_type_active(preview_,"rgsml.dsp.transient-shaper")){
            operationError_=QStringLiteral("AutoMatch owns the active Transient Shaper. Transfer or suspend the match first.");
            emit changed();return;
        }
        auto status=state_->set_user_bypass(*id_,value);
        if(!status){operationError_=QString::fromStdString(status.error()->message());emit changed();return;}
        operationError_.clear();publish();
    }
    void undo() override {
        if(preview_&&preview_->private_manual_transfer_preview_ticket()) {
            if(!id_)return;
            const auto result=preview_->change_private_transferred_edit_history(*id_,false);
            if(!result)operationError_=QString::fromStdString(result.error()->message());
            else operationError_.clear();
            emit changed();return;
        }
        move_history(undo_,redo_);
    }
    void redo() override {
        if(preview_&&preview_->private_manual_transfer_preview_ticket()) {
            if(!id_)return;
            const auto result=preview_->change_private_transferred_edit_history(*id_,true);
            if(!result)operationError_=QString::fromStdString(result.error()->message());
            else operationError_.clear();
            emit changed();return;
        }
        move_history(redo_,undo_);
    }
    void resetToDefault() override {
        auto p=dsp::TransientShaperParameters::create_default();
        if(p&&accept(*p.value())){attackError_.clear();sustainError_.clear();emit changed();}
    }
private:
    using Params=dsp::TransientShaperParameters;
    [[nodiscard]] const Params* params() const noexcept {
        if(!state_||!id_||state_->transient_shaper_instance_id()!=id_||
            !state_->transient_shaper_parameters())return nullptr;
        return &*state_->transient_shaper_parameters();
    }
    [[nodiscard]] bool history_current() const noexcept {
        return params()&&observed_&&*observed_==*params();
    }
    void refresh() {
        if(!history_current()){
            undo_.clear();redo_.clear();
            observed_=params()?std::optional<Params>(*params()):std::nullopt;
        }
        emit changed();
    }
    void publish() {if(preview_&&!preview_->private_manual_transfer_preview_ticket())preview_->request_preview();emit changed();}
    bool write(const Params& p) {
        const bool transferred=preview_&&preview_->private_manual_transfer_preview_ticket();
        auto status=transferred
            ?preview_->edit_private_transferred_parameter(*id_,p)
            :state_->set_transient_shaper_parameters(p);
        if(!status){operationError_=QString::fromStdString(status.error()->message());emit changed();return false;}
        operationError_.clear();observed_=p;return true;
    }
    bool accept(const Params& p) {
        if(!params())return false;
        if(!history_current())refresh();
        if(*params()==p){operationError_.clear();emit changed();return true;}
        const auto previous=*params();
        if(!write(p))return false;
        undo_.push_back(previous);redo_.clear();publish();return true;
    }
    bool edit(double attack,double sustain,bool attackField) {
        if(!params())return false;
        auto p=Params::create(attack,sustain);
        auto& error=attackField?attackError_:sustainError_;
        if(!p){error=QString::fromStdString(p.error()->message());emit changed();return false;}
        if(!accept(*p.value()))return false;
        error.clear();emit changed();return true;
    }
    void move_history(std::vector<Params>& from,std::vector<Params>& to) {
        if(!history_current()||from.empty())return;
        const auto previous=*params(),next=from.back();
        if(!write(next))return;
        from.pop_back();to.push_back(previous);attackError_.clear();sustainError_.clear();publish();
    }
    MasteringChainState* state_{};
    MasteringPreviewController* preview_{};
    QString context_;
    std::optional<dsp::ModuleInstanceId> id_;
    std::optional<Params> observed_;
    std::vector<Params> undo_,redo_;
    QString attackError_,sustainError_,operationError_;
};

// Real MANUAL C01 editor. Stable band IDs, typed validation and history
// are independent of the selected GUI band and of immutable linked tickets.
class ManualDynamicEqModuleAdapter final : public DspModuleAdapter {
    Q_OBJECT
    Q_PROPERTY(bool manualEditable READ manual_editable NOTIFY changed)
    Q_PROPERTY(QString validationField READ validation_field NOTIFY changed)
    Q_PROPERTY(QString validationMessage READ validation_message NOTIFY changed)
    Q_PROPERTY(bool routeAvailable READ route_available NOTIFY changed)
public:
    ManualDynamicEqModuleAdapter(MasteringChainState* state,
        MasteringPreviewController* preview,QString context,QObject* parent=nullptr);
    [[nodiscard]] bool manual_editable() const noexcept {
        return params()!=nullptr&&(!preview_||!preview_->private_manual_transfer_preview_ticket()||
            (id_&&preview_->can_edit_private_transferred_parameter(*id_)));
    }
    [[nodiscard]] QString validation_field() const {return validationField_;}
    [[nodiscard]] QString validation_message() const {return validationMessage_;}
    [[nodiscard]] bool route_available() const;
    [[nodiscard]] QString instance_id() const override {return id_?QString::fromStdString(id_->to_string()):QString{};}
    [[nodiscard]] QString type_id() const override {return QStringLiteral("rgsml.dsp.dynamic-eq");}
    [[nodiscard]] QString display_name() const override {return QStringLiteral("Dynamic EQ");}
    [[nodiscard]] QString workspace_context_label() const override {return context_;}
    [[nodiscard]] QString configuration_state() const override;
    [[nodiscard]] bool bypass_supported() const noexcept override {
        return manual_editable()&&(!preview_||!preview_->private_manual_transfer_preview_ticket());
    }
    [[nodiscard]] bool bypass() const noexcept override;
    [[nodiscard]] bool preview_supported() const noexcept override {return preview_!=nullptr;}
    [[nodiscard]] QString preview_status() const override {return preview_?preview_->preview_status():QStringLiteral("IDLE");}
    [[nodiscard]] QString preview_error() const override {return !operationError_.isEmpty()?operationError_:(preview_?preview_->preview_error():QString{});}
    [[nodiscard]] bool history_supported() const noexcept override {return manual_editable();}
    [[nodiscard]] bool can_undo() const noexcept override {
        if(preview_&&preview_->private_manual_transfer_preview_ticket())
            return id_&&preview_->can_change_private_transferred_edit_history(*id_,false);
        return manual_editable()&&history_current()&&!undo_.empty();
    }
    [[nodiscard]] bool can_redo() const noexcept override {
        if(preview_&&preview_->private_manual_transfer_preview_ticket())
            return id_&&preview_->can_change_private_transferred_edit_history(*id_,true);
        return manual_editable()&&history_current()&&!redo_.empty();
    }
    [[nodiscard]] bool reset_supported() const noexcept override {return manual_editable();}
    [[nodiscard]] QString editor_content_key() const override {return QStringLiteral("DYNAMIC_EQ");}
    [[nodiscard]] QString live_change_policy() const override {return QStringLiteral("MANUAL_COMMIT_REBUILDS_PREVIEW");}
    [[nodiscard]] GainViewModel* gain_view_model() const noexcept override {return nullptr;}
    [[nodiscard]] EqViewModel* eq_view_model() const noexcept override {return nullptr;}
    [[nodiscard]] CompressorViewModel* compressor_view_model() const noexcept override {return nullptr;}
    [[nodiscard]] QString family_accent() const override {return QStringLiteral("#4E86C6");}
    [[nodiscard]] QString state_text() const override;
    [[nodiscard]] bool has_error() const noexcept override;
    [[nodiscard]] QVariantMap linked_parameters() const override;
    [[nodiscard]] QVariantList linkedBandResponse(int index) const override;
    Q_INVOKABLE bool editBandNumber(const QString& id,const QString& field,double value);
    Q_INVOKABLE bool editBandText(const QString& id,const QString& field,const QString& value);
    Q_INVOKABLE bool editBandFilter(const QString& id,const QString& filter);
    Q_INVOKABLE bool editBandRouting(const QString& id,const QString& routing);
    Q_INVOKABLE bool editBandEnabled(const QString& id,bool value);
    Q_INVOKABLE bool editBandGraph(const QString& id,double hz,double gain);
    Q_INVOKABLE bool appendBand();
    Q_INVOKABLE bool deleteBand(const QString& id);
    Q_INVOKABLE void clearValidation();
    void setBypass(bool value) override;
    void undo() override {
        if(preview_&&preview_->private_manual_transfer_preview_ticket()) {
            if(!id_)return;
            const auto result=preview_->change_private_transferred_edit_history(*id_,false);
            if(!result)operationError_=QString::fromStdString(result.error()->message());
            else operationError_.clear();
            emit changed();return;
        }
        move_history(undo_,redo_);
    }
    void redo() override {
        if(preview_&&preview_->private_manual_transfer_preview_ticket()) {
            if(!id_)return;
            const auto result=preview_->change_private_transferred_edit_history(*id_,true);
            if(!result)operationError_=QString::fromStdString(result.error()->message());
            else operationError_.clear();
            emit changed();return;
        }
        move_history(redo_,undo_);
    }
    void resetToDefault() override;
private:
    using Params=dsp::DynamicEqParameters;
    using Result=core::Result<Params>;
    [[nodiscard]] const Params* params() const noexcept;
    [[nodiscard]] bool history_current() const noexcept;
    [[nodiscard]] std::shared_ptr<const render::RenderResult> prepared() const;
    bool reject(const QString& field,const QString& message);
    bool accept(Result result,const QString& field);
    bool write(const Params& p);
    void refresh();
    void publish();
    void move_history(std::vector<Params>& from,std::vector<Params>& to);
    MasteringChainState* state_{};
    MasteringPreviewController* preview_{};
    QString context_;
    std::optional<dsp::ModuleInstanceId> id_;
    std::optional<Params> observed_;
    std::vector<Params> undo_,redo_;
    QString validationField_,validationMessage_,operationError_;
};

class StreamingTruePeakPreviewModuleAdapter final : public DspModuleAdapter {
    Q_OBJECT
    Q_PROPERTY(double ceilingDbTp READ ceiling_db_tp WRITE setCeilingDbTp NOTIFY changed)
    Q_PROPERTY(double driveDb READ drive_db WRITE setDriveDb NOTIFY changed)
    Q_PROPERTY(int marginPercent READ margin_percent WRITE setMarginPercent NOTIFY changed)
    Q_PROPERTY(QString ceilingValidationMessage READ ceiling_validation_message NOTIFY changed)
    Q_PROPERTY(QString driveValidationMessage READ drive_validation_message NOTIFY changed)
public:
    StreamingTruePeakPreviewModuleAdapter(
        MasteringChainState* state, MasteringPreviewController* preview,
        QString context, QObject* parent=nullptr)
        : DspModuleAdapter(parent), state_(state), preview_(preview),
          context_(std::move(context)) {
        if(preview_)connect(preview_, &MasteringPreviewController::changed,
                            this, &DspModuleAdapter::changed);
    }
    [[nodiscard]] QString instance_id() const override {
        return state_ && state_->streaming_p2_preview_instance_id()
            ? QString::fromStdString(
                state_->streaming_p2_preview_instance_id()->to_string()) : QString{};
    }
    [[nodiscard]] QString type_id() const override {
        return QStringLiteral("rgsml.dsp.streaming-true-peak-limiter");
    }
    [[nodiscard]] QString display_name() const override {
        return QStringLiteral("Streaming TP Limiter");
    }
    [[nodiscard]] QString workspace_context_label() const override {return context_;}
    [[nodiscard]] QString configuration_state() const override {
        if(!params())return QStringLiteral("Default");
        const auto defaultParams=dsp::StreamingTruePeakLimiterParameters::create_default();
        return defaultParams && *defaultParams.value()==*params()
            ?QStringLiteral("Default"):QStringLiteral("Manual");
    }
    [[nodiscard]] bool bypass_supported() const noexcept override {return true;}
    [[nodiscard]] bool bypass() const noexcept override {
        if(!state_ || !state_->streaming_p2_preview_instance_id()) return true;
        const auto state=state_->is_bypassed(*state_->streaming_p2_preview_instance_id());
        return !state || *state.value();
    }
    [[nodiscard]] bool preview_supported() const noexcept override {return preview_!=nullptr;}
    [[nodiscard]] QString preview_status() const override {
        return preview_ ? preview_->preview_status():QStringLiteral("IDLE");
    }
    [[nodiscard]] QString preview_error() const override {
        return !localError_.isEmpty() ? localError_
            :(preview_?preview_->preview_error():QString{});
    }
    [[nodiscard]] bool history_supported() const noexcept override {return false;}
    [[nodiscard]] bool can_undo() const noexcept override {return false;}
    [[nodiscard]] bool can_redo() const noexcept override {return false;}
    [[nodiscard]] bool reset_supported() const noexcept override {return true;}
    [[nodiscard]] QString editor_content_key() const override {
        return QStringLiteral("STREAMING_TRUE_PEAK_LIMITER");
    }
    [[nodiscard]] QString live_change_policy() const override {
        return QStringLiteral("RESEARCH_PREPARED_PCM_REBUILD_NO_WASAPI_CERTIFICATION");
    }
    [[nodiscard]] GainViewModel* gain_view_model() const noexcept override {return nullptr;}
    [[nodiscard]] EqViewModel* eq_view_model() const noexcept override {return nullptr;}
    [[nodiscard]] CompressorViewModel* compressor_view_model() const noexcept override {return nullptr;}
    [[nodiscard]] QString family_accent() const override {return QStringLiteral("#C4774A");}
    [[nodiscard]] QString state_text() const override {
        if(!state_ || !state_->streaming_p2_preview_instance_id())
            return QStringLiteral("Not installed");
        if(bypass()) return QStringLiteral("Bypass · Preview");
        if(!localError_.isEmpty() || preview_status()==QStringLiteral("ERROR"))
            return QStringLiteral("Error · No new audio");
        // READY proves an immutable P2 render was published, not that
        // the transport is currently playing its realization. Never claim
        // ACTIVE based on a renderer status alone.
        if(preview_status()==QStringLiteral("READY"))
            return QStringLiteral("READY · PROCESSED PCM (select to listen)");
        if(preview_status()==QStringLiteral("RENDERING"))
            return QStringLiteral("Preparing audio");
        return QStringLiteral("ON · Awaiting audio");
    }
    [[nodiscard]] bool has_error() const noexcept override {
        return !localError_.isEmpty()
            || (preview_ && preview_->preview_status()==QStringLiteral("ERROR"));
    }
    [[nodiscard]] double ceiling_db_tp() const noexcept {
        return params()?params()->ceiling_db_tp():-1.0;
    }
    [[nodiscard]] double drive_db() const noexcept {
        return params()?params()->drive_db():0.0;
    }
    [[nodiscard]] int margin_percent() const noexcept {
        return params()?params()->margin_percent():2;
    }
    [[nodiscard]] QString ceiling_validation_message() const { return ceilingValidationMessage_; }
    [[nodiscard]] QString drive_validation_message() const { return driveValidationMessage_; }
    Q_INVOKABLE void reportNumericValidation(const QString& field,const QString& message) {
        QString* target=nullptr;
        if(field==QStringLiteral("Ceiling"))target=&ceilingValidationMessage_;
        else if(field==QStringLiteral("Drive"))target=&driveValidationMessage_;
        if(!target||*target==message)return;
        *target=message;
        emit changed();
    }
    Q_INVOKABLE void setCeilingDbTp(double value) {
        commit(value,drive_db(),margin_percent());
    }
    Q_INVOKABLE void setDriveDb(double value) {
        commit(ceiling_db_tp(),value,margin_percent());
    }
    Q_INVOKABLE void setMarginPercent(int value) {
        commit(ceiling_db_tp(),drive_db(),value);
    }
    void setBypass(bool value) override {
        if(!state_||!state_->streaming_p2_preview_instance_id())return;
        // A duplicate QML command is NOT a new DSP epoch: otherwise one
        // unchanged bypass click can start a full multi-second P2 render.
        auto current = state_->is_bypassed(
            *state_->streaming_p2_preview_instance_id());
        if(current && *current.value()==value) {
            if(!localError_.isEmpty()) {
                localError_.clear();
                emit changed();
            }
            return;
        }
        auto status=state_->set_user_bypass(
            *state_->streaming_p2_preview_instance_id(),value);
        if(!status) localError_=QString::fromStdString(status.error()->message());
        else {
            localError_.clear();
            if(preview_)preview_->request_preview();
        }
        emit changed();
    }
    void undo() override {}
    void redo() override {}
    void resetToDefault() override {
        ceilingValidationMessage_.clear();driveValidationMessage_.clear();
        commit(-1.0,0.0,2);emit changed();
    }
private:
    [[nodiscard]] const dsp::StreamingTruePeakLimiterParameters* params() const noexcept {
        return state_&&state_->streaming_p2_preview_parameters()
            ?&*state_->streaming_p2_preview_parameters():nullptr;
    }
    void commit(double ceiling,double drive,int margin) {
        if(!state_)return;
        auto p=dsp::StreamingTruePeakLimiterParameters::create(ceiling,drive,margin);
        if(!p) localError_=QString::fromStdString(p.error()->message());
        else {
            // Normalize through the typed parameter factory FIRST (including
            // signed zero), then compare the complete accepted triple.
            // Identical Ceiling/Drive/Margin must not invalidate M18,
            // enqueue a full-chain preview, or replace audible PCM.
            if(params() && *params()==*p.value()) {
                if(!localError_.isEmpty()) {
                    localError_.clear();
                    emit changed();
                }
                return;
            }
            auto status=state_->set_streaming_p2_preview_parameters(*p.value());
            if(!status) localError_=QString::fromStdString(status.error()->message());
            else {
                localError_.clear();
                if(preview_)preview_->request_preview();
            }
        }
        emit changed();
    }
    MasteringChainState* state_{nullptr};
    MasteringPreviewController* preview_{nullptr};
    QString context_;
    QString localError_;
    QString ceilingValidationMessage_;
    QString driveValidationMessage_;
};

// Genuine product M17 terminal INSTANCE. Presentation is BYP/read-only
// until a verified output-rate realization becomes the processed source.
class TruePeakLimiterModuleAdapter final : public DspModuleAdapter {
    Q_OBJECT
public:
    TruePeakLimiterModuleAdapter(TerminalLimiterState* state,
        TerminalLimiterViewModel* vm, QString context, QObject* parent=nullptr)
      : DspModuleAdapter(parent),state_(state),vm_(vm),context_(std::move(context))
    {
        if(vm_)connect(vm_,&TerminalLimiterViewModel::changed,
                       this,&DspModuleAdapter::changed);
    }
    [[nodiscard]] QString instance_id() const override
    { return state_ ? QString::fromStdString(state_->instance_id().to_string()) : QString{}; }
    [[nodiscard]] QString type_id() const override
    { return QStringLiteral("rgsml.dsp.true-peak-limiter"); }
    [[nodiscard]] QString display_name() const override
    { return QStringLiteral("Export TP Limiter"); }
    [[nodiscard]] QString workspace_context_label() const override { return context_; }
    [[nodiscard]] QString configuration_state() const override
    { return state_ && state_->parameters().ceiling_db_tp()!=0.0
        ? QStringLiteral("Manual") : QStringLiteral("Default"); }
    [[nodiscard]] bool bypass_supported() const noexcept override { return vm_!=nullptr; }
    [[nodiscard]] bool bypass() const noexcept override { return !state_||state_->bypassed(); }
    [[nodiscard]] bool preview_supported() const noexcept override { return false; }
    [[nodiscard]] QString preview_status() const override
    { return QStringLiteral("OUTPUT_RATE_AUDITION_UNQUALIFIED"); }
    [[nodiscard]] QString preview_error() const override { return {}; }
    [[nodiscard]] bool history_supported() const noexcept override { return false; }
    [[nodiscard]] bool can_undo() const noexcept override { return false; }
    [[nodiscard]] bool can_redo() const noexcept override { return false; }
    [[nodiscard]] bool reset_supported() const noexcept override { return vm_!=nullptr; }
    [[nodiscard]] QString editor_content_key() const override
    { return QStringLiteral("TRUE_PEAK_LIMITER"); }
    [[nodiscard]] QString live_change_policy() const override
    { return QStringLiteral("OFFLINE_TERMINAL_RENDER_ON_ACCEPTED_CHANGE"); }
    [[nodiscard]] GainViewModel* gain_view_model() const noexcept override { return nullptr; }
    [[nodiscard]] EqViewModel* eq_view_model() const noexcept override { return nullptr; }
    [[nodiscard]] CompressorViewModel* compressor_view_model() const noexcept override { return nullptr; }
    [[nodiscard]] QString family_accent() const override
    { return bypass() ? QStringLiteral("#6CBCCF") : QStringLiteral("#C4774A"); }
    [[nodiscard]] QString state_text() const override
    { return vm_ ? vm_->audio_status()
        : QStringLiteral("UNAVAILABLE · NO AUDIO OWNER"); }
    [[nodiscard]] bool has_error() const noexcept override
    { return vm_ && !vm_->validation_message().isEmpty(); }
    void setBypass(bool value) override { if(vm_)vm_->setBypass(value); }
    void undo() override {}
    void redo() override {}
    void resetToDefault() override { if(vm_)vm_->resetToDefault(); }
private:
    TerminalLimiterState* state_{nullptr};
    TerminalLimiterViewModel* vm_{nullptr};
    QString context_;
};

// A presentation-only observer in the DSP Chain selector. It never enters
// MasteringChainState, ProcessingChain or ModuleRegistry; no sonic bypass exists.
class TruePeakAnalysisModuleAdapter final : public DspModuleAdapter {
    Q_OBJECT
public:
    explicit TruePeakAnalysisModuleAdapter(QString workflowContext, QObject* parent = nullptr)
        : DspModuleAdapter(parent), workflowContext_(workflowContext) {}
    [[nodiscard]] QString instance_id() const override
        { return QStringLiteral("analysis:rgsml.true-peak-meter"); }
    [[nodiscard]] QString type_id() const override
        { return QStringLiteral("rgsml.analysis.true-peak-meter"); }
    [[nodiscard]] QString display_name() const override
        { return QStringLiteral("TP Meter"); }
    [[nodiscard]] QString workspace_context_label() const override
        { return workflowContext_; }
    [[nodiscard]] QString configuration_state() const override
        { return QStringLiteral("Default"); }
    [[nodiscard]] bool bypass_supported() const noexcept override { return false; }
    [[nodiscard]] bool non_processing_service() const noexcept override { return true; }
    [[nodiscard]] bool bypass() const noexcept override { return false; }
    [[nodiscard]] bool preview_supported() const noexcept override { return false; }
    [[nodiscard]] QString preview_status() const override
        { return QStringLiteral("NO_PREVIEW"); }
    [[nodiscard]] QString preview_error() const override { return {}; }
    [[nodiscard]] bool history_supported() const noexcept override { return false; }
    [[nodiscard]] bool can_undo() const noexcept override { return false; }
    [[nodiscard]] bool can_redo() const noexcept override { return false; }
    [[nodiscard]] bool reset_supported() const noexcept override { return false; }
    [[nodiscard]] QString editor_content_key() const override
        { return QStringLiteral("TRUE_PEAK_METER"); }
    [[nodiscard]] QString live_change_policy() const override
        { return QStringLiteral("OBSERVATION_ONLY"); }
    [[nodiscard]] GainViewModel* gain_view_model() const noexcept override { return nullptr; }
    [[nodiscard]] EqViewModel* eq_view_model() const noexcept override { return nullptr; }
    [[nodiscard]] CompressorViewModel* compressor_view_model() const noexcept override { return nullptr; }
    [[nodiscard]] QString family_accent() const override
        { return QStringLiteral("#6CBCCF"); }
    [[nodiscard]] QString state_text() const override
        { return QStringLiteral("Read-only · TP"); }
    [[nodiscard]] bool has_error() const noexcept override { return false; }
    void setBypass(bool) override {} // The observer cannot affect audio.
    void undo() override {}
    void redo() override {}
    void resetToDefault() override {}
private:
    QString workflowContext_;
};

// Product analysis navigation only. These rows live in the visual chain
// between M17 and MONITOR, never in ProcessingChain or Project serialization.
// They do not publish fictional measurements: actual numerical authority
// remains with the canonical M19/M20/M21 Product/Phase-6 owner.
class ProductAnalysisModuleAdapter final : public DspModuleAdapter {
public:
    ProductAnalysisModuleAdapter(int milestone,QObject* statusVm,
        QString context,QObject* parent=nullptr)
      :DspModuleAdapter(parent),milestone_(milestone),
       statusVm_(statusVm),context_(std::move(context))
    {
        if(statusVm_)QObject::connect(statusVm_,SIGNAL(changed()),
            this,SIGNAL(changed()));
    }
    [[nodiscard]] QString instance_id() const override
    {return QStringLiteral("analysis:rgsml.m%1").arg(milestone_);}
    [[nodiscard]] QString type_id() const override
    {return QStringLiteral("rgsml.analysis.m%1").arg(milestone_);}
    // Product-visible names are stable functional labels. M19/M20/M21
    // remain ONLY the internal routing, serialized-free service identities.
    [[nodiscard]] QString display_name() const override {
        switch(milestone_) {
        case 19:return QStringLiteral("Levels");
        case 20:return QStringLiteral("Spectrum / Stereo");
        default:return QStringLiteral("Metrics / Delta");
        }
    }
    [[nodiscard]] QString workspace_context_label() const override
    {return context_+QStringLiteral(" · Read-only analysis");}
    [[nodiscard]] QString configuration_state() const override
    {return QStringLiteral("Read-only");}
    [[nodiscard]] bool bypass_supported() const noexcept override{return false;}
    [[nodiscard]] bool non_processing_service() const noexcept override{return true;}
    [[nodiscard]] bool bypass() const noexcept override{return false;}
    [[nodiscard]] bool preview_supported() const noexcept override{return false;}
    [[nodiscard]] QString preview_status() const override
    {return QStringLiteral("NO_PREVIEW");}
    [[nodiscard]] QString preview_error() const override{return {};}
    [[nodiscard]] bool history_supported() const noexcept override{return false;}
    [[nodiscard]] bool can_undo() const noexcept override{return false;}
    [[nodiscard]] bool can_redo() const noexcept override{return false;}
    [[nodiscard]] bool reset_supported() const noexcept override{return false;}
    [[nodiscard]] QString editor_content_key() const override
    {return QStringLiteral("M%1_ANALYSIS").arg(milestone_);}
    [[nodiscard]] QString live_change_policy() const override
    {return QStringLiteral("ANALYSIS_READ_ONLY_NO_DSP_MUTATION");}
    [[nodiscard]] GainViewModel* gain_view_model() const noexcept override{return nullptr;}
    [[nodiscard]] EqViewModel* eq_view_model() const noexcept override{return nullptr;}
    [[nodiscard]] CompressorViewModel* compressor_view_model() const noexcept override{return nullptr;}
    [[nodiscard]] QString family_accent() const override
    {return QStringLiteral("#6CBCCF");}
    [[nodiscard]] QString state_text() const override {
        // An always-on read-only analyzer has no configuration/Bypass
        // state. Its values are presented independently with genuine
        // CURRENT/STALE/MISSING source data, not a release-gate badge.
        return statusVm_ ? QStringLiteral("Read-only analysis")
                         : QStringLiteral("No analysis source");
    }
    [[nodiscard]] bool has_error() const noexcept override{return false;}
    void setBypass(bool) override {}
    void undo() override {}
    void redo() override {}
    void resetToDefault() override {}
private:
    int milestone_{19};
    QObject* statusVm_{nullptr};
    QString context_;
};

// MONITOR is a selectable workflow/service row, NOT a sonic DSP module.
// This adapter exists only at presentation level; it is never installed
// in MasteringChainState, ModuleRegistry or a render ProcessingChain.
class MonitorServiceModuleAdapter final : public DspModuleAdapter {
public:
    explicit MonitorServiceModuleAdapter(QObject* vm,
        QString context, QObject* parent=nullptr)
      : DspModuleAdapter(parent),vm_(vm),context_(std::move(context))
    {
        if(vm_) QObject::connect(vm_, SIGNAL(changed()),
                                this, SIGNAL(changed()));
    }
    [[nodiscard]] QString instance_id() const override
    { return QStringLiteral("utility:rgsml.monitor.master-device"); }
    [[nodiscard]] QString type_id() const override
    { return QStringLiteral("rgsml.utility.master-monitor"); }
    [[nodiscard]] QString display_name() const override
    { return QStringLiteral("Master Audition"); }
    [[nodiscard]] QString workspace_context_label() const override
    { return context_ + QStringLiteral(" · Device audition only"); }
    [[nodiscard]] QString configuration_state() const override
    { return QStringLiteral("Default"); }
    [[nodiscard]] bool bypass_supported() const noexcept override { return false; }
    [[nodiscard]] bool non_processing_service() const noexcept override { return true; }
    [[nodiscard]] bool bypass() const noexcept override { return false; }
    [[nodiscard]] bool preview_supported() const noexcept override { return false; }
    [[nodiscard]] QString preview_status() const override
    { return QStringLiteral("MONITOR_SERVICE"); }
    [[nodiscard]] QString preview_error() const override
    { return vm_ ? vm_->property("errorText").toString() : QString{}; }
    [[nodiscard]] bool history_supported() const noexcept override { return false; }
    [[nodiscard]] bool can_undo() const noexcept override { return false; }
    [[nodiscard]] bool can_redo() const noexcept override { return false; }
    [[nodiscard]] bool reset_supported() const noexcept override { return false; }
    [[nodiscard]] QString editor_content_key() const override
    { return QStringLiteral("MASTER_MONITOR"); }
    [[nodiscard]] QString live_change_policy() const override
    { return QStringLiteral("UTILITY_DEVICE_AUDITION_NO_MASTER_MODIFICATION"); }
    [[nodiscard]] GainViewModel* gain_view_model() const noexcept override { return nullptr; }
    [[nodiscard]] EqViewModel* eq_view_model() const noexcept override { return nullptr; }
    [[nodiscard]] CompressorViewModel* compressor_view_model() const noexcept override { return nullptr; }
    [[nodiscard]] QString family_accent() const override
    { return QStringLiteral("#718092"); }
    [[nodiscard]] QString state_text() const override
    {
        if(!vm_)return QStringLiteral("Unavailable");
        if(vm_->property("monitorInstalled").toBool())
            return QStringLiteral("Device audition");
        if(vm_->property("canInstall").toBool())
            return QStringLiteral("MASTER ready");
        if(!vm_->property("errorText").toString().isEmpty())
            return QStringLiteral("Audition error");
        return QStringLiteral("Audition idle");
    }
    [[nodiscard]] bool has_error() const noexcept override
    { return vm_ && !vm_->property("errorText").toString().isEmpty(); }
    void setBypass(bool) override {}
    void undo() override {}
    void redo() override {}
    void resetToDefault() override {}
private:
    QObject* vm_{nullptr};
    QString context_;
};

}  // namespace rgsml::app
