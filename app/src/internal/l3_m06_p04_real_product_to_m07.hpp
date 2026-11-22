#pragma once
// L3 M06 REAL Product P04 -> EXACT original three D07-D09 M07 slots.
// Native detector, original event population, descriptor counts and
// actual Product source grade carry all eligibility; no free-floating
// Type7 vector is promoted to an authenticated source by itself.
#include "l3_m06_p04_real_product_observation.hpp"
#include "l3_m06_d07_d09_native_binding.hpp"
#include <array>
#include <optional>
#include <string>
#include <utility>

namespace rgsml::app::l3_automatch::m05m06_real {
struct CurrentP04TripleProduct final {
    std::array<P04ProductObservation,3> true_pcm_product_measurements;
    std::array<SourceIdentity,3> independent_live_product_owners;
    std::array<std::string,3> exact_phase6_feature_definition_sha;
    bool exact_current_three_source_epochs_and_p04_policy_attested{};
    std::array<bool,3> exact_source_compatibility_and_grade{};
};
[[nodiscard]] inline TransientBindingProjection
bind_current_product_p04_d07_d09(
    const CurrentP04TripleProduct& product)
{
    if(!product.exact_current_three_source_epochs_and_p04_policy_attested)
        return {};
    TransientTripleEvidence bridge;
    bridge.same_exact_p04_policy_context_and_generation_attested=true;
    bridge.exact_feature_definition_digests=
        product.exact_phase6_feature_definition_sha;
    for(std::size_t j=0;j<3U;++j) {
        const auto& measurement=product.true_pcm_product_measurements[j];
        if(measurement.reason!=P04ProductObservationReason::ready)
            return {};
        auto& source=bridge.current_sources[j];
        source.owner=product.independent_live_product_owners[j];
        source.measured=measurement.original_d07_d08_d09_type7;
        for(std::size_t d=0;d<3U;++d)
            bridge.per_descriptor_source_grade[d][j]=
                product.exact_source_compatibility_and_grade[j]&&
                measurement.original_d07_d08_d09_type7.
                    d07_d08_d09[d].reference_grade();
    }
    return bind_real_d07_d09(bridge);
}
} // namespace rgsml::app::l3_automatch::m05m06_real
