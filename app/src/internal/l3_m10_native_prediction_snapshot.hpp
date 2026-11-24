#pragma once
// Immutable numerical prediction from a genuinely scored M10 candidate.
// It is retained with the measured key, never used as M13 fresh PCM.
#include "l3_m08_frozen_targeted_population.hpp"
#include "l3_m11_p03_protected_no_regression.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <memory>
#include <string>
#include <vector>

namespace rgsml::app::l3_automatch::m10_frozen {
struct P2OriginalRealNativeTriple;
struct NativePredictionSnapshot final {
    std::vector<m08_frozen::AuthenticatedPairSlot> native_population;
    std::vector<m11_frozen::ProtectedComponent> protected_k;
    // Reporting requested set is independent of the corrective mask and
    // Amount. Missing authority remains absent, not assumed FULL.
    std::optional<std::array<bool,4>> requested_reporting_domains;
    std::string candidate_resource_identity{};
    std::uint64_t candidate_generation{};
    std::string exact_candidate_ticket{};
    std::string exact_m07_generation{};
    std::vector<std::uint8_t> exact_linked_graph_content{};
    std::vector<std::uint8_t> exact_execution_graph_content{};
    std::shared_ptr<const P2OriginalRealNativeTriple> original_definition_authority{};
    std::vector<std::uint8_t> exact_definition_authority_content{};
    friend bool operator==(const NativePredictionSnapshot& a,
                           const NativePredictionSnapshot& b) {
        return a.native_population==b.native_population&&
            a.protected_k==b.protected_k&&
            a.requested_reporting_domains==b.requested_reporting_domains&&
            a.candidate_resource_identity==b.candidate_resource_identity&&
            a.candidate_generation==b.candidate_generation&&
            a.exact_candidate_ticket==b.exact_candidate_ticket&&
            a.exact_m07_generation==b.exact_m07_generation&&
            a.exact_linked_graph_content==b.exact_linked_graph_content&&
            a.exact_execution_graph_content==b.exact_execution_graph_content&&
            static_cast<bool>(a.original_definition_authority)==
                static_cast<bool>(b.original_definition_authority)&&
            a.exact_definition_authority_content==b.exact_definition_authority_content;
    }
};
} // namespace rgsml::app::l3_automatch::m10_frozen
