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
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QByteArray>
#include <QString>
#include <span>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
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
    bool full_m11_protected_safety_current{};
};
struct M12PreparedProjectTransaction final {
    project::ProjectSnapshot candidate_not_yet_committed;
    core::Uuid plan_id;
    core::Uuid controller_id;
    core::Uuid ref_linked_chain_id;
    std::string accepted_plan_checksum;
    std::string pre_commit_project_revision_dependency;
    // Retain the ACTUAL complete before-state, including unknown optional
    // resources, until the Product's exclusive epoch rechecks its content.
    // A structural revision or a user-supplied "current" boolean is not
    // a substitute for this immutable pre-publication equality.
    std::optional<project::ProjectSnapshot> exact_precommit_project;
    [[nodiscard]] bool matches_exact_precommit(
        const project::ProjectSnapshot& current) const;
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
template<class T,class Predicate>
[[nodiscard]] inline bool m12_exact_vector_equal(
    const std::vector<T>& a,const std::vector<T>& b,Predicate predicate)
{
    return a.size()==b.size()&&
        std::equal(a.begin(),a.end(),b.begin(),predicate);
}
// Whole-document *content* comparison, including fields that M12 is not
// permitted to edit. Do not narrow this to just the MANUAL DSP revision.
[[nodiscard]] inline bool m12_exact_project_snapshot_equal(
    const project::ProjectSnapshot& a,const project::ProjectSnapshot& b)
{
    const auto& x=a.document();
    const auto& y=b.document();
    const auto loc=[](const project::LocatorHint& l,
                      const project::LocatorHint& r){
        return l.providerId==r.providerId&&l.locator==r.locator&&
               l.displayName==r.displayName;
    };
    const auto resource=[&](const project::Resource& l,
                            const project::Resource& r){
        return l.resourceId==r.resourceId&&
               l.resourceType==r.resourceType&&l.identity==r.identity&&
               m12_exact_vector_equal(l.locatorHints,r.locatorHints,loc);
    };
    const auto reference=[](const project::Reference& l,
                            const project::Reference& r){
        return l.referenceId==r.referenceId&&
               l.referenceType==r.referenceType&&
               l.resourceId==r.resourceId;
    };
    const auto region=[](const project::AuditionRegion& l,
                         const project::AuditionRegion& r){
        return l.regionId==r.regionId&&l.resourceId==r.resourceId&&
               l.frameDomain==r.frameDomain&&
               l.startFrame==r.startFrame&&
               l.endFrameExclusive==r.endFrameExclusive&&
               l.loopEnabled==r.loopEnabled;
    };
    const auto optional=[](const project::OptionalEntry& l,
                           const project::OptionalEntry& r){
        return l.path==r.path&&l.mediaType==r.mediaType&&
               l.bytes==r.bytes;
    };
    return x.projectId==y.projectId&&x.displayName==y.displayName&&
        x.sourceResourceId==y.sourceResourceId&&
        m12_exact_vector_equal(x.resources,y.resources,resource)&&
        x.references.activeReferenceId==y.references.activeReferenceId&&
        m12_exact_vector_equal(x.references.items,y.references.items,
                               reference)&&
        m12_exact_vector_equal(x.timeSelections.auditionRegions,
                               y.timeSelections.auditionRegions,region)&&
        x.timeSelections.analysisScopes==y.timeSelections.analysisScopes&&
        x.timeSelections.processingRegions==y.timeSelections.processingRegions&&
        m12_exact_vector_equal(x.chains,y.chains,exact_project_chain_equal)&&
        x.pipeline.repairChainId==y.pipeline.repairChainId&&
        x.pipeline.conditioningChainId==y.pipeline.conditioningChainId&&
        x.pipeline.masteringChainId==y.pipeline.masteringChainId&&
        x.activeProfileApplication==y.activeProfileApplication&&
        x.analyses==y.analyses&&x.plans==y.plans&&
        x.referenceMatch==y.referenceMatch&&
        x.masteringDNA==y.masteringDNA&&x.output==y.output&&
        x.derivedArtifacts==y.derivedArtifacts&&
        x.history==y.history&&x.extensions==y.extensions&&
        m12_exact_vector_equal(a.optional_entries(),b.optional_entries(),
                               optional);
}
[[nodiscard]] inline bool
M12PreparedProjectTransaction::matches_exact_precommit(
    const project::ProjectSnapshot& current) const
{
    return exact_precommit_project&&
        m12_exact_project_snapshot_equal(*exact_precommit_project,current);
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
       !expected.full_m11_protected_safety_current||
       expected.exact_live_source_id.is_nil()||
       expected.exact_live_reference_id.is_nil()||
       expected.exact_protected_manual_chain_id.is_nil()||
       !accepted_stage.registry_owner||
       accepted_stage.accepted_identity.no_correction_required||
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
    // The canonical REF_LINKED owner exists only in MASTER. A
    // foreign-stage linked segment is not a spare graph that may be
    // silently retained while another controller is materialized.
    if(std::any_of(live.chains.begin(),live.chains.end(),
        [](const project::Chain& chain) {
            return chain.segment=="REF_LINKED"&&chain.stage!="MASTER";
        }))
        return fail("L3_M12_PROJECT_TRANSACTION_FOREIGN_LINKED_STAGE");
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
    // Compare the persisted MANUAL owner to the ACTUAL frozen M11
    // DSP modules and exact typed parameter payloads captured with
    // the accepted stage. Caller-provided expected snapshots alone
    // cannot launder a changed Input Gain or fixed module.
    if(manual->revision!=
           accepted_stage.frozen_protected_manual_revision||
       manual->modules.size()!=
           accepted_stage.frozen_protected_manual_instances.size()||
       manual->modules.size()!=
           accepted_stage.frozen_protected_manual_bindings.size())
        return fail("L3_M12_PROJECT_TRANSACTION_FIXED_DSP_STALE");
    for(std::size_t i=0;i<manual->modules.size();++i) {
        const auto& persisted=manual->modules[i];
        const auto& actual=
            accepted_stage.frozen_protected_manual_instances[i];
        const auto& typed=
            accepted_stage.frozen_protected_manual_bindings[i];
        if(!binding_matches_instance(actual,typed)||
           persisted.instanceId!=actual.instance_id().uuid()||
           persisted.typeId!=actual.module_type_id()||
           persisted.enabled!=actual.enabled()||
           persisted.userBypass!=actual.user_bypass()||
           persisted.controllerSuspended!=
               actual.controller_suspended()||
           persisted.domainSuspended!=actual.domain_suspended()||
           persisted.provenance!="MANUAL"||
           persisted.owner!="USER"||
           persisted.linkState!="UNLINKED"||
           persisted.semanticNodeId.has_value())
            return fail("L3_M12_PROJECT_TRANSACTION_FIXED_DSP_STALE");
        const auto json=dsp::encode_module_parameters_json(
            typed.parameters);
        if(!json)return R::failure(*json.error());
        const auto canonical_fixed=project::OpaqueJsonValue::parse(
            *json.value());
        if(!canonical_fixed)return R::failure(
            *canonical_fixed.error());
        if(canonical_fixed.value()->canonical_utf8()!=
           persisted.parameters.canonical_utf8())
            return fail("L3_M12_PROJECT_TRANSACTION_FIXED_PAYLOAD_STALE");
    }
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
    bool existing_linked_content_changed=false;
    if(expected.exact_existing_linked_snapshot) {
        if(linked==candidate.chains.end()||
           !exact_project_chain_equal(
               *linked,*expected.exact_existing_linked_snapshot)||
           linked->chainId!=
               exact_native_carrier.native_project_chain.chainId)
            return fail("L3_M12_PROJECT_TRANSACTION_STALE_UPDATE");
        // P02/P04: an UPDATE is the same semantic node, same registered
        // type/algorithm/schema AND the same concrete instance. A REPLACE
        // requires a fresh instance whenever any of those identities
        // changes. Never smuggle a different semantic node into the old
        // instance just because it belonged to this controller.
        // Existing linked materialization is not trusted merely because
        // the caller copied it into expected.exact_existing_linked_snapshot.
        // Corrupt/foreign provenance or duplicate semantic identities must
        // never be silently overwritten by a new controller-owned stage.
        std::set<core::Uuid> existing_semantics;
        for(const auto& prior:linked->modules) {
            if(prior.instanceId.is_nil()||!prior.semanticNodeId||
               prior.semanticNodeId->is_nil()||
               !existing_semantics.insert(*prior.semanticNodeId).second||
               prior.provenance!="REFERENCE_MATCH"||
               prior.owner!="REFERENCE_MATCH"||
               prior.linkState!="REF_LINKED"||
               !prior.algorithmVersion||!prior.parameterSchemaId||
               prior.typeId=="rgsml.dsp.true-peak-limiter"||
               prior.typeId=="rgsml.dsp.dither")
                return fail("L3_M12_PROJECT_TRANSACTION_PRIOR_LINKED_CORRUPT");
            if(prior.typeId=="rgsml.dsp.streaming-true-peak-limiter" &&
               (&prior!=&linked->modules.back() ||
                std::any_of(manual->modules.begin(),manual->modules.end(),[](const auto& fixed){
                    return fixed.typeId=="rgsml.dsp.streaming-true-peak-limiter";
                })))
                return fail("L3_M12_PROJECT_TRANSACTION_PRIOR_P2_CONFLICT");
            const auto descriptor=accepted_stage.registry_owner->find_descriptor(prior.typeId);
            if(!descriptor||descriptor.value()->get().algorithm_version()!=
                  std::optional<std::string_view>{*prior.algorithmVersion}||
               descriptor.value()->get().parameter_schema_id()!=
                  std::optional<std::string_view>{*prior.parameterSchemaId}||
               !accepted_stage.registry_owner->has_factory(prior.typeId))
                return fail("L3_M12_PROJECT_TRANSACTION_PRIOR_SCHEMA_UNAVAILABLE");
            const auto decoded=dsp::decode_module_parameters_json(
                *prior.parameterSchemaId,prior.parameters.canonical_utf8());
            if(!decoded||!accepted_stage.registry_owner->create_module(prior.typeId,*decoded.value()))
                return fail("L3_M12_PROJECT_TRANSACTION_PRIOR_PAYLOAD_INVALID");
        }
        for(const auto& node:exact_native_carrier.native_project_chain.modules) {
            const auto semantic=std::find_if(
                linked->modules.begin(),linked->modules.end(),
                [&](const project::Module& prior){
                    return prior.semanticNodeId==node.semanticNodeId;
                });
            const auto concrete=std::find_if(
                linked->modules.begin(),linked->modules.end(),
                [&](const project::Module& prior){
                    return prior.instanceId==node.instanceId;
                });
            if(concrete!=linked->modules.end()&&
               (concrete->semanticNodeId!=node.semanticNodeId||
                concrete->typeId!=node.typeId||
                concrete->algorithmVersion!=node.algorithmVersion||
                concrete->parameterSchemaId!=node.parameterSchemaId))
                return fail("L3_M12_PROJECT_TRANSACTION_REPLACE_REUSED_ID");
            if(semantic!=linked->modules.end()) {
                const bool compatible=
                    semantic->typeId==node.typeId&&
                    semantic->algorithmVersion==node.algorithmVersion&&
                    semantic->parameterSchemaId==node.parameterSchemaId;
                const bool retained=semantic->instanceId==node.instanceId;
                if(compatible!=retained)
                    return fail("L3_M12_PROJECT_TRANSACTION_UPDATE_ID_MISMATCH");
            }
            if(occupied_module_ids.contains(node.instanceId)&&
               concrete==linked->modules.end())
                return fail("L3_M12_PROJECT_TRANSACTION_FOREIGN_NODE_ID");
        }
        existing_linked_content_changed=!exact_project_chain_equal(
            *linked,exact_native_carrier.native_project_chain);
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
    QJsonObject record;
    if(live.referenceMatch.canonical_utf8()!="{}") {
        QJsonParseError err;
        const auto doc=QJsonDocument::fromJson(
            QByteArray::fromStdString(live.referenceMatch.canonical_utf8()),
            &err);
        if(err.error!=QJsonParseError::NoError||!doc.isObject())
            return fail("L3_M12_PROJECT_TRANSACTION_METADATA_INVALID");
        record=doc.object();
        const auto priorValue=record.value(QStringLiteral("materialization"));
        if(!priorValue.isUndefined()) {
            if(!priorValue.isObject()||
               !expected.exact_existing_linked_snapshot)
                return fail("L3_M12_PROJECT_TRANSACTION_FOREIGN_CONTROLLER");
            const auto prior=priorValue.toObject();
            if(prior.value(QStringLiteral("controllerId")).toString()!=
               QString::fromStdString(
                   exact_native_carrier.exact_owner_controller.to_string())||
               prior.value(QStringLiteral("linkedChainId")).toString()!=
               QString::fromStdString(
                   exact_native_carrier.native_project_chain.chainId.to_string()))
                return fail("L3_M12_PROJECT_TRANSACTION_FOREIGN_CONTROLLER");
            // A mutation of immutable plan content must carry new
            // accepted plan evidence, not old M10/M12 checksum bytes.
            const auto prior_id=prior.value(QStringLiteral("planId")).toString();
            const auto prior_checksum=prior.value(
                QStringLiteral("planContentChecksum")).toString();
            const auto prior_revision_text=prior.value(
                QStringLiteral("planRevision")).toString();
            bool parsed_revision=false;
            const auto prior_revision=prior_revision_text.toULongLong(
                &parsed_revision);
            const auto prior_linked_revision=QString::fromStdString(
                std::to_string(
                    expected.exact_existing_linked_snapshot->revision));
            if(!parsed_revision||prior_id.isEmpty()||
               prior_checksum.isEmpty()||
               prior.value(QStringLiteral("materializationState")).toString()!=
                   QStringLiteral("MATERIALIZED")||
               prior.value(QStringLiteral("schemaId")).toString()!=
                   QStringLiteral("rgsml.reference-match.materialization.schema/1.0.0")||
               prior.value(QStringLiteral("policyId")).toString()!=
                   QStringLiteral("rgsml.reference-match.materialization-policy/1.0.0")||
               prior.value(QStringLiteral("linkedChainRevision")).toString()!=
                   prior_linked_revision||
               prior_revision_text!=prior_linked_revision)
                return fail("L3_M12_PROJECT_TRANSACTION_PRIOR_PLAN_STALE");
            if(existing_linked_content_changed) {
                const auto& next=accepted_stage.accepted_identity;
                const auto next_id=QString::fromStdString(
                    next.plan_id.to_string());
                const auto next_checksum=QString::fromStdString(
                    next.exact_plan_content_checksum);
                if(next_checksum==prior_checksum||
                   (next_id==prior_id&&
                    next.exact_accepted_plan_revision<=prior_revision))
                    return fail("L3_M12_PROJECT_TRANSACTION_PLAN_IDENTITY_REUSED");
            }
        }else if(expected.exact_existing_linked_snapshot)
            return fail("L3_M12_PROJECT_TRANSACTION_MISSING_PRIOR_OWNER");
    }else if(expected.exact_existing_linked_snapshot)
        return fail("L3_M12_PROJECT_TRANSACTION_PRIOR_META_ABSENT");
    const auto& ident=accepted_stage.accepted_identity;
    const auto q=[](const std::string& value){
        return QString::fromStdString(value);
    };
    // JSON stores exact revision integers as decimal *strings* to
    // avoid losing U64 bits in the Qt/JSON double representation.
    QJsonObject materialization{
        {"schemaId",QStringLiteral("rgsml.reference-match.materialization.schema/1.0.0")},
        {"policyId",QStringLiteral("rgsml.reference-match.materialization-policy/1.0.0")},
        {"controllerId",q(ident.reference_match_controller_id.to_string())},
        {"planId",q(ident.plan_id.to_string())},
        {"planRevision",q(std::to_string(ident.exact_accepted_plan_revision))},
        {"planContentChecksum",q(ident.exact_plan_content_checksum)},
        {"winnerContentChecksum",q(ident.exact_m10_result_and_topology_checksum)},
        {"protectedFixedChecksum",q(ident.exact_m11_fixed_and_protection_checksum)},
        {"sourceReferenceScopeRevision",q(ident.exact_source_reference_amount_scope_revision)},
        {"linkedChainId",q(exact_native_carrier.native_project_chain.chainId.to_string())},
        {"linkedChainRevision",q(std::to_string(exact_native_carrier.native_project_chain.revision))},
        {"materializationState",QStringLiteral("MATERIALIZED")},
        {"verificationState",QStringLiteral("NOT_VERIFIED")}
    };
    record.insert(QStringLiteral("materialization"),materialization);
    const auto canonical_record=project::OpaqueJsonValue::parse(
        QJsonDocument(record).toJson(QJsonDocument::Compact).toStdString());
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
        ident.exact_source_reference_amount_scope_revision,
        current_project});
}
} // namespace rgsml::app::l3_automatch::m12_frozen
