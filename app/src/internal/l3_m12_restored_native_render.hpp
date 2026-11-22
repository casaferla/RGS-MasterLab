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
    // The worker must validate the live, typed graph itself, not merely
    // trust a prepared ticket supplied by another component. Each native
    // segment retains its frozen owner/link semantics and one exact binding
    // per module. A forged aggregate plan is never executable.
    const auto valid_segment=[&](
        const dsp::ProcessingChain& chain,
        const dsp::ProcessingChainContext expected,
        const std::span<const dsp::ModuleExecutionBinding> bindings,
        const bool is_reference) {
        if(chain.context()!=expected||
           chain.instances().size()!=bindings.size())
            return false;
        for(std::size_t i=0;i<bindings.size();++i) {
            const auto& node=chain.instances()[i];
            if(!binding_matches_instance(node,bindings[i])||
               !plan.registry_owner||
               !plan.registry_owner->has_factory(node.module_type_id()))
                return false;
            if(is_reference) {
                if(node.provenance()!=dsp::ModuleProvenance::REFERENCE_MATCH||
                   node.owner()!=dsp::ModuleOwner::REFERENCE_MATCH||
                   node.link_state()!=dsp::ModuleLinkState::REF_LINKED||
                   !node.semantic_node_id()||
                   node.semantic_node_id()->is_nil()||
                   node.module_type_id()=="rgsml.dsp.gain"||
                   node.module_type_id()=="rgsml.dsp.streaming-true-peak-limiter")
                    return false;
            } else if(node.provenance()!=dsp::ModuleProvenance::MANUAL||
                      node.owner()!=dsp::ModuleOwner::USER||
                      node.link_state()!=dsp::ModuleLinkState::UNLINKED||
                      node.semantic_node_id())
                return false;
        }
        return true;
    };
    if(!plan.registry_owner)return fail("L3_M12_REPLAY_REGISTRY_MISSING");
    if(!plan.manual_prefix.instances().empty()&&
       plan.manual_prefix.instances().front().module_type_id()!=
           "rgsml.dsp.gain")
        return fail("L3_M12_REPLAY_INPUT_GAIN_NOT_PROTECTED_FIRST");
    // Direct aggregates cannot smuggle a second P2 inside MANUAL or
    // collide a controller-owned node with a protected user module.
    if(std::any_of(plan.manual_prefix.instances().begin(),
                   plan.manual_prefix.instances().end(),
                   [](const dsp::ModuleInstance& node){
                       return node.module_type_id()==
                           "rgsml.dsp.streaming-true-peak-limiter";
                   }))
        return fail("L3_M12_REPLAY_DUPLICATE_TERMINAL_P2");
    for(const auto& linked:plan.reference_linked.instances()) {
        if(linked.module_type_id()=="rgsml.dsp.true-peak-limiter"||
           linked.module_type_id()=="rgsml.dsp.dither")
            return fail("L3_M12_REPLAY_EXPORT_SAFETY_IN_INLINE_REF");
        for(const auto& manual:plan.manual_prefix.instances())
            if(linked.instance_id()==manual.instance_id())
                return fail("L3_M12_REPLAY_MANUAL_REF_ID_COLLISION");
        if(plan.terminal_p2)
            for(const auto& terminal:plan.terminal_p2->instances())
                if(linked.instance_id()==terminal.instance_id())
                    return fail("L3_M12_REPLAY_REF_P2_ID_COLLISION");
    }
    if(!valid_segment(plan.manual_prefix,{
           dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL},
           plan.manual_bindings,false)||
       !valid_segment(plan.reference_linked,{
           dsp::ProcessingStage::MASTER,dsp::ChainSegment::REF_LINKED},
           plan.reference_bindings,true)||
       (plan.terminal_p2 &&
        !valid_segment(*plan.terminal_p2,{
            dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL},
            plan.terminal_p2_bindings,false)))
        return fail("L3_M12_REPLAY_OWNER_BINDING_OR_SEGMENT_INVALID");
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
