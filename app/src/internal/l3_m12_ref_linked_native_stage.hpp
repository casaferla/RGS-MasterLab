#pragma once
// L3-M12 native Product REF_LINKED stage, real DSP ModuleInstance/Registry
// and typed ModuleExecutionBinding, not a second/parallel DSP chain.
// This stage is NOT project publication or M13 verification. Persistent
// carrier / atomic receipt follow. WAV Export M17 is NEVER owned or
// configured by AutoMatch; it is independent of M16 analytical safety.
#include "l3_automatch_typed_gain_trial.hpp"
#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/module_execution_binding.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m12_frozen {
struct ExactLinkedNode final {
    dsp::ModuleInstanceId allocated_instance_id;
    core::Uuid semantic_node_id;
    std::string exact_registered_type_id;
    dsp::ModuleParameterPayload exact_m10_winning_parameters;
    bool enabled{true};
    bool user_bypass{};
    bool controller_suspended{};
    bool domain_suspended{};
};
struct AcceptedPlanAndOwner final {
    core::Uuid plan_id;
    core::Uuid reference_match_controller_id;
    std::string exact_plan_content_checksum;
    std::string exact_m10_result_and_topology_checksum;
    std::string exact_m11_fixed_and_protection_checksum;
    std::string exact_source_reference_amount_scope_revision;
    std::uint64_t exact_accepted_plan_revision{};
    bool current_source_reference_owner_and_plan_attested{};
    bool m11_protected_and_full_candidate_safety_passed{};
    bool independent_m16_measurement_and_m11_safety_attested{};
    bool no_correction_required{};
};
struct StagedRefLinkedSegment final {
    // Registry lifetime precedes chain pointer lifetime.
    std::shared_ptr<const dsp::ModuleRegistry> registry_owner;
    dsp::ProcessingChain chain;
    std::vector<dsp::ModuleExecutionBinding> exact_bindings;
    AcceptedPlanAndOwner accepted_identity;
    // Exact frozen M11 current MANUAL source to protect against
    // an altered Input Gain/MANUAL graph between M10 and M13.
    std::uint64_t frozen_protected_manual_revision{};
    std::vector<dsp::ModuleInstance> frozen_protected_manual_instances;
    std::vector<dsp::ModuleExecutionBinding>
        frozen_protected_manual_bindings;
    [[nodiscard]] constexpr bool authorizes_project_mutation()
        const noexcept{return false;}
    [[nodiscard]] constexpr bool authorizes_m13_verified()
        const noexcept{return false;}
};
[[nodiscard]] inline core::Result<StagedRefLinkedSegment>
stage_exact_m12_native_ref_linked(
    const AcceptedPlanAndOwner& accepted,
    const dsp::ProcessingChain& protected_manual,
    std::span<const dsp::ModuleExecutionBinding> protected_bindings,
    std::shared_ptr<const dsp::ModuleRegistry> live_registry,
    std::vector<ExactLinkedNode> original_m10_ordered_nodes)
{
    using R=core::Result<StagedRefLinkedSegment>;
    const auto fail=[](const char* reason) {
        return R::failure(core::Error{
            core::ErrorCode::InvalidState,reason});
    };
    if(accepted.plan_id.is_nil()||
       accepted.reference_match_controller_id.is_nil()||
       !accepted.exact_accepted_plan_revision||
       accepted.exact_plan_content_checksum.empty()||
       accepted.exact_m10_result_and_topology_checksum.empty()||
       accepted.exact_m11_fixed_and_protection_checksum.empty()||
       accepted.exact_source_reference_amount_scope_revision.empty()||
       !accepted.current_source_reference_owner_and_plan_attested||
       !accepted.m11_protected_and_full_candidate_safety_passed||
       !accepted.independent_m16_measurement_and_m11_safety_attested||
       (accepted.no_correction_required!=original_m10_ordered_nodes.empty())||
       !live_registry||
       protected_manual.context()!=dsp::ProcessingChainContext{
           dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL}||
       protected_manual.instances().size()!=protected_bindings.size())
        return fail("L3_M12_ACCEPTED_PLAN_PRECONDITION_MISSING");
    for(std::size_t j=0;j<protected_bindings.size();++j)
        if(!binding_matches_instance(
            protected_manual.instances()[j],protected_bindings[j]))
            return fail("L3_M12_PROTECTED_MANUAL_SNAPSHOT_INVALID");
    std::vector<dsp::ModuleInstance> staged_instances;
    std::vector<dsp::ModuleExecutionBinding> staged_bindings;
    staged_instances.reserve(original_m10_ordered_nodes.size());
    staged_bindings.reserve(original_m10_ordered_nodes.size());
    for(const auto& n:original_m10_ordered_nodes) {
        if(n.semantic_node_id.is_nil()||
           n.exact_registered_type_id.empty()||
           n.exact_registered_type_id=="rgsml.dsp.true-peak-limiter"||
           n.exact_registered_type_id=="rgsml.dsp.dither")
            return fail("L3_M12_FORBIDDEN_OR_MISSING_NODE");
        if(n.exact_registered_type_id=="rgsml.dsp.streaming-true-peak-limiter" &&
           (&n!=&original_m10_ordered_nodes.back() ||
            std::any_of(protected_manual.instances().begin(),
                        protected_manual.instances().end(),[](const auto& m){
                return m.module_type_id()=="rgsml.dsp.streaming-true-peak-limiter";
            })))
            return fail("L3_M12_OWNED_P2_TERMINAL_CONFLICT");
        for(const auto& existing:protected_manual.instances())
            if(existing.instance_id()==n.allocated_instance_id)
                return fail("L3_M12_PROTECTED_INSTANCE_ID_COLLISION");
        for(const auto& previous:staged_instances)
            if(previous.instance_id()==n.allocated_instance_id||
               previous.semantic_node_id()==n.semantic_node_id)
                return fail("L3_M12_DUPLICATE_NODE_IDENTITY");
        auto instance=dsp::ModuleInstance::create({
            n.allocated_instance_id,n.exact_registered_type_id,
            n.enabled,n.user_bypass,n.controller_suspended,
            n.domain_suspended,dsp::ModuleProvenance::REFERENCE_MATCH,
            dsp::ModuleOwner::REFERENCE_MATCH,
            dsp::ModuleLinkState::REF_LINKED,
            n.semantic_node_id,{}});
        if(!instance)return R::failure(*instance.error());
        const dsp::ModuleExecutionBinding binding{
            n.allocated_instance_id,n.exact_m10_winning_parameters};
        if(!binding_matches_instance(*instance.value(),binding))
            return fail("L3_M12_M10_PARAMETER_TYPE_MISMATCH");
        if(!live_registry->has_factory(n.exact_registered_type_id))
            return fail("L3_M12_REQUIRED_DSP_FACTORY_UNAVAILABLE");
        auto d=live_registry->create_module(
            n.exact_registered_type_id,binding.parameters);
        if(!d)return R::failure(*d.error());
        staged_instances.push_back(std::move(*instance.value()));
        staged_bindings.push_back(binding);
    }
    auto linked=dsp::ProcessingChain::restore(
        *live_registry,
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::REF_LINKED},
        accepted.exact_accepted_plan_revision,
        std::move(staged_instances));
    if(!linked)return R::failure(*linked.error());
    return R::success(StagedRefLinkedSegment{
        std::move(live_registry),std::move(*linked.value()),
        std::move(staged_bindings),accepted,
        protected_manual.revision(),
        {protected_manual.instances().begin(),
         protected_manual.instances().end()},
        {protected_bindings.begin(),protected_bindings.end()}});
}
} // namespace rgsml::app::l3_automatch::m12_frozen
