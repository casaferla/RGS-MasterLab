#pragma once
// Immutable numerical prediction from a genuinely scored M10 candidate.
// It is retained with the measured key, never used as M13 fresh PCM.
#include "l3_m08_frozen_targeted_population.hpp"
#include "l3_m11_p03_protected_no_regression.hpp"
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace rgsml::app::l3_automatch::m10_frozen {
struct NativePredictionSnapshot final {
    std::vector<m08_frozen::AuthenticatedPairSlot> native_population;
    std::vector<m11_frozen::ProtectedComponent> protected_k;
    // Reporting requested set is independent of the corrective mask and
    // Amount. Missing authority remains absent, not assumed FULL.
    std::optional<std::array<bool,4>> requested_reporting_domains;
    friend bool operator==(const NativePredictionSnapshot&,
                           const NativePredictionSnapshot&)=default;
};
} // namespace rgsml::app::l3_automatch::m10_frozen
