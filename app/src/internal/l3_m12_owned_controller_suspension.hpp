#pragma once
// L3 M12 Product-only Amount-zero and stale non-destructive suspension.
// Exact owner+REF_LINKED identity; only controllerSuspended changes.
// No silent unlink/delete, bypass rewrite or manual module takeover.
#include "l3_m12_project_transaction_draft.hpp"
#include <rgsml/project/project_snapshot.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/core/error.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace rgsml::app::l3_automatch::m12_frozen {
enum class ControllerSuspensionCause:std::uint8_t {
    amount_zero,plan_stale,current_amount_restored
};
struct M12ControllerSuspensionDraft final {
    project::ProjectSnapshot updated_project_not_committed;
    bool controller_suspended{};
    ControllerSuspensionCause cause{};
    [[nodiscard]] constexpr bool authorizes_persisted_commit()
        const noexcept{return false;}
};
[[nodiscard]] inline core::Result<M12ControllerSuspensionDraft>
prepare_m12_owned_controller_suspension(
    const project::ProjectSnapshot& live,
    core::Uuid current_controller_id,
    core::Uuid original_ref_chain_id,
    ControllerSuspensionCause cause,
    bool current_exclusive_product_owner_epoch_and_plan_admitted)
{
    using R=core::Result<M12ControllerSuspensionDraft>;
    const auto fail=[](const char* why) {
        return R::failure(core::Error{core::ErrorCode::InvalidState,why});
    };
    if(!current_exclusive_product_owner_epoch_and_plan_admitted||
       current_controller_id.is_nil()||original_ref_chain_id.is_nil())
        return fail("L3_M12_CONTROLLER_SUSPENSION_OWNER_STALE");
    auto doc=live.document();
    nlohmann::json meta;
    try {
        meta=nlohmann::json::parse(doc.referenceMatch.canonical_utf8());
        if(!meta.is_object()||!meta.contains("materialization")||
           !meta.at("materialization").is_object())
            return fail("L3_M12_CONTROLLER_SUSPENSION_META_MISSING");
        const auto& data=meta.at("materialization");
        if(!data.contains("controllerId")||
           !data.at("controllerId").is_string()||
           data.at("controllerId").get<std::string>()!=
               current_controller_id.to_string()||
           !data.contains("linkedChainId")||
           !data.at("linkedChainId").is_string()||
           data.at("linkedChainId").get<std::string>()!=
               original_ref_chain_id.to_string()||
           !data.contains("materializationState")||
           data.at("materializationState")!="MATERIALIZED"||
           !data.contains("planId")||
           !data.at("planId").is_string()||
           !data.contains("planContentChecksum")||
           !data.at("planContentChecksum").is_string())
            return fail("L3_M12_CONTROLLER_SUSPENSION_NOT_OWNED");
    }catch(const nlohmann::json::exception&) {
        return fail("L3_M12_CONTROLLER_SUSPENSION_META_INVALID");
    }
    auto it=std::find_if(doc.chains.begin(),doc.chains.end(),
        [&](const project::Chain& chain) {
            return chain.chainId==original_ref_chain_id&&
                chain.segment=="REF_LINKED"&&chain.stage=="MASTER";
        });
    if(it==doc.chains.end())
        return fail("L3_M12_CONTROLLER_SUSPENSION_NO_OWNED_CHAIN");
    const bool suspend=cause!=ControllerSuspensionCause::
        current_amount_restored;
    if(cause==ControllerSuspensionCause::current_amount_restored) {
        const auto& materialization=meta.at("materialization");
        if(materialization.contains("freshnessState")&&
           materialization.at("freshnessState")=="STALE")
            return fail("L3_M12_CONTROLLER_STALE_PLAN_CANNOT_REACTIVATE");
    }
    for(auto& module:it->modules) {
        if(module.provenance!="REFERENCE_MATCH"||
           module.owner!="REFERENCE_MATCH"||
           module.linkState!="REF_LINKED"||
           !module.semanticNodeId||module.semanticNodeId->is_nil())
            return fail("L3_M12_CONTROLLER_SUSPENSION_FOREIGN_NODE");
        module.controllerSuspended=suspend;
    }
    // Amount-zero is reversible without retuning; stale requires a
    // new accepted plan/review and cannot become current by Amount.
    if(cause==ControllerSuspensionCause::plan_stale)
        meta["materialization"]["freshnessState"]="STALE";
    else if(!meta["materialization"].contains("freshnessState"))
        meta["materialization"]["freshnessState"]="CURRENT";
    meta["materialization"]["verificationState"]="NOT_VERIFIED";
    const auto canonical=project::OpaqueJsonValue::parse(
        meta.dump(-1,' ',false,nlohmann::json::error_handler_t::strict));
    if(!canonical)return R::failure(*canonical.error());
    doc.referenceMatch=*canonical.value();
    auto changed=project::ProjectSnapshot::create(
        std::move(doc),
        {live.optional_entries().begin(),live.optional_entries().end()});
    if(!changed)return R::failure(*changed.error());
    return R::success(M12ControllerSuspensionDraft{
        std::move(*changed.value()),suspend,cause});
}
} // namespace rgsml::app::l3_automatch::m12_frozen
