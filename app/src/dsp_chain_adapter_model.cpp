#include "dsp_chain_adapter_model.hpp"
#include "internal/l3_linked_dsp_module_adapter.hpp"

#include <QVariant>

namespace rgsml::app {

DspChainAdapterModel::DspChainAdapterModel(
    GainViewModel* gainViewModel,
    EqViewModel* eqViewModel,
    CompressorViewModel* compressorViewModel,
    MasteringChainState* chainState,
    QString workflowContext,
    QObject* parent,
    StereoMsViewModel* stereoMsViewModel,
    bool includeTruePeakAnalysis,
    TerminalLimiterState* terminalLimiterState,
    TerminalLimiterViewModel* terminalLimiterViewModel,
    QObject* terminalMonitorViewModel,
    MasteringPreviewController* p2PreviewController)
    : QObject(parent)
    , gainViewModel_(gainViewModel)
    , eqViewModel_(eqViewModel)
    , compressorViewModel_(compressorViewModel)
    , stereoMsViewModel_(stereoMsViewModel)
    , p2PreviewController_(p2PreviewController)
    , includeTruePeakAnalysis_(includeTruePeakAnalysis)
    , terminalLimiterState_(terminalLimiterState)
    , terminalLimiterViewModel_(terminalLimiterViewModel)
    , terminalMonitorViewModel_(terminalMonitorViewModel)
    , chainState_(chainState)
    , workflowContext_(std::move(workflowContext))
{
    rebuild_adapters_from_authority();
}

void DspChainAdapterModel::rebuild_adapters_from_authority()
{
    moduleAdapters_.clear();

    lastLinkedEditorOwner_.reset();
    lastLinkedEditorIds_.clear();
    const auto linked=(linkedEditorPreviewController_&&chainState_)
        ?l3_automatch::inspect_current_linked_editors(
            *linkedEditorPreviewController_,*chainState_)
        :std::nullopt;
    bool linkedAppended=false;
    const auto appendLinked=[&] {
        if(linkedAppended)return;
        linkedAppended=true;
        if(!linked||linked->empty())return;
        lastLinkedEditorOwner_=linked->front().exact_owner;
        for(const auto& snapshot:*linked) {
            auto adapter=std::make_unique<l3_automatch::LinkedDspModuleAdapter>(
                snapshot,*linkedEditorPreviewController_,*chainState_,workflowContext_);
            lastLinkedEditorIds_.append(adapter->instance_id());
            connect(adapter.get(),&DspModuleAdapter::changed,
                this,&DspChainAdapterModel::changed);
            moduleAdapters_.push_back(std::move(adapter));
        }
    };

    if (chainState_ != nullptr) {
        for (const auto& instance : chainState_->instances()) {
            const auto typeId = instance.module_type_id();
            if (typeId == "rgsml.dsp.gain") {
                auto gainAdapter = std::make_unique<InputGainModuleAdapter>(gainViewModel_, chainState_, workflowContext_);
                connect(gainAdapter.get(), &DspModuleAdapter::changed, this, &DspChainAdapterModel::changed);
                moduleAdapters_.push_back(std::move(gainAdapter));
            } else if (typeId == "rgsml.dsp.parametric-eq") {
                auto eqAdapter = std::make_unique<ParametricEqModuleAdapter>(eqViewModel_, chainState_, workflowContext_);
                connect(eqAdapter.get(), &DspModuleAdapter::changed, this, &DspChainAdapterModel::changed);
                moduleAdapters_.push_back(std::move(eqAdapter));
            } else if (typeId == "rgsml.dsp.compressor") {
                auto compAdapter = std::make_unique<CompressorModuleAdapter>(compressorViewModel_, chainState_, workflowContext_);
                connect(compAdapter.get(), &DspModuleAdapter::changed, this, &DspChainAdapterModel::changed);
                moduleAdapters_.push_back(std::move(compAdapter));
            } else if (typeId == "rgsml.dsp.stereo-ms" && stereoMsViewModel_ != nullptr) {
                // Authority-driven enumeration: never promote a 3-node project.
                auto adapter = std::make_unique<StereoMsModuleAdapter>(
                    stereoMsViewModel_, chainState_, workflowContext_);
                connect(adapter.get(), &DspModuleAdapter::changed,
                        this, &DspChainAdapterModel::changed);
                moduleAdapters_.push_back(std::move(adapter));
            } else if (typeId=="rgsml.dsp.streaming-true-peak-limiter") {
                // Native linked processing precedes the optional terminal P2.
                appendLinked();
                // Real fifth MANUAL instance only; no presentation-only fake.
                auto adapter=std::make_unique<StreamingTruePeakPreviewModuleAdapter>(
                    chainState_,p2PreviewController_,workflowContext_);
                connect(adapter.get(),&DspModuleAdapter::changed,
                        this,&DspChainAdapterModel::changed);
                moduleAdapters_.push_back(std::move(adapter));
            }
        }
    } else {
        // Fallback default topology when no chainState provided
        auto gainAdapter = std::make_unique<InputGainModuleAdapter>(gainViewModel_, chainState_, workflowContext_);
        connect(gainAdapter.get(), &DspModuleAdapter::changed, this, &DspChainAdapterModel::changed);
        moduleAdapters_.push_back(std::move(gainAdapter));

        auto eqAdapter = std::make_unique<ParametricEqModuleAdapter>(eqViewModel_, chainState_, workflowContext_);
        connect(eqAdapter.get(), &DspModuleAdapter::changed, this, &DspChainAdapterModel::changed);
        moduleAdapters_.push_back(std::move(eqAdapter));

        auto compAdapter = std::make_unique<CompressorModuleAdapter>(compressorViewModel_, chainState_, workflowContext_);
        connect(compAdapter.get(), &DspModuleAdapter::changed, this, &DspChainAdapterModel::changed);
        moduleAdapters_.push_back(std::move(compAdapter));
    }

    appendLinked();

    // Analysis-only presentation entry, never a ProcessingChain instance.
    // Opt-in keeps legacy project/standalone tests and DSP topology unchanged.
    if (includeTruePeakAnalysis_) {
        auto analysis = std::make_unique<TruePeakAnalysisModuleAdapter>(workflowContext_);
        connect(analysis.get(), &DspModuleAdapter::changed,
                this, &DspChainAdapterModel::changed);
        moduleAdapters_.push_back(std::move(analysis));
    }

    // Terminal instance is separate from the MANUAL mastering chain.
    if (terminalLimiterState_ && terminalLimiterViewModel_
        && terminalLimiterState_->chain().context().stage == dsp::ProcessingStage::MASTER
        && terminalLimiterState_->chain().context().segment == dsp::ChainSegment::TERMINAL
        && terminalLimiterState_->chain().instances().size() == 1
        && terminalLimiterState_->chain().instances()[0].module_type_id()
            == "rgsml.dsp.true-peak-limiter") {
        auto limiter=std::make_unique<TruePeakLimiterModuleAdapter>(
            terminalLimiterState_,terminalLimiterViewModel_,workflowContext_);
        connect(limiter.get(),&DspModuleAdapter::changed,this,
                &DspChainAdapterModel::changed);
        moduleAdapters_.push_back(std::move(limiter));
    }

    // Visual order: M17, M19 Levels, M20 Tonal/Stereo, M21 Delta,
    // Master Audition. These observers are not ProcessingChain modules.
    if(analysisStatusViewModel_) {
        for(const int milestone:{19,20,21}) {
            auto panel=std::make_unique<ProductAnalysisModuleAdapter>(
                milestone,analysisStatusViewModel_,workflowContext_);
            connect(panel.get(),&DspModuleAdapter::changed,
                    this,&DspChainAdapterModel::changed);
            moduleAdapters_.push_back(std::move(panel));
        }
    }

    // The utility comes after M17 in the VISUAL chain only.
    // No ModuleRegistry/ProcessingChain insert and no audio transform.
    if (terminalMonitorViewModel_) {
        auto monitor=std::make_unique<MonitorServiceModuleAdapter>(
            terminalMonitorViewModel_,workflowContext_);
        connect(monitor.get(),&DspModuleAdapter::changed,
                this,&DspChainAdapterModel::changed);
        moduleAdapters_.push_back(std::move(monitor));
    }

    // Authoritative selection reconciliation following rehydration
    bool selectionFound = false;
    for (const auto& adapter : moduleAdapters_) {
        if (adapter->instance_id() == selectedInstanceId_) {
            selectionFound = true;
            selectedTypeId_ = adapter->type_id();
            break;
        }
    }

    // Never replace a retired REF_LINKED instance with a different one
    // merely because the new plan happens to use the same DSP type.
    const bool selectedWasLinked=selectedTypeId_==QStringLiteral("rgsml.dsp.dynamic-eq")||
        selectedTypeId_==QStringLiteral("rgsml.dsp.transient-shaper");
    if (!selectionFound && !selectedTypeId_.isEmpty()&&!selectedWasLinked) {
        for (const auto& adapter : moduleAdapters_) {
            if (adapter->type_id() == selectedTypeId_) {
                selectedInstanceId_ = adapter->instance_id();
                selectionFound = true;
                break;
            }
        }
    }

    if (!selectionFound && !moduleAdapters_.empty()) {
        selectedInstanceId_ = moduleAdapters_.front()->instance_id();
        selectedTypeId_ = moduleAdapters_.front()->type_id();
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
                selectedTypeId_ = adapter->type_id();
                emit changed();
            }
            return;
        }
    }
}

