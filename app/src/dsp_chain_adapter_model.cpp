#include "dsp_chain_adapter_model.hpp"

#include <QVariant>

namespace rgsml::app {

DspChainAdapterModel::DspChainAdapterModel(
    GainViewModel* gainViewModel,
    EqViewModel* eqViewModel,
    MasteringChainState* chainState,
    QObject* parent)
    : QObject(parent)
    , gainViewModel_(gainViewModel)
    , eqViewModel_(eqViewModel)
    , chainState_(chainState)
{
    rebuild_adapters_from_authority();
}

void DspChainAdapterModel::rebuild_adapters_from_authority()
{
    moduleAdapters_.clear();

    if (chainState_ != nullptr) {
        for (const auto& instance : chainState_->instances()) {
            const auto typeId = instance.module_type_id();
            if (typeId == "rgsml.dsp.gain") {
                auto gainAdapter = std::make_unique<InputGainModuleAdapter>(gainViewModel_, chainState_);
                connect(gainAdapter.get(), &DspModuleAdapter::changed, this, &DspChainAdapterModel::changed);
                moduleAdapters_.push_back(std::move(gainAdapter));
            } else if (typeId == "rgsml.dsp.parametric-eq") {
                auto eqAdapter = std::make_unique<ParametricEqModuleAdapter>(eqViewModel_, chainState_);
                connect(eqAdapter.get(), &DspModuleAdapter::changed, this, &DspChainAdapterModel::changed);
                moduleAdapters_.push_back(std::move(eqAdapter));
            }
        }
    } else {
        // Fallback default topology when no chainState provided
        auto gainAdapter = std::make_unique<InputGainModuleAdapter>(gainViewModel_, chainState_);
        connect(gainAdapter.get(), &DspModuleAdapter::changed, this, &DspChainAdapterModel::changed);
        moduleAdapters_.push_back(std::move(gainAdapter));

        auto eqAdapter = std::make_unique<ParametricEqModuleAdapter>(eqViewModel_, chainState_);
        connect(eqAdapter.get(), &DspModuleAdapter::changed, this, &DspChainAdapterModel::changed);
        moduleAdapters_.push_back(std::move(eqAdapter));
    }

    // Default selection to first module if valid and no selection currently set
    if (selectedInstanceId_.isEmpty() && !moduleAdapters_.empty()) {
        selectedInstanceId_ = moduleAdapters_.front()->instance_id();
    }
}

QVariantList DspChainAdapterModel::modules() const
{
    QVariantList list;
    list.reserve(static_cast<qsizetype>(moduleAdapters_.size()));
    for (const auto& adapter : moduleAdapters_) {
        list.append(QVariant::fromValue(adapter.get()));
    }
    return list;
}

QString DspChainAdapterModel::selected_instance_id() const
{
    return selectedInstanceId_;
}

int DspChainAdapterModel::selected_index() const noexcept
{
    for (std::size_t idx = 0; idx < moduleAdapters_.size(); ++idx) {
        if (moduleAdapters_[idx]->instance_id() == selectedInstanceId_) {
            return static_cast<int>(idx);
        }
    }
    return 0;
}

DspModuleAdapter* DspChainAdapterModel::selected_module() const noexcept
{
    for (const auto& adapter : moduleAdapters_) {
        if (adapter->instance_id() == selectedInstanceId_) {
            return adapter.get();
        }
    }
    if (!moduleAdapters_.empty()) {
        return moduleAdapters_.front().get();
    }
    return nullptr;
}

DspModuleAdapter* DspChainAdapterModel::active_module() const noexcept
{
    return selected_module();
}

void DspChainAdapterModel::setSelectedIndex(int index)
{
    if (index >= 0 && static_cast<std::size_t>(index) < moduleAdapters_.size()) {
        const QString newId = moduleAdapters_[static_cast<std::size_t>(index)]->instance_id();
        selectModuleByInstanceId(newId);
    }
}

void DspChainAdapterModel::selectModuleByInstanceId(const QString& instanceId)
{
    if (instanceId.isEmpty()) {
        return;
    }
    for (const auto& adapter : moduleAdapters_) {
        if (adapter->instance_id() == instanceId) {
            if (selectedInstanceId_ != instanceId) {
                selectedInstanceId_ = instanceId;
                emit changed();
            }
            return;
        }
    }
}

void DspChainAdapterModel::refreshFromAuthority()
{
    if (gainViewModel_ != nullptr) {
        gainViewModel_->refreshFromAuthority();
    }
    if (eqViewModel_ != nullptr) {
        eqViewModel_->refreshFromAuthority();
    }
    rebuild_adapters_from_authority();
    emit changed();
}

void DspChainAdapterModel::resetForNewSource()
{
    if (gainViewModel_ != nullptr) {
        gainViewModel_->resetForNewSource();
    }
    if (eqViewModel_ != nullptr) {
        eqViewModel_->resetForNewSource();
    }
    emit changed();
}

}  // namespace rgsml::app
