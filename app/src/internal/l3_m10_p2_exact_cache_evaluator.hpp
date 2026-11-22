#pragma once
// L3-M10 canonical P2 trial -> existing exact completed-result cache.
// The Product owner MUST provide the frozen M07-M11 canonical key,
// full current-graph preflight, and a genuinely rendered/analyzed 27-slot
// FreshCompleteEvidence. This adapter never produces a fake score,
// supplies M21 display metrics, edits M17, or skips P2 DSP processing.
#include "l3_m10_p2_canonical_coordinate_port.hpp"
#include "l3_m10_cache_aware_evaluator.hpp"
#include <rgsml/dsp/module_parameter_codec.hpp>
#include <cstdint>
#include <string_view>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace rgsml::app::l3_automatch::m10_frozen {
namespace p2_cache_detail {
// Length-delimited typed addendum to the *existing* canonical full
// execution bytes. This prevents stale result aliasing when only P2
// Ceiling/Drive changes, even before the broader Product cache bridge.
// It does NOT replace the M07-M11 full candidate canonical serializer.
inline void append_exact_field(
    ExactCacheBytes& dst,std::string_view bytes)
{
    const auto count=static_cast<std::uint64_t>(bytes.size());
    for(unsigned i=0;i<8U;++i)
        dst.push_back(static_cast<std::uint8_t>(
            (count>>(8U*i))&0xffU));
    for(const unsigned char b:bytes)dst.push_back(b);
}
inline void append_exact_byte_field(
    ExactCacheBytes& dst,const ExactCacheBytes& bytes)
{
    const auto count=static_cast<std::uint64_t>(bytes.size());
    for(unsigned i=0;i<8U;++i)
        dst.push_back(static_cast<std::uint8_t>(
            (count>>(8U*i))&0xffU));
    dst.insert(dst.end(),bytes.begin(),bytes.end());
}
} // namespace p2_cache_detail
template<typename Decode,typename MakeExactKey,
         typename Preflight,typename FreshExecute>
[[nodiscard]] inline Evaluation evaluate_m10_p2_with_exact_cache(
    std::span<const Coordinate> inventory,
    std::span<const double> proposed_normalized_values,
    const ExactP2CoordinateNames& canonical_p2_fields,
    const dsp::ProcessingChain& current_protected_manual,
    std::span<const dsp::ModuleExecutionBinding> current_bindings,
    const dsp::ModuleInstanceId& exact_p2_instance,
    std::string exact_ticket_identity,
    ExactCompletedEvaluationCache& completed_cache,
    bool allow_new_attempt,
    Decode&& m09_canonical_inverse,
    MakeExactKey&& m07_m11_exact_key_from_full_typed_candidate,
    Preflight&& product_owner_and_full_candidate_preflight,
    FreshExecute&& actual_full_render_27_feature_analyze_and_score)
{
    const auto bound=bind_m10_p2_joint_coordinates(
        inventory,proposed_normalized_values,canonical_p2_fields,
        current_protected_manual,current_bindings,exact_p2_instance,
        std::move(exact_ticket_identity),
        std::forward<Decode>(m09_canonical_inverse));
    if(!bound)return {EvaluationKind::preflight_rejected,{}};
    // Even a possible exact-cache hit requires a still-current owner.
    if(!bound->typed.still_matches(
          current_protected_manual,current_bindings)||
       !std::invoke(product_owner_and_full_candidate_preflight,*bound))
        return {EvaluationKind::preflight_rejected,{}};
    auto exact=std::invoke(
        m07_m11_exact_key_from_full_typed_candidate,*bound);
    if(!exact)return {EvaluationKind::preflight_rejected,{}};
    const auto* p2=bound->typed.chosen_p2_parameters();
    if(!p2)return {EvaluationKind::preflight_rejected,{}};
    const auto canonical_p2=dsp::encode_module_parameters_json(
        dsp::ModuleParameterPayload{*p2});
    if(!canonical_p2)return {EvaluationKind::preflight_rejected,{}};
    ExactCacheBytes effective_content;
    p2_cache_detail::append_exact_field(
        effective_content,"rgsml.m10.p2-effective/1");
    p2_cache_detail::append_exact_byte_field(
        effective_content,exact->candidate_execution_content);
    const auto exact_id=exact_p2_instance.uuid().to_string();
    p2_cache_detail::append_exact_field(effective_content,exact_id);
    p2_cache_detail::append_exact_field(
        effective_content,*canonical_p2.value());
    exact->candidate_execution_content=std::move(effective_content);
    // The existing M10 cache accounts for attempted failures, full
    // authoritative results, and completed hits with the frozen cap.
    return evaluate_with_exact_completed_cache(
        completed_cache,*exact,true,allow_new_attempt,[&]() {
            return std::invoke(
                actual_full_render_27_feature_analyze_and_score,*bound);
        });
}
} // namespace rgsml::app::l3_automatch::m10_frozen
