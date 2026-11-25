#pragma once
// Real Product-native M10 evaluation without optimizing user P2.
// Same exact-cache and attempt accounting as the frozen refiner.
#include "l3_m10_p2_bound_native_worker_score.hpp"
#include "l3_m10_exact_native_graph_content.hpp"
#include "l3_m10_p2_canonical_trial_ticket.hpp"
#include "l3_m11_native_fixed_assessment.hpp"
#include "l3_m10_native_scope_key.hpp"

namespace rgsml::app::l3_automatch::m10_frozen {
struct PreservedNativeGraph final {
    dsp::ProcessingChain linked;
    std::vector<dsp::ModuleExecutionBinding> bindings;
};
struct PreservedNativeSearchEnvironment final {
    const P2NativeProductWorkerScope& owners;
    const OriginalPreparedReferenceNativeFacts& originals;
    const m11_frozen::NativeFixedPopulation& fixed;
    const ProductNativeOnePassSpec& spec;
    const std::string& generation;
    ExactCompletedEvaluationCache& cache;
    double m11_ceiling_db_tp{};
    bool file_lra_policy_attested{};
};
[[nodiscard]] inline std::optional<ExactExecutionCacheKey>
bind_preserved_native_execution_key(
    ExactExecutionCacheKey key,const M10P2RealNativeProductJob& job,
    const AuthoritativeCostInput& cost)
{
    if(!job.preserves_user_terminal()||job.ticket_identity().empty()||
       key.canonical_current_tuple!=cost.canonical_current_tuple||
       key.topology_evidence_identity!=cost.exact_topology_evidence_identity||
       key.candidate_execution_content.empty())return std::nullopt;
    const auto graph=job.exact_execution_graph_content();
    if(!graph)return std::nullopt;
    ExactCacheBytes content;
    using namespace p2_cache_detail;
    append_exact_field(content,"rgsml.l3.native-preserved-execution/1");
    append_exact_byte_field(content,key.candidate_execution_content);
    append_exact_field(content,job.ticket_identity());
    append_exact_byte_field(content,*graph);
    key.candidate_execution_content=std::move(content);
    return key;
}
template<class ResolveOriginalTriple>
[[nodiscard]] inline Evaluation evaluate_preserved_native_cached(
    const PreservedNativeSearchEnvironment& env,
    const PreservedNativeGraph& graph,std::string ticket,
    ExactExecutionCacheKey key,AuthoritativeCostInput cost,
    bool current_typed_graph_preflight_passed,bool allow_new,
    ResolveOriginalTriple&& resolve,
    const std::atomic<bool>* cancellation=nullptr)
{
    const auto rejected=[](){return Evaluation{EvaluationKind::preflight_rejected,{}};};
    const auto& owners=env.owners;
    if(!current_typed_graph_preflight_passed||
       !owners.current_qt_product_epoch_and_m09_m11_attested||
       !owners.current_owner.populated()||!owners.current_prepared||
       !owners.frozen_registry||ticket.empty()||env.generation.empty()||
       !std::isfinite(env.m11_ceiling_db_tp)||env.m11_ceiling_db_tp>0.0||
       !original_pair_matches_product_owner(env.originals,owners.current_owner)||
       (cancellation&&cancellation->load(std::memory_order_acquire)))
        return rejected();
    const auto& fixed=env.fixed;
    const m05m06_real::SourceIdentity fixed_owner{
        fixed.exact_fixed_resource_identity,fixed.exact_fixed_source_generation,true};
    if(!fixed.exact_independent_fixed_and_endpoint_safety_attested||
       fixed.exact_fixed_ticket==ticket||fixed.exact_m07_generation!=env.generation||
       !authentic_product_candidate_identity(fixed_owner,owners.current_owner,
                                            fixed.exact_fixed_ticket))
        return rejected();
    auto job=M10P2RealNativeProductJob::create_preserving_user_terminal(
        owners.current_owner,owners.current_prepared,owners.frozen_registry,
        owners.frozen_manual,owners.frozen_manual_bindings,graph.linked,graph.bindings,
        ticket,owners.maximum_block_frames,true);
    if(!job||fixed.exact_fixed_graph_content.empty()||
       job.value()->exact_fixed_graph_content()!=
           std::optional<ExactCacheBytes>{fixed.exact_fixed_graph_content})
        return rejected();
    // A production Fixed carries its original measured definition
    // authority. Rebind to the new ticket before ANY cache lookup.
    // The old callback-only path remains available to nonpublishing
    // research fixtures without a Fixed definition snapshot.
    std::optional<P2OriginalRealNativeTriple> original_definitions;
    if(fixed.original_definition_authority) {
        original_definitions=resolve_original_definitions_from_fixed(
            fixed,env.originals,owners.current_owner,env.generation,ticket);
        if(!original_definitions)return rejected();
        if(!cost.m08_reporting_requested_domains)return rejected();
        const auto fixed_targets=m11_frozen::assess_native_fixed_targets(
            fixed,cost.amount,*cost.m08_reporting_requested_domains);
        if(!fixed_targets.valid_current||fixed_targets.all_requested_targets_met)
            return rejected(); // distinct no-correction owner path
        cost.m08_corrective_mask=fixed_targets.corrective;
        cost.m11_no_correction_full_evidence_attested=false;
        // Candidate stereo safety is remeasured by the scorer. Its
        // policy/owner anchors must be the real independently measured
        // Fixed record, never a second caller-provided set of numbers.
        if(fixed.stereo_safety_required) {
            if(!fixed.measured_stereo_safety||
               !fixed.measured_stereo_safety->exact_current_source_candidate_and_canonical_safety_revision||
               !fixed.measured_stereo_safety->no_forbidden_decorrelation_or_haas)return rejected();
            cost.native_stereo_raw_safety_required_by_current_owner=true;
            cost.native_stereo_raw_safety=fixed.measured_stereo_safety;
        } else if(cost.native_stereo_raw_safety_required_by_current_owner||cost.native_stereo_raw_safety)
            return rejected();
    }
    cost.independently_measured_fixed=fixed;
    const auto execution=job.value()->exact_execution_graph_content();
    auto exact=bind_preserved_native_execution_key(std::move(key),*job.value(),cost);
    auto effective_spec=env.spec;
    if(!bind_original_pair_profile_common(effective_spec,env.originals))return rejected();
    const auto analysis_scope=exact_native_analysis_scope_content(effective_spec);
    if(!exact||!execution||!analysis_scope)return rejected();
    using namespace p2_cache_detail;
    append_exact_byte_field(exact->candidate_execution_content,*analysis_scope);
    append_exact_field(exact->candidate_execution_content,env.generation);
    append_exact_field(exact->candidate_execution_content,
        std::to_string(std::bit_cast<std::uint64_t>(env.m11_ceiling_db_tp)));
    append_exact_field(exact->candidate_execution_content,
        env.file_lra_policy_attested?"file-lra-attested":"file-lra-unattested");
    append_exact_byte_field(exact->candidate_execution_content,fixed.exact_fixed_graph_content);
    append_exact_byte_field(exact->candidate_execution_content,fixed.exact_definition_authority_content);
    const auto add_bits=[&](double value){
        append_exact_field(exact->candidate_execution_content,
            std::to_string(std::bit_cast<std::uint64_t>(value)));
    };
    add_bits(cost.amount.global);
    for(auto value:cost.amount.domains)add_bits(value);
    append_exact_field(exact->candidate_execution_content,
        std::to_string(cost.amount.independent_amount_revision));
    for(bool requested:cost.m08_corrective_mask)
        exact->candidate_execution_content.push_back(requested?1U:0U);
    exact->candidate_execution_content.push_back(cost.m08_reporting_requested_domains?1U:0U);
    if(cost.m08_reporting_requested_domains)
        for(bool requested:*cost.m08_reporting_requested_domains)
            exact->candidate_execution_content.push_back(requested?1U:0U);
    append_exact_field(exact->candidate_execution_content,
        std::to_string(cost.active_coordinates.size()));
    for(const auto& c:cost.active_coordinates) {
        add_bits(c.current_u);add_bits(c.immutable_reference_u);add_bits(c.reference_weight_r);
    }
    append_exact_field(exact->candidate_execution_content,std::to_string(cost.active_owned_modules));
    append_exact_field(exact->candidate_execution_content,std::to_string(cost.enabled_artistic_bands));
    const auto candidate_owner=native_product_candidate_identity(owners.current_owner,ticket);
    if(const auto hit=env.cache.find(*exact)) {
        if(!hit->native_prediction||
           hit->native_prediction->exact_candidate_ticket!=ticket||
           hit->native_prediction->candidate_resource_identity!=candidate_owner.resource_identity||
           hit->native_prediction->candidate_generation!=candidate_owner.generation||
           hit->native_prediction->exact_m07_generation!=env.generation||
           hit->native_prediction->exact_execution_graph_content!=*execution||
           (original_definitions&&
             (!hit->native_prediction->original_definition_authority||
              hit->native_prediction->exact_definition_authority_content!=
                  exact_original_native_definition_content(*original_definitions))))
            return rejected();
    }
    return evaluate_with_exact_completed_cache(env.cache,*exact,true,allow_new,[&](){
        const auto score=measure_and_score_m10_p2_full_native_candidate(
            *job.value(),std::move(effective_spec),nullptr,env.generation,true,std::move(cost),
            env.m11_ceiling_db_tp,candidate_owner,&env.originals,
            env.file_lra_policy_attested,
            [&](const M10P2FullNativeEvidence& measured)
                ->std::optional<P2OriginalRealNativeTriple> {
                if(original_definitions)return std::move(original_definitions);
                return std::invoke(resolve,measured);
            },cancellation);
        return score.as_exact_cache_evidence();
    });
}
} // namespace rgsml::app::l3_automatch::m10_frozen
