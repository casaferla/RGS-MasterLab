#pragma once
// Execute Fixed independently of each candidate, with the same preserved
// user terminal and the same native binding definitions. Retain native
// measurements, never the Fixed PCM or a proxy score.
#include "l3_m10_p2_authoritative_native_score.hpp"

namespace rgsml::app::l3_automatch::m10_frozen {
template<class ResolveOriginalNativeTriple>
[[nodiscard]] inline std::optional<m11_frozen::NativeFixedPopulation>
measure_native_fixed_once(
    const M10P2RealNativeProductJob& fixed_job,
    const M10P2RealNativeProductJob& candidate,
    ProductNativeOnePassSpec spec,
    const std::string& exact_generation,
    const m05m06_real::SourceIdentity& fixed_owner,
    const OriginalPreparedReferenceNativeFacts& originals,
    bool exact_current_fixed_and_endpoint_safety_attested,
    bool file_based_lra_attested,
    ResolveOriginalNativeTriple&& resolve,
    const std::atomic<bool>* cancellation=nullptr)
{
    if(!fixed_job.fixed_only()||!fixed_job.same_fixed_baseline_as(candidate)||
       fixed_job.ticket_identity()==candidate.ticket_identity()||
       exact_generation.empty()||!exact_current_fixed_and_endpoint_safety_attested||
       !original_pair_matches_product_owner(originals,fixed_job.owner_stamp())||
       !authentic_product_candidate_identity(fixed_owner,fixed_job.owner_stamp(),
                                             fixed_job.ticket_identity())||
       (cancellation&&cancellation->load(std::memory_order_acquire)))
        return std::nullopt;
    const auto fixed_graph=fixed_job.exact_fixed_graph_content();
    if(!fixed_graph||candidate.exact_fixed_graph_content()!=fixed_graph)
        return std::nullopt;
    if(!bind_original_pair_profile_common(spec,originals))return std::nullopt;
    auto measured=measure_m10_p2_full_native_once(
        fixed_job,std::move(spec),nullptr,cancellation);
    if(!measured||(cancellation&&cancellation->load(std::memory_order_acquire)))
        return std::nullopt;
    const auto active_frames=static_cast<std::int64_t>(
        measured.value()->native.exact_observed_active_frames);
    auto definitions=std::invoke(std::forward<ResolveOriginalNativeTriple>(resolve),
                                  *measured.value());
    if(!definitions)return std::nullopt;
    auto authority=std::make_shared<const P2OriginalRealNativeTriple>(
        retain_original_native_definitions(*definitions));
    auto authority_content=exact_original_native_definition_content(*authority);
    m05m06_real::D03D04NativeSeries native_windows;
    auto bound=bind_p2_original_native_triple_from_independent_full_pair(
        std::move(*definitions),originals,*measured.value(),fixed_owner,
        active_frames,file_based_lra_attested,&native_windows);
    if(!bound.fragments)return std::nullopt;
    m07_frozen::ExactTwentySevenFeatureAssembler assembly{exact_generation,true};
    const auto admission=m07_frozen::collect_existing_m05m06_native_fragments(
        assembly,*bound.fragments);
    if(admission.status!=m07_frozen::NativeFragmentAdmission::accepted_complete)
        return std::nullopt;
    auto population=assembly.complete_canonical_population();
    if(!population)return std::nullopt;
    const auto actual=m05m06_real::import_actual_full_source(
        measured.value()->native.actual.levels,measured.value()->native.actual.tonal,
        fixed_owner,measured.value()->native.selected_original_m20_ordinals);
    if(!actual.source)return std::nullopt;
    const bool stereo_required=originals.prepared.original_m19_m20.channels==2U;
    std::optional<m11_frozen::NativeStereoRawSafety> stereo;
    if(stereo_required)
        stereo=m11_frozen::bind_actual_native_stereo_safety(
            originals.prepared.original_m19_m20,originals.prepared.full_native.s03_s06,
            *actual.source,measured.value()->native.s03_s06,
            exact_current_fixed_and_endpoint_safety_attested,
            exact_current_fixed_and_endpoint_safety_attested);
    return m11_frozen::NativeFixedPopulation{std::move(*population),true,
        fixed_job.ticket_identity(),fixed_owner.resource_identity,
        fixed_owner.generation,exact_generation,*fixed_graph,
        std::move(authority),std::move(authority_content),
        measured.value()->m16_actual_p2_output.global_linear_peak(),
        stereo_required,std::move(stereo),std::move(native_windows)};
}
} // namespace rgsml::app::l3_automatch::m10_frozen
