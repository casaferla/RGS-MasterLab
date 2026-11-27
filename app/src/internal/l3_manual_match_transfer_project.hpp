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
    auto identity=identify_manual_match_transfer_graph(transfer);
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
// Reconstruct only the before-state of the exact single-slot transfer.
// This is untrusted recovery data until the original typed accepted
// owner is decoded and the entire forward draft is independently rebuilt.
[[nodiscard]] inline core::Result<project::ProjectSnapshot>
recover_manual_match_transfer_before(const project::ProjectSnapshot& persisted)
{
    using R=core::Result<project::ProjectSnapshot>;
    const auto fail=[](const char* why){return R::failure(core::Error{
        core::ErrorCode::InvalidState,why});};
    auto doc=persisted.document();
    const auto parsed=QJsonDocument::fromJson(
        QByteArray::fromStdString(doc.referenceMatch.canonical_utf8()));
    if(!parsed.isObject()||!doc.pipeline.masteringChainId)
        return fail("MANUAL_MATCH_RECOVERY_NO_PROJECT_OWNER");
    auto metadata=parsed.object();
    if(metadata.contains(QStringLiteral("materialization")))
        return fail("MANUAL_MATCH_RECOVERY_ACCEPTED_RECEIPT_CONTAMINATION");
    const auto candidate=metadata.take(QStringLiteral("manualTransferCandidate")).toObject();
    const auto content=candidate.value(QStringLiteral("content")).toObject();
    const auto accepted=candidate.value(QStringLiteral("priorAcceptedMaterialization"));
    if(candidate.value(QStringLiteral("verificationState")).toString()!=QStringLiteral("NOT_VERIFIED")||
       content.value(QStringLiteral("schemaId")).toString()!=QStringLiteral("rgsml.reference-match.manual-transfer-candidate/1.0.0")||
       content.value(QStringLiteral("verificationState")).toString()!=QStringLiteral("NOT_VERIFIED")||
       content.value(QStringLiteral("candidateState")).toString()!=QStringLiteral("PROVISIONAL")||
       !accepted.isObject())
        return fail("MANUAL_MATCH_RECOVERY_INVALID_CANDIDATE");
    const auto canonical=project::OpaqueJsonValue::parse(
        QJsonDocument(content).toJson(QJsonDocument::Compact).toStdString());
    if(!canonical)return R::failure(*canonical.error());
    const auto hash=QCryptographicHash::hash(
        QByteArray::fromStdString(canonical.value()->canonical_utf8()),
        QCryptographicHash::Sha256).toHex();
    if(QString::fromLatin1(hash)!=candidate.value(QStringLiteral("identitySha256")).toString())
        return fail("MANUAL_MATCH_RECOVERY_DIGEST_MISMATCH");
    const auto q=[](const auto& id){return QString::fromStdString(id.to_string());};
    const auto provenance=content.value(QStringLiteral("priorAcceptedProvenance")).toObject();
    if(q(doc.projectId)!=provenance.value(QStringLiteral("projectId")).toString()||
       q(doc.sourceResourceId)!=provenance.value(QStringLiteral("sourceId")).toString()||
       !doc.references.activeReferenceId||
       q(*doc.references.activeReferenceId)!=provenance.value(QStringLiteral("referenceId")).toString())
        return fail("MANUAL_MATCH_RECOVERY_OWNER_MISMATCH");
    const auto manualId=content.value(QStringLiteral("manualInstance")).toString();
    const auto linkedId=content.value(QStringLiteral("linkedInstance")).toString();
    const auto linkedChainId=provenance.value(QStringLiteral("linkedChainId")).toString();
    auto manual=std::find_if(doc.chains.begin(),doc.chains.end(),[&](const auto& c){
        return c.chainId==*doc.pipeline.masteringChainId&&c.stage=="MASTER"&&c.segment=="MANUAL";});
    auto linked=std::find_if(doc.chains.begin(),doc.chains.end(),[&](const auto& c){
        return q(c.chainId)==linkedChainId&&c.stage=="MASTER"&&c.segment=="REF_LINKED";});
    if(manual==doc.chains.end()||linked==doc.chains.end()||manualId.isEmpty()||linkedId.isEmpty())
        return fail("MANUAL_MATCH_RECOVERY_MISSING_GRAPH");
    const auto recover=[&](project::Chain& chain,const QJsonObject& graph,
                           const QString& instance,bool priorBypass)->bool {
        bool revisionOk=false;
        const auto revision=graph.value(QStringLiteral("revision")).toString().toULongLong(&revisionOk);
        if(!revisionOk||revision==std::numeric_limits<std::uint64_t>::max()||
           chain.revision!=revision+1U)return false;
        const auto nodes=graph.value(QStringLiteral("modules")).toArray();
        if(static_cast<std::size_t>(nodes.size())!=chain.modules.size())return false;
        std::size_t matches=0;
        for(std::size_t i=0;i<chain.modules.size();++i) {
            auto& module=chain.modules[i];
            const auto node=nodes.at(static_cast<qsizetype>(i)).toObject();
            if(q(module.instanceId)!=node.value(QStringLiteral("instanceId")).toString()||
               QString::fromStdString(module.typeId)!=node.value(QStringLiteral("typeId")).toString())
                return false;
            if(q(module.instanceId)!=instance)continue;
            ++matches;
            const auto bypass=node.value(QStringLiteral("userBypass"));
            const auto params=node.value(QStringLiteral("parameters"));
            if(!bypass.isBool()||bypass.toBool()!=priorBypass||module.userBypass==priorBypass||
               !params.isObject())return false;
            auto payload=project::OpaqueJsonValue::parse(
                QJsonDocument(params.toObject()).toJson(QJsonDocument::Compact).toStdString());
            if(!payload)return false;
            module.parameters=std::move(*payload.value());
            module.userBypass=priorBypass;
        }
        if(matches!=1U)return false;
        chain.revision=revision;
        return true;
    };
    if(!recover(*manual,content.value(QStringLiteral("priorManual")).toObject(),manualId,true)||
       !recover(*linked,content.value(QStringLiteral("priorReferenceLinked")).toObject(),linkedId,false))
        return fail("MANUAL_MATCH_RECOVERY_INVALID_GRAPH_DELTA");
    metadata.insert(QStringLiteral("materialization"),accepted);
    auto beforeMetadata=project::OpaqueJsonValue::parse(
        QJsonDocument(metadata).toJson(QJsonDocument::Compact).toStdString());
    if(!beforeMetadata)return R::failure(*beforeMetadata.error());
    doc.referenceMatch=std::move(*beforeMetadata.value());
    return project::ProjectSnapshot::create(std::move(doc),
        {persisted.optional_entries().begin(),persisted.optional_entries().end()});
}
// Recovery bytes are not a receipt. Rebuild the one legal native delta
// from a separately decoded original owner, then compare the COMPLETE
// persisted after snapshot. Never use recovered bytes as an executable
// graph until this forward proof has succeeded.
[[nodiscard]] inline core::Result<ManualMatchTransferProjectDraft>
validate_recovered_manual_match_transfer(
    const project::ProjectSnapshot& persisted,
    const ManualMatchTransferDraft& exact_native)
{
    using R=core::Result<ManualMatchTransferProjectDraft>;
    auto before=recover_manual_match_transfer_before(persisted);
    if(!before)return R::failure(*before.error());
    auto draft=prepare_manual_match_transfer_project(*before.value(),exact_native);
    if(!draft)return draft;
    const auto checked=validate_manual_match_transfer_project_readback(
        persisted,*draft.value(),exact_native);
    if(!checked)return R::failure(*checked.error());
    return draft;
}
// This immutable capture is issued by the Qt Project owner under its
// physical mutation lease. The worker owns PCM and value snapshots only;
// it cannot write the Product or manufacture an acceptance receipt.
struct ManualMatchTransferCapture final {
    std::uint64_t product_epoch{};
    ManualMatchTransferDraft native;
    ManualMatchTransferProjectDraft project;
};
[[nodiscard]] inline core::Result<render::RenderResult>
render_captured_manual_match_transfer(const ManualMatchTransferCapture& capture,
    core::FrameCount maxBlock,const std::atomic<bool>* cancellation=nullptr)
{
    if(!capture.product_epoch||!capture.native.exact_prior_owner||
       !capture.native.exact_prior_owner->exact_prepared_owner)
        return core::Result<render::RenderResult>::failure(core::Error{
            core::ErrorCode::InvalidState,"MANUAL_MATCH_TRANSFER_CAPTURE_NO_EPOCH"});
    const auto checked=validate_manual_match_transfer_project_readback(
        capture.project.after,capture.project,capture.native);
    if(!checked)return core::Result<render::RenderResult>::failure(*checked.error());
    return render_manual_match_transfer_candidate(capture.native,
        capture.project.identity,maxBlock,cancellation);
}
// Provisional playback ownership is deliberately NOT an M12 replay
// ticket. It retains the independently checked transfer identity and
// exact PREPARED owner, without reusing accepted solver checksums.
class ManualMatchTransferPreviewTicket final {
public:
    [[nodiscard]] static core::Result<ManualMatchTransferPreviewTicket> create(
        std::shared_ptr<const ManualMatchTransferCapture> capture)
    {
        using R=core::Result<ManualMatchTransferPreviewTicket>;
        if(!capture||!capture->product_epoch||!capture->native.exact_prior_owner||
           !capture->native.exact_prior_owner->exact_prepared_owner)
            return R::failure(core::Error{core::ErrorCode::InvalidState,
                "MANUAL_MATCH_PREVIEW_NO_CAPTURE"});
        const auto checked=validate_manual_match_transfer_project_readback(
            capture->project.after,capture->project,capture->native);
        if(!checked)return R::failure(*checked.error());
        return R::success(ManualMatchTransferPreviewTicket(std::move(capture)));
    }
    [[nodiscard]] const ManualMatchTransferCapture& capture() const noexcept {return *capture_;}
    [[nodiscard]] bool matches_manual(
        const dsp::ProcessingChain& chain,
        std::span<const dsp::ModuleExecutionBinding> bindings) const
    {
        const auto& expected=capture_->native.manual_after;
        const auto& proposed=expected.proposed_manual_chain;
        return chain.context()==proposed.context()&&chain.revision()==proposed.revision()&&
            std::equal(chain.instances().begin(),chain.instances().end(),
                       proposed.instances().begin(),proposed.instances().end())&&
            std::equal(bindings.begin(),bindings.end(),
                expected.proposed_bindings.begin(),expected.proposed_bindings.end());
    }
    [[nodiscard]] bool matches_prepared(
        const std::shared_ptr<const render::RenderResult>& owner) const noexcept
    {return owner&&owner==capture_->native.exact_prior_owner->exact_prepared_owner;}
    [[nodiscard]] bool matches_project(const project::ProjectSnapshot& snapshot) const
    {return capture_->project.matches_after(snapshot);}
    [[nodiscard]] core::Result<render::RenderResult> render(
        core::FrameCount maxBlock,const std::atomic<bool>* cancellation=nullptr) const
    {return render_captured_manual_match_transfer(*capture_,maxBlock,cancellation);}
    [[nodiscard]] constexpr bool is_verified() const noexcept {return false;}
private:
    explicit ManualMatchTransferPreviewTicket(std::shared_ptr<const ManualMatchTransferCapture> capture)
        :capture_(std::move(capture)) {}
    std::shared_ptr<const ManualMatchTransferCapture> capture_;
};
// Project Open must check the original accepted graph and the complete
// forward delta BEFORE replacing the current Source or any live DSP.
// The preflight has no PREPARED PCM and can never be played or installed.
struct ManualMatchTransferReopenPreflight final {
    ManualMatchTransferDraft graph;
    ManualMatchTransferProjectDraft project;
};
[[nodiscard]] inline core::Result<ManualMatchTransferReopenPreflight>
prepare_manual_match_transfer_reopen(
    const project::ProjectSnapshot& persisted,
    const MasteringChainState& before,
    const MasteringChainState& after)
{
    using R=core::Result<ManualMatchTransferReopenPreflight>;
    const auto fail=[](const char* why){return R::failure(core::Error{
        core::ErrorCode::InvalidState,why});};
    auto recovered=recover_manual_match_transfer_before(persisted);
    if(!recovered)return R::failure(*recovered.error());
    const auto& doc=recovered.value()->document();
    auto prior=m12_frozen::prepare_m12_product_reopen_preflight(doc,before,true);
    if(!prior)return R::failure(*prior.error());
    const auto metadata=QJsonDocument::fromJson(QByteArray::fromStdString(
        persisted.document().referenceMatch.canonical_utf8())).object();
    const auto instance=core::Uuid::parse(metadata.value(QStringLiteral("manualTransferCandidate"))
        .toObject().value(QStringLiteral("content")).toObject()
        .value(QStringLiteral("linkedInstance")).toString().toStdString());
    if(!instance)return R::failure(*instance.error());
    const auto id=dsp::ModuleInstanceId::from_uuid(*instance.value());
    if(!id)return R::failure(*id.error());
    const auto registry=before.registry_snapshot();
    const auto bindings=before.execution_bindings();
    const auto nodes=before.instances();
    if(!registry)return fail("MANUAL_MATCH_REOPEN_NO_MANUAL_REGISTRY");
    const auto graphOnly=std::make_shared<const m12_frozen::M12ImmutableProductPreviewTicket>(
        m12_frozen::M12ImmutableProductPreviewTicket{prior.value()->replay,{},
            before.chain().revision(),{nodes.begin(),nodes.end()},bindings});
    auto native=prepare_manual_match_transfer_graph(graphOnly,before.chain(),bindings,*registry,*id.value());
    if(!native)return R::failure(*native.error());
    auto checked=validate_recovered_manual_match_transfer(persisted,*native.value());
    if(!checked)return R::failure(*checked.error());
    const auto& expected=native.value()->manual_after;
    const auto actual=after.instances();
    const auto proposed=expected.proposed_manual_chain.instances();
    if(before.chain_id()!=after.chain_id()||
       after.chain().context()!=expected.proposed_manual_chain.context()||
       after.chain().revision()!=expected.proposed_manual_chain.revision()||
       !std::equal(actual.begin(),actual.end(),proposed.begin(),proposed.end())||
       after.execution_bindings()!=expected.proposed_bindings)
        return fail("MANUAL_MATCH_REOPEN_AFTER_MANUAL_MISMATCH");
    return R::success(ManualMatchTransferReopenPreflight{
        std::move(*native.value()),std::move(*checked.value())});
}
// After successful Source handoff bind the actual newly published
// PREPARED owner. Repeat the full project proof before issuing a ticket.
[[nodiscard]] inline core::Result<ManualMatchTransferPreviewTicket>
bind_manual_match_transfer_reopen(
    const ManualMatchTransferReopenPreflight& preflight,
    std::shared_ptr<const render::RenderResult> prepared,
    std::uint64_t epoch)
{
    using R=core::Result<ManualMatchTransferPreviewTicket>;
    const auto old=preflight.graph.exact_prior_owner;
    if(!old||!old->replay||!old->replay->registry_owner||!prepared||!epoch)
        return R::failure(core::Error{core::ErrorCode::InvalidState,
            "MANUAL_MATCH_REOPEN_NO_PREPARED_OWNER"});
    auto manual=dsp::ProcessingChain::restore(*old->replay->registry_owner,
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL},
        old->exact_manual_chain_revision,old->exact_manual_nodes);
    if(!manual)return R::failure(*manual.error());
    auto bound=m12_frozen::prepare_m12_immutable_product_preview_ticket(
        old->replay,std::move(prepared),*manual.value(),old->exact_manual_bindings);
    if(!bound)return R::failure(*bound.error());
    auto native=prepare_manual_match_transfer(
        std::make_shared<const m12_frozen::M12ImmutableProductPreviewTicket>(std::move(*bound.value())),
        *manual.value(),old->exact_manual_bindings,*old->replay->registry_owner,preflight.graph.linked_instance);
    if(!native)return R::failure(*native.error());
    auto project=validate_recovered_manual_match_transfer(preflight.project.after,*native.value());
    if(!project)return R::failure(*project.error());
    if(!project.value()->matches_before(preflight.project.before)||
       project.value()->identity.sha256!=preflight.project.identity.sha256||
       project.value()->identity.canonical_content!=preflight.project.identity.canonical_content)
        return R::failure(core::Error{core::ErrorCode::InvalidState,
            "MANUAL_MATCH_REOPEN_PREFLIGHT_CHANGED"});
    return ManualMatchTransferPreviewTicket::create(
        std::make_shared<const ManualMatchTransferCapture>(ManualMatchTransferCapture{
            epoch,std::move(*native.value()),std::move(*project.value())}));
}
// Worker completion binds PCM to the exact immutable capture that
// produced it. A caller cannot construct this envelope from unrelated
// PCM. The GUI still has to revalidate the Product owner and request
// explicit confirmation before any live transaction.
class ManualMatchTransferAudition final {
public:
    [[nodiscard]] static core::Result<ManualMatchTransferAudition> create(
        std::shared_ptr<const ManualMatchTransferCapture> capture,
        core::FrameCount maxBlock,const std::atomic<bool>* cancellation=nullptr)
    {
        using R=core::Result<ManualMatchTransferAudition>;
        if(!capture)return R::failure(core::Error{
            core::ErrorCode::InvalidState,"MANUAL_MATCH_TRANSFER_AUDITION_NO_CAPTURE"});
        auto rendered=render_captured_manual_match_transfer(*capture,maxBlock,cancellation);
        if(!rendered)return R::failure(*rendered.error());
        if(cancellation&&cancellation->load(std::memory_order_acquire))
            return R::failure(core::Error{core::ErrorCode::InvalidState,
                "MANUAL_MATCH_TRANSFER_AUDITION_CANCELLED"});
        return R::success(ManualMatchTransferAudition(
            std::move(capture),std::move(*rendered.value())));
    }
    [[nodiscard]] const ManualMatchTransferCapture& capture() const noexcept {return *capture_;}
    [[nodiscard]] const render::RenderResult& rendered() const noexcept {return rendered_;}
    [[nodiscard]] constexpr bool authorizes_project_commit() const noexcept {return false;}
    [[nodiscard]] constexpr bool is_verified() const noexcept {return false;}
private:
    ManualMatchTransferAudition(std::shared_ptr<const ManualMatchTransferCapture> capture,
        render::RenderResult rendered)
        :capture_(std::move(capture)),rendered_(std::move(rendered)) {}
    std::shared_ptr<const ManualMatchTransferCapture> capture_;
    render::RenderResult rendered_;
};
// A confirmation is bound to the exact rendered candidate and epoch,
// not to a generic "yes" or to a DSP row number. It is UI intent only;
// the live Project writer must still acquire its own exclusive lease.
struct ManualMatchTransferConfirmation final {
    std::uint64_t product_epoch{};
    std::string identity_sha256;
};
[[nodiscard]] inline bool manual_match_transfer_confirmation_matches(
    const ManualMatchTransferAudition& audition,
    const ManualMatchTransferConfirmation& confirmation)
{
    const auto& capture=audition.capture();
    return confirmation.product_epoch!=0U&&
        confirmation.product_epoch==capture.product_epoch&&
        !confirmation.identity_sha256.empty()&&
        confirmation.identity_sha256==capture.project.identity.sha256;
}
struct ManualMatchTransferAdoptionDraft final {
    MasteringChainState manual_after;
    std::shared_ptr<const ManualMatchTransferAudition> audition;
    [[nodiscard]] constexpr bool authorizes_project_commit() const noexcept {return false;}
};
[[nodiscard]] inline core::Result<ManualMatchTransferAdoptionDraft>
prepare_manual_match_transfer_adoption(
    const project::ProjectSnapshot& live,
    const MasteringChainState& manual,
    std::shared_ptr<const ManualMatchTransferAudition> audition,
    const ManualMatchTransferConfirmation& confirmation)
{
    using R=core::Result<ManualMatchTransferAdoptionDraft>;
    const auto fail=[](const char* why){return R::failure(core::Error{
        core::ErrorCode::InvalidState,why});};
    if(!audition||!manual_match_transfer_confirmation_matches(*audition,confirmation))
        return fail("MANUAL_MATCH_ADOPTION_CONFIRMATION_MISMATCH");
    const auto& capture=audition->capture();
    const auto& prior=capture.native.exact_prior_owner;
    const auto nodes=manual.instances();
    if(!prior||!prior->replay||!prior->exact_prepared_owner||
       !live.document().pipeline.masteringChainId||
       *live.document().pipeline.masteringChainId!=manual.chain_id()||
       !capture.project.matches_before(live)||
       manual.chain().revision()!=prior->exact_manual_chain_revision||
       !std::equal(nodes.begin(),nodes.end(),prior->exact_manual_nodes.begin(),
            prior->exact_manual_nodes.end())||
       manual.execution_bindings()!=prior->exact_manual_bindings)
        return fail("MANUAL_MATCH_ADOPTION_LIVE_OWNER_STALE");
    const auto checked=validate_manual_match_transfer_project_readback(
        capture.project.after,capture.project,capture.native);
    if(!checked)return R::failure(*checked.error());
    const auto& proposed=capture.native.manual_after;
    auto after=manual.prepare_explicit_manual_preset_state(
        proposed.proposed_manual_chain,proposed.proposed_bindings);
    if(!after)return R::failure(*after.error());
    if(after.value()->execution_bindings()!=proposed.proposed_bindings||
       after.value()->chain().revision()!=proposed.proposed_manual_chain.revision()||
       conflicting_active_match_dsps(after.value()->instances(),capture.native.linked_after.instances()))
        return fail("MANUAL_MATCH_ADOPTION_MANUAL_DELTA_INVALID");
    return R::success(ManualMatchTransferAdoptionDraft{
        std::move(*after.value()),std::move(audition)});
}
} // namespace rgsml::app::l3_automatch
