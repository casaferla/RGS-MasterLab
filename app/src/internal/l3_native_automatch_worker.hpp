#pragma once
#include "l3_m10_native_fixed_worker.hpp"
#include "l3_m10_native_definition_builder.hpp"
#include "l3_m09_native_fixed_seed_geometry.hpp"
#include "l3_m09_native_owner_allocations.hpp"
#include <type_traits>
namespace rgsml::app::l3_automatch {
enum class NativeAutoMatchWorkerPhase:std::uint8_t {
    invalid_owner,unavailable_fixed,unavailable_seed_geometry,
    unavailable_seed_bank,search_completed,cancelled
};
struct NativeAutoMatchWorkerResult final {
    NativeAutoMatchWorkerPhase phase{NativeAutoMatchWorkerPhase::invalid_owner};
    std::optional<m11_frozen::NativeFixedPopulation> fixed;
    m09_frozen::NativeProductSearchResult search;
    [[nodiscard]] constexpr bool authorizes_product_publication()const noexcept{return false;}
};
// Borrowed from the immutable Product session. This worker owns no
// QObject, persistence, GUI state, M17 export or second solver.
struct NativeAutoMatchWorkerInputs final {
    const m10_frozen::P2NativeProductWorkerScope& owners;
    const OriginalPreparedReferenceNativeFacts& originals;
    ProductNativeOnePassSpec native_spec;
    std::string exact_generation;
    m09_frozen::ExactBytes exact_generation_and_policy;
    std::string fixed_ticket;
    std::uint64_t accepted_linked_revision{};
    m08_frozen::AmountConfig amount;
    std::array<bool,4> requested{};
    double independent_m11_ceiling_db_tp{};
    bool exact_current_endpoint_safety_attested{};
    bool file_based_lra_policy_attested{};
    bool exact_event_context{};
    bool exact_fixed_stereo_insertion{};
    std::vector<core::Uuid> dynamic_band_ids;
    m10_frozen::AuthoritativeCostInput constraints;
    std::optional<m09_frozen::NativeOwnedP2Seed> owned_p2{};
};
// Seed geometry only: one full native SIDE-prefix observation, not a
// scored M10 trial. The original fixed graph and M11 mask remain intact.
[[nodiscard]] inline std::optional<m09_frozen::NativeFamilySeedBank::PostSideStereo>
measure_native_post_side_stereo_seed_row(
    const NativeAutoMatchWorkerInputs& input,const m11_frozen::NativeFixedPopulation& fixed,
    const m09_frozen::NativeFamilySeedBank& bank,std::size_t prefix,
    const m09_frozen::NativeFamilyBindingAuthority& authority,
    const std::atomic<bool>* cancellation)
{
    namespace m09=m09_frozen;
    const auto& owners=input.owners;
    if(!input.exact_fixed_stereo_insertion||prefix==0U||prefix>bank.side.size()||
       authority.disposition!=m09::NativeDomainDisposition::legal||
       !authority.available_currently||authority.allocations.size()!=1U||
       authority.allocations.front().role!=m09::IntentRole::stereo_side_eq||
       (cancellation&&cancellation->load(std::memory_order_acquire)))return {};
    if(std::any_of(owners.frozen_manual.instances().begin(),owners.frozen_manual.instances().end(),
        [](const auto& node){return node.active()&&node.module_type_id()=="rgsml.dsp.streaming-true-peak-limiter";}))
        return {};
    m09::AbstractFamilyChoice family;family.stereo_side_eq_prefix=static_cast<std::uint8_t>(prefix);
    const auto& allocation=authority.allocations.front();
    const std::vector<m09::NativeRoleSeed> role{{allocation.role,allocation.instance,allocation.semantic,
        bank.side[prefix-1U].payload}};
    const auto graph=m09::assemble_native_artistic_role_graph(*owners.frozen_registry,family,role,
        input.accepted_linked_revision);
    if(!graph)return {};
    const auto ticket=input.fixed_ticket+"/side-insertion/"+std::to_string(prefix);
    const auto job=M10P2RealNativeProductJob::create_preserving_user_terminal(
        owners.current_owner,owners.current_prepared,owners.frozen_registry,
        owners.frozen_manual,owners.frozen_manual_bindings,graph->linked,graph->bindings,ticket,
        owners.maximum_block_frames,true);
    if(!job||job.value()->exact_fixed_graph_content()!=
        std::optional<m10_frozen::ExactCacheBytes>{fixed.exact_fixed_graph_content})return {};
    const auto execution=job.value()->exact_execution_graph_content();
    if(!execution)return {};
    auto spec=input.native_spec;spec.retain_native_spectral_energy=false;
    const auto measured=measure_m10_p2_full_native_once(*job.value(),std::move(spec),nullptr,cancellation);
    if(!measured||(cancellation&&cancellation->load(std::memory_order_acquire)))return {};
    auto definitions=m10_frozen::resolve_original_definitions_from_fixed(fixed,input.originals,
        owners.current_owner,input.exact_generation,ticket);
    if(!definitions)return {};
    const auto owner=native_product_candidate_identity(owners.current_owner,ticket);
    const auto bound=m10_frozen::bind_p2_original_native_triple_from_independent_full_pair(
        std::move(*definitions),input.originals,*measured.value(),owner,
        static_cast<std::int64_t>(measured.value()->native.exact_observed_active_frames),
        input.file_based_lra_policy_attested);
    if(!bound.fragments)return {};
    m07_frozen::ExactTwentySevenFeatureAssembler assembler{input.exact_generation,true};
    const auto admission=m07_frozen::collect_existing_m05m06_native_fragments(assembler,*bound.fragments);
    if(admission.status!=m07_frozen::NativeFragmentAdmission::accepted_complete)return {};
    const auto insertion=assembler.complete_canonical_population();
    if(!insertion||!m11_frozen::bind_native_fixed_to_candidate(fixed,*insertion,input.amount))return {};
    m09::NativeFamilySeedBank::PostSideStereo row{*execution,{},bank.side[prefix-1U].payload};
    for(std::size_t i=0;i<row.modes.size();++i)
        row.modes[i]=m09::derive_native_stereo_seed(fixed,input.amount,input.requested,
            i+1U,true,true,&*insertion);
    return row;
}
template<class ResolveOriginalDefinitions,class AllocateBand,class ResolveAuthority,class ResolveBandIdentity>
[[nodiscard]] inline NativeAutoMatchWorkerResult run_native_automatch_worker(
    NativeAutoMatchWorkerInputs input,ResolveOriginalDefinitions&& resolve_definitions,
    AllocateBand&& allocate_band,ResolveAuthority&& resolve_authority,
    ResolveBandIdentity&& resolve_band_identity,const std::atomic<bool>* cancellation=nullptr)
{
    NativeAutoMatchWorkerResult out;
    const auto cancelled=[&](){return cancellation&&cancellation->load(std::memory_order_acquire);};
    if(cancelled()){out.phase=NativeAutoMatchWorkerPhase::cancelled;return out;}
    const auto& owners=input.owners;
    if(!owners.current_qt_product_epoch_and_m09_m11_attested||!owners.current_prepared||
       !owners.frozen_registry||input.exact_generation.empty()||input.exact_generation_and_policy.empty()||
       !input.accepted_linked_revision||!std::isfinite(input.independent_m11_ceiling_db_tp)||
       input.independent_m11_ceiling_db_tp>0.0||
       !original_pair_matches_product_owner(input.originals,owners.current_owner)||
       (input.owned_p2&&!m09_frozen::valid_native_owned_p2_seed(*input.owned_p2)))return out;
    const auto format=owners.current_prepared->view().format();
    const auto rate=format.sample_rate();
    const auto channels=format.channel_count();
    if(!bind_product_native_fft_metadata(input.native_spec,static_cast<std::uint32_t>(rate.value()))||
       !bind_original_pair_profile_common(input.native_spec,input.originals))return out;
    out.fixed=m10_frozen::measure_current_product_fixed(owners,input.originals,input.native_spec,
        input.exact_generation,input.fixed_ticket,input.accepted_linked_revision,
        input.exact_current_endpoint_safety_attested,input.file_based_lra_policy_attested,
        std::forward<ResolveOriginalDefinitions>(resolve_definitions),cancellation);
    if(cancelled()){out.phase=NativeAutoMatchWorkerPhase::cancelled;return out;}
    if(!out.fixed){out.phase=NativeAutoMatchWorkerPhase::unavailable_fixed;return out;}
    // The matrix belongs to Fixed only, not to the hundreds of trial renders.
    input.native_spec.retain_native_spectral_energy=false;
    m10_frozen::ExactCompletedEvaluationCache cache{input.exact_generation_and_policy};
    m10_frozen::NativeFixedPrefixCache fixed_prefix_cache;
    const m10_frozen::PreservedNativeSearchEnvironment env{owners,input.originals,*out.fixed,
        input.native_spec,input.exact_generation,cache,input.independent_m11_ceiling_db_tp,
        input.file_based_lra_policy_attested,&fixed_prefix_cache};
    out.search.fixed_gate=m10_frozen::assess_native_fixed_search_gate(env,input.amount,input.requested,
        input.exact_current_endpoint_safety_attested);
    if(out.search.fixed_gate.status==m10_frozen::NativeFixedSearchStatus::no_correction_required) {
        out.search.status=m09_frozen::NativeProductSearchStatus::no_correction_required;
        out.phase=NativeAutoMatchWorkerPhase::search_completed;return out;
    }
    if(out.search.fixed_gate.status!=m10_frozen::NativeFixedSearchStatus::correction_required) {
        out.phase=NativeAutoMatchWorkerPhase::unavailable_fixed;return out;
    }
    const auto geometry=m09_frozen::derive_native_fixed_seed_geometry(*out.fixed,input.originals,
        input.native_spec,static_cast<std::uint32_t>(rate.value()));
    if(!geometry){out.phase=NativeAutoMatchWorkerPhase::unavailable_seed_geometry;return out;}
    auto bank=m09_frozen::derive_native_automatic_seed_bank(*out.fixed,input.amount,input.requested,
        geometry->tonal,geometry->side,geometry->side_denominator,geometry->common_low,
        geometry->common_high,rate,static_cast<std::uint32_t>(channels),input.independent_m11_ceiling_db_tp,
        input.exact_event_context,input.exact_fixed_stereo_insertion,input.dynamic_band_ids,
        std::forward<AllocateBand>(allocate_band));
    if(!bank){out.phase=NativeAutoMatchWorkerPhase::unavailable_seed_bank;return out;}
    // The normal Product path supplies a deterministic v1 policy when
    // no accepted explicit override is present. Never take over a
    // protected MANUAL P2, even if bypassed.
    const auto manual_nodes=owners.frozen_manual.instances();
    const bool protected_p2=std::any_of(manual_nodes.begin(),manual_nodes.end(),[](const auto& node){
        return node.module_type_id()=="rgsml.dsp.streaming-true-peak-limiter";
    });
    const auto p2_descriptor=owners.frozen_registry->find_descriptor("rgsml.dsp.streaming-true-peak-limiter");
    bool p2_available=false;
    if(p2_descriptor&&owners.frozen_registry->has_factory("rgsml.dsp.streaming-true-peak-limiter")) {
        const auto segments=p2_descriptor.value()->get().allowed_segments();
        p2_available=std::find(segments.begin(),segments.end(),dsp::ChainSegment::REF_LINKED)!=segments.end();
    }
    if(input.owned_p2&&!protected_p2&&!p2_available) {
        out.phase=NativeAutoMatchWorkerPhase::unavailable_seed_bank;return out;
    }
    if(out.search.fixed_gate.targets.corrective[3]&&!protected_p2&&p2_available)
        bank->owned_p2=input.owned_p2?input.owned_p2:
            m09_frozen::default_native_owned_p2_seed(input.independent_m11_ceiling_db_tp);
    const auto resolve_current=[&](auto domain,const auto& family){
        if constexpr(std::is_invocable_v<ResolveAuthority&,const m09_frozen::NativeFamilySeedBank&,
            const m11_frozen::NativeFixedPopulation&,decltype(domain),decltype(family)>)
            return std::invoke(resolve_authority,*bank,*out.fixed,domain,family);
        else return std::invoke(resolve_authority,*bank,domain,family);
    };
    if(channels==2U&&out.search.fixed_gate.targets.corrective[2]&&input.exact_fixed_stereo_insertion) {
        for(std::size_t prefix=1U;prefix<=bank->side.size()&&prefix<=bank->post_side_stereo.size();++prefix) {
            if(cancelled()){out.phase=NativeAutoMatchWorkerPhase::cancelled;return out;}
            m09_frozen::AbstractFamilyChoice family;
            family.stereo_side_eq_prefix=static_cast<std::uint8_t>(prefix);
            const auto authority=resolve_current(std::optional<std::size_t>{2U},family);
            bank->post_side_stereo[prefix-1U]=measure_native_post_side_stereo_seed_row(
                input,*out.fixed,*bank,prefix,authority,cancellation);
        }
    }
    const m09_frozen::NativeFamilyBindingEnvironment binding{owners.frozen_registry,*bank,*out.fixed,
        input.amount,input.accepted_linked_revision,rate.value(),geometry->common_low,
        geometry->common_high,input.constraints};
    out.search=m09_frozen::run_authorized_native_family_search(input.exact_generation_and_policy,
        binding,env,input.requested,channels==2U,input.exact_current_endpoint_safety_attested,
        resolve_current,std::forward<ResolveBandIdentity>(resolve_band_identity),cancellation);
    out.phase=cancelled()?NativeAutoMatchWorkerPhase::cancelled:NativeAutoMatchWorkerPhase::search_completed;
    return out;
}
// Product-facing entry: use the accepted native definition inventory
// rather than an arbitrary triple resolver. The definitions are built
// only after the actual independently rendered Fixed PCM is measured.
template<class AllocateBand,class ResolveAuthority,class ResolveBandIdentity>
[[nodiscard]] inline NativeAutoMatchWorkerResult run_native_automatch_with_definitions(
    NativeAutoMatchWorkerInputs input,const m10_frozen::NativeDefinitionPolicy& definitions,
    AllocateBand&& allocate_band,ResolveAuthority&& resolve_authority,
    ResolveBandIdentity&& resolve_band_identity,const std::atomic<bool>* cancellation=nullptr)
{
    if(!m10_frozen::valid_native_definition_policy(definitions))return {};
    const auto& owners=input.owners;
    const auto& originals=input.originals;
    const auto ticket=input.fixed_ticket;
    const auto spec=input.native_spec;
    return run_native_automatch_worker(std::move(input),
        [&](const M10P2FullNativeEvidence& fixed){
            return m10_frozen::build_current_native_definitions(definitions,originals,fixed,
                owners.current_owner,ticket,spec);
        },std::forward<AllocateBand>(allocate_band),std::forward<ResolveAuthority>(resolve_authority),
        std::forward<ResolveBandIdentity>(resolve_band_identity),cancellation);
}
// The ordinary artistic route no longer requires a caller to supply
// a separate candidate recipe or UUID resolver per combination.
// The immutable accepted dependency bytes and owner UUID allocator
// still come from the Product session, not from a second registry.
[[nodiscard]] inline NativeAutoMatchWorkerResult run_native_automatch_with_owner_allocations(
    NativeAutoMatchWorkerInputs input,const m10_frozen::NativeDefinitionPolicy& definitions,
    m09_frozen::NativeOwnerAllocations::AllocateUuid allocate_uuid,
    std::span<const core::Uuid> protected_project_ids,
    const m09_frozen::ExactBytes& exact_accepted_dependencies,
    const std::atomic<bool>* cancellation=nullptr)
{
    if(cancellation&&cancellation->load(std::memory_order_acquire)) {
        NativeAutoMatchWorkerResult cancelled;cancelled.phase=NativeAutoMatchWorkerPhase::cancelled;
        return cancelled;
    }
    if(!input.owners.frozen_registry||exact_accepted_dependencies.empty()||
       !m10_frozen::valid_native_definition_policy(definitions))return {};
    std::vector<core::Uuid> protected_ids(protected_project_ids.begin(),protected_project_ids.end());
    for(const auto& instance:input.owners.frozen_manual.instances())
        protected_ids.push_back(instance.instance_id().uuid());
    m09_frozen::NativeOwnerAllocations allocations(std::move(allocate_uuid),protected_ids);
    input.dynamic_band_ids.clear();
    for(std::size_t i=0;i<3U;++i) {
        const auto id=allocations.dynamic_band(i);
        if(!id)return {};
        input.dynamic_band_ids.push_back(*id);
    }
    const auto generation=input.exact_generation_and_policy;
    const auto registry=input.owners.frozen_registry;
    const double ceiling=input.independent_m11_ceiling_db_tp;
    const auto manual=input.owners.frozen_manual.instances();
    const bool existing_user_terminal=std::any_of(manual.begin(),manual.end(),[](const auto& n){
        return n.module_type_id()=="rgsml.dsp.streaming-true-peak-limiter";
    });
    auto result=run_native_automatch_with_definitions(std::move(input),definitions,
        [&](auto role,const auto& atom){return allocations.band(role,atom);},
        [&](const auto& bank,const auto& fixed,auto,const auto& family){
            m09_frozen::NativeFamilyBindingAuthority unresolved;
            unresolved.exact_reason="fixed-m16-or-dependency-unavailable";
            if(!fixed.measured_m16_linear_peak||fixed.exact_definition_authority_content.empty()||
               fixed.exact_fixed_graph_content.empty())return unresolved;
            auto dependencies=exact_accepted_dependencies;
            using namespace m10_frozen::p2_cache_detail;
            append_exact_field(dependencies,"rgsml.l3.native-owner-dependencies/1");
            append_exact_byte_field(dependencies,fixed.exact_definition_authority_content);
            append_exact_byte_field(dependencies,fixed.exact_fixed_graph_content);
            const bool peak_exceeds=!m10_frozen::check_actual_p2_m16_linear_peak(
                *fixed.measured_m16_linear_peak,ceiling).passes();
            return m09_frozen::resolve_native_owner_family(bank,family,allocations,*registry,
                generation,dependencies,peak_exceeds,existing_user_terminal);
        },[&](auto role,const auto& id){return allocations.band_identity(role,id);},cancellation);
    if(allocations.failed()) {
        result.search={};
        result.phase=NativeAutoMatchWorkerPhase::unavailable_seed_bank;
    }
    return result;
}
} // namespace rgsml::app::l3_automatch
