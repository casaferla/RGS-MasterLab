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
} // namespace rgsml::app::l3_automatch::m10_frozen
