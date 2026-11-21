#pragma once
// L3-M09 final frozen PRE-RENDER topology proxy order. This adapter
// combines ONLY caller-authenticated M05-M08 hierarchical covered-loss
// with M10-owned exact seed displacement and complexity evidence.
// An unavailable operand is NULL/UNBOUND, NEVER zero or +infinity.
// Proxy is NOT a seed render/optimized render/verified proximity.
#include "l3_m10_frozen_coordinate_refiner.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace rgsml::app::l3_automatch::m09_frozen {
struct ProxyOperands final {
    bool exact_m05_m08_and_m10_operands_attested{};
    std::optional<double> frozen_covered_target_loss;
    std::optional<double> m10_seed_normalized_parameter_displacement;
    std::optional<double> m10_topology_complexity;
    std::uint32_t active_owned_modules{};
    std::uint32_t enabled_artistic_bands{};
    std::vector<std::uint8_t> canonical_unsigned_topology_key;
};
enum class ProxyReason:std::uint8_t {
    ready,unbound_missing_exact_operand,
    invalid_numeric_or_canonical_identity
};
struct PreRenderTopologyProxy final {
    std::optional<double> raw_value;
    std::optional<std::int64_t> q1e9;
    std::uint32_t active_modules{};
    std::uint32_t active_bands{};
    std::vector<std::uint8_t> unsigned_topology_key;
    ProxyReason reason{ProxyReason::unbound_missing_exact_operand};
    [[nodiscard]] constexpr bool is_bound() const noexcept {
        return reason==ProxyReason::ready&&raw_value.has_value()
            &&q1e9.has_value();
    }
    [[nodiscard]] constexpr bool authorizes_seed_render_or_publishing()
        const noexcept {return false;}
};
[[nodiscard]] inline PreRenderTopologyProxy make_pre_render_proxy(
    const ProxyOperands& source)
{
    PreRenderTopologyProxy result;
    result.active_modules=source.active_owned_modules;
    result.active_bands=source.enabled_artistic_bands;
    result.unsigned_topology_key=source.canonical_unsigned_topology_key;
    if(!source.exact_m05_m08_and_m10_operands_attested||
       !source.frozen_covered_target_loss||
       !source.m10_seed_normalized_parameter_displacement||
       !source.m10_topology_complexity)
        return result;
    if(source.canonical_unsigned_topology_key.empty()||
       source.active_owned_modules>8U||
       source.enabled_artistic_bands>12U||
       !std::isfinite(*source.frozen_covered_target_loss)||
       *source.frozen_covered_target_loss<0.0||
       !std::isfinite(*source.m10_seed_normalized_parameter_displacement)||
       *source.m10_seed_normalized_parameter_displacement<0.0||
       !std::isfinite(*source.m10_topology_complexity)||
       *source.m10_topology_complexity<0.0) {
        result.reason=ProxyReason::invalid_numeric_or_canonical_identity;
        return result;
    }
    // Recovered proxy(T)=proxyTargetLoss+.02*seedR+.01*complexity.
    // These inputs are separate authoritative M10 computations, not
    // guessed from no-op counts or an old nine-cell M21 surrogate.
    const double raw=*source.frozen_covered_target_loss+
                     0.02*(*source.m10_seed_normalized_parameter_displacement)+
                     0.01*(*source.m10_topology_complexity);
    const auto quantized=m10_frozen::quantize_q1e9(raw);
    if(!quantized) {
        result.reason=ProxyReason::invalid_numeric_or_canonical_identity;
        return result;
    }
    result.raw_value=raw;
    result.q1e9=*quantized;
    result.reason=ProxyReason::ready;
    return result;
}
[[nodiscard]] inline bool proxy_precedes(
    const PreRenderTopologyProxy& a,
    const PreRenderTopologyProxy& b)
{
    // ONLY bound operands allowed: no synthetic comparison of unbound.
    if(*a.q1e9!=*b.q1e9)return *a.q1e9<*b.q1e9;
    if(a.active_modules!=b.active_modules)
        return a.active_modules<b.active_modules;
    if(a.active_bands!=b.active_bands)
        return a.active_bands<b.active_bands;
    return std::lexicographical_compare(
        a.unsigned_topology_key.begin(),a.unsigned_topology_key.end(),
        b.unsigned_topology_key.begin(),b.unsigned_topology_key.end());
}
struct ProxyPartition final {
    std::vector<std::size_t> fully_bound_sorted_indices;
    std::vector<std::size_t> unbound_original_order_indices;
    [[nodiscard]] constexpr bool authorizes_product_publication()
        const noexcept{return false;}
};
[[nodiscard]] inline ProxyPartition partition_and_rank_proxy(
    std::span<const PreRenderTopologyProxy> input)
{
    ProxyPartition out;
    for(std::size_t i=0;i<input.size();++i)
        if(input[i].is_bound())
            out.fully_bound_sorted_indices.push_back(i);
        else
            out.unbound_original_order_indices.push_back(i);
    // Equal complete ranking tuples retain original deterministic M09
    // structural enumeration ordinal. Unbound entries remain visible
    // with their own reasons and NEVER participate in ranked shortlist.
    std::stable_sort(out.fully_bound_sorted_indices.begin(),
                     out.fully_bound_sorted_indices.end(),
        [&](std::size_t a,std::size_t b) {
            return proxy_precedes(input[a],input[b]);
        });
    return out;
}
} // namespace rgsml::app::l3_automatch::m09_frozen
