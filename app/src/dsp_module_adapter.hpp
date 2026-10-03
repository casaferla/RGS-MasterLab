#pragma once

#include "gain_view_model.hpp"
#include "eq_view_model.hpp"
#include "mastering_chain_state.hpp"

#include <QObject>
#include <QString>

#include <memory>

namespace rgsml::app {

class DspModuleAdapter : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString instanceId READ instance_id NOTIFY changed)
    Q_PROPERTY(QString typeId READ type_id NOTIFY changed)
    Q_PROPERTY(QString displayName READ display_name NOTIFY changed)
    Q_PROPERTY(QString workspaceContextLabel READ workspace_context_label NOTIFY changed)
    Q_PROPERTY(QString configurationState READ configuration_state NOTIFY changed)
    Q_PROPERTY(bool bypassSupported READ bypass_supported NOTIFY changed)
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

}  // namespace rgsml::app
