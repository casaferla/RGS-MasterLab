#pragma once
#include "l3_m12_product_preview_worker_ticket.hpp"
#include "dsp_manual_preset_bundle.hpp"
#include "l3_manual_match_dsp_conflict.hpp"

namespace rgsml::app::l3_automatch {
// Nonpublishing preflight. Copying a linked processor into the MANUAL
// segment can change its execution position. Never label this operation
// an equivalent replay or carry forward a numerical verification.
// A later owner transaction must retire/suspend the former correction,
// confirm the changed topology and keep the prior accepted snapshot.
struct ManualMatchTransferDraft final {
    std::shared_ptr<const m12_frozen::M12ImmutableProductPreviewTicket> exact_prior_owner;
    dsp::ModuleInstanceId linked_instance;
    dsp::ModuleInstanceId manual_instance;
    dsp_presets::ProposedManualPresetRestore manual_after;
    // Provisional REF graph only: unchanged peers keep their exact payload
    // and relative order. The transferred node is inactive. This is NOT
    // the original accepted plan and must acquire new content identity.
    dsp::ProcessingChain linked_after;
    std::vector<dsp::ModuleExecutionBinding> linked_bindings_after;
    [[nodiscard]] constexpr bool requires_explicit_confirmation() const noexcept {return true;}
    [[nodiscard]] constexpr bool requires_fresh_verification() const noexcept {return true;}
    [[nodiscard]] constexpr bool authorizes_project_commit() const noexcept {return false;}
};
// Pure graph preparation for the pre-destructive Project Open gate.
// Its result has no PCM/owner authority; live capture uses the wrapper below.
[[nodiscard]] inline core::Result<ManualMatchTransferDraft> prepare_manual_match_transfer_graph(
    std::shared_ptr<const m12_frozen::M12ImmutableProductPreviewTicket> ticket,
    const dsp::ProcessingChain& manual,
    std::span<const dsp::ModuleExecutionBinding> bindings,
    const dsp::ModuleRegistry& registry,
    const dsp::ModuleInstanceId& requested)
{
    using R=core::Result<ManualMatchTransferDraft>;
    const auto fail=[](const char* reason){
        return R::failure(core::Error{core::ErrorCode::InvalidState,reason});
    };
    if(!ticket||!ticket->replay||
        manual.context()!=dsp::ProcessingChainContext{dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL}||
        manual.revision()!=ticket->exact_manual_chain_revision||
        !std::equal(manual.instances().begin(),manual.instances().end(),
            ticket->exact_manual_nodes.begin(),ticket->exact_manual_nodes.end())||
        !std::equal(bindings.begin(),bindings.end(),
            ticket->exact_manual_bindings.begin(),ticket->exact_manual_bindings.end()))
        return fail("MANUAL_MATCH_TRANSFER_STALE_OWNER");
    const auto linked=ticket->replay->reference_linked.instances();
    const auto& linkedBindings=ticket->replay->reference_bindings;
    if(linked.size()!=linkedBindings.size())return fail("MANUAL_MATCH_TRANSFER_INVALID_BINDINGS");
    const auto from=std::find_if(linked.begin(),linked.end(),[&](const auto& n){
        return n.instance_id()==requested;
    });
    if(from==linked.end()||!from->active()||
        from->owner()!=dsp::ModuleOwner::REFERENCE_MATCH||
        from->link_state()!=dsp::ModuleLinkState::REF_LINKED)
        return fail("MANUAL_MATCH_TRANSFER_NOT_ACTIVE_LINKED");
    const auto type=from->module_type_id();
    if(type!="rgsml.dsp.dynamic-eq"&&type!="rgsml.dsp.transient-shaper")
        return fail("MANUAL_MATCH_TRANSFER_TYPE_UNSUPPORTED");
    // Never silently collapse multiple linked processors of one type.
    if(std::count_if(linked.begin(),linked.end(),[&](const auto& n){
        return n.active()&&n.module_type_id()==type;
    })!=1)return fail("MANUAL_MATCH_TRANSFER_AMBIGUOUS_LINKED_TYPE");
    const auto nodes=manual.instances();
    if(std::count_if(nodes.begin(),nodes.end(),[&](const auto& n){return n.module_type_id()==type;})!=1)
        return fail("MANUAL_MATCH_TRANSFER_MANUAL_SLOT_MISSING_OR_AMBIGUOUS");
    const auto to=std::find_if(nodes.begin(),nodes.end(),[&](const auto& n){return n.module_type_id()==type;});
    if(to->active()||!to->user_bypass()||to->controller_suspended()||to->domain_suspended())
        return fail("MANUAL_MATCH_TRANSFER_MANUAL_SLOT_PROTECTED");
    const auto index=static_cast<std::size_t>(from-linked.begin());
    if(linkedBindings[index].instance_id!=requested)
        return fail("MANUAL_MATCH_TRANSFER_BINDING_ID_MISMATCH");
    const auto desc=registry.find_descriptor(type);
    if(!desc||!registry.has_factory(type)||!desc.value()->get().algorithm_version()||
        !desc.value()->get().parameter_schema_id())
        return fail("MANUAL_MATCH_TRANSFER_FACTORY_UNAVAILABLE");
    const auto& d=desc.value()->get();
    // Stage both sides of the transfer together. The target is activated
    // only in this provisional graph. No live setter is called.
    if(!to->enabled()||!ticket->replay->registry_owner||
        ticket->replay->reference_linked.revision()==std::numeric_limits<std::uint64_t>::max())
        return fail("MANUAL_MATCH_TRANSFER_INVALID_DESTINATION_OR_REVISION");
    dsp_presets::ManualPreset preset{dsp_presets::Scope::module,{
        {std::string(type),std::string(*d.algorithm_version()),std::string(*d.parameter_schema_id()),
         linkedBindings[index].parameters,to->enabled(),false}}};
    auto after=dsp_presets::prepare_explicit_manual_preset_restore(preset,manual,bindings,
        registry,static_cast<std::size_t>(to-nodes.begin()));
    if(!after)return R::failure(*after.error());
    std::vector<dsp::ModuleInstance> nextLinked(linked.begin(),linked.end());
    auto inactive=dsp::ModuleInstance::create({
        from->instance_id(),std::string(type),from->enabled(),true,
        from->controller_suspended(),from->domain_suspended(),
        from->provenance(),from->owner(),from->link_state(),
        from->semantic_node_id(),from->parameter_state()});
    if(!inactive)return R::failure(*inactive.error());
    nextLinked[index]=std::move(*inactive.value());
    auto linkedAfter=dsp::ProcessingChain::restore(*ticket->replay->registry_owner,
        ticket->replay->reference_linked.context(),
        ticket->replay->reference_linked.revision()+1U,std::move(nextLinked));
    if(!linkedAfter)return R::failure(*linkedAfter.error());
    if(conflicting_active_match_dsps(after.value()->proposed_manual_chain.instances(),
            linkedAfter.value()->instances()))
        return fail("MANUAL_MATCH_TRANSFER_CONFLICT_REMAINS");
    auto copiedBindings=linkedBindings;
    return R::success(ManualMatchTransferDraft{std::move(ticket),requested,to->instance_id(),
        std::move(*after.value()),std::move(*linkedAfter.value()),std::move(copiedBindings)});
}
[[nodiscard]] inline core::Result<ManualMatchTransferDraft> prepare_manual_match_transfer(
    std::shared_ptr<const m12_frozen::M12ImmutableProductPreviewTicket> ticket,
    const dsp::ProcessingChain& manual,
    std::span<const dsp::ModuleExecutionBinding> bindings,
    const dsp::ModuleRegistry& registry,
    const dsp::ModuleInstanceId& requested)
{
    using R=core::Result<ManualMatchTransferDraft>;
    if(!ticket||!ticket->exact_prepared_owner)
        return R::failure(core::Error{core::ErrorCode::InvalidState,
            "MANUAL_MATCH_TRANSFER_NO_PREPARED_OWNER"});
    return prepare_manual_match_transfer_graph(
        std::move(ticket),manual,bindings,registry,requested);
}
} // namespace rgsml::app::l3_automatch
