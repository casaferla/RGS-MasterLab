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
    const auto priorManualParameters=target->parameters.canonical_utf8();
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
        {"priorReferenceMatchCanonical",QString::fromStdString(doc.referenceMatch.canonical_utf8())},
        {"priorManualParametersCanonical",QString::fromStdString(priorManualParameters)},
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
    const auto priorMeta=project::OpaqueJsonValue::parse(
        candidate.value(QStringLiteral("priorReferenceMatchCanonical")).toString().toStdString());
    const auto priorParams=project::OpaqueJsonValue::parse(
        candidate.value(QStringLiteral("priorManualParametersCanonical")).toString().toStdString());
    if(!priorMeta||!priorParams)return fail("MANUAL_MATCH_RECOVERY_MISSING_CANONICAL_PRIOR");
    const auto exactPriorMeta=QJsonDocument::fromJson(
        QByteArray::fromStdString(priorMeta.value()->canonical_utf8()));
    if(!exactPriorMeta.isObject()||
       exactPriorMeta.object().value(QStringLiteral("materialization"))!=accepted)
        return fail("MANUAL_MATCH_RECOVERY_PRIOR_METADATA_MISMATCH");
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
            // The linked payload never changed. For MANUAL use the exact
            // canonical prior bytes, not Qt's numeric reserialization.
            const auto& payload=priorBypass?*priorParams.value():module.parameters;
            const auto parsedPayload=QJsonDocument::fromJson(
                QByteArray::fromStdString(payload.canonical_utf8()));
            if(!parsedPayload.isObject()||parsedPayload.object()!=params.toObject())return false;
            if(priorBypass)module.parameters=payload;
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
    if(metadata!=exactPriorMeta.object())
        return fail("MANUAL_MATCH_RECOVERY_UNRELATED_METADATA_CHANGED");
    doc.referenceMatch=std::move(*priorMeta.value());
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
[[nodiscard]] inline core::Result<dsp_presets::ProposedManualPresetRestore>
validate_transferred_manual_edit_overlay(
    const ManualMatchTransferDraft& native,
    const ManualMatchTransferProjectDraft& base,
    const project::ProjectSnapshot& persisted);
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
    [[nodiscard]] static core::Result<ManualMatchTransferPreviewTicket> create_edited(
        std::shared_ptr<const ManualMatchTransferCapture> capture,
        const project::ProjectSnapshot& persisted)
    {
        using R=core::Result<ManualMatchTransferPreviewTicket>;
        auto ticket=create(std::move(capture));
        if(!ticket)return ticket;
        const auto& origin=ticket.value()->capture();
        auto state=validate_transferred_manual_edit_overlay(origin.native,origin.project,persisted);
        if(!state)return R::failure(*state.error());
        if(origin.project.matches_after(persisted))
            return R::failure(core::Error{core::ErrorCode::InvalidState,"MANUAL_FINE_TUNE_NO_OVERLAY"});
        ticket.value()->edited_state_.emplace(std::move(*state.value()));
        ticket.value()->edited_project_.emplace(persisted);
        return ticket;
    }
    [[nodiscard]] const ManualMatchTransferCapture& capture() const noexcept {return *capture_;}
    [[nodiscard]] std::shared_ptr<const ManualMatchTransferCapture> capture_owner() const noexcept {return capture_;}
    [[nodiscard]] const dsp_presets::ProposedManualPresetRestore& manual_state() const noexcept {
        return edited_state_?*edited_state_:capture_->native.manual_after;
    }
    [[nodiscard]] const project::ProjectSnapshot& project_state() const noexcept {
        return edited_project_?*edited_project_:capture_->project.after;
    }
    [[nodiscard]] bool matches_manual(
        const dsp::ProcessingChain& chain,
        std::span<const dsp::ModuleExecutionBinding> bindings) const
    {
        const auto& expected=manual_state();
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
    {return m12_frozen::m12_exact_project_snapshot_equal(project_state(),snapshot);}
    [[nodiscard]] core::Result<render::RenderResult> render(
        core::FrameCount maxBlock,const std::atomic<bool>* cancellation=nullptr) const
    {
        if(edited_state_)return manual_transfer_detail::render_native_graph(
            capture_->native,*edited_state_,maxBlock,cancellation);
        return render_captured_manual_match_transfer(*capture_,maxBlock,cancellation);
    }
    [[nodiscard]] constexpr bool is_verified() const noexcept {return false;}
private:
    explicit ManualMatchTransferPreviewTicket(std::shared_ptr<const ManualMatchTransferCapture> capture)
        :capture_(std::move(capture)) {}
    std::shared_ptr<const ManualMatchTransferCapture> capture_;
    std::optional<dsp_presets::ProposedManualPresetRestore> edited_state_;
    std::optional<project::ProjectSnapshot> edited_project_;
};
// Stage an explicit edit of the transferred MANUAL slot only. This is
// not another transfer or a new accepted solver graph. No live mutation,
// persisted identity replacement, or PCM publication is authorized here.
[[nodiscard]] inline core::Result<dsp_presets::ProposedManualPresetRestore>
prepare_transferred_manual_parameter_edit(
    const ManualMatchTransferPreviewTicket& owner,
    const dsp::ProcessingChain& current,
    std::span<const dsp::ModuleExecutionBinding> bindings,
    const std::shared_ptr<const render::RenderResult>& prepared,
    const dsp::ModuleInstanceId& requested,
    const dsp::ModuleParameterPayload& replacement)
{
    using R=core::Result<dsp_presets::ProposedManualPresetRestore>;
    const auto fail=[](const char* why){return R::failure(core::Error{
        core::ErrorCode::InvalidState,why});};
    const auto& native=owner.capture().native;
    if(!owner.matches_prepared(prepared)||!owner.matches_manual(current,bindings)||
       native.manual_instance!=requested)
        return fail("MANUAL_MATCH_EDIT_STALE_OR_WRONG_SLOT");
    const auto nodes=current.instances();
    const auto it=std::find_if(nodes.begin(),nodes.end(),[&](const auto& node){
        return node.instance_id()==requested;});
    if(it==nodes.end()||!it->enabled())return fail("MANUAL_MATCH_EDIT_SLOT_UNAVAILABLE");
    const auto type=it->module_type_id();
    if(type=="rgsml.dsp.dynamic-eq") {
        const auto* value=std::get_if<dsp::DynamicEqParameters>(&replacement);
        if(!value)return fail("MANUAL_MATCH_EDIT_WRONG_PARAMETER_TYPE");
        const auto fmt=prepared->view().format();
        const auto valid=value->validate_for_audio(fmt.sample_rate().value(),
            static_cast<std::uint32_t>(fmt.channel_count()));
        if(!valid)return R::failure(*valid.error());
    } else if(type=="rgsml.dsp.transient-shaper") {
        if(!std::holds_alternative<dsp::TransientShaperParameters>(replacement))
            return fail("MANUAL_MATCH_EDIT_WRONG_PARAMETER_TYPE");
    } else return fail("MANUAL_MATCH_EDIT_UNSUPPORTED_TYPE");
    const auto index=static_cast<std::size_t>(it-nodes.begin());
    if(bindings[index].parameters==replacement)return fail("MANUAL_MATCH_EDIT_NO_CHANGE");
    const auto& registry=*native.exact_prior_owner->replay->registry_owner;
    const auto descriptor=registry.find_descriptor(type);
    if(!descriptor)return R::failure(*descriptor.error());
    const auto& d=descriptor.value()->get();
    if(!d.algorithm_version()||!d.parameter_schema_id())return fail("MANUAL_MATCH_EDIT_SCHEMA_MISSING");
    const dsp_presets::ManualPreset preset{dsp_presets::Scope::module,{
        {std::string(type),std::string(*d.algorithm_version()),std::string(*d.parameter_schema_id()),
         replacement,it->enabled(),it->user_bypass()}}};
    auto staged=dsp_presets::prepare_explicit_manual_preset_restore(
        preset,current,bindings,registry,index);
    if(!staged)return staged;
    if(conflicting_active_match_dsps(staged.value()->proposed_manual_chain.instances(),
        native.linked_after.instances()))return fail("MANUAL_MATCH_EDIT_ACTIVE_CONFLICT");
    return staged;
}

