#pragma once
// L3 M13 PRE-verifier native probe: one FRESH logical render and
// NEW M19/M20 analysis of ordinary current REF_LINKED DSP segment.
// Never reuses M10 candidate PCM; the actual post-M12 combined master,
// M11 protection, independent M16 TP, QTP and report remain mandatory.
// WAV Export M17 and optional Streaming TP P2 are NOT M13 prerequisites.
// No VERIFIED status exists at this preflight boundary.
#include "l3_m12_ref_linked_native_stage.hpp"
#include <rgsml/render/render_request.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/analysis/basic_level_analyzer.hpp>
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>

namespace rgsml::app::l3_automatch::m13_frozen {
struct FreshRefLinkedProbe final {
    render::RenderResult new_render_not_solver_pcm;
    analysis::BasicLevelAnalysisResult fresh_m19;
    analysis::TonalStereoAnalysisResult fresh_m20;
    std::uint32_t verification_evaluations{1U};
    [[nodiscard]] constexpr bool is_m13_verified()
        const noexcept{return false;}
    [[nodiscard]] constexpr bool authorizes_export()
        const noexcept{return false;}
};
[[nodiscard]] inline core::Result<FreshRefLinkedProbe>
probe_fresh_m13_ref_linked_segment_once(
    const m12_frozen::StagedRefLinkedSegment& actual_linked,
    const render::RenderResult& current_prepared,
    core::FrameCount maximum_block_frames,
    bool exact_current_m12_materialized_receipt_and_owner_attested)
{
    using R=core::Result<FreshRefLinkedProbe>;
    const auto fail=[](const char* reason) {
        return R::failure(core::Error{core::ErrorCode::InvalidState,reason});
    };
    if(!exact_current_m12_materialized_receipt_and_owner_attested||
       !actual_linked.registry_owner||
       actual_linked.chain.context()!=dsp::ProcessingChainContext{
           dsp::ProcessingStage::MASTER,dsp::ChainSegment::REF_LINKED}||
       actual_linked.chain.instances().size()!=
           actual_linked.exact_bindings.size()||
       maximum_block_frames.value()<=0||
       current_prepared.view().absolute_start_frame().value()!=0||
       current_prepared.view().frame_count().value()<=0||
       current_prepared.view().timebase().frame_domain_id()!=
           audio::FrameDomainId::SOURCE_PROCESSING_RATE)
        return fail("L3_M13_M12_MATERIALIZATION_NOT_CURRENT");
    for(std::size_t i=0;i<actual_linked.exact_bindings.size();++i)
        if(!binding_matches_instance(actual_linked.chain.instances()[i],
                                     actual_linked.exact_bindings[i]))
            return fail("L3_M13_ACTUAL_LINKED_BINDINGS_MISMATCH");
    const auto source=current_prepared.view();
    auto request=render::RenderRequest::create(
        source,source.absolute_range(),actual_linked.chain,
        actual_linked.exact_bindings,maximum_block_frames);
    if(!request)return R::failure(*request.error());
    // Even an exact same DSP candidate has to be rendered anew:
    // no M10 RenderResult or measured score enters this function.
    auto fresh=render::render_preview(
        *request.value(),*actual_linked.registry_owner);
    if(!fresh)return R::failure(*fresh.error());
    if(fresh.value()->view().absolute_range()!=source.absolute_range()||
       fresh.value()->view().format()!=source.format())
        return fail("L3_M13_FRESH_RENDER_GEOMETRY_MISMATCH");
    auto m19=analysis::measure_basic_level_full_resource(
        fresh.value()->buffer());
    if(!m19)return R::failure(*m19.error());
    const auto v=fresh.value()->view();
    auto m20=analysis::measure_tonal_stereo_scope(
        v,v.absolute_start_frame(),v.frame_count(),
        v.timebase().frame_domain_id());
    if(!m20)return R::failure(*m20.error());
    return R::success(FreshRefLinkedProbe{
        std::move(*fresh.value()),std::move(*m19.value()),
        std::move(*m20.value()),1U});
}
} // namespace rgsml::app::l3_automatch::m13_frozen
