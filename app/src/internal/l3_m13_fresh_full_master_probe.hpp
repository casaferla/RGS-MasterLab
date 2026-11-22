#pragma once
// L3 M13 independent fresh FULL-MASTER native render preflight.
// Runs the genuine frozen Product segments in original logical order:
// current Prepared -> protected MANUAL -> REF_LINKED accepted winner
// -> existing protected TERMINAL/M17. This is one *logical* verifier
// render, never an M10 candidate PCM or M13 VERIFIED report.
#include "l3_m13_fresh_native_verification_probe.hpp"
#include "l3_m12_terminal_native_hard_preflight.hpp"
#include <rgsml/render/true_peak_limiter_terminal_candidate.hpp>
#include <rgsml/audio/full_resource_output_rate_candidate.hpp>
#include <rgsml/dsp/true_peak_limiter_parameters.hpp>
#include <rgsml/analysis/basic_level_analyzer.hpp>
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <rgsml/render/render_request.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>

namespace rgsml::app::l3_automatch::m13_frozen {
struct FreshNativeMasterCompositionProbe final {
    // The final domain is OUTPUT_RATE, never a fake SOURCE_RATE
    // RenderPreview terminal result. No export/publication authority.
    audio::AudioBuffer independent_final_output_pcm;
    render::TruePeakLimiterFinalCheck independent_m17_m16_check;
    analysis::BasicLevelAnalysisResult independent_final_m19;
    analysis::TonalStereoAnalysisResult independent_final_m20;
    std::uint64_t genuine_m17_applied_gain_witness_frames{};
    std::uint32_t logical_verification_evaluations{1U};
    std::uint32_t genuine_dsp_segment_passes{3U};
    [[nodiscard]] constexpr bool is_m13_verified() const noexcept {
        return false;
    }
    [[nodiscard]] constexpr bool authorizes_master_export() const noexcept {
        return false;
    }
};
[[nodiscard]] inline core::Result<FreshNativeMasterCompositionProbe>
probe_fresh_m13_full_native_master_once(
    const m12_frozen::StagedRefLinkedSegment& current_linked,
    const dsp::ProcessingChain& protected_manual,
    std::span<const dsp::ModuleExecutionBinding> protected_manual_bindings,
    const dsp::ProcessingChain& current_terminal,
    std::span<const dsp::ModuleExecutionBinding> current_terminal_bindings,
    const render::RenderResult& current_prepared,
    core::FrameCount maximum_block_frames,
    double exact_accepted_terminal_ceiling_db_tp,
    bool current_materialized_m12_receipt_verified_by_product_owner,
    bool current_full_m11_fixed_and_independent_safety_attested)
{
    using R=core::Result<FreshNativeMasterCompositionProbe>;
    const auto fail=[](const char* why) {
        return R::failure(core::Error{core::ErrorCode::InvalidState,why});
    };
    if(!current_materialized_m12_receipt_verified_by_product_owner||
       !current_full_m11_fixed_and_independent_safety_attested||
       !current_linked.registry_owner||
       !current_linked.accepted_identity.
           current_source_reference_owner_and_plan_attested||
       current_linked.accepted_identity.plan_id.is_nil()||
       current_linked.accepted_identity.
           exact_plan_content_checksum.empty()||
       current_linked.chain.context()!=dsp::ProcessingChainContext{
          dsp::ProcessingStage::MASTER,dsp::ChainSegment::REF_LINKED}||
       protected_manual.context()!=dsp::ProcessingChainContext{
          dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL}||
       protected_manual.instances().size()!=protected_manual_bindings.size()||
       protected_manual.revision()!=
           current_linked.frozen_protected_manual_revision||
       protected_manual.instances().size()!=
           current_linked.frozen_protected_manual_instances.size()||
       !std::equal(protected_manual.instances().begin(),
                   protected_manual.instances().end(),
                   current_linked.frozen_protected_manual_instances.begin())||
       protected_manual_bindings.size()!=
           current_linked.frozen_protected_manual_bindings.size()||
       !std::equal(protected_manual_bindings.begin(),
                   protected_manual_bindings.end(),
                   current_linked.frozen_protected_manual_bindings.begin())||
       current_linked.chain.instances().size()!=
           current_linked.exact_bindings.size()||
       maximum_block_frames.value()<=0)
        return fail("L3_M13_FULL_MASTER_M12_M11_OR_OWNER_INVALID");
    for(std::size_t i=0;i<protected_manual_bindings.size();++i) {
        const auto& instance=protected_manual.instances()[i];
        if(!binding_matches_instance(instance,protected_manual_bindings[i])||
           instance.provenance()!=dsp::ModuleProvenance::MANUAL||
           instance.owner()!=dsp::ModuleOwner::USER||
           instance.link_state()!=dsp::ModuleLinkState::UNLINKED)
            return fail("L3_M13_FULL_MASTER_PROTECTED_MANUAL_MISMATCH");
    }
    for(std::size_t i=0;i<current_linked.exact_bindings.size();++i) {
        const auto& instance=current_linked.chain.instances()[i];
        if(!binding_matches_instance(
            instance,current_linked.exact_bindings[i])||
           instance.provenance()!=
              dsp::ModuleProvenance::REFERENCE_MATCH||
           instance.owner()!=dsp::ModuleOwner::REFERENCE_MATCH||
           instance.link_state()!=dsp::ModuleLinkState::REF_LINKED)
            return fail("L3_M13_FULL_MASTER_LINKED_BINDINGS_MISMATCH");
        for(const auto& fixed:protected_manual.instances())
            if(instance.instance_id()==fixed.instance_id())
                return fail("L3_M13_FULL_MASTER_MANUAL_ID_COLLISION");
    }
    const auto terminal=
        m12_frozen::verify_current_m12_terminal_limiter_composition(
            current_terminal,current_terminal_bindings,
            exact_accepted_terminal_ceiling_db_tp,
            current_full_m11_fixed_and_independent_safety_attested);
    if(!terminal.admissible_without_new_terminal_action())
        return fail("L3_M13_FULL_MASTER_TERMINAL_TRUE_PEAK_UNSAFE");
    const auto original=current_prepared.view();
    if(original.absolute_start_frame().value()!=0||
       original.frame_count().value()<=0||
       original.timebase().frame_domain_id()!=
           audio::FrameDomainId::SOURCE_PROCESSING_RATE)
        return fail("L3_M13_FULL_MASTER_PREPARED_NOT_CURRENT");
    // Deliberately NEW native RenderPreview at each stage. M10 PCM
    // and prior achieved features are not supplied or accepted.
    const auto run_stage=[&](
        const audio::AudioBufferView& source,
        const dsp::ProcessingChain& chain,
        std::span<const dsp::ModuleExecutionBinding> bindings)
        ->core::Result<render::RenderResult> {
        auto request=render::RenderRequest::create(
            source,source.absolute_range(),chain,bindings,
            maximum_block_frames);
        if(!request)return core::Result<render::RenderResult>::failure(
            *request.error());
        auto rendered=render::render_preview(
            *request.value(),*current_linked.registry_owner);
        if(!rendered)return rendered;
        if(rendered.value()->view().absolute_range()!=
              original.absolute_range()||
           rendered.value()->view().format()!=original.format())
            return core::Result<render::RenderResult>::failure(
                core::Error{core::ErrorCode::InvalidState,
                            "L3_M13_FULL_MASTER_SEGMENT_SCOPE_CHANGED"});
        return rendered;
    };
    auto manual=run_stage(
        original,protected_manual,protected_manual_bindings);
    if(!manual)return R::failure(*manual.error());
    auto linked=run_stage(
        manual.value()->view(),
        current_linked.chain,current_linked.exact_bindings);
    if(!linked)return R::failure(*linked.error());
    // M17 MUST run on a true full-resource OUTPUT_RATE owner. The
    // legacy RenderPreview intentionally refuses active M17 and MUST
    // NOT be used for this terminal phase. First integrate the native
    // same-rate full-resource Source->Output bridge. Future nonidentity
    // master SRC requires the exact pinned final Q0 asset/policy.
    const auto source_rate=original.format().sample_rate();
    auto output=audio::materialize_full_resource_output_rate_candidate(
        linked.value()->view(),source_rate,maximum_block_frames);
    if(!output)return R::failure(*output.error());
    if(output.value()->used_src||
       output.value()->pcm.view().frame_count()!=
           original.frame_count()||
       output.value()->pcm.view().timebase().frame_domain_id()!=
           audio::FrameDomainId::OUTPUT_RATE)
        return fail("L3_M13_FULL_MASTER_OUTPUT_DOMAIN_OR_SRC_STALE");
    auto terminal_output=render::
        render_true_peak_limiter_bound_terminal_candidate(
            output.value()->pcm.view(),current_terminal,
            current_terminal_bindings,
            *current_linked.registry_owner,maximum_block_frames);
    if(!terminal_output)
        return R::failure(*terminal_output.error());
    auto& checked=terminal_output.value()->checked;
    if(checked.terminal.bypassed||!checked.final_check||
       checked.terminal.applied_gain_frames.size()!=
           static_cast<std::size_t>(original.frame_count().value()))
        return fail("L3_M13_FULL_MASTER_M17_OR_M16_INCOMPLETE");
    const auto witness=checked.terminal.applied_gain_frames.size();
    auto m19=analysis::measure_basic_level_full_resource(
        checked.terminal.compensated_pcm);
    if(!m19)return R::failure(*m19.error());
    const auto v=checked.terminal.compensated_pcm.view();
    auto m20=analysis::measure_tonal_stereo_scope(
        v,v.absolute_start_frame(),v.frame_count(),
        v.timebase().frame_domain_id());
    if(!m20)return R::failure(*m20.error());
    return R::success(FreshNativeMasterCompositionProbe{
        std::move(checked.terminal.compensated_pcm),
        *checked.final_check,
        std::move(*m19.value()),std::move(*m20.value()),
        static_cast<std::uint64_t>(witness),1U,3U});
}
} // namespace rgsml::app::l3_automatch::m13_frozen
