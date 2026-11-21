#pragma once
// M18 PRIVATE; no main.cpp registration. A separate M17 Product target
// sees the terminal stage gate without imposing its dependencies on
// portable SOURCE, audition or Project-only test targets.
#include "terminal_candidate_gate.hpp"
#include "../../../platform/windows/src/internal/m18_owner_mutation_fanout.hpp"

namespace rgsml::app::internal {
struct M18ProductTerminalCandidateBinding final {
    static void bind(
        TerminalCandidateGate& candidates,
        const platform::windows::internal::M18OwnerMutationFanout& product)
    {
        candidates.set_private_m18_mutation_admission(
            product.status_admission());
    }
};
} // namespace rgsml::app::internal
