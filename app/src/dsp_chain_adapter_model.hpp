#pragma once

#include "compressor_view_model.hpp"
#include "dsp_module_adapter.hpp"
#include "eq_view_model.hpp"
#include "gain_view_model.hpp"
#include "mastering_chain_state.hpp"
#include "stereo_ms_view_model.hpp"

#include <QObject>
#include <QString>
#include <QVariantList>

#include <memory>
#include <vector>

namespace rgsml::app {

class DspChainAdapterModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList modules READ modules NOTIFY changed)
    Q_PROPERTY(QString selectedInstanceId READ selected_instance_id WRITE selectModuleByInstanceId NOTIFY changed)
    Q_PROPERTY(int selectedIndex READ selected_index WRITE setSelectedIndex NOTIFY changed)
    Q_PROPERTY(DspModuleAdapter* selectedModule READ selected_module NOTIFY changed)
    Q_PROPERTY(DspModuleAdapter* activeModule READ active_module NOTIFY changed)
    Q_PROPERTY(QString workflowContext READ workflow_context NOTIFY changed)

public:
    explicit DspChainAdapterModel(
        GainViewModel* gainViewModel = nullptr,
        EqViewModel* eqViewModel = nullptr,
        CompressorViewModel* compressorViewModel = nullptr,
        MasteringChainState* chainState = nullptr,
        QString workflowContext = QStringLiteral("Mastering"),
        QObject* parent = nullptr,
        StereoMsViewModel* stereoMsViewModel = nullptr);

    explicit DspChainAdapterModel(
        GainViewModel* gainViewModel,
        EqViewModel* eqViewModel,
        MasteringChainState* chainState,
        QString workflowContext = QStringLiteral("Mastering"),
        QObject* parent = nullptr)
        : DspChainAdapterModel(gainViewModel, eqViewModel, nullptr, chainState, std::move(workflowContext), parent) {}
    ~DspChainAdapterModel() override = default;

    [[nodiscard]] QVariantList modules() const;
    [[nodiscard]] QString selected_instance_id() const;
    [[nodiscard]] int selected_index() const noexcept;
    [[nodiscard]] DspModuleAdapter* selected_module() const noexcept;
    [[nodiscard]] DspModuleAdapter* active_module() const noexcept;
    [[nodiscard]] QString workflow_context() const { return workflowContext_; }

    Q_INVOKABLE void setSelectedIndex(int index);
    Q_INVOKABLE void selectModuleByInstanceId(const QString& instanceId);

    void refreshFromAuthority();
    void resetForNewSource();

signals:
    void changed();

private:
    void rebuild_adapters_from_authority();

    GainViewModel* gainViewModel_{nullptr};
    EqViewModel* eqViewModel_{nullptr};
    CompressorViewModel* compressorViewModel_{nullptr};
    StereoMsViewModel* stereoMsViewModel_{nullptr};
    MasteringChainState* chainState_{nullptr};
    QString workflowContext_;

    std::vector<std::unique_ptr<DspModuleAdapter>> moduleAdapters_;
    QString selectedInstanceId_;
    QString selectedTypeId_;
};

}  // namespace rgsml::app