// Keep the accepted transfer record immutable. A fine-tuning edit has
// its own canonical identity and updates exactly one MANUAL payload.
// This draft is deliberately not installable until the preview and
// Project Open paths can reproduce the same overlay.
struct TransferredManualEditProjectDraft final {
    project::ProjectSnapshot before;
    project::ProjectSnapshot after;
    dsp_presets::ProposedManualPresetRestore staged;
    std::string identity_sha256;
};
[[nodiscard]] inline core::Result<TransferredManualEditProjectDraft>
prepare_transferred_manual_edit_project(
    const ManualMatchTransferPreviewTicket& owner,
    const project::ProjectSnapshot& live,
    const dsp::ProcessingChain& chain,
    std::span<const dsp::ModuleExecutionBinding> bindings,
    const std::shared_ptr<const render::RenderResult>& prepared,
    const dsp::ModuleInstanceId& requested,
    const dsp::ModuleParameterPayload& replacement)
{
    using R=core::Result<TransferredManualEditProjectDraft>;
    const auto fail=[](const char* why){return R::failure(core::Error{
        core::ErrorCode::InvalidState,why});};
    if(!owner.matches_project(live))return fail("MANUAL_MATCH_EDIT_PROJECT_STALE");
    auto staged=prepare_transferred_manual_parameter_edit(
        owner,chain,bindings,prepared,requested,replacement);
    if(!staged)return R::failure(*staged.error());
    auto doc=live.document();
    if(!doc.pipeline.masteringChainId)return fail("MANUAL_MATCH_EDIT_PROJECT_NO_MANUAL");
    auto manual=std::find_if(doc.chains.begin(),doc.chains.end(),[&](const auto& c){
        return c.chainId==*doc.pipeline.masteringChainId&&c.stage=="MASTER"&&c.segment=="MANUAL";});
    if(manual==doc.chains.end()||manual->revision!=chain.revision())
        return fail("MANUAL_MATCH_EDIT_PROJECT_REVISION_STALE");
    auto slot=std::find_if(manual->modules.begin(),manual->modules.end(),[&](const auto& m){
        return m.instanceId==requested.uuid();});
    if(slot==manual->modules.end())return fail("MANUAL_MATCH_EDIT_PROJECT_SLOT_MISSING");
    auto encoded=dsp::encode_module_parameters_json(replacement);
    if(!encoded)return R::failure(*encoded.error());
    auto payload=project::OpaqueJsonValue::parse(*encoded.value());
    if(!payload)return R::failure(*payload.error());
    if(!slot->parameterSchemaId)return fail("MANUAL_MATCH_EDIT_PROJECT_SCHEMA_MISSING");
    const auto decoded=dsp::decode_module_parameters_json(*slot->parameterSchemaId,
        payload.value()->canonical_utf8());
    if(!decoded||*decoded.value()!=replacement)
        return fail("MANUAL_MATCH_EDIT_PROJECT_SCHEMA_MISMATCH");
    const auto params=QJsonDocument::fromJson(QByteArray::fromStdString(payload.value()->canonical_utf8()));
    auto metadata=QJsonDocument::fromJson(QByteArray::fromStdString(doc.referenceMatch.canonical_utf8()));
    if(!metadata.isObject()||!params.isObject())
        return fail("MANUAL_MATCH_EDIT_PROJECT_METADATA_INVALID");
    auto record=metadata.object();
    if(record.contains(QStringLiteral("materialization")))
        return fail("MANUAL_MATCH_EDIT_PROJECT_UNEXPECTED_RECORD");
    const auto& base=owner.capture().project.after.document();
    const auto baseChain=std::find_if(base.chains.begin(),base.chains.end(),[&](const auto& c){
        return c.chainId==manual->chainId;});
    if(baseChain==base.chains.end())return fail("MANUAL_FINE_TUNE_BASE_CHAIN_MISSING");
    const auto baseSlot=std::find_if(baseChain->modules.begin(),baseChain->modules.end(),[&](const auto& m){
        return m.instanceId==requested.uuid();});
    if(baseSlot==baseChain->modules.end())return fail("MANUAL_FINE_TUNE_BASE_SLOT_MISSING");
    const QJsonObject edit{
        {"schemaId",QStringLiteral("rgsml.reference-match.manual-fine-tune/1.0.0")},
        {"baseMetadataCanonical",QString::fromStdString(base.referenceMatch.canonical_utf8())},
        {"baseParametersCanonical",QString::fromStdString(baseSlot->parameters.canonical_utf8())},
        {"editedParametersCanonical",QString::fromStdString(payload.value()->canonical_utf8())},
        {"transferIdentity",QString::fromStdString(owner.capture().project.identity.sha256)},
        {"instanceId",QString::fromStdString(requested.to_string())},
        {"priorRevision",QString::number(static_cast<qulonglong>(manual->revision))},
        {"revision",QString::number(static_cast<qulonglong>(staged.value()->proposed_manual_chain.revision()))},
        {"parameters",params.object()},
        {"verificationState",QStringLiteral("NOT_VERIFIED")}};
    auto canonical=project::OpaqueJsonValue::parse(
        QJsonDocument(edit).toJson(QJsonDocument::Compact).toStdString());
    if(!canonical)return R::failure(*canonical.error());
    const auto digest=QCryptographicHash::hash(
        QByteArray::fromStdString(canonical.value()->canonical_utf8()),QCryptographicHash::Sha256).toHex();
    record.insert(QStringLiteral("manualFineTune"),QJsonObject{
        {"content",edit},{"identitySha256",QString::fromLatin1(digest)}});
    auto nextMetadata=project::OpaqueJsonValue::parse(
        QJsonDocument(record).toJson(QJsonDocument::Compact).toStdString());
    if(!nextMetadata)return R::failure(*nextMetadata.error());
    slot->parameters=std::move(*payload.value());
    manual->revision=staged.value()->proposed_manual_chain.revision();
    doc.referenceMatch=std::move(*nextMetadata.value());
    auto after=project::ProjectSnapshot::create(std::move(doc),
        {live.optional_entries().begin(),live.optional_entries().end()});
    if(!after)return R::failure(*after.error());
    return R::success(TransferredManualEditProjectDraft{
        live,std::move(*after.value()),std::move(*staged.value()),digest.toStdString()});
}

