#pragma once

#include "compressor_view_model.hpp"
#include "eq_view_model.hpp"
#include "gain_view_model.hpp"
#include "mastering_chain_state.hpp"
#include "stereo_ms_view_model.hpp"
#include "terminal_limiter_state.hpp"
#include "terminal_limiter_view_model.hpp"

#include <QObject>
#include <QString>

#include <memory>
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

public:
    explicit DspModuleAdapter(QObject* parent = nullptr);
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

    Q_INVOKABLE virtual void setBypass(bool bypass) = 0;
    Q_INVOKABLE virtual void undo() = 0;
    Q_INVOKABLE virtual void redo() = 0;
    Q_INVOKABLE virtual void resetToDefault() = 0;

signals:
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
    { return QStringLiteral("True Peak Limiter"); }
    [[nodiscard]] QString workspace_context_label() const override { return context_; }
    [[nodiscard]] QString configuration_state() const override
    { return state_ && state_->parameters().ceiling_db_tp()!=-1.0
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
        { return QStringLiteral("True Peak Meter"); }
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
        { return QStringLiteral("Read-only · True Peak"); }
    [[nodiscard]] bool has_error() const noexcept override { return false; }
    void setBypass(bool) override {} // The observer cannot affect audio.
    void undo() override {}
    void redo() override {}
    void resetToDefault() override {}
private:
    QString workflowContext_;
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
