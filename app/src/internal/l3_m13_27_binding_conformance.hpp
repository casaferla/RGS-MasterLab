#pragma once
// L3-M13: bounded P03 conformance of independently supplied native
// authoritative M05/M06/M07 binding evidence. This does NOT extract
// source features from M19 display cells, accept solver PCM or mint
// VERIFIED. The caller must provide the complete selected M07 population.
#include "l3_m13_qtp_27_projection.hpp"
#include "l3_m08_frozen_amount_target.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m13_frozen {
enum class BindingConformance:std::uint8_t {
    passed,failed,unavailable,not_applicable
};
struct ExpectedFreshNativeComponent final {
    std::string expected_selector;
    std::string fresh_selector;
    std::string expected_population_id;
    std::string fresh_population_id;
    std::optional<double> predicted_native;
    std::optional<double> actual_fresh_native;
    m08_frozen::Endpoint predicted_endpoint{
        m08_frozen::Endpoint::ordinary};
    m08_frozen::Endpoint actual_endpoint{
        m08_frozen::Endpoint::ordinary};
    std::uint64_t expected_support_numerator{};
    std::uint64_t expected_support_denominator{};
    std::uint64_t actual_support_numerator{};
    std::uint64_t actual_support_denominator{};
    bool required{true};
    bool exact_fresh_native_transform_attested{};
};
struct M13BindingNativeEvidence final {
    std::string binding_id;
    std::string selected_source_feature_id;
    std::string exact_phase6_feature_dependency;
    bool current_authoritative_source_and_config{};
    std::vector<ExpectedFreshNativeComponent> configured;
};
struct M13BindingConformanceRow final {
    std::string binding_id;
    QtpCategory category{QtpCategory::generic_db};
    BindingConformance outcome{BindingConformance::unavailable};
    std::vector<QtpNumericPredicate> component_comparisons;
    std::optional<QtpProfilePredicate> t1_profile;
};
struct M13TwentySevenConformance final {
    std::array<M13BindingConformanceRow,27> rows;
    bool every_required_binding_complete{};
    bool every_required_binding_passed{};
    std::size_t configured_components{};
    std::size_t required_components{};
    [[nodiscard]] bool complete_passing_evidence() const noexcept {
        if(!every_required_binding_complete||!every_required_binding_passed||
           required_components==0U||required_components>configured_components)
            return false;
        std::size_t counted=0U,passed=0U;
        for(std::size_t i=0;i<rows.size();++i) {
            const auto& row=rows[i];
            const auto& binding=m07_frozen::kBindings[i];
            if(row.binding_id!=binding.binding_id||
               row.category!=kQtpBindings[i].category||
               row.component_comparisons.empty()||
               (binding.component_share==m07_frozen::ComponentShare::one&&
                row.component_comparisons.size()!=1U)||
               (binding.component_share==m07_frozen::ComponentShare::thirds&&
                row.component_comparisons.size()!=3U)||
               (row.outcome!=BindingConformance::passed&&
                row.outcome!=BindingConformance::not_applicable))
                return false;
        }
        return true;
    }
    [[nodiscard]] constexpr bool authorizes_release_verified()
        const noexcept {return false;}
};
[[nodiscard]] inline M13TwentySevenConformance
compare_m13_frozen_twenty_seven_native_bindings(
    const std::vector<M13BindingNativeEvidence>& evidence)
{
    M13TwentySevenConformance result;
    result.every_required_binding_complete=
        evidence.size()==kQtpBindings.size();
    result.every_required_binding_passed=
        evidence.size()==kQtpBindings.size();
    for(std::size_t i=0;i<kQtpBindings.size();++i) {
        auto& row=result.rows[i];
        row.binding_id=std::string(kQtpBindings[i].binding_id);
        row.category=kQtpBindings[i].category;
        if(i>=evidence.size()) {
            result.every_required_binding_complete=false;
            result.every_required_binding_passed=false;
            continue;
        }
        const auto& slot=evidence[i];
        const auto& frozen=m07_frozen::kBindings[i];
        if(slot.binding_id!=frozen.binding_id||
           (slot.selected_source_feature_id!=frozen.source_feature_id&&
            (frozen.exclusive_fallback_feature_id.empty()||
             slot.selected_source_feature_id!=
                frozen.exclusive_fallback_feature_id))||
           slot.exact_phase6_feature_dependency.empty()||
           !slot.current_authoritative_source_and_config||
           slot.configured.empty()||
           (frozen.component_share==m07_frozen::ComponentShare::one&&
            slot.configured.size()!=1U)||
           (frozen.component_share==m07_frozen::ComponentShare::thirds&&
            slot.configured.size()!=3U)) {
            result.every_required_binding_complete=false;
            result.every_required_binding_passed=false;
            continue;
        }
        // Each configured selector is a distinct frozen component; a
        // duplicate would count the same evidence twice in profile RMS.
        bool duplicate_selector=false;
        for(std::size_t j=0;j<slot.configured.size();++j) {
            const auto& selector=slot.configured[j].expected_selector;
            if(selector.empty())continue;
            for(std::size_t k=0;k<j;++k)
                if(slot.configured[k].expected_selector==selector)
                    duplicate_selector=true;
        }
        if(duplicate_selector) {
            row.outcome=BindingConformance::failed;
            result.every_required_binding_complete=false;
            result.every_required_binding_passed=false;
            continue;
        }
        row.outcome=BindingConformance::not_applicable;
        std::vector<double> expected_t1,actual_t1;
        bool missing=false,failed=false,has_required=false;
        for(const auto& component:slot.configured) {
            ++result.configured_components;
            if(!component.required) {
                row.component_comparisons.push_back({});
                continue; // retain original configured denominator/mass
            }
            ++result.required_components;
            has_required=true;
            // Absent structural evidence is not a measured disagreement.
            if(component.expected_selector.empty()||
               component.fresh_selector.empty()||
               component.expected_population_id.empty()||
               component.fresh_population_id.empty()||
               component.expected_support_denominator==0U||
               component.actual_support_denominator==0U) {
                missing=true;
                row.component_comparisons.push_back({});
                continue;
            }
            if(component.fresh_selector!=component.expected_selector||
               component.fresh_population_id!=
                   component.expected_population_id||
               component.actual_support_denominator!=
                   component.expected_support_denominator||
               component.actual_support_numerator!=
                   component.expected_support_numerator||
               component.expected_support_numerator>
                   component.expected_support_denominator||
               component.actual_support_numerator>
                   component.actual_support_denominator||
               component.predicted_endpoint!=component.actual_endpoint) {
                failed=true;
                row.component_comparisons.push_back({
                    QtpOutcome::mismatch,{},{}});
                continue;
            }
            if(!component.exact_fresh_native_transform_attested||
               !component.predicted_native||
               !component.actual_fresh_native||
               !std::isfinite(*component.predicted_native)||
               !std::isfinite(*component.actual_fresh_native)) {
                missing=true;
                row.component_comparisons.push_back({});
                continue;
            }
            if(row.category==QtpCategory::band_profile_db) {
                expected_t1.push_back(*component.predicted_native);
                actual_t1.push_back(*component.actual_fresh_native);
                const double delta=std::abs(
                    *component.actual_fresh_native-*component.predicted_native);
                const bool component_pass=std::isfinite(delta)&&delta<=0.01;
                if(!component_pass)failed=true;
                row.component_comparisons.push_back({
                    component_pass?QtpOutcome::pass:QtpOutcome::mismatch,
                    std::isfinite(delta)?std::optional<double>{delta}:std::nullopt,0.01});
            } else {
                auto check=compare_qtp_native_scalar(
                    row.category,component.predicted_native,
                    component.actual_fresh_native);
                if(check.outcome==QtpOutcome::unavailable)missing=true;
                if(check.outcome==QtpOutcome::mismatch)failed=true;
                row.component_comparisons.push_back(check);
            }
        }
        if(row.category==QtpCategory::band_profile_db&&
           !failed&&!missing&&has_required) {
            row.t1_profile=compare_qtp_t1_profile(
                expected_t1,actual_t1);
            if(row.t1_profile->outcome==QtpOutcome::mismatch)failed=true;
            if(row.t1_profile->outcome==QtpOutcome::unavailable)missing=true;
        }
        row.outcome=failed?BindingConformance::failed:
            missing?BindingConformance::unavailable:
            has_required?BindingConformance::passed:
                         BindingConformance::not_applicable;
        // Evaluated mismatch is COMPLETE but FAILED, not missing
        // evidence. P02 keeps that distinction even if other later
        // mandatory observations were unavailable.
        if(missing||row.outcome==BindingConformance::unavailable)
            result.every_required_binding_complete=false;
        if(row.outcome!=BindingConformance::passed&&has_required)
            result.every_required_binding_passed=false;
    }
    // A complete 27-slot nominal input with zero required components
    // does not supply a verifier's mandatory artistic evidence.
    if(result.required_components==0U) {
        result.every_required_binding_complete=false;
        result.every_required_binding_passed=false;
    }
    return result;
}
} // namespace rgsml::app::l3_automatch::m13_frozen