void DspChainAdapterModel::bindAnalysisStatusViewModel(QObject* statusVm)
{
    if(analysisStatusViewModel_==statusVm)return;
    analysisStatusViewModel_=statusVm;
    rebuild_adapters_from_authority();
    emit changed();
}

void DspChainAdapterModel::bindLinkedEditorPreviewController(MasteringPreviewController* preview)
{
    if(linkedEditorPreviewController_==preview)return;
    disconnect(linkedEditorConnection_);
    linkedEditorPreviewController_=preview;
    if(preview) {
        // A queued rebuild cannot destroy an adapter while its own
        // preview-change notification is still on the stack.
        linkedEditorConnection_=connect(preview,&MasteringPreviewController::changed,
            this,&DspChainAdapterModel::reconcile_linked_editor_rows,
            Qt::QueuedConnection);
    }
    rebuild_adapters_from_authority();
    emit changed();
}

void DspChainAdapterModel::reconcile_linked_editor_rows()
{
    QStringList ids;
    std::shared_ptr<const l3_automatch::LinkedEditorSnapshot::Ticket> owner;
    if(linkedEditorPreviewController_&&chainState_) {
        const auto linked=l3_automatch::inspect_current_linked_editors(
            *linkedEditorPreviewController_,*chainState_);
        if(linked&&!linked->empty()) {
            owner=linked->front().exact_owner;
            for(const auto& snapshot:*linked)
                ids.append(QString::fromStdString(snapshot.instance.instance_id().to_string()));
        }
    }
    if(ids==lastLinkedEditorIds_&&owner==lastLinkedEditorOwner_.lock())return;
    rebuild_adapters_from_authority();
    emit changed();
}

void DspChainAdapterModel::refreshFromAuthority()
{
    if (gainViewModel_ != nullptr) {
        gainViewModel_->refreshFromAuthority();
    }
    if (eqViewModel_ != nullptr) {
        eqViewModel_->refreshFromAuthority();
    }
    if (compressorViewModel_ != nullptr) {
        compressorViewModel_->refreshFromAuthority();
    }
    if (stereoMsViewModel_ != nullptr) {
        stereoMsViewModel_->refreshFromAuthority();
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
    if (compressorViewModel_ != nullptr) {
        compressorViewModel_->resetForNewSource();
    }
    if (stereoMsViewModel_ != nullptr) {
        stereoMsViewModel_->resetForNewSource();
    }
    emit changed();
}

}  // namespace rgsml::app
