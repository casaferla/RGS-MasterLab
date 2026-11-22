#pragma once
// L3-M12 read-only bridge from the exact persisted REF_LINKED Project
// to the EXISTING native renderer. No Project mutation, owner receipt,
// M13 verification, second engine, M17 WAV export or M10 proxy PCM.
#include "l3_m12_ref_linked_project_restore.hpp"
#include "l3_m12_reference_insertion_anchor.hpp"
#include <rgsml/dsp/module_parameter_codec.hpp>
#include <rgsml/project/opaque_json_value.hpp>
#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m12_frozen {
struct RestoredM12NativeReplayPlan final {
    // Owned registry lifetime covers every copied native chain.
    std::shared_ptr<const dsp::ModuleRegistry> registry_owner;
    dsp::ProcessingChain manual_prefix;
    std::vector<dsp::ModuleExecutionBinding> manual_bindings;
    dsp::ProcessingChain reference_linked;
    std::vector<dsp::ModuleExecutionBinding> reference_bindings;
    std::optional<dsp::ProcessingChain> terminal_p2;
    std::vector<dsp::ModuleExecutionBinding> terminal_p2_bindings;
    core::Uuid project_id;
    core::Uuid source_id;
    core::Uuid reference_id;
    core::Uuid plan_id;
    core::Uuid linked_chain_id;
    std::string exact_plan_checksum;
    std::string exact_m10_winner_checksum;
    std::string exact_m11_protection_checksum;
    std::string exact_source_reference_scope_revision;
    [[nodiscard]] constexpr bool authorizes_project_adoption() const noexcept {
        return false;
    }
    [[nodiscard]] constexpr bool is_m12_materialization_receipt() const noexcept {
        return false;
    }
    [[nodiscard]] constexpr bool is_m13_verified() const noexcept {
        return false;
    }
};
[[nodiscard]] inline core::Result<RestoredM12NativeReplayPlan>
prepare_restored_m12_native_replay_plan(
    const project::ProjectDocument& persisted,
    const RestoredM12LinkedProductState& linked,
    core::Uuid current_manual_chain_id,
    const dsp::ProcessingChain& current_manual,
    std::span<const dsp::ModuleExecutionBinding> current_manual_bindings,
    bool current_qt_product_source_reference_and_fixed_epoch_attested)
{
    using R=core::Result<RestoredM12NativeReplayPlan>;
    const auto fail=[](const char* reason) {
        return R::failure(core::Error{core::ErrorCode::InvalidState,reason});
    };
    if(!current_qt_product_source_reference_and_fixed_epoch_attested||
       !linked.registry_owner||persisted.projectId.is_nil()||
       persisted.sourceResourceId.is_nil()||
       !persisted.references.activeReferenceId||
       !persisted.pipeline.masteringChainId||
       *persisted.pipeline.masteringChainId!=current_manual_chain_id||
       current_manual_chain_id.is_nil()||
       current_manual.context()!=dsp::ProcessingChainContext{
           dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL}||
       linked.exact_native_chain.context()!=dsp::ProcessingChainContext{
           dsp::ProcessingStage::MASTER,dsp::ChainSegment::REF_LINKED}||
       current_manual.instances().size()!=current_manual_bindings.size())
        return fail("L3_M12_REPLAY_OWNER_OR_SEGMENT_STALE");
    const auto referenced=std::find_if(
        persisted.references.items.begin(),persisted.references.items.end(),
        [&](const project::Reference& ref){
            return ref.referenceId==*persisted.references.activeReferenceId&&
                   ref.resourceId!=persisted.sourceResourceId;
        });
    if(referenced==persisted.references.items.end())
        return fail("L3_M12_REPLAY_REFERENCE_IDENTITY_INVALID");
    const auto manuals=std::count_if(
        persisted.chains.begin(),persisted.chains.end(),
        [&](const project::Chain& c){return c.chainId==current_manual_chain_id;});
    const auto saved=std::find_if(
        persisted.chains.begin(),persisted.chains.end(),
        [&](const project::Chain& c){return c.chainId==current_manual_chain_id;});
    if(manuals!=1||saved==persisted.chains.end()||
       saved->stage!="MASTER"||saved->segment!="MANUAL"||
       saved->revision!=current_manual.revision()||
       saved->modules.size()!=current_manual_bindings.size())
        return fail("L3_M12_REPLAY_PROTECTED_MANUAL_CHANGED");
    // Reopen the original stored linked DSP graph again and compare it
    // with the caller's decoded snapshot: neither may be substituted.
    const auto again=decode_exact_m12_ref_linked_product_state(
        persisted,linked.registry_owner);
    if(!again)return R::failure(*again.error());
    if(again.value()->plan_id!=linked.plan_id||
       again.value()->controller_id!=linked.controller_id||
       again.value()->persisted_linked_chain_id!=
           linked.persisted_linked_chain_id||
       again.value()->exact_plan_revision!=linked.exact_plan_revision||
       again.value()->exact_plan_checksum!=linked.exact_plan_checksum||
       again.value()->exact_m10_winning_checksum!=
           linked.exact_m10_winning_checksum||
       again.value()->exact_m11_protected_fixed_checksum!=
           linked.exact_m11_protected_fixed_checksum||
       again.value()->exact_source_reference_scope_revision!=
           linked.exact_source_reference_scope_revision||
       again.value()->exact_native_bindings!=linked.exact_native_bindings||
       again.value()->exact_native_chain.instances().size()!=
           linked.exact_native_chain.instances().size()||
       !std::equal(again.value()->exact_native_chain.instances().begin(),
                   again.value()->exact_native_chain.instances().end(),
                   linked.exact_native_chain.instances().begin()))
        return fail("L3_M12_REPLAY_LINKED_SNAPSHOT_SUBSTITUTED");
    for(std::size_t i=0;i<current_manual_bindings.size();++i) {
        const auto& actual=current_manual.instances()[i];
        const auto& binding=current_manual_bindings[i];
        const auto& stored=saved->modules[i];
        if(!binding_matches_instance(actual,binding)||
           actual.provenance()!=dsp::ModuleProvenance::MANUAL||
           actual.owner()!=dsp::ModuleOwner::USER||
           actual.link_state()!=dsp::ModuleLinkState::UNLINKED||
           actual.semantic_node_id().has_value()||
           stored.instanceId!=actual.instance_id().uuid()||
           stored.typeId!=actual.module_type_id()||
           stored.enabled!=actual.enabled()||
           stored.userBypass!=actual.user_bypass()||
           stored.controllerSuspended!=actual.controller_suspended()||
           stored.domainSuspended!=actual.domain_suspended()||
           stored.provenance!="MANUAL"||stored.owner!="USER"||
           stored.linkState!="UNLINKED"||
           stored.semanticNodeId.has_value())
            return fail("L3_M12_REPLAY_MANUAL_INSTANCE_OR_OWNER_STALE");
        const auto encoded=dsp::encode_module_parameters_json(
            binding.parameters);
        if(!encoded)return R::failure(*encoded.error());
        const auto canonical=project::OpaqueJsonValue::parse(*encoded.value());
        if(!canonical)return R::failure(*canonical.error());
        if(canonical.value()->canonical_utf8()!=
           stored.parameters.canonical_utf8())
            return fail("L3_M12_REPLAY_MANUAL_PARAMETERS_STALE");
    }
    const auto anchor=plan_canonical_m12_insertion_anchor(
        current_manual,*linked.registry_owner,true);
    if(!anchor.exact_current())
        return fail("L3_M12_REPLAY_TERMINAL_ANCHOR_INVALID");
    const auto nodes=current_manual.instances();
    const bool has_p2=!nodes.empty()&&nodes.back().module_type_id()==
        "rgsml.dsp.streaming-true-peak-limiter";
    if(has_p2!=anchor.current_gap->right_neighbor_instance_id.has_value())
        return fail("L3_M12_REPLAY_UNSUPPORTED_TERMINAL_SUFFIX");
    if(has_p2 && *anchor.current_gap->right_neighbor_instance_id!=
           nodes.back().instance_id())
        return fail("L3_M12_REPLAY_TERMINAL_MOVED");
    const auto prefix_count=nodes.size()-(has_p2?1U:0U);
    std::vector<dsp::ModuleInstance> prefixNodes(
        nodes.begin(),nodes.begin()+static_cast<std::ptrdiff_t>(prefix_count));
    auto prefix=dsp::ProcessingChain::restore(
        *linked.registry_owner,current_manual.context(),
        current_manual.revision(),std::move(prefixNodes));
    if(!prefix)return R::failure(*prefix.error());
    std::optional<dsp::ProcessingChain> terminal;
    std::vector<dsp::ModuleExecutionBinding> terminalBindings;
    if(has_p2) {
        auto final=dsp::ProcessingChain::restore(
            *linked.registry_owner,current_manual.context(),
            current_manual.revision(),{nodes.back()});
        if(!final)return R::failure(*final.error());
        terminal=std::move(*final.value());
        terminalBindings.push_back(current_manual_bindings.back());
    }
    return R::success(RestoredM12NativeReplayPlan{
        linked.registry_owner,std::move(*prefix.value()),
        {current_manual_bindings.begin(),
         current_manual_bindings.begin()+
             static_cast<std::ptrdiff_t>(prefix_count)},
        linked.exact_native_chain,linked.exact_native_bindings,
        std::move(terminal),std::move(terminalBindings),
        persisted.projectId,persisted.sourceResourceId,
        *persisted.references.activeReferenceId,linked.plan_id,
        linked.persisted_linked_chain_id,linked.exact_plan_checksum,
        linked.exact_m10_winning_checksum,
        linked.exact_m11_protected_fixed_checksum,
        linked.exact_source_reference_scope_revision});
}
} // namespace rgsml::app::l3_automatch::m12_frozen