// Reverse only the fine-tuning overlay. The returned transfer snapshot
// remains untrusted until the original accepted graph and its transfer
// are independently reconstructed by the existing reopen gate.
[[nodiscard]] inline core::Result<project::ProjectSnapshot>
recover_transferred_manual_edit_base(const project::ProjectSnapshot& persisted)
{
    using R=core::Result<project::ProjectSnapshot>;
    const auto fail=[](const char* why){return R::failure(core::Error{
        core::ErrorCode::InvalidState,why});};
    auto doc=persisted.document();
    const auto parsed=QJsonDocument::fromJson(
        QByteArray::fromStdString(doc.referenceMatch.canonical_utf8()));
    if(!parsed.isObject()||!doc.pipeline.masteringChainId)
        return fail("MANUAL_FINE_TUNE_RECOVERY_NO_OWNER");
    auto metadata=parsed.object();
    if(!metadata.contains(QStringLiteral("manualFineTune")))return R::success(persisted);
    if(metadata.contains(QStringLiteral("materialization")))
        return fail("MANUAL_FINE_TUNE_RECOVERY_ACCEPTED_RECORD");
    const auto overlay=metadata.take(QStringLiteral("manualFineTune")).toObject();
    const auto edit=overlay.value(QStringLiteral("content")).toObject();
    const auto transfer=metadata.value(QStringLiteral("manualTransferCandidate")).toObject();
    const auto content=transfer.value(QStringLiteral("content")).toObject();
    const auto baseMeta=project::OpaqueJsonValue::parse(edit.value(QStringLiteral("baseMetadataCanonical")).toString().toStdString());
    const auto baseParams=project::OpaqueJsonValue::parse(edit.value(QStringLiteral("baseParametersCanonical")).toString().toStdString());
    const auto editedParams=project::OpaqueJsonValue::parse(edit.value(QStringLiteral("editedParametersCanonical")).toString().toStdString());
    if(!baseMeta||!baseParams||!editedParams)return fail("MANUAL_FINE_TUNE_RECOVERY_CANONICAL_BYTES_MISSING");
    const auto baseObject=QJsonDocument::fromJson(QByteArray::fromStdString(baseMeta.value()->canonical_utf8()));
    if(!baseObject.isObject()||baseObject.object()!=metadata)return fail("MANUAL_FINE_TUNE_RECOVERY_METADATA_MISMATCH");
    if(edit.value(QStringLiteral("schemaId")).toString()!=QStringLiteral("rgsml.reference-match.manual-fine-tune/1.0.0")||
       edit.value(QStringLiteral("verificationState")).toString()!=QStringLiteral("NOT_VERIFIED")||
       edit.value(QStringLiteral("transferIdentity")).toString()!=transfer.value(QStringLiteral("identitySha256")).toString()||
       edit.value(QStringLiteral("instanceId")).toString()!=content.value(QStringLiteral("manualInstance")).toString())
        return fail("MANUAL_FINE_TUNE_RECOVERY_IDENTITY_MISMATCH");
    const auto canonical=project::OpaqueJsonValue::parse(
        QJsonDocument(edit).toJson(QJsonDocument::Compact).toStdString());
    if(!canonical)return R::failure(*canonical.error());
    const auto hash=QCryptographicHash::hash(
        QByteArray::fromStdString(canonical.value()->canonical_utf8()),QCryptographicHash::Sha256).toHex();
    if(QString::fromLatin1(hash)!=overlay.value(QStringLiteral("identitySha256")).toString())
        return fail("MANUAL_FINE_TUNE_RECOVERY_DIGEST_MISMATCH");
    const auto graph=content.value(QStringLiteral("manual")).toObject();
    bool baseOk=false,priorOk=false,revisionOk=false;
    const auto baseRevision=graph.value(QStringLiteral("revision")).toString().toULongLong(&baseOk);
    const auto priorRevision=edit.value(QStringLiteral("priorRevision")).toString().toULongLong(&priorOk);
    const auto revision=edit.value(QStringLiteral("revision")).toString().toULongLong(&revisionOk);
    if(!baseOk||!priorOk||!revisionOk||priorRevision<baseRevision||
       priorRevision==std::numeric_limits<std::uint64_t>::max()||revision!=priorRevision+1U)
        return fail("MANUAL_FINE_TUNE_RECOVERY_REVISION_MISMATCH");
    auto manual=std::find_if(doc.chains.begin(),doc.chains.end(),[&](const auto& c){
        return c.chainId==*doc.pipeline.masteringChainId&&c.stage=="MASTER"&&c.segment=="MANUAL";});
    if(manual==doc.chains.end()||manual->revision!=revision)
        return fail("MANUAL_FINE_TUNE_RECOVERY_MANUAL_MISMATCH");
    const auto nodes=graph.value(QStringLiteral("modules")).toArray();
    if(static_cast<std::size_t>(nodes.size())!=manual->modules.size())
        return fail("MANUAL_FINE_TUNE_RECOVERY_TOPOLOGY_MISMATCH");
    const auto id=edit.value(QStringLiteral("instanceId")).toString();
    std::size_t found=0;
    for(std::size_t i=0;i<manual->modules.size();++i) {
        auto& slot=manual->modules[i];
        const auto node=nodes.at(static_cast<qsizetype>(i)).toObject();
        if(QString::fromStdString(slot.instanceId.to_string())!=node.value(QStringLiteral("instanceId")).toString()||
           QString::fromStdString(slot.typeId)!=node.value(QStringLiteral("typeId")).toString())
            return fail("MANUAL_FINE_TUNE_RECOVERY_TOPOLOGY_MISMATCH");
        if(node.value(QStringLiteral("instanceId")).toString()!=id)continue;
        ++found;
        if(!edit.value(QStringLiteral("parameters")).isObject()||
           !node.value(QStringLiteral("parameters")).isObject())
            return fail("MANUAL_FINE_TUNE_RECOVERY_PAYLOAD_MISSING");
        const auto current=QJsonDocument::fromJson(QByteArray::fromStdString(editedParams.value()->canonical_utf8()));
        const auto original=QJsonDocument::fromJson(QByteArray::fromStdString(baseParams.value()->canonical_utf8()));
        if(!current.isObject()||!original.isObject()||
           current.object()!=edit.value(QStringLiteral("parameters")).toObject()||
           original.object()!=node.value(QStringLiteral("parameters")).toObject()||
           slot.parameters!=*editedParams.value())
            return fail("MANUAL_FINE_TUNE_RECOVERY_PAYLOAD_MISMATCH");
        slot.parameters=*baseParams.value();
    }
    if(found!=1U)return fail("MANUAL_FINE_TUNE_RECOVERY_SLOT_MISSING");
    manual->revision=baseRevision;
    doc.referenceMatch=*baseMeta.value();
    return project::ProjectSnapshot::create(std::move(doc),
        {persisted.optional_entries().begin(),persisted.optional_entries().end()});
}

