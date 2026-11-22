#pragma once
// L3-M12 real PCM replay of a read-only, fully preflighted Product
// candidate. The existing RenderRequest/RenderPreview implementation
// processes three real ordered DSP segments: fixed MANUAL -> linked
// artist DSP -> optional *unchanged* user P2 terminal. M17 Export is
// not part of this path. No mutation, publish, verification or receipt.
#include "l3_m12_ref_linked_native_replay_plan.hpp"
#include <rgsml/render/render_request.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/core/result.hpp>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m12_frozen {
[[nodiscard]] inline core::Result<render::RenderResult>
render_restored_m12_full_native_pcm_once(
    const RestoredM12NativeReplayPlan& plan,
    const render::RenderResult& current_owned_prepared,
    core::FrameCount maximum_block_frames)
{
    using R=core::Result<render::RenderResult>;
    const auto fail=[](const char* reason){
        return R::failure(core::Error{core::ErrorCode::InvalidState,reason});
    };
    const auto original=current_owned_prepared.view();
    if(!plan.registry_owner||plan.project_id.is_nil()||
       plan.source_id.is_nil()||plan.reference_id.is_nil()||
       plan.plan_id.is_nil()||plan.linked_chain_id.is_nil()||
       plan.exact_plan_checksum.empty()||
       plan.exact_m10_winner_checksum.empty()||
       plan.exact_m11_protection_checksum.empty()||
       plan.exact_source_reference_scope_revision.empty()||
       plan.manual_prefix.instances().size()!=plan.manual_bindings.size()||
       plan.reference_linked.instances().size()!=
           plan.reference_bindings.size()||
       plan.terminal_p2.has_value()!=
           !plan.terminal_p2_bindings.empty()||
       original.absolute_start_frame().value()!=0||
       original.frame_count().value()<=0||
       original.timebase().frame_domain_id()!=
           audio::FrameDomainId::SOURCE_PROCESSING_RATE||
       maximum_block_frames.value()<=0)
        return fail("L3_M12_REPLAY_INVALID_PREPARED_OR_PLAN");
    const auto run=[&](const audio::AudioBufferView& input,
                       const dsp::ProcessingChain& chain,
                       std::span<const dsp::ModuleExecutionBinding> bindings)
                       ->R {
        auto request=render::RenderRequest::create(
            input,input.absolute_range(),chain,
            std::vector<dsp::ModuleExecutionBinding>{
                bindings.begin(),bindings.end()},maximum_block_frames);
        if(!request)return R::failure(*request.error());
        auto pcm=render::render_preview(*request.value(),*plan.registry_owner);
        if(!pcm)return pcm;
        if(pcm.value()->view().absolute_range()!=original.absolute_range()||
           pcm.value()->view().format()!=original.format())
            return fail("L3_M12_REPLAY_DSP_CHANGED_FRAME_DOMAIN");
        return pcm;
    };
    auto manual=run(original,plan.manual_prefix,plan.manual_bindings);
    if(!manual)return manual;
    auto linked=run(manual.value()->view(),plan.reference_linked,
                    plan.reference_bindings);
    if(!linked)return linked;
    if(!plan.terminal_p2)return linked;
    if(plan.terminal_p2_bindings.size()!=1U||
       plan.terminal_p2->instances().size()!=1U||
       plan.terminal_p2->instances().front().module_type_id()!=
           "rgsml.dsp.streaming-true-peak-limiter")
        return fail("L3_M12_REPLAY_TERMINAL_NOT_OWNED_P2");
    return run(linked.value()->view(),*plan.terminal_p2,
               plan.terminal_p2_bindings);
}
} // namespace rgsml::app::l3_automatch::m12_frozen
