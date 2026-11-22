#pragma once
// L3 M13 independent fresh FULL-MASTER native render preflight.
// Runs the genuine frozen Product segments in original logical order:
// Prepared -> protected MANUAL -> current REF_LINKED winner.
// Its true peak is measured by M16 on actual fresh PCM. It never
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
       !std::isfinite(exact_frozen_automatch_m16_safety_max_db_tp)||
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
    // The selected native candidate is the complete exact in-memory
    // SOURCE_PROCESSING_RATE output BEFORE exporting. TruePeak M16 is
    // fresh, whole-resource and independent of M17/P2 ceiling settings.
    const auto m16=analysis::measure_true_peak_full_resource(
        linked.value()->buffer());
    if(!m16)return R::failure(*m16.error());
    const auto measured_linear=m16.value()->global_linear_peak();
    const auto safe_linear=std::pow(
        10.0,exact_frozen_automatch_m16_safety_max_db_tp/20.0);
    if(!std::isfinite(measured_linear)||!std::isfinite(safe_linear)||
       safe_linear<=0.0||measured_linear>safe_linear)
        return fail("L3_M13_FRESH_M16_AUTOMATCH_SAFETY_EXCEEDED");
    auto m19=analysis::measure_basic_level_full_resource(
        linked.value()->buffer());
    if(!m19)return R::failure(*m19.error());
    const auto v=linked.value()->view();
    auto m20=analysis::measure_tonal_stereo_scope(
        v,v.absolute_start_frame(),v.frame_count(),
        v.timebase().frame_domain_id());
    if(!m20)return R::failure(*m20.error());
    return R::success(FreshNativeMasterCompositionProbe{
        std::move(*linked.value()),std::move(*m16.value()),
        std::move(*m19.value()),std::move(*m20.value()),
        exact_frozen_automatch_m16_safety_max_db_tp,1U,2U});
}
} // namespace rgsml::app::l3_automatch::m13_frozen