// Reconstruct the complete typed after-graph from the checked original
// transfer and the one allowed overlay. No persisted node can supply its
// own registry, ownership, bypass or execution order.
[[nodiscard]] inline core::Result<dsp_presets::ProposedManualPresetRestore>
validate_transferred_manual_edit_overlay(
    const ManualMatchTransferDraft& native,
    const ManualMatchTransferProjectDraft& base,
    const project::ProjectSnapshot& persisted)
{
    using R=core::Result<dsp_presets::ProposedManualPresetRestore>;
    const auto fail=[](const char* why){return R::failure(core::Error{
        core::ErrorCode::InvalidState,why});};
    const auto verifiedBase=validate_manual_match_transfer_project_readback(base.after,base,native);
    if(!verifiedBase)return R::failure(*verifiedBase.error());
    auto recovered=recover_transferred_manual_edit_base(persisted);
    if(!recovered)return R::failure(*recovered.error());
    if(!base.matches_after(*recovered.value()))return fail("MANUAL_FINE_TUNE_BASE_MISMATCH");
    if(base.matches_after(persisted))return R::success(native.manual_after);
    const auto meta=QJsonDocument::fromJson(QByteArray::fromStdString(
        persisted.document().referenceMatch.canonical_utf8())).object();
    const auto overlay=meta.value(QStringLiteral("manualFineTune")).toObject();
    const auto edit=overlay.value(QStringLiteral("content")).toObject();
    if(overlay.size()!=2||edit.size()!=10)return fail("MANUAL_FINE_TUNE_UNKNOWN_FIELDS");
    const auto& original=native.manual_after;
    const auto nodes=original.proposed_manual_chain.instances();
    const auto it=std::find_if(nodes.begin(),nodes.end(),[&](const auto& node){
        return node.instance_id()==native.manual_instance;});
    if(it==nodes.end())return fail("MANUAL_FINE_TUNE_SLOT_MISSING");
    const auto index=static_cast<std::size_t>(it-nodes.begin());
    const auto& registry=*native.exact_prior_owner->replay->registry_owner;
    const auto descriptor=registry.find_descriptor(it->module_type_id());
    if(!descriptor||!descriptor.value()->get().parameter_schema_id())
        return fail("MANUAL_FINE_TUNE_SCHEMA_MISSING");
    const auto canonical=project::OpaqueJsonValue::parse(
        edit.value(QStringLiteral("editedParametersCanonical")).toString().toStdString());
    if(!canonical)return R::failure(*canonical.error());
    const auto decoded=dsp::decode_module_parameters_json(
        *descriptor.value()->get().parameter_schema_id(),canonical.value()->canonical_utf8());
    if(!decoded)return R::failure(*decoded.error());
    if(it->module_type_id()=="rgsml.dsp.dynamic-eq") {
        const auto* p=std::get_if<dsp::DynamicEqParameters>(decoded.value());
        if(!p)return fail("MANUAL_FINE_TUNE_WRONG_TYPE");
        if(native.exact_prior_owner->exact_prepared_owner) {
            const auto fmt=native.exact_prior_owner->exact_prepared_owner->view().format();
            const auto valid=p->validate_for_audio(fmt.sample_rate().value(),static_cast<std::uint32_t>(fmt.channel_count()));
            if(!valid)return R::failure(*valid.error());
        }
    } else if(it->module_type_id()!="rgsml.dsp.transient-shaper"||
       !std::holds_alternative<dsp::TransientShaperParameters>(*decoded.value()))
        return fail("MANUAL_FINE_TUNE_WRONG_TYPE");
    bool ok=false;
    const auto revision=edit.value(QStringLiteral("revision")).toString().toULongLong(&ok);
    if(!ok||revision<=original.proposed_manual_chain.revision())return fail("MANUAL_FINE_TUNE_REVISION_INVALID");
    auto chain=dsp::ProcessingChain::restore(registry,original.proposed_manual_chain.context(),
        revision,{nodes.begin(),nodes.end()});
    if(!chain)return R::failure(*chain.error());
    auto bindings=original.proposed_bindings;
    bindings[index].parameters=*decoded.value();
    return R::success(dsp_presets::ProposedManualPresetRestore{
        std::move(*chain.value()),std::move(bindings)});
}

