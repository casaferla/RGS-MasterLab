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
    // Authoritative module chain order:
    // 0: Input Gain
    // 1: Parametric EQ
    auto gainAdapter = std::make_unique<InputGainModuleAdapter>(gainViewModel_, chainState_);
    connect(gainAdapter.get(), &DspModuleAdapter::changed, this, &DspChainAdapterModel::changed);
    moduleAdapters_.push_back(std::move(gainAdapter));

    auto eqAdapter = std::make_unique<ParametricEqModuleAdapter>(eqViewModel_, chainState_);
    connect(eqAdapter.get(), &DspModuleAdapter::changed, this, &DspChainAdapterModel::changed);
    moduleAdapters_.push_back(std::move(eqAdapter));
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

int DspChainAdapterModel::selected_index() const noexcept
{
    return selectedIndex_;
}

DspModuleAdapter* DspChainAdapterModel::selected_module() const noexcept
{
    if (selectedIndex_ >= 0 && static_cast<std::size_t>(selectedIndex_) < moduleAdapters_.size()) {
        return moduleAdapters_[static_cast<std::size_t>(selectedIndex_)].get();
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
        if (selectedIndex_ != index) {
            selectedIndex_ = index;
            emit changed();
        }
    }
}

void DspChainAdapterModel::selectModuleByInstanceId(const QString& instanceId)
{
    for (std::size_t idx = 0; idx < moduleAdapters_.size(); ++idx) {
        if (moduleAdapters_[idx]->instance_id() == instanceId) {
            setSelectedIndex(static_cast<int>(idx));
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
