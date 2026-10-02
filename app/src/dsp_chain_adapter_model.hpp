#pragma once

#include "dsp_module_adapter.hpp"
#include "gain_view_model.hpp"
#include "eq_view_model.hpp"
#include "mastering_chain_state.hpp"

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

public:
    explicit DspChainAdapterModel(
        GainViewModel* gainViewModel = nullptr,
        EqViewModel* eqViewModel = nullptr,
        MasteringChainState* chainState = nullptr,
        QObject* parent = nullptr);
    ~DspChainAdapterModel() override = default;

    [[nodiscard]] QVariantList modules() const;
    [[nodiscard]] QString selected_instance_id() const;
    [[nodiscard]] int selected_index() const noexcept;
    [[nodiscard]] DspModuleAdapter* selected_module() const noexcept;
    [[nodiscard]] DspModuleAdapter* active_module() const noexcept;

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
    MasteringChainState* chainState_{nullptr};

    std::vector<std::unique_ptr<DspModuleAdapter>> moduleAdapters_;
    QString selectedInstanceId_;
    QString selectedTypeId_;
};

}  // namespace rgsml::app