// Project Open must check the original accepted graph and the complete
// forward delta BEFORE replacing the current Source or any live DSP.
// The preflight has no PREPARED PCM and can never be played or installed.
struct ManualMatchTransferReopenPreflight final {
    ManualMatchTransferDraft graph;
    ManualMatchTransferProjectDraft project;
    std::optional<project::ProjectSnapshot> edited_project;
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
    auto base=recover_transferred_manual_edit_base(persisted);
    if(!base)return R::failure(*base.error());
    auto recovered=recover_manual_match_transfer_before(*base.value());
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
    auto checked=validate_recovered_manual_match_transfer(*base.value(),*native.value());
    if(!checked)return R::failure(*checked.error());
    auto edited=validate_transferred_manual_edit_overlay(*native.value(),*checked.value(),persisted);
    if(!edited)return R::failure(*edited.error());
    const auto& expected=*edited.value();
    const auto actual=after.instances();
    const auto proposed=expected.proposed_manual_chain.instances();
    if(before.chain_id()!=after.chain_id()||
       after.chain().context()!=expected.proposed_manual_chain.context()||
       after.chain().revision()!=expected.proposed_manual_chain.revision()||
       !std::equal(actual.begin(),actual.end(),proposed.begin(),proposed.end())||
       after.execution_bindings()!=expected.proposed_bindings)
        return fail("MANUAL_MATCH_REOPEN_AFTER_MANUAL_MISMATCH");
    return R::success(ManualMatchTransferReopenPreflight{
        std::move(*native.value()),std::move(*checked.value()),
        m12_frozen::m12_exact_project_snapshot_equal(*base.value(),persisted)
            ?std::nullopt:std::optional<project::ProjectSnapshot>{persisted}});
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
    auto capture=std::make_shared<const ManualMatchTransferCapture>(ManualMatchTransferCapture{
        epoch,std::move(*native.value()),std::move(*project.value())});
    if(preflight.edited_project)return ManualMatchTransferPreviewTicket::create_edited(
        std::move(capture),*preflight.edited_project);
    return ManualMatchTransferPreviewTicket::create(std::move(capture));
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
// Session-local undo data keeps value states and immutable graph owners,
// not another full rendered candidate PCM. It cannot issue M12 receipts.
struct ManualMatchTransferHistory final {
    MasteringChainState before_manual;
    MasteringChainState after_manual;
    std::shared_ptr<const ManualMatchTransferPreviewTicket> after_ticket;
};
struct TransferredManualEditHistory final {
    MasteringChainState before_manual;
    MasteringChainState after_manual;
    std::shared_ptr<const ManualMatchTransferPreviewTicket> before_ticket;
    std::shared_ptr<const ManualMatchTransferPreviewTicket> after_ticket;
};
} // namespace rgsml::app::l3_automatch
