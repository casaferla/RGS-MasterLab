#pragma once
// Bind an independently measured Fixed population to the actual candidate.
// Preserve all configured entries; K contains only the current supported
// Prepared/REFERENCE comparison population. Never derive Fixed from C.
#include "l3_m08_frozen_targeted_population.hpp"
#include "l3_m11_p03_protected_no_regression.hpp"
#include <cmath>
#include <optional>
#include <memory>
#include <vector>

namespace rgsml::app::l3_automatch::m10_frozen { struct P2OriginalRealNativeTriple; }
namespace rgsml::app::l3_automatch::m11_frozen {
struct NativeFixedPopulation final {
    std::vector<m08_frozen::AuthenticatedPairSlot> measured_fixed;
    bool exact_independent_fixed_and_endpoint_safety_attested{};
    // Retained provenance of the independent worker result; the Product
    // scorer must revalidate these before consuming a reusable Fixed.
    std::string exact_fixed_ticket{};
    std::string exact_fixed_resource_identity{};
    std::uint64_t exact_fixed_source_generation{};
    std::string exact_m07_generation{};
    std::vector<std::uint8_t> exact_fixed_graph_content{};
    std::shared_ptr<const m10_frozen::P2OriginalRealNativeTriple> original_definition_authority{};
    std::vector<std::uint8_t> exact_definition_authority_content{};
};
[[nodiscard]] inline std::optional<std::vector<ProtectedComponent>>
bind_native_fixed_to_candidate(
    const NativeFixedPopulation& fixed,
    const std::vector<m08_frozen::AuthenticatedPairSlot>& candidate,
    const m08_frozen::AmountConfig& amount)
{
    if(!fixed.exact_independent_fixed_and_endpoint_safety_attested||
       candidate.size()!=m07_frozen::kBindings.size()||
       fixed.measured_fixed.size()!=candidate.size())return std::nullopt;
    std::vector<ProtectedComponent> result;
    for(std::size_t i=0;i<candidate.size();++i) {
        const auto& binding=m07_frozen::kBindings[i];
        const auto& c=candidate[i];
        const auto& f=fixed.measured_fixed[i];
        if(c.binding_id!=binding.binding_id||c.binding_id!=f.binding_id||
           c.source_feature_id!=f.source_feature_id||
           c.exact_feature_dependency.empty()||
           c.exact_feature_dependency!=f.exact_feature_dependency||
           !c.feature_authority_attested||!f.feature_authority_attested||
           c.configured.empty()||c.configured.size()!=f.configured.size())
            return std::nullopt;
        const auto a=m08_frozen::effective_amount(
            amount,static_cast<std::size_t>(binding.domain));
        if(a.positive_underflow_to_zero||!std::isfinite(a.rounded)||
           a.rounded<0.0||a.rounded>1.0)return std::nullopt;
        for(std::size_t j=0;j<c.configured.size();++j) {
            const auto& x=c.configured[j];
            const auto& y=f.configured[j];
            if(x.current_valid!=y.current_valid||
               x.paired_native_support!=y.paired_native_support||
               x.prepared!=y.prepared||x.reference!=y.reference||
               !std::isfinite(x.paired_native_support)||
               x.paired_native_support<0.0||x.paired_native_support>1.0)
                return std::nullopt;
            const bool required=x.current_valid&&x.paired_native_support>0.0;
            if(!required) {
                result.push_back({false,false,true});
                continue;
            }
            if(!x.prepared.z||!x.reference.z||!y.current_candidate_z||
               !x.current_candidate_z)return std::nullopt;
            const double p=*x.prepared.z,g=*x.reference.z;
            const double z=*y.current_candidate_z,actual=*x.current_candidate_z;
            const double d=binding.deadband,delta=g-p;
            if(!std::isfinite(p)||!std::isfinite(g)||!std::isfinite(z)||
               !std::isfinite(actual)||!std::isfinite(delta))
                return std::nullopt;
            const double distance=std::abs(z-g);
            if(!std::isfinite(distance))return std::nullopt;
            bool protected_component=false;
            if(std::abs(delta)<=d) {
                protected_component=distance<=d;
            } else {
                volatile double directional=delta;
                const double q=(z-p)/directional;
                if(!std::isfinite(q))return std::nullopt;
                // Scalar directional target has no overshoot-distance
                // restriction. Vector partial protection has the frozen
                // per-component distance guard as well.
                protected_component=q>=a.rounded&&
                    (binding.component_share==m07_frozen::ComponentShare::one||
                     distance<=std::abs(delta)+d);
            }
            result.push_back({true,protected_component,true,p,g,z,actual,d});
        }
    }
    return result;
}
} // namespace rgsml::app::l3_automatch::m11_frozen
