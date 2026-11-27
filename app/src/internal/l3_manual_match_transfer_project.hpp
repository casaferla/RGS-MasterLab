#pragma once
#include "l3_manual_match_transfer_identity.hpp"
#include "l3_m12_product_reopen_preflight.hpp"

namespace rgsml::app::l3_automatch {
struct ManualMatchTransferProjectDraft final {
    project::ProjectSnapshot before;
    project::ProjectSnapshot after;
    ManualMatchTransferIdentity identity;
    [[nodiscard]] bool matches_before(const project::ProjectSnapshot& live) const {
        return m12_frozen::m12_exact_project_snapshot_equal(before,live);
    }
    [[nodiscard]] bool matches_after(const project::ProjectSnapshot& live) const {
        return m12_frozen::m12_exact_project_snapshot_equal(after,live);
    }
    [[nodiscard]] constexpr bool authorizes_project_commit() const noexcept {return false;}
};
// Use the same canonical typed module payloads as the actual project
// serializer. Retain module identities and flags except the exact
// requested transfer; preserve all unrelated document/optional entries.
[[nodiscard]] inline core::Result<ManualMatchTransferProjectDraft>
prepare_manual_match_transfer_project(const project::ProjectSnapshot& before,
    const ManualMatchTransferDraft& transfer)
{
    using R=core::Result<ManualMatchTransferProjectDraft>;
    const auto fail=[](const char* why){
        return R::failure(core::Error{core::ErrorCode::InvalidState,why});
    };
    auto identity=identify_manual_match_transfer(transfer);
    if(!identity)return R::failure(*identity.error());
    const auto prior=transfer.exact_prior_owner;
    const auto& replay=*prior->replay;
    const auto& doc=before.document();
    if(!doc.pipeline.masteringChainId||
        !m12_frozen::m12_reopened_persisted_replay_matches_exact_ticket(doc,replay))
        return fail("MANUAL_MATCH_PROJECT_PRIOR_LINKED_OWNER_STALE");
    auto originalManual=dsp::ProcessingChain::restore(*replay.registry_owner,
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL},
        prior->exact_manual_chain_revision,prior->exact_manual_nodes);
    if(!originalManual)return R::failure(*originalManual.error());
    const auto decoded=m12_frozen::decode_exact_m12_ref_linked_product_state(
        doc,replay.registry_owner);
    if(!decoded)return R::failure(*decoded.error());
    const auto exact=m12_frozen::prepare_restored_m12_native_replay_plan(
        doc,*decoded.value(),*doc.pipeline.masteringChainId,*originalManual.value(),
        prior->exact_manual_bindings,true);
    if(!exact)return R::failure(*exact.error());
    // A later atomic Product commit must recheck this full before state
    // under its exclusive epoch; this helper never mutates a live owner.
    auto candidate=doc;
    auto manual=std::find_if(candidate.chains.begin(),candidate.chains.end(),
        [&](const auto& c){return c.chainId==*doc.pipeline.masteringChainId;});
    auto linked=std::find_if(candidate.chains.begin(),candidate.chains.end(),
        [&](const auto& c){return c.chainId==replay.linked_chain_id;});
    if(manual==candidate.chains.end()||linked==candidate.chains.end())
        return fail("MANUAL_MATCH_PROJECT_MISSING_CHAIN");
    const auto target=std::find_if(manual->modules.begin(),manual->modules.end(),
        [&](const auto& n){return n.instanceId==transfer.manual_instance.uuid();});
    const auto source=std::find_if(linked->modules.begin(),linked->modules.end(),
        [&](const auto& n){return n.instanceId==transfer.linked_instance.uuid();});
    if(target==manual->modules.end()||source==linked->modules.end()||
        target->typeId!=source->typeId||target->algorithmVersion!=source->algorithmVersion||
        target->parameterSchemaId!=source->parameterSchemaId)
        return fail("MANUAL_MATCH_PROJECT_PARAMETER_SCHEMA_MISMATCH");
    target->parameters=source->parameters;
    target->userBypass=false;
    source->userBypass=true;
    manual->revision=transfer.manual_after.proposed_manual_chain.revision();
    linked->revision=transfer.linked_after.revision();
    // The ordinary accepted-plan decoder intentionally refuses this
    // provisional carrier. Do not label it MATERIALIZED or carry any
    // original M10/M11 checksums as evidence for the derived graph.
    auto meta=QJsonDocument::fromJson(
        QByteArray::fromStdString(doc.referenceMatch.canonical_utf8()));
    if(!meta.isObject())return fail("MANUAL_MATCH_PROJECT_METADATA_INVALID");
    auto record=meta.object();
    const auto old=record.take(QStringLiteral("materialization"));
    if(!old.isObject()||record.contains(QStringLiteral("manualTransferCandidate")))
        return fail("MANUAL_MATCH_PROJECT_PRIOR_RECORD_INVALID");
    const auto canonical=QJsonDocument::fromJson(QByteArray::fromStdString(
        identity.value()->canonical_content.canonical_utf8()));
    if(!canonical.isObject())return fail("MANUAL_MATCH_PROJECT_IDENTITY_INVALID");
    record.insert(QStringLiteral("manualTransferCandidate"),QJsonObject{
        {"identitySha256",QString::fromStdString(identity.value()->sha256)},
        {"content",canonical.object()},
        {"priorAcceptedMaterialization",old.toObject()},
        {"verificationState",QStringLiteral("NOT_VERIFIED")}});
    auto opaque=project::OpaqueJsonValue::parse(
        QJsonDocument(record).toJson(QJsonDocument::Compact).toStdString());
    if(!opaque)return R::failure(*opaque.error());
    candidate.referenceMatch=std::move(*opaque.value());
    auto after=project::ProjectSnapshot::create(std::move(candidate),
        {before.optional_entries().begin(),before.optional_entries().end()});
    if(!after)return R::failure(*after.error());
    return R::success(ManualMatchTransferProjectDraft{
        before,std::move(*after.value()),std::move(*identity.value())});
}
// Read back the persisted candidate against its exact immutable draft.
// The candidate checksum covers BOTH ordered graphs. The old accepted
// materialization is archival provenance, not a new receipt.
[[nodiscard]] inline core::Status validate_manual_match_transfer_project_readback(
    const project::ProjectSnapshot& readback,
    const ManualMatchTransferProjectDraft& expected,
    const ManualMatchTransferDraft& native)
{
    const auto fail=[](const char* why){
        return core::Status::failure(core::Error{core::ErrorCode::InvalidState,why});
    };
    // Independently reconstruct from the original owner. Neither an
    // edited after-snapshot nor an altered expected identity is trusted.
    auto reconstructed=prepare_manual_match_transfer_project(expected.before,native);
    if(!reconstructed)return core::Status::failure(*reconstructed.error());
    if(reconstructed.value()->identity.sha256!=expected.identity.sha256||
       reconstructed.value()->identity.canonical_content!=expected.identity.canonical_content||
       !reconstructed.value()->matches_after(expected.after)||
       !reconstructed.value()->matches_after(readback))
        return fail("MANUAL_MATCH_PROJECT_READBACK_MISMATCH");
    const auto metadata=QJsonDocument::fromJson(
        QByteArray::fromStdString(readback.document().referenceMatch.canonical_utf8()));
    if(!metadata.isObject()||metadata.object().contains(QStringLiteral("materialization")))
        return fail("MANUAL_MATCH_PROJECT_READBACK_ACCEPTED_RECEIPT_CONTAMINATION");
    const auto candidate=metadata.object().value(QStringLiteral("manualTransferCandidate")).toObject();
    if(candidate.value(QStringLiteral("identitySha256")).toString().toStdString()!=expected.identity.sha256||
       candidate.value(QStringLiteral("verificationState")).toString()!=QStringLiteral("NOT_VERIFIED"))
        return fail("MANUAL_MATCH_PROJECT_READBACK_IDENTITY_MISMATCH");
    const auto content=project::OpaqueJsonValue::parse(
        QJsonDocument(candidate.value(QStringLiteral("content")).toObject()).toJson(QJsonDocument::Compact).toStdString());
    if(!content||*content.value()!=expected.identity.canonical_content)
        return fail("MANUAL_MATCH_PROJECT_READBACK_CONTENT_MISMATCH");
    return core::Status::success();
}
} // namespace rgsml::app::l3_automatch
