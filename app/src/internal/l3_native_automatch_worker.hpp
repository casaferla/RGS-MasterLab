#pragma once
#include "l3_m10_native_fixed_worker.hpp"
#include "l3_m10_native_definition_builder.hpp"
#include "l3_m09_native_fixed_seed_geometry.hpp"
#include "l3_m09_native_domain_stream.hpp"
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
};
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
       !original_pair_matches_product_owner(input.originals,owners.current_owner))return out;
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
    const m10_frozen::PreservedNativeSearchEnvironment env{owners,input.originals,*out.fixed,
        input.native_spec,input.exact_generation,cache,input.independent_m11_ceiling_db_tp,
        input.file_based_lra_policy_attested};
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
    const auto bank=m09_frozen::derive_native_automatic_seed_bank(*out.fixed,input.amount,input.requested,
        geometry->tonal,geometry->side,geometry->side_denominator,geometry->common_low,
        geometry->common_high,rate,static_cast<std::uint32_t>(channels),input.independent_m11_ceiling_db_tp,
        input.exact_event_context,input.exact_fixed_stereo_insertion,input.dynamic_band_ids,
        std::forward<AllocateBand>(allocate_band));
    if(!bank){out.phase=NativeAutoMatchWorkerPhase::unavailable_seed_bank;return out;}
    const m09_frozen::NativeFamilyBindingEnvironment binding{owners.frozen_registry,*bank,*out.fixed,
        input.amount,input.accepted_linked_revision,rate.value(),geometry->common_low,
        geometry->common_high,input.constraints};
    out.search=m09_frozen::run_authorized_native_family_search(input.exact_generation_and_policy,
        binding,env,input.requested,channels==2U,input.exact_current_endpoint_safety_attested,
        [&](auto domain,const auto& family){
            return std::invoke(resolve_authority,*bank,domain,family);
        },std::forward<ResolveBandIdentity>(resolve_band_identity),cancellation);
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
} // namespace rgsml::app::l3_automatch
