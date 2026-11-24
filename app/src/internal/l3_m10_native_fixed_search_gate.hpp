#pragma once
// Independent Fixed evidence can prove no correction before M09 top8
// or M10 top32. Missing coverage or safety cannot be a numeric zero J.
#include "l3_m10_preserved_native_search.hpp"
#include "l3_m11_native_fixed_assessment.hpp"

namespace rgsml::app::l3_automatch::m10_frozen {
enum class NativeFixedSearchStatus:std::uint8_t {
    unavailable,no_correction_required,correction_required,
    targets_met_but_safety_unavailable_or_failed
};
struct NativeFixedSearchGate final {
    NativeFixedSearchStatus status{NativeFixedSearchStatus::unavailable};
    m11_frozen::NativeFixedTargetAssessment targets;
};
[[nodiscard]] inline NativeFixedSearchGate assess_native_fixed_search_gate(
    const PreservedNativeSearchEnvironment& env,
    const m08_frozen::AmountConfig& amount,
    const std::array<bool,4>& requested,
    bool independent_fixed_safety_scope_current)
{
    NativeFixedSearchGate out;
    const auto& fixed=env.fixed;
    const auto& owners=env.owners;
    if(!owners.current_qt_product_epoch_and_m09_m11_attested||
       !owners.current_owner.populated()||!owners.current_prepared||
       !owners.frozen_registry||env.generation.empty()||
       !fixed.original_definition_authority||
       !std::isfinite(env.m11_ceiling_db_tp)||env.m11_ceiling_db_tp>0.0||
       !resolve_original_definitions_from_fixed(fixed,env.originals,
            owners.current_owner,env.generation,fixed.exact_fixed_ticket+"/gate"))
        return out;
    auto empty=dsp::ProcessingChain::restore(*owners.frozen_registry,
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::REF_LINKED},1U,{});
    if(!empty)return out;
    auto job=M10P2RealNativeProductJob::create_preserving_user_terminal(
        owners.current_owner,owners.current_prepared,owners.frozen_registry,
        owners.frozen_manual,owners.frozen_manual_bindings,*empty.value(),{},
        fixed.exact_fixed_ticket,owners.maximum_block_frames,true);
    if(!job||!job.value()->fixed_only()||
       fixed.exact_fixed_graph_content.empty()||
       job.value()->exact_fixed_graph_content()!=
           std::optional<ExactCacheBytes>{fixed.exact_fixed_graph_content})
        return out;
    out.targets=m11_frozen::assess_native_fixed_targets(fixed,amount,requested);
    if(!out.targets.valid_current)return out;
    if(!out.targets.all_requested_targets_met) {
        out.status=NativeFixedSearchStatus::correction_required;
        return out;
    }
    const bool stereo=env.originals.prepared.original_m19_m20.channels==2U;
    const bool safe=independent_fixed_safety_scope_current&&
        fixed.measured_m16_linear_peak&&
        check_actual_p2_m16_linear_peak(*fixed.measured_m16_linear_peak,
                                      env.m11_ceiling_db_tp).passes()&&
        fixed.stereo_safety_required==stereo&&
        (!stereo||(fixed.measured_stereo_safety&&
          m11_frozen::verify_m11_native_stereo_hard_safety(
              *fixed.measured_stereo_safety).passes_hard_gate()));
    out.status=safe?NativeFixedSearchStatus::no_correction_required:
        NativeFixedSearchStatus::targets_met_but_safety_unavailable_or_failed;
    return out;
}
} // namespace rgsml::app::l3_automatch::m10_frozen
