#pragma once
// L3-M09 P04 -> M10 P04 exact legal combined-candidate boundary.
// M09 already produced/authorized legal Cartesian alternatives; M10
// consumes them without availability-driven backfill, typeId-only
// collapsing, seeded result invention or synthetic proxy readiness.
#include "l3_m09_exact_candidate_dedup.hpp"
#include "l3_m10_p04_numeric_domain_consumer.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m10_frozen {
struct BoundLegalCombinedCandidate final {
    m09_frozen::CandidateCarrier description;
    m09_frozen::PreRenderTopologyProxy pre_render_proxy;
    std::size_t exact_active_coordinate_count{};
    bool coordinate_inventory_and_bounds_attested{};
    bool availability_currently{};
};
enum class BoundLegalStatus:std::uint8_t {
    selected,unbound_ranking,all_candidates_parameter_budget_excluded,
    invalid_or_stale_generation,candidate_key_seed_collision,
    inconsistent_exact_annotation
};
struct BoundLegalCombinedResult final {
    BoundLegalStatus status{BoundLegalStatus::invalid_or_stale_generation};
    std::vector<m09_frozen::CandidateCarrier> exact_descriptions;
    CombinedConsumerResult combined_consumer;
    [[nodiscard]] constexpr bool authorizes_product_publication()
        const noexcept {return false;}
};
namespace combined_detail {
using ExactTriple=std::tuple<
    m09_frozen::ExactBytes,m09_frozen::ExactBytes,m09_frozen::ExactBytes>;
[[nodiscard]] inline ExactTriple triple(
    const m09_frozen::CandidateCarrier& x)
{
    return {x.semantic_tuple,x.seed_exact_content,x.dependency_tuple};
}
[[nodiscard]] inline bool same_annotation(
    const BoundLegalCombinedCandidate& a,
    const BoundLegalCombinedCandidate& b) noexcept
{
    const auto& x=a.pre_render_proxy;
    const auto& y=b.pre_render_proxy;
    return a.exact_active_coordinate_count==
                b.exact_active_coordinate_count
        &&a.coordinate_inventory_and_bounds_attested==
                b.coordinate_inventory_and_bounds_attested
        &&a.availability_currently==b.availability_currently
        &&x.reason==y.reason
        &&x.raw_value==y.raw_value
        &&x.q1e9==y.q1e9
        &&x.active_modules==y.active_modules
        &&x.active_bands==y.active_bands
        &&x.unsigned_topology_key==y.unsigned_topology_key;
}
[[nodiscard]] inline bool proxy_annotation_valid(
    const BoundLegalCombinedCandidate& e)
{
    const auto& p=e.pre_render_proxy;
    if(p.unsigned_topology_key!=e.description.semantic_tuple||
       p.active_modules>8U||p.active_bands>12U)
        return false;
    if(!p.is_bound())return !p.raw_value&&!p.q1e9;
    const auto q=quantize_q1e9(*p.raw_value);
    return q&&*q==*p.q1e9;
}
} // namespace combined_detail
[[nodiscard]] inline BoundLegalCombinedResult
bind_exact_legal_combined_and_select_top32(
    m09_frozen::ExactBytes authenticated_snapshot,
    std::span<const BoundLegalCombinedCandidate> ordered_full_m09,
    bool exact_current_m09_cartesian_legality_attested)
{
    BoundLegalCombinedResult result;
    if(!exact_current_m09_cartesian_legality_attested||
       authenticated_snapshot.empty()||ordered_full_m09.empty())
        return result;
    m09_frozen::ExactCandidateGeneration generation{
        std::move(authenticated_snapshot)};
    std::map<combined_detail::ExactTriple,std::size_t> first_ordinal_by_triple;
    std::vector<CombinedConsumerInput> consumed;
    consumed.reserve(ordered_full_m09.size());
    for(std::size_t i=0;i<ordered_full_m09.size();++i) {
        const auto& e=ordered_full_m09[i];
        if(!combined_detail::proxy_annotation_valid(e))
            return result;
        const auto trip=combined_detail::triple(e.description);
        const auto first=first_ordinal_by_triple.find(trip);
        if(first!=first_ordinal_by_triple.end()&&
           !combined_detail::same_annotation(
               e,ordered_full_m09[first->second])) {
            result.status=BoundLegalStatus::inconsistent_exact_annotation;
            return result;
        }
        const auto inserted=generation.add(e.description);
        if(inserted==m09_frozen::InsertStatus::
                         candidate_key_seed_collision) {
            result.status=BoundLegalStatus::candidate_key_seed_collision;
            return result;
        }
        if(inserted!=m09_frozen::InsertStatus::inserted&&
           inserted!=m09_frozen::InsertStatus::exact_duplicate)
            return result;
        if(inserted==m09_frozen::InsertStatus::inserted) {
            first_ordinal_by_triple.emplace(trip,i);
            consumed.push_back({
                {e.pre_render_proxy,e.availability_currently},
                e.exact_active_coordinate_count,
                e.coordinate_inventory_and_bounds_attested});
        }
    }
    const auto* unique=generation.descriptions();
    if(!unique||unique->size()!=consumed.size())
        return result;
    result.exact_descriptions=*unique; // includes ordered provenance
    result.combined_consumer=select_combined_top32(consumed);
    if(result.combined_consumer.status==ConsumerStatus::ready)
        result.status=BoundLegalStatus::selected;
    else if(result.combined_consumer.status==ConsumerStatus::ranking_unready)
        result.status=BoundLegalStatus::unbound_ranking;
    else if(result.combined_consumer.status==ConsumerStatus::empty_stream)
        result.status=
            BoundLegalStatus::all_candidates_parameter_budget_excluded;
    else
        result.status=BoundLegalStatus::invalid_or_stale_generation;
    return result;
}
} // namespace rgsml::app::l3_automatch::m10_frozen
