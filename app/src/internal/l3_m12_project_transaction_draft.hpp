#pragma once
// L3-M12 native Product ProjectDocument ONE transaction candidate.
// This is deliberately an UNCOMMITTED immutable ProjectSnapshot:
// only the Qt Product owner, holding its single-writer mutation batch,
// may adopt/persist it and then issue a true materialization receipt.
// A prepared document is NOT an M12 receipt, M13 VERIFIED, or export.
#include "l3_m12_ref_linked_project_carrier.hpp"
#include <rgsml/project/project_snapshot.hpp>
#include <rgsml/project/opaque_json_value.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m12_frozen {
struct M12ProjectTransactionExpected final {
    core::Uuid exact_live_source_id;
    core::Uuid exact_live_reference_id;
    core::Uuid exact_protected_manual_chain_id;
    project::Chain exact_protected_manual_snapshot;
    std::optional<project::Chain> exact_existing_linked_snapshot;
    bool current_product_epoch_and_accepted_diff_attested{};
    bool full_m11_and_terminal_safety_current{};
};
struct M12PreparedProjectTransaction final {
    project::ProjectSnapshot candidate_not_yet_committed;
    core::Uuid plan_id;
    core::Uuid controller_id;
    core::Uuid ref_linked_chain_id;
    std::string accepted_plan_checksum;
    std::string pre_commit_project_revision_dependency;
    [[nodiscard]] constexpr bool authorizes_project_commit() const noexcept {
        return false;
    }
    [[nodiscard]] constexpr bool is_materialization_receipt() const noexcept {
        return false;
    }
    [[nodiscard]] constexpr bool authorizes_m13_verification() const noexcept {
        return false;
    }
};
[[nodiscard]] inline bool exact_project_module_equal(
    const project::Module& a,const project::Module& b) {
    return a.instanceId==b.instanceId&&a.typeId==b.typeId&&
        a.enabled==b.enabled&&a.userBypass==b.userBypass&&
        a.controllerSuspended==b.controllerSuspended&&
        a.domainSuspended==b.domainSuspended&&
        a.provenance==b.provenance&&a.owner==b.owner&&
        a.linkState==b.linkState&&a.semanticNodeId==b.semanticNodeId&&
        a.algorithmVersion==b.algorithmVersion&&
        a.parameterSchemaId==b.parameterSchemaId&&
        a.parameters==b.parameters;
}
[[nodiscard]] inline bool exact_project_chain_equal(
    const project::Chain& a,const project::Chain& b) {
    if(a.chainId!=b.chainId||a.stage!=b.stage||
       a.segment!=b.segment||a.revision!=b.revision||
       a.modules.size()!=b.modules.size())return false;
    for(std::size_t k=0;k<a.modules.size();++k)
        if(!exact_project_module_equal(a.modules[k],b.modules[k]))
            return false;
    return true;
}
[[nodiscard]] inline core::Result<M12PreparedProjectTransaction>
prepare_exact_m12_project_transaction(
    const project::ProjectSnapshot& current_project,
    const FrozenProjectLinkedCarrier& exact_native_carrier,
    const StagedRefLinkedSegment& accepted_stage,
    M12ProjectTransactionExpected expected)
{
    using R=core::Result<M12PreparedProjectTransaction>;
    const auto fail=[](const char* code) {
        return R::failure(core::Error{
            core::ErrorCode::InvalidState,code});
    };
    if(!expected.current_product_epoch_and_accepted_diff_attested||
       !expected.full_m11_and_terminal_safety_current||
       expected.exact_live_source_id.is_nil()||
       expected.exact_live_reference_id.is_nil()||
       expected.exact_protected_manual_chain_id.is_nil()||
       !accepted_stage.registry_owner||
       accepted_stage.accepted_identity.no_correction_required!=
           exact_native_carrier.native_project_chain.modules.empty()||
       exact_native_carrier.exact_owner_controller!=
           accepted_stage.accepted_identity.reference_match_controller_id||
       exact_native_carrier.exact_accepted_plan_id!=
           accepted_stage.accepted_identity.plan_id||
       exact_native_carrier.exact_accepted_plan_checksum!=
           accepted_stage.accepted_identity.exact_plan_content_checksum||
       exact_native_carrier.native_project_chain.stage!="MASTER"||
       exact_native_carrier.native_project_chain.segment!="REF_LINKED"||
       exact_native_carrier.native_project_chain.chainId.is_nil())
        return fail("L3_M12_PROJECT_TRANSACTION_WRONG_OWNER_OR_PLAN");
    // Independently reconstruct the actual native codec carrier.
    // A caller cannot present alternate linked PCM/parameters with the
    // same accepted plan checksum.
    const auto canonical=encode_exact_ref_linked_project_carrier(
        accepted_stage,exact_native_carrier.native_project_chain.chainId);
    if(!canonical)return R::failure(*canonical.error());
    if(!exact_project_chain_equal(
         canonical.value()->native_project_chain,
         exact_native_carrier.native_project_chain))
        return fail("L3_M12_PROJECT_TRANSACTION_LINKED_CODEC_CONFLICT");
    const auto& live=current_project.document();
    if(live.sourceResourceId!=expected.exact_live_source_id||
       !live.references.activeReferenceId||
       *live.references.activeReferenceId!=expected.exact_live_reference_id||
       !live.pipeline.masteringChainId||
       *live.pipeline.masteringChainId!=
           expected.exact_protected_manual_chain_id||
       live.projectId.is_nil())
        return fail("L3_M12_PROJECT_TRANSACTION_SOURCE_REFERENCE_STALE");
    const auto r=std::find_if(live.references.items.begin(),
        live.references.items.end(),[&](const project::Reference& ref) {
            return ref.referenceId==expected.exact_live_reference_id&&
                   ref.resourceId!=live.sourceResourceId;
        });
    if(r==live.references.items.end())
        return fail("L3_M12_PROJECT_TRANSACTION_REFERENCE_MISSING");
    const auto manual=std::find_if(
        live.chains.begin(),live.chains.end(),
        [&](const project::Chain& chain) {
            return chain.chainId==expected.exact_protected_manual_chain_id;
        });
    if(manual==live.chains.end()||manual->stage!="MASTER"||
       manual->segment!="MANUAL"||
       !exact_project_chain_equal(
           *manual,expected.exact_protected_manual_snapshot))
        return fail("L3_M12_PROJECT_TRANSACTION_MANUAL_CHANGED");
    if(exact_native_carrier.native_project_chain.chainId==
       manual->chainId)
        return fail("L3_M12_PROJECT_TRANSACTION_ID_COLLISION");
    // Retain ALL unowned native segments, Input Gain, terminal,
    // metadata, optional entries and pipeline IDs byte-for-byte.
    project::ProjectDocument candidate=live;
    std::set<core::Uuid> occupied_module_ids;
    for(const auto& chain:live.chains)
        for(const auto& node:chain.modules)
            if(!occupied_module_ids.insert(node.instanceId).second)
                return fail("L3_M12_PROJECT_TRANSACTION_DUPLICATE_INSTANCE");
    for(const auto& node:exact_native_carrier.native_project_chain.modules)
        if(occupied_module_ids.contains(node.instanceId) &&
           !expected.exact_existing_linked_snapshot)
            return fail("L3_M12_PROJECT_TRANSACTION_NODE_ID_COLLISION");
    auto linked=std::find_if(candidate.chains.begin(),candidate.chains.end(),
        [](const project::Chain& chain) {
            return chain.stage=="MASTER"&&chain.segment=="REF_LINKED";
        });
    const auto second=linked==candidate.chains.end()
        ?linked:std::find_if(std::next(linked),candidate.chains.end(),
            [](const project::Chain& chain) {
                return chain.stage=="MASTER"&&chain.segment=="REF_LINKED";
            });
    if(second!=candidate.chains.end())
        return fail("L3_M12_PROJECT_TRANSACTION_AMBIGUOUS_LINKED_OWNER");
    if(expected.exact_existing_linked_snapshot) {
        if(linked==candidate.chains.end()||
           !exact_project_chain_equal(
               *linked,*expected.exact_existing_linked_snapshot)||
           linked->chainId!=
               exact_native_carrier.native_project_chain.chainId)
            return fail("L3_M12_PROJECT_TRANSACTION_STALE_UPDATE");
        for(const auto& node:exact_native_carrier.native_project_chain.modules)
            if(occupied_module_ids.contains(node.instanceId)) {
                const auto old=std::find_if(
                    linked->modules.begin(),linked->modules.end(),
                    [&](const project::Module& x){
                        return x.instanceId==node.instanceId;
                    });
                if(old==linked->modules.end())
                    return fail("L3_M12_PROJECT_TRANSACTION_FOREIGN_NODE_ID");
            }
        *linked=exact_native_carrier.native_project_chain;
    } else {
        if(linked!=candidate.chains.end())
            return fail("L3_M12_PROJECT_TRANSACTION_FOREIGN_OR_STALE_LINKED");
        const auto conflicting=std::find_if(
            candidate.chains.begin(),candidate.chains.end(),
            [&](const project::Chain& c) {
                return c.chainId==
                    exact_native_carrier.native_project_chain.chainId;
            });
        if(conflicting!=candidate.chains.end())
            return fail("L3_M12_PROJECT_TRANSACTION_CHAIN_ID_COLLISION");
        candidate.chains.push_back(
            exact_native_carrier.native_project_chain);
    }
    // Reuse project::OpaqueJsonValue canonical JSON and the existing
    // project writer. No duplicate plan hash or experimental JSON
    // serialization. All durable metadata is tied to the SAME stage.
    nlohmann::json record=nlohmann::json::object();
    try {
        if(live.referenceMatch.canonical_utf8()!="{}") {
            record=nlohmann::json::parse(
                live.referenceMatch.canonical_utf8());
            if(!record.is_object())return fail(
                "L3_M12_PROJECT_TRANSACTION_METADATA_INVALID");
            // Unknown existing owned/foreign materialization is not
            // overwritten by a guessed controller identity.
            if(record.contains("materialization")) {
                const auto& prior=record.at("materialization");
                if(!prior.is_object()||
                   !expected.exact_existing_linked_snapshot||
                   !prior.contains("controllerId")||
                   !prior.at("controllerId").is_string()||
                   prior.at("controllerId").get<std::string>()!=
                     exact_native_carrier.exact_owner_controller.to_string())
                    return fail("L3_M12_PROJECT_TRANSACTION_FOREIGN_CONTROLLER");
            } else if(expected.exact_existing_linked_snapshot)
                return fail("L3_M12_PROJECT_TRANSACTION_MISSING_PRIOR_OWNER");
        }else if(expected.exact_existing_linked_snapshot)
            return fail("L3_M12_PROJECT_TRANSACTION_PRIOR_META_ABSENT");
    }catch(const nlohmann::json::exception&) {
        return fail("L3_M12_PROJECT_TRANSACTION_METADATA_INVALID");
    }
    const auto& ident=accepted_stage.accepted_identity;
    record["materialization"]={
        {"schemaId","rgsml.reference-match.materialization.schema/1.0.0"},
        {"policyId","rgsml.reference-match.materialization-policy/1.0.0"},
        {"controllerId",ident.reference_match_controller_id.to_string()},
        {"planId",ident.plan_id.to_string()},
        {"planRevision",ident.exact_accepted_plan_revision},
        {"planContentChecksum",ident.exact_plan_content_checksum},
        {"winnerContentChecksum",ident.exact_m10_result_and_topology_checksum},
        {"protectedFixedChecksum",ident.exact_m11_fixed_and_protection_checksum},
        {"sourceReferenceScopeRevision",ident.exact_source_reference_amount_scope_revision},
        {"linkedChainId",exact_native_carrier.native_project_chain.chainId.to_string()},
        {"linkedChainRevision",exact_native_carrier.native_project_chain.revision},
        {"materializationState","MATERIALIZED"},
        {"verificationState","NOT_VERIFIED"}
    };
    const auto canonical_record=project::OpaqueJsonValue::parse(
        record.dump(-1,' ',false,nlohmann::json::error_handler_t::strict));
    if(!canonical_record)return R::failure(*canonical_record.error());
    candidate.referenceMatch=*canonical_record.value();
    auto snapshot=project::ProjectSnapshot::create(
        std::move(candidate),
        {current_project.optional_entries().begin(),
         current_project.optional_entries().end()});
    if(!snapshot)return R::failure(*snapshot.error());
    // No mutation of the incoming current_project occurred. The
    // proposed state MATERIALIZED is applied ONLY if a future Qt
    // Product transaction physically adopts this entire snapshot.
    return R::success(M12PreparedProjectTransaction{
        std::move(*snapshot.value()),ident.plan_id,
        ident.reference_match_controller_id,
        exact_native_carrier.native_project_chain.chainId,
        ident.exact_plan_content_checksum,
        ident.exact_source_reference_amount_scope_revision});
}
} // namespace rgsml::app::l3_automatch::m12_frozen
