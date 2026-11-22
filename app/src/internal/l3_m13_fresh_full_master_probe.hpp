#pragma once
// L3 M13 independent fresh FULL-MASTER native render preflight.
// Replays the genuine Product segments as PREPARED -> protected MANUAL
// fixed DSP -> current REF_LINKED winner -> optional active P2 suffix.
// An explicit, typed M10 P2 winner is required when manual P2 is active;
// without it the probe fails closed rather than misordering the limiter.
// True Peak is measured with M16 on the actual final fresh PCM. It never
// invokes M17 Export TP Limiter, overwrites independent M17 ceiling,
// or modifies optional P2 Streaming TP Limiter configuration.
#include "l3_m13_fresh_native_verification_probe.hpp"
#include <rgsml/analysis/true_peak_measurement.hpp>
#include <rgsml/analysis/basic_level_analyzer.hpp>
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <rgsml/render/render_request.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include <utility>

namespace rgsml::app::l3_automatch::m13_frozen {
struct FreshNativeMasterCompositionProbe final {
    // Pure actual SOURCE_PROCESSING_RATE DSP result BEFORE WAV Export.
    render::RenderResult independent_match_render;
    analysis::TruePeakMeasurement authentic_full_resource_m16;
    analysis::BasicLevelAnalysisResult independent_final_m19;
    analysis::TonalStereoAnalysisResult independent_final_m20;
    double frozen_automatch_safety_max_db_tp{};
    std::uint32_t logical_verification_evaluations{1U};
    std::uint32_t genuine_dsp_segment_passes{2U};
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
    const render::RenderResult& current_prepared,
    core::FrameCount maximum_block_frames,
    double exact_frozen_automatch_m16_safety_max_db_tp,
    bool current_materialized_m12_receipt_verified_by_product_owner,
    bool current_full_m11_fixed_and_independent_safety_attested,
    std::optional<dsp::StreamingTruePeakLimiterParameters>
        exact_m10_winning_p2_parameters=std::nullopt)
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
       !std::isfinite(exact_frozen_automatch_m16_safety_max_db_tp)||
       maximum_block_frames.value()<=0)
        return fail("L3_M13_FULL_MASTER_M12_M11_OR_OWNER_INVALID");
    // P2 may be manually present, but a live P2 may NEVER be rendered
    // before the artistic REF_LINKED corrections. M10 supplies the
    // validated winning Ceiling/Drive typed binding; the manual graph is
    // immutable and its Margin/user bypass remain protected.
    bool p2_present=false;
    std::optional<std::size_t> active_p2_at;
    for(std::size_t i=0;i<protected_manual_bindings.size();++i) {
        const auto& instance=protected_manual.instances()[i];
        if(!binding_matches_instance(instance,protected_manual_bindings[i])||
           instance.provenance()!=dsp::ModuleProvenance::MANUAL||
           instance.owner()!=dsp::ModuleOwner::USER||
           instance.link_state()!=dsp::ModuleLinkState::UNLINKED)
            return fail("L3_M13_FULL_MASTER_PROTECTED_MANUAL_MISMATCH");
        if(instance.module_type_id()==
               "rgsml.dsp.streaming-true-peak-limiter") {
            if(p2_present||i+1U!=protected_manual_bindings.size())
                return fail("L3_M13_P2_MANUAL_TERMINAL_ORDER_INVALID");
            p2_present=true;
            if(instance.active())active_p2_at=i;
        }
    }
    if(active_p2_at.has_value()!=
           exact_m10_winning_p2_parameters.has_value())
        return fail("L3_M13_P2_WINNER_MISSING_OR_MANUAL_BYPASS");
    if(active_p2_at) {
        const auto* manual_p2=
            std::get_if<dsp::StreamingTruePeakLimiterParameters>(
                &protected_manual_bindings[*active_p2_at].parameters);
        if(!manual_p2||manual_p2->margin_percent()!=
                          exact_m10_winning_p2_parameters->margin_percent())
            return fail("L3_M13_P2_PROTECTED_MARGIN_CHANGED");
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
            source,source.absolute_range(),chain,
            std::vector<dsp::ModuleExecutionBinding>{
                bindings.begin(),bindings.end()},maximum_block_frames);
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
    std::optional<render::RenderResult> selected;
    std::uint32_t segment_passes=2U;
    if(active_p2_at) {
        // Keep the original MANUAL chain untouched in memory and Project.
        // Recompose its exact fixed prefix, the current REF_LINKED winner,
        // and the same manual P2 instance with the M10 winner parameters.
        const auto nodes=protected_manual.instances();
        auto fixed=dsp::ProcessingChain::restore(
            *current_linked.registry_owner,protected_manual.context(),
            protected_manual.revision(),
            {nodes.begin(),nodes.end()-1});
        if(!fixed)return R::failure(*fixed.error());
        std::vector<dsp::ModuleExecutionBinding> fixed_bindings{
            protected_manual_bindings.begin(),
            protected_manual_bindings.end()-1};
        auto manual=run_stage(original,*fixed.value(),fixed_bindings);
        if(!manual)return R::failure(*manual.error());
        auto linked=run_stage(
            manual.value()->view(),
            current_linked.chain,current_linked.exact_bindings);
        if(!linked)return R::failure(*linked.error());
        auto p2=dsp::ProcessingChain::restore(
            *current_linked.registry_owner,protected_manual.context(),
            protected_manual.revision(),{nodes.back()});
        if(!p2)return R::failure(*p2.error());
        const std::vector<dsp::ModuleExecutionBinding> p2_binding{{
            nodes.back().instance_id(),*exact_m10_winning_p2_parameters}};
        auto limited=run_stage(linked.value()->view(),*p2.value(),
                               p2_binding);
        if(!limited)return R::failure(*limited.error());
        selected.emplace(std::move(*limited.value()));
        segment_passes=3U;
    } else {
        auto manual=run_stage(
            original,protected_manual,protected_manual_bindings);
        if(!manual)return R::failure(*manual.error());
        auto linked=run_stage(
            manual.value()->view(),
            current_linked.chain,current_linked.exact_bindings);
        if(!linked)return R::failure(*linked.error());
        selected.emplace(std::move(*linked.value()));
    }
    // Fresh actual SOURCE_RATE PCM and M16 analysis, never M17 WAV export.
    const auto m16=analysis::measure_true_peak_full_resource(
        selected->buffer());
    if(!m16)return R::failure(*m16.error());
    const auto measured_linear=m16.value()->global_linear_peak();
    const auto safe_linear=std::pow(
        10.0,exact_frozen_automatch_m16_safety_max_db_tp/20.0);
    if(!std::isfinite(measured_linear)||!std::isfinite(safe_linear)||
       safe_linear<=0.0||measured_linear>safe_linear)
        return fail("L3_M13_FRESH_M16_AUTOMATCH_SAFETY_EXCEEDED");
    auto m19=analysis::measure_basic_level_full_resource(
        selected->buffer());
    if(!m19)return R::failure(*m19.error());
    const auto v=selected->view();
    auto m20=analysis::measure_tonal_stereo_scope(
        v,v.absolute_start_frame(),v.frame_count(),
        v.timebase().frame_domain_id());
    if(!m20)return R::failure(*m20.error());
    return R::success(FreshNativeMasterCompositionProbe{
        std::move(*selected),std::move(*m16.value()),
        std::move(*m19.value()),std::move(*m20.value()),
        exact_frozen_automatch_m16_safety_max_db_tp,1U,segment_passes});
}
} // namespace rgsml::app::l3_automatch::m13_frozen
