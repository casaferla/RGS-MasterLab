#pragma once
// L3-M10 P04 ACCEPTED_FROZEN: strictly numeric domain8 consumer.
// This does not enumerate M09 families, compute a proxy from missing
// operands, change M07/M08 objective weights, or choose based on
// installed/factory availability. NO reserved none position.
#include "l3_m09_pre_render_proxy.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace rgsml::app::l3_automatch::m10_frozen {
inline constexpr std::size_t kDomainConsumerCap=8U;
inline constexpr std::size_t kCombinedConsumerCap=32U;
inline constexpr std::size_t kMeasuredShortlistCap=2U;
enum class ConsumerReason:std::uint8_t {
    selected,domain_consumer_cap_excluded,
    combined_consumer_cap_excluded,
    topology_parameter_budget_exceeded,
    ranking_unready,
    current_implementation_unavailable
};
enum class ConsumerStatus:std::uint8_t {
    ready,empty_stream,ranking_unready,invalid_canonical_stream
};
struct ConsumerInput final {
    // The authoritative M09 structured semantic and exact source
    // identity is supplied; lexical order never uses UI labels.
    m09_frozen::PreRenderTopologyProxy proxy;
    bool available_currently{};
};
struct ConsumerLedgerItem final {
    std::size_t original_ordinal{};
    ConsumerReason reason{ConsumerReason::ranking_unready};
    bool selected_but_unavailable{};
};
struct DomainConsumerResult final {
    ConsumerStatus status{ConsumerStatus::ranking_unready};
    std::vector<std::size_t> numeric_selected_indices;
    std::vector<ConsumerLedgerItem> ledger;
    [[nodiscard]] constexpr bool authorizes_audible_master() const noexcept {
        return false;
    }
};
[[nodiscard]] inline DomainConsumerResult select_domain_top8(
    std::span<const ConsumerInput> legal_m09_stream)
{
    DomainConsumerResult result;
    if(legal_m09_stream.empty()) {
        result.status=ConsumerStatus::empty_stream;
        return result;
    }
    std::vector<m09_frozen::PreRenderTopologyProxy> proxies;
    proxies.reserve(legal_m09_stream.size());
    for(const auto& candidate:legal_m09_stream) {
        if(candidate.proxy.unsigned_topology_key.empty()) {
            result.status=ConsumerStatus::invalid_canonical_stream;
            return result;
        }
        proxies.push_back(candidate.proxy);
    }
    // It is forbidden to take top8 from an available/bound subset
    // while an otherwise legal candidate lacks a current proxy.
    const auto ordered=m09_frozen::partition_and_rank_proxy(proxies);
    if(!ordered.unbound_original_order_indices.empty()) {
        result.status=ConsumerStatus::ranking_unready;
        for(std::size_t i=0;i<legal_m09_stream.size();++i)
            result.ledger.push_back(
                {i,ConsumerReason::ranking_unready,false});
        return result;
    }
    result.status=ConsumerStatus::ready;
    result.ledger.resize(legal_m09_stream.size());
    for(std::size_t i=0;i<result.ledger.size();++i) {
        result.ledger[i]={
            i,ConsumerReason::domain_consumer_cap_excluded,false};
    }
    const auto retained=std::min(
        kDomainConsumerCap,ordered.fully_bound_sorted_indices.size());
    result.numeric_selected_indices.reserve(retained);
    for(std::size_t i=0;i<retained;++i) {
        const auto ordinal=ordered.fully_bound_sorted_indices[i];
        result.numeric_selected_indices.push_back(ordinal);
        result.ledger[ordinal].reason=ConsumerReason::selected;
        result.ledger[ordinal].selected_but_unavailable=
            !legal_m09_stream[ordinal].available_currently;
    }
    return result;
}
struct CombinedConsumerInput final {
    // Already complete M09 Cartesian legal candidates, including
    // full coordinate inventory and canonical topology evidence.
    ConsumerInput candidate;
    std::size_t exact_active_coordinate_count{};
    bool coordinate_inventory_and_legal_bounds_attested{};
};
struct CombinedConsumerResult final {
    ConsumerStatus status{ConsumerStatus::ranking_unready};
    std::vector<std::size_t> numeric_selected_indices;
    std::vector<ConsumerLedgerItem> ledger;
    [[nodiscard]] constexpr bool authorizes_seed_render() const noexcept {
        return false;
    }
};
[[nodiscard]] inline CombinedConsumerResult select_combined_top32(
    std::span<const CombinedConsumerInput> m09_cartesian_legal_deduped)
{
    CombinedConsumerResult result;
    if(m09_cartesian_legal_deduped.empty()) {
        result.status=ConsumerStatus::empty_stream;
        return result;
    }
    result.ledger.reserve(m09_cartesian_legal_deduped.size());
    std::vector<m09_frozen::PreRenderTopologyProxy> viable_proxies;
    std::vector<std::size_t> viable_original_indices;
    for(std::size_t i=0;i<m09_cartesian_legal_deduped.size();++i) {
        const auto& v=m09_cartesian_legal_deduped[i];
        if(v.candidate.proxy.unsigned_topology_key.empty()||
           !v.coordinate_inventory_and_legal_bounds_attested) {
            result.status=ConsumerStatus::invalid_canonical_stream;
            result.ledger.clear();
            return result;
        }
        if(v.exact_active_coordinate_count>kMaxActiveCoordinates) {
            result.ledger.push_back({
                i,ConsumerReason::topology_parameter_budget_exceeded,false});
            continue; // must be excluded BEFORE top32/seed shortlist
        }
        result.ledger.push_back({
            i,ConsumerReason::combined_consumer_cap_excluded,false});
        viable_original_indices.push_back(i);
        viable_proxies.push_back(v.candidate.proxy);
    }
    if(viable_original_indices.empty()) {
        result.status=ConsumerStatus::empty_stream;
        return result;
    }
    const auto rank=m09_frozen::partition_and_rank_proxy(viable_proxies);
    if(!rank.unbound_original_order_indices.empty()) {
        // An unbound canonical candidate prevents a numerically honest
        // complete shortlist. >48 exclusions retain their own reasons.
        result.status=ConsumerStatus::ranking_unready;
        for(const auto ordinal:viable_original_indices)
            result.ledger[ordinal].reason=ConsumerReason::ranking_unready;
        return result;
    }
    result.status=ConsumerStatus::ready;
    const auto selected=std::min(
        kCombinedConsumerCap,rank.fully_bound_sorted_indices.size());
    result.numeric_selected_indices.reserve(selected);
    for(std::size_t i=0;i<selected;++i) {
        const auto original=viable_original_indices[
            rank.fully_bound_sorted_indices[i]];
        result.numeric_selected_indices.push_back(original);
        result.ledger[original].reason=ConsumerReason::selected;
        result.ledger[original].selected_but_unavailable=
            !m09_cartesian_legal_deduped[original].candidate.available_currently;
    }
    return result;
}
} // namespace rgsml::app::l3_automatch::m10_frozen
