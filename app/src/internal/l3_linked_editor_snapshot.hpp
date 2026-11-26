#pragma once
// First post-AutoMatch editor integration seam. Inspect the actual current
// Product replay, never a research demo, MANUAL placeholder or solver seed.
// A returned snapshot is presentation only; no mutation or Activity grant.
#include "l3_m12_product_preview_worker_ticket.hpp"
#include <QThread>
#include <algorithm>
#include <memory>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace rgsml::app::l3_automatch {
struct LinkedEditorSnapshot final {
    using Ticket=m12_frozen::M12ImmutableProductPreviewTicket;
    std::shared_ptr<const Ticket> exact_owner;
    dsp::ModuleInstance instance;
    dsp::ModuleParameterPayload parameters;
    [[nodiscard]] constexpr bool manual_editable() const noexcept{return false;}
    [[nodiscard]] constexpr bool activity_authorized() const noexcept{return false;}
};

// One coherent list for the Product chain model. Empty is a valid current
// linked graph with no supported editors; nullopt means authority failed.
[[nodiscard]] inline std::optional<std::vector<LinkedEditorSnapshot>> inspect_current_linked_editors(
    const MasteringPreviewController& preview,
    const std::shared_ptr<const render::RenderResult>& current_prepared,
    const dsp::ProcessingChain& current_manual,
    std::span<const dsp::ModuleExecutionBinding> current_manual_bindings)
{
    if(QThread::currentThread()!=preview.thread())return {};
    const auto ticket=preview.private_m12_product_preview_ticket();
    if(!ticket||!ticket->replay||!ticket->replay->registry_owner||
       !current_prepared||ticket->exact_prepared_owner!=current_prepared||
       current_manual.context()!=dsp::ProcessingChainContext{
           dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL}||
       current_manual.revision()!=ticket->exact_manual_chain_revision||
       current_manual.instances().size()!=ticket->exact_manual_nodes.size()||
       !std::equal(current_manual.instances().begin(),current_manual.instances().end(),
                   ticket->exact_manual_nodes.begin())||
       current_manual_bindings.size()!=ticket->exact_manual_bindings.size()||
       !std::equal(current_manual_bindings.begin(),current_manual_bindings.end(),
                   ticket->exact_manual_bindings.begin()))return {};
    const auto& replay=*ticket->replay;
    if(replay.reference_linked.context()!=dsp::ProcessingChainContext{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::REF_LINKED}||
       replay.plan_id.is_nil()||replay.controller_id.is_nil()||
       replay.linked_chain_id.is_nil()||replay.exact_plan_checksum.empty())return {};
    const auto nodes=replay.reference_linked.instances();
    if(nodes.size()!=replay.reference_bindings.size())return {};
    std::vector<LinkedEditorSnapshot> result;
    for(std::size_t i=0;i<nodes.size();++i) {
        const auto& node=nodes[i];
        const auto& binding=replay.reference_bindings[i];
        if(!binding_matches_instance(node,binding)||
           node.owner()!=dsp::ModuleOwner::REFERENCE_MATCH||
           node.provenance()!=dsp::ModuleProvenance::REFERENCE_MATCH||
           node.link_state()!=dsp::ModuleLinkState::REF_LINKED||
           !node.semantic_node_id()||node.semantic_node_id()->is_nil())
            return {};
        if(std::any_of(current_manual.instances().begin(),current_manual.instances().end(),
            [&](const auto& manual){return manual.instance_id()==node.instance_id();}))return {};
        for(std::size_t j=0;j<i;++j)
            if(nodes[j].instance_id()==node.instance_id()||
               nodes[j].semantic_node_id()==node.semantic_node_id())return {};
        const auto type=node.module_type_id();
        const bool dynamic=type=="rgsml.dsp.dynamic-eq";
        const bool transient=type=="rgsml.dsp.transient-shaper";
        if(!dynamic&&!transient)continue;
        if((dynamic&&!std::holds_alternative<dsp::DynamicEqParameters>(binding.parameters))||
           (transient&&!std::holds_alternative<dsp::TransientShaperParameters>(binding.parameters))||
           !replay.registry_owner->has_factory(type))return {};
        result.push_back(LinkedEditorSnapshot{ticket,node,binding.parameters});
    }
    return result;
}
[[nodiscard]] inline std::optional<LinkedEditorSnapshot> inspect_current_linked_editor(
    const MasteringPreviewController& preview,
    const std::shared_ptr<const render::RenderResult>& current_prepared,
    const dsp::ProcessingChain& current_manual,
    std::span<const dsp::ModuleExecutionBinding> current_manual_bindings,
    const dsp::ModuleInstanceId& requested_instance)
{
    const auto snapshots=inspect_current_linked_editors(
        preview,current_prepared,current_manual,current_manual_bindings);
    if(!snapshots)return {};
    const auto it=std::find_if(snapshots->begin(),snapshots->end(),
        [&](const auto& snapshot){return snapshot.instance.instance_id()==requested_instance;});
    if(it==snapshots->end())return {};
    return *it;
}
} // namespace rgsml::app::l3_automatch
