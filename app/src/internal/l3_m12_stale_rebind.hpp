#pragma once
#include "l3_m12_owned_controller_suspension.hpp"
#include "l3_m12_product_reopen_preflight.hpp"
#include "l3_m12_product_preview_worker_ticket.hpp"
namespace rgsml::app::l3_automatch::m12_frozen {
struct StaleM12Rebind final {
    project::ProjectSnapshot suspended;
    std::shared_ptr<const M12ImmutableProductPreviewTicket> ticket;
};
[[nodiscard]] inline core::Result<StaleM12Rebind>
prepare_m12_stale_rebind(const project::ProjectSnapshot& prior,
    const project::ProjectSnapshot& live,const M12ImmutableProductPreviewTicket& old,
    const MasteringChainState& manual,std::shared_ptr<const render::RenderResult> prepared)
{
    using R=core::Result<StaleM12Rebind>;
    const auto fail=[](const char* why){return R::failure(core::Error{core::ErrorCode::InvalidState,why});};
    if(!old.replay||!prepared||prior.document().projectId!=live.document().projectId||
       !m12_reopened_persisted_replay_matches_exact_ticket(prior.document(),*old.replay)||
       prior.document().referenceMatch!=live.document().referenceMatch)
        return fail("L3_M12_STALE_REBIND_PRIOR_OWNER_UNAVAILABLE");
    const auto owned=[&](const project::ProjectDocument& doc)->const project::Chain* {
        for(const auto& c:doc.chains)if(c.chainId==old.replay->linked_chain_id&&
           c.stage=="MASTER"&&c.segment=="REF_LINKED")return &c;
        return nullptr;
    };
    const auto before=owned(prior.document()),current=owned(live.document());
    if(!before||!current||!exact_project_chain_equal(*before,*current))
        return fail("L3_M12_STALE_REBIND_LINKED_GRAPH_CHANGED");
    auto draft=prepare_m12_owned_controller_suspension(live,old.replay->controller_id,
        old.replay->linked_chain_id,ControllerSuspensionCause::plan_stale,true);
    if(!draft)return R::failure(*draft.error());
    auto suspended=std::move(draft.value()->updated_project_not_committed);
    const auto after=owned(suspended.document());
    if(!after||before->modules.size()!=after->modules.size())
        return fail("L3_M12_STALE_REBIND_LINKED_GRAPH_LOST");
    for(std::size_t i=0;i<after->modules.size();++i) {
        auto exact=before->modules[i];exact.controllerSuspended=true;
        if(!exact_project_module_equal(exact,after->modules[i]))
            return fail("L3_M12_STALE_REBIND_NON_STATE_MUTATION");
    }
    const auto rebound=prepare_m12_product_reopen_preflight(suspended.document(),manual,true);
    if(!rebound)return R::failure(*rebound.error());
    const auto next=prepare_m12_immutable_product_preview_ticket(rebound.value()->replay,
        std::move(prepared),manual.chain(),manual.execution_bindings());
    if(!next)return R::failure(*next.error());
    return R::success(StaleM12Rebind{std::move(suspended),
        std::make_shared<const M12ImmutableProductPreviewTicket>(*next.value())});
}
} // namespace rgsml::app::l3_automatch::m12_frozen
