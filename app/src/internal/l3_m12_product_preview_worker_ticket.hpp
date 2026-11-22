#pragma once
// L3-M12 immutable bridge to the EXISTING Product PreviewJob contract.
// This is a nonpublishing, read-only preflight: ProjectSession must
// acquire a real owner transaction before future live installation.
// The worker never chooses a source, ref, DSP, P2 value or M17 ceiling.
#include "l3_m12_restored_native_render.hpp"
#include "mastering_preview_controller.hpp"
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <algorithm>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m12_frozen {
struct M12ImmutableProductPreviewTicket final {
    std::shared_ptr<const RestoredM12NativeReplayPlan> replay;
    std::shared_ptr<const render::RenderResult> exact_prepared_owner;
    std::vector<dsp::ModuleInstance> exact_manual_nodes;
    std::vector<dsp::ModuleExecutionBinding> exact_manual_bindings;
    [[nodiscard]] constexpr bool authorizes_product_installation() const noexcept {
        return false;
    }
    [[nodiscard]] constexpr bool authorizes_m12_receipt() const noexcept {
        return false;
    }
    [[nodiscard]] constexpr bool authorizes_m13_verified() const noexcept {
        return false;
    }
};
[[nodiscard]] inline core::Result<M12ImmutableProductPreviewTicket>
prepare_m12_immutable_product_preview_ticket(
    std::shared_ptr<const RestoredM12NativeReplayPlan> replay,
    std::shared_ptr<const render::RenderResult> exact_current_prepared,
    const dsp::ProcessingChain& current_manual,
    std::span<const dsp::ModuleExecutionBinding> current_manual_bindings)
{
    using R=core::Result<M12ImmutableProductPreviewTicket>;
    const auto fail=[](const char* reason){
        return R::failure(core::Error{core::ErrorCode::InvalidState,reason});
    };
    if(!replay||!replay->registry_owner||!exact_current_prepared||
       current_manual.context()!=dsp::ProcessingChainContext{
           dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL}||
       current_manual.instances().size()!=current_manual_bindings.size())
        return fail("L3_M12_WORKER_NO_EXACT_PRODUCT_OWNERS");
    const auto nodes=current_manual.instances();
    const auto prefix=replay->manual_prefix.instances();
    const auto has_terminal=replay->terminal_p2.has_value();
    if(nodes.size()!=prefix.size()+(has_terminal?1U:0U)||
       replay->manual_bindings.size()!=prefix.size()||
       replay->terminal_p2_bindings.size()!=(has_terminal?1U:0U)||
       !std::equal(nodes.begin(),nodes.begin()+prefix.size(),prefix.begin())||
       !std::equal(current_manual_bindings.begin(),
                   current_manual_bindings.begin()+prefix.size(),
                   replay->manual_bindings.begin()))
        return fail("L3_M12_WORKER_PROTECTED_FIXED_CHANGED");
    if(has_terminal&&
       (replay->terminal_p2->instances().size()!=1U||
        nodes.back()!=replay->terminal_p2->instances().front()||
        current_manual_bindings.back()!=replay->terminal_p2_bindings.front()))
        return fail("L3_M12_WORKER_TERMINAL_CHANGED");
    const auto prepared=exact_current_prepared->view();
    if(prepared.absolute_start_frame().value()!=0||
       prepared.frame_count().value()<=0||
       prepared.timebase().frame_domain_id()!=
           audio::FrameDomainId::SOURCE_PROCESSING_RATE)
        return fail("L3_M12_WORKER_PREPARED_INVALID");
    return R::success(M12ImmutableProductPreviewTicket{
        std::move(replay),std::move(exact_current_prepared),
        {nodes.begin(),nodes.end()},
        {current_manual_bindings.begin(),current_manual_bindings.end()}});
}
[[nodiscard]] inline core::Result<render::RenderResult>
render_m12_exact_product_preview_job(
    const M12ImmutableProductPreviewTicket& ticket,
    const MasteringPreviewController::PreviewJob& job,
    core::FrameCount maximum_block_frames)
{
    using R=core::Result<render::RenderResult>;
    if(!ticket.replay||!job.generation||
       !job.registry||!job.preparedSnapshot||
       job.registry!=ticket.replay->registry_owner||
       job.preparedSnapshot!=ticket.exact_prepared_owner||
       job.chain.context()!=dsp::ProcessingChainContext{
           dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL}||
       job.chain.instances().size()!=ticket.exact_manual_nodes.size()||
       !std::equal(job.chain.instances().begin(),
                   job.chain.instances().end(),
                   ticket.exact_manual_nodes.begin())||
       job.bindings!=ticket.exact_manual_bindings)
        return R::failure(core::Error{
            core::ErrorCode::InvalidState,
            "L3_M12_WORKER_PREVIEW_OWNER_OR_MANUAL_STALE"});
    return render_restored_m12_full_native_pcm_once(
        *ticket.replay,*ticket.exact_prepared_owner,maximum_block_frames);
}
} // namespace rgsml::app::l3_automatch::m12_frozen
