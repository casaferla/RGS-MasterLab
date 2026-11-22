#include "internal/l3_m12_ref_linked_project_carrier.hpp"
#include "internal/l3_m12_project_transaction_draft.hpp"
#include "internal/l3_m12_p2_persisted_sound_guard.hpp"
#include "internal/l3_m12_exact_postcommit_readback.hpp"
#include "internal/l3_m12_ref_linked_project_restore.hpp"
#include "internal/l3_m12_ref_linked_native_replay_plan.hpp"
#include "internal/l3_m12_restored_native_render.hpp"
#include "internal/l3_m12_product_preview_worker_ticket.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/streaming_true_peak_limiter_parameters.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>
#include <bit>
#include <cmath>
#include <numbers>
#include "internal/l3_m12_ref_linked_project_open_gate.hpp"
#include "internal/dsp_manual_preset_bundle.hpp"
#include "internal/l3_m12_owned_controller_suspension.hpp"
#include <rgsml/dsp/true_peak_limiter_parameters.hpp>
#include "canonical_json.hpp"
#include <rgsml/project/project_snapshot.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>
#include <rgsml/dsp/dynamic_eq_parameters.hpp>
#include <rgsml/dsp/transient_shaper_parameters.hpp>
#include <QtTest/QTest>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
namespace rgsml::tests {
namespace {
namespace m=rgsml::app::l3_automatch::m12_frozen;
[[nodiscard]] core::Uuid uuid(const char* s) {
    auto u=core::Uuid::parse(s);Q_ASSERT(u);return *u.value();
}
[[nodiscard]] dsp::ModuleInstanceId id(const char* s) {
    auto u=dsp::ModuleInstanceId::from_uuid(uuid(s));Q_ASSERT(u);
    return *u.value();
}
[[nodiscard]] m::AcceptedPlanAndOwner accepted() {
    return {uuid("b1000000-0000-4000-8000-000000000001"),
            uuid("b1000000-0000-4000-8000-000000000002"),
            "full-current-plan-sha","full-m10-winning-sha",
            "full-fixed-chain-sha","full-scope-sha",11,
            true,true,true,false};
}
class M12NativeProjectCarrierTest final:public QObject{
    Q_OBJECT
private slots:
    void realDynamicEqTransientAndEqRoundtripAsOrdinaryProjectModules();
    void wrongProjectChainOrUnavailableFactoryCannotBePublished();
    void userManualChainAndSingleModulePresetsAreStrictlyTyped();
    void optimizedManualP2CannotBeSilentlyPersistedAsDifferentSound();
};
void M12NativeProjectCarrierTest::
realDynamicEqTransientAndEqRoundtripAsOrdinaryProjectModules() {
    auto base=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(base);
    auto with_deq=dsp::ModuleRegistry::with_dynamic_eq_offline(
        std::move(*base.value()));
    QVERIFY(with_deq);
    auto with_ts=dsp::ModuleRegistry::with_transient_shaper_offline(
        std::move(*with_deq.value()));
    QVERIFY(with_ts);
    auto reg=std::make_shared<const dsp::ModuleRegistry>(
        std::move(*with_ts.value()));
    auto manual=dsp::ProcessingChain::create(*reg,{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(manual);
    auto deq=dsp::DynamicEqParameters::create_default(
        uuid("b2000000-0000-4000-8000-000000000001"));
    auto ts=dsp::TransientShaperParameters::create(6.0,-3.0);
    auto eq=dsp::ParametricEqParameters::create_legacy_default();
    QVERIFY(deq&&ts&&eq);
    const std::vector<m::ExactLinkedNode> nodes{
        {id("b3000000-0000-4000-8000-000000000001"),
         uuid("b4000000-0000-4000-8000-000000000001"),
         "rgsml.dsp.dynamic-eq",*deq.value()},
        {id("b3000000-0000-4000-8000-000000000002"),
         uuid("b4000000-0000-4000-8000-000000000002"),
         "rgsml.dsp.transient-shaper",*ts.value()},
        {id("b3000000-0000-4000-8000-000000000003"),
         uuid("b4000000-0000-4000-8000-000000000003"),
         "rgsml.dsp.parametric-eq",*eq.value()}};
    const std::vector<dsp::ModuleExecutionBinding> none;
    const auto stage=m::stage_exact_m12_native_ref_linked(
        accepted(),*manual.value(),none,reg,nodes);
    QVERIFY(stage);
    const auto refChainId=uuid("b5000000-0000-4000-8000-000000000001");
    const auto carrier=m::encode_exact_ref_linked_project_carrier(
        *stage.value(),refChainId);
    QVERIFY(carrier);
    QCOMPARE(carrier.value()->native_project_chain.modules.size(),
             std::size_t{3});
    QCOMPARE(carrier.value()->native_project_chain.segment,
             std::string("REF_LINKED"));
    QVERIFY(!carrier.value()->authorizes_project_commit());
    for(const auto& module:carrier.value()->native_project_chain.modules) {
        QCOMPARE(module.provenance,std::string("REFERENCE_MATCH"));
        QCOMPARE(module.owner,std::string("REFERENCE_MATCH"));
        QCOMPARE(module.linkState,std::string("REF_LINKED"));
        QVERIFY(module.semanticNodeId);
        QVERIFY(module.algorithmVersion);
        QVERIFY(module.parameterSchemaId);
        QVERIFY(module.parameters.canonical_utf8().starts_with("{"));
    }
    project::ProjectDocument doc;
    doc.projectId=uuid("b6000000-0000-4000-8000-000000000001");
    doc.displayName="RGS L3 M12 nonpublishing native stage";
    doc.sourceResourceId=uuid("b7000000-0000-4000-8000-000000000001");
    doc.resources.push_back({
        doc.sourceResourceId,"AUDIO",
        {{"rgsml.windows.local-file","C:/source.wav","source.wav"}},
        std::nullopt});
    const auto referenceResource=uuid(
        "b7000000-0000-4000-8000-000000000002");
    const auto activeReference=uuid(
        "b7000000-0000-4000-8000-000000000003");
    doc.resources.push_back({
        referenceResource,"AUDIO",
        {{"rgsml.windows.local-file","C:/reference.wav","reference.wav"}},
        std::nullopt});
    doc.references.activeReferenceId=activeReference;
    doc.references.items.push_back({
        activeReference,"GOLD",referenceResource});
    project::Chain manualChain;
    manualChain.chainId=uuid("b8000000-0000-4000-8000-000000000001");
    manualChain.stage="MASTER";
    manualChain.segment="MANUAL";
    manualChain.revision=0;
    doc.pipeline.masteringChainId=manualChain.chainId;
    doc.chains.push_back(manualChain);
    const auto currentBefore=project::ProjectSnapshot::create(doc);
    QVERIFY(currentBefore);
    // AutoMatch M12 needs no M17 WAV export limiter or ceiling.
    m::M12ProjectTransactionExpected identity{
        doc.sourceResourceId,activeReference,manualChain.chainId,
        manualChain,std::nullopt,true,true};
    const auto transaction=m::prepare_exact_m12_project_transaction(
        *currentBefore.value(),*carrier.value(),*stage.value(),
        identity);
    QVERIFY(transaction);
    QVERIFY(!transaction.value()->authorizes_project_commit());
    QVERIFY(!transaction.value()->is_materialization_receipt());
    const auto& proposed=
        transaction.value()->candidate_not_yet_committed.document();
    QCOMPARE(proposed.chains.size(),std::size_t{2});
    // Until ProjectSession can atomically replay REF_LINKED DSP, even
    // a valid M12 candidate must NOT be silently reopened as MANUAL
    // alone. Legacy projects with no linked segment remain supported.
    QCOMPARE(m::assess_current_product_ref_linked_open_safety(doc),
             m::ProjectOpenLinkedSafety::
                 ordinary_project_without_ref_linked);
    QCOMPARE(m::assess_current_product_ref_linked_open_safety(proposed),
             m::ProjectOpenLinkedSafety::
                 blocked_until_product_ref_linked_replay_is_installed);
    // Even a deliberately malformed/partially published REF plan
    // cannot be interpreted as playable ordinary MANUAL sound.
    auto metadataOnly=proposed;
    metadataOnly.chains.pop_back();
    QCOMPARE(m::assess_current_product_ref_linked_open_safety(metadataOnly),
             m::ProjectOpenLinkedSafety::
                 blocked_until_product_ref_linked_replay_is_installed);
    auto misplaced=doc;
    misplaced.chains.front().segment="REF_LINKED";
    QCOMPARE(m::assess_current_product_ref_linked_open_safety(misplaced),
             m::ProjectOpenLinkedSafety::
                 blocked_until_product_ref_linked_replay_is_installed);
    auto foreignNode=doc;
    auto ownershipNode=carrier.value()->native_project_chain.modules.front();
    ownershipNode.linkState="UNLINKED";
    ownershipNode.owner="USER";
    foreignNode.chains.front().modules.push_back(ownershipNode);
    QCOMPARE(m::assess_current_product_ref_linked_open_safety(foreignNode),
             m::ProjectOpenLinkedSafety::
                 blocked_until_product_ref_linked_replay_is_installed);
    foreignNode.chains.front().modules.back().provenance="MANUAL";
    foreignNode.chains.front().modules.back().owner="REFERENCE_MATCH";
    QCOMPARE(m::assess_current_product_ref_linked_open_safety(foreignNode),
             m::ProjectOpenLinkedSafety::
                 blocked_until_product_ref_linked_replay_is_installed);
    foreignNode.chains.front().modules.back().owner="USER";
    foreignNode.chains.front().modules.back().linkState="REF_LINKED";
    QCOMPARE(m::assess_current_product_ref_linked_open_safety(foreignNode),
             m::ProjectOpenLinkedSafety::
                 blocked_until_product_ref_linked_replay_is_installed);
    // Dynamic EQ / Transient Shaper / Parametric EQ are real
    // registered DSPs, not opaque plan-only placeholders. Rebuild
    // them from the actual persisted REF_LINKED project segment.
    const auto productReopen=m::decode_exact_m12_ref_linked_product_state(
        proposed,reg);
    QVERIFY(productReopen);
    // The persisted metadata must round-trip every original plan,
    // winning result, M11 protection and scope dependency: omitting
    // any one is not a legitimate restored candidate.
    const auto fullRecord=QJsonDocument::fromJson(
        QByteArray::fromStdString(
            proposed.referenceMatch.canonical_utf8())).object();
    for(const char* key:{"policyId","winnerContentChecksum",
                         "protectedFixedChecksum",
                         "sourceReferenceScopeRevision","planRevision"}) {
        auto badRecord=fullRecord;
        auto badMaterialization=badRecord.value(
            QStringLiteral("materialization")).toObject();
        badMaterialization.remove(QString::fromLatin1(key));
        badRecord.insert(QStringLiteral("materialization"),
                         badMaterialization);
        auto badProject=proposed;
        const auto parsed=project::OpaqueJsonValue::parse(
            QJsonDocument(badRecord).toJson(
                QJsonDocument::Compact).toStdString());
        QVERIFY(parsed);
        badProject.referenceMatch=*parsed.value();
        QVERIFY(!m::decode_exact_m12_ref_linked_product_state(
            badProject,reg));
    }
    auto wrongRevisionRecord=fullRecord;
    auto wrongRevisionMaterialization=wrongRevisionRecord.value(
        QStringLiteral("materialization")).toObject();
    wrongRevisionMaterialization.insert(
        QStringLiteral("planRevision"),QStringLiteral("999999"));
    wrongRevisionRecord.insert(QStringLiteral("materialization"),
                               wrongRevisionMaterialization);
    auto wrongRevisionProject=proposed;
    const auto wrongRevisionPayload=project::OpaqueJsonValue::parse(
        QJsonDocument(wrongRevisionRecord).toJson(
            QJsonDocument::Compact).toStdString());
    QVERIFY(wrongRevisionPayload);
    wrongRevisionProject.referenceMatch=*wrongRevisionPayload.value();
    QVERIFY(!m::decode_exact_m12_ref_linked_product_state(
        wrongRevisionProject,reg));
    QCOMPARE(productReopen.value()->persisted_linked_chain_id,refChainId);
    QCOMPARE(productReopen.value()->exact_plan_revision,
             std::uint64_t{11});
    QCOMPARE(productReopen.value()->exact_m10_winning_checksum,
             accepted().exact_m10_result_and_topology_checksum);
    QCOMPARE(productReopen.value()->exact_m11_protected_fixed_checksum,
             accepted().exact_m11_fixed_and_protection_checksum);
    QCOMPARE(productReopen.value()->exact_source_reference_scope_revision,
             accepted().exact_source_reference_amount_scope_revision);
    // Read-only Product preflight requires the exact protected MANUAL
    // snapshot. The original synthetic fixture has no Gain, so neither
    // a forged owner-attestation nor a linked-only graph may play.
    QVERIFY(!m::prepare_restored_m12_native_replay_plan(
        proposed,*productReopen.value(),
        manualChain.chainId,*manual.value(),none,true));
    QVERIFY(!m::prepare_restored_m12_native_replay_plan(
        proposed,*productReopen.value(),
        manualChain.chainId,*manual.value(),none,false));
    QCOMPARE(productReopen.value()->exact_native_chain.instances().size(),
             stage.value()->chain.instances().size());
    QVERIFY(std::equal(
        productReopen.value()->exact_native_chain.instances().begin(),
        productReopen.value()->exact_native_chain.instances().end(),
        stage.value()->chain.instances().begin()));
    QCOMPARE(productReopen.value()->exact_native_bindings,
             stage.value()->exact_bindings);
    QCOMPARE(productReopen.value()->exact_native_bindings.size(),
             std::size_t{3});
    QCOMPARE(productReopen.value()->plan_id,accepted().plan_id);
    QCOMPARE(productReopen.value()->controller_id,
             accepted().reference_match_controller_id);
    QVERIFY(!productReopen.value()->authorizes_project_adoption());
    QVERIFY(!productReopen.value()->is_m12_materialization_receipt());
    // Construct a Product-shaped protected MANUAL graph and preflight
    // real owned native replay without accepting/adopting the M12 draft.
    auto manualGain=dsp::ProcessingChain::create(*reg,{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(manualGain);
    const auto gainId=id("bf100000-0000-4000-8000-000000000001");
    QVERIFY(manualGain.value()->add(gainId,"rgsml.dsp.gain",0));
    const auto gainParams=dsp::GainParameters::create(-3.0);
    QVERIFY(gainParams);
    const std::vector<dsp::ModuleExecutionBinding> gainBindings{{
        gainId,*gainParams.value()}};
    const auto serializedGain=dsp::encode_module_parameters_json(
        gainBindings.front().parameters);
    QVERIFY(serializedGain);
    const auto savedGainJson=project::OpaqueJsonValue::parse(
        *serializedGain.value());
    QVERIFY(savedGainJson);
    project::Module savedGain;
    savedGain.instanceId=gainId.uuid();
    savedGain.typeId="rgsml.dsp.gain";
    savedGain.parameters=*savedGainJson.value();
    const auto gainDescriptor=reg->find_descriptor(savedGain.typeId);
    QVERIFY(gainDescriptor);
    savedGain.algorithmVersion=gainDescriptor.value()->get().algorithm_version();
    savedGain.parameterSchemaId=gainDescriptor.value()->get().parameter_schema_id();
    auto replayDocument=proposed;
    replayDocument.chains.front().revision=manualGain.value()->revision();
    replayDocument.chains.front().modules.push_back(savedGain);
    const auto replayPlan=m::prepare_restored_m12_native_replay_plan(
        replayDocument,*productReopen.value(),manualChain.chainId,
        *manualGain.value(),gainBindings,true);
    QVERIFY(replayPlan);
    QVERIFY(!replayPlan.value()->authorizes_project_adoption());
    QVERIFY(!replayPlan.value()->is_m12_materialization_receipt());
    QVERIFY(!replayPlan.value()->is_m13_verified());
    QCOMPARE(replayPlan.value()->manual_prefix.instances().size(),
             std::size_t{1});
    QCOMPARE(replayPlan.value()->reference_linked.instances().size(),
             std::size_t{3});
    QVERIFY(!replayPlan.value()->terminal_p2);
    QVERIFY(!m::prepare_restored_m12_native_replay_plan(
        replayDocument,*productReopen.value(),manualChain.chainId,
        *manualGain.value(),gainBindings,false));
    auto changedGain=replayDocument;
    changedGain.chains.front().modules.front().userBypass=true;
    QVERIFY(!m::prepare_restored_m12_native_replay_plan(
        changedGain,*productReopen.value(),manualChain.chainId,
        *manualGain.value(),gainBindings,true));
    changedGain=replayDocument;
    changedGain.chains.front().modules.front().parameters=
        *project::OpaqueJsonValue::parse("{\"gainDb\":5.0}").value();
    QVERIFY(!m::prepare_restored_m12_native_replay_plan(
        changedGain,*productReopen.value(),manualChain.chainId,
        *manualGain.value(),gainBindings,true));
    auto forgedLinked=*productReopen.value();
    forgedLinked.exact_m10_winning_checksum="forged-winner";
    QVERIFY(!m::prepare_restored_m12_native_replay_plan(
        replayDocument,forgedLinked,manualChain.chainId,
        *manualGain.value(),gainBindings,true));
    forgedLinked=*productReopen.value();
    forgedLinked.exact_native_chain=std::move(
        *dsp::ProcessingChain::restore(*reg,{
            dsp::ProcessingStage::MASTER,dsp::ChainSegment::REF_LINKED},
            12,{forgedLinked.exact_native_chain.instances().begin(),
                forgedLinked.exact_native_chain.instances().end()}).value());
    QVERIFY(!m::prepare_restored_m12_native_replay_plan(
        replayDocument,forgedLinked,manualChain.chainId,
        *manualGain.value(),gainBindings,true));
    auto forgedSchema=replayDocument;
    forgedSchema.chains.front().modules.front().parameterSchemaId=
        std::string("rgsml.dsp.gain.wrong/999");
    QVERIFY(!m::prepare_restored_m12_native_replay_plan(
        forgedSchema,*productReopen.value(),manualChain.chainId,
        *manualGain.value(),gainBindings,true));
    auto removedReference=replayDocument;
    removedReference.references.items.clear();
    QVERIFY(!m::prepare_restored_m12_native_replay_plan(
        removedReference,*productReopen.value(),manualChain.chainId,
        *manualGain.value(),gainBindings,true));

    // Real 48k PCM through the EXISTING native renderer, no WAV export.
    const auto sampleRate=core::SampleRate::create(48000);
    QVERIFY(sampleRate);
    const auto audioFormat=audio::AudioFormat::create(
        *sampleRate.value(),audio::ChannelLayout::STEREO_LR);
    QVERIFY(audioFormat);
    const auto frameCount=core::FrameCount::create(48000);
    const auto maxBlock=core::FrameCount::create(1024);
    QVERIFY(frameCount&&maxBlock);
    auto input=audio::AudioBuffer::create(
        *audioFormat.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*frameCount.value());
    QVERIFY(input);
    const auto left=input.value()->mutable_view().channel(0);
    const auto right=input.value()->mutable_view().channel(1);
    QVERIFY(left&&right);
    for(std::size_t i=0;i<left.value()->size();++i) {
        const double phase=2.0*std::numbers::pi*440.0*
                           static_cast<double>(i)/48000.0;
        (*left.value())[i]=0.35*std::sin(phase);
        (*right.value())[i]=0.27*std::sin(phase);
    }
    auto empty=dsp::ProcessingChain::create(*reg,{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(empty);
    const auto preparedRequest=render::RenderRequest::create(
        input.value()->view(),input.value()->view().absolute_range(),
        *empty.value(),{},*maxBlock.value());
    QVERIFY(preparedRequest);
    const auto prepared=render::render_preview(
        *preparedRequest.value(),*reg);
    QVERIFY(prepared);
    m::M12NativeStageExecutionWitness firstStageWitness;
    const auto realReplay=m::render_restored_m12_full_native_pcm_once(
        *replayPlan.value(),*prepared.value(),*maxBlock.value(),
        &firstStageWitness);
    QVERIFY(realReplay);
    QCOMPARE(firstStageWitness.protected_manual.size(),std::size_t{1});
    QCOMPARE(firstStageWitness.reference_linked.size(),std::size_t{3});
    QVERIFY(firstStageWitness.user_terminal_p2.empty());
    QVERIFY(!firstStageWitness.is_m12_committed());
    QVERIFY(!firstStageWitness.is_m13_verified());
    QCOMPARE(firstStageWitness.protected_manual.front().instance_id,gainId);
    QCOMPARE(firstStageWitness.reference_linked.front().instance_id,
             replayPlan.value()->reference_linked.instances().front().instance_id());
    QCOMPARE(realReplay.value()->view().frame_count().value(),
             std::int64_t{48000});
    QCOMPARE(realReplay.value()->view().format(),input.value()->view().format());
    // The renderer must reject structurally forged replay tickets as
    // well as wrong Project Open input. No plausible PCM is emitted from
    // an unrelated native binding or manually recontextualized segment.
    auto forgedPlan=*replayPlan.value();
    forgedPlan.reference_bindings.front().instance_id=gainId;
    QVERIFY(!m::render_restored_m12_full_native_pcm_once(
        forgedPlan,*prepared.value(),*maxBlock.value(),
        &firstStageWitness));
    QVERIFY(firstStageWitness.protected_manual.empty());
    QVERIFY(firstStageWitness.reference_linked.empty());
    QVERIFY(firstStageWitness.user_terminal_p2.empty());
    forgedPlan=*replayPlan.value();
    forgedPlan.registry_owner.reset();
    QVERIFY(!m::render_restored_m12_full_native_pcm_once(
        forgedPlan,*prepared.value(),*maxBlock.value()));
    forgedPlan=*replayPlan.value();
    auto wrongSegment=dsp::ProcessingChain::create(*reg,{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::REF_LINKED});
    QVERIFY(wrongSegment);
    forgedPlan.manual_prefix=*wrongSegment.value();
    QVERIFY(!m::render_restored_m12_full_native_pcm_once(
        forgedPlan,*prepared.value(),*maxBlock.value()));
    // Input Gain is mandatory. An empty MANUAL prefix cannot be
    // mistaken for an ordinary accepted plan with no upstream processing.
    auto emptyManualPrefix=dsp::ProcessingChain::create(*reg,{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(emptyManualPrefix);
    forgedPlan=*replayPlan.value();
    forgedPlan.manual_prefix=*emptyManualPrefix.value();
    forgedPlan.manual_bindings.clear();
    QVERIFY(!m::render_restored_m12_full_native_pcm_once(
        forgedPlan,*prepared.value(),*maxBlock.value()));
    // Product's queued PreviewJob is valid only for the exact immutable
    // PREPARED allocation plus the current live MANUAL/registry identity.
    auto preparedForWorker=render::render_preview(
        *preparedRequest.value(),*reg);
    QVERIFY(preparedForWorker);
    const auto ownedPrepared=std::make_shared<const render::RenderResult>(
        std::move(*preparedForWorker.value()));
    const auto ownedReplayPlan=
        std::make_shared<const m::RestoredM12NativeReplayPlan>(
            *replayPlan.value());
    const auto workerTicket=m::prepare_m12_immutable_product_preview_ticket(
        ownedReplayPlan,ownedPrepared,*manualGain.value(),gainBindings);
    QVERIFY(workerTicket);
    QVERIFY(!workerTicket.value()->authorizes_product_installation());
    QVERIFY(!workerTicket.value()->authorizes_m12_receipt());
    QVERIFY(!workerTicket.value()->authorizes_m13_verified());
    rgsml::app::MasteringPreviewController::PreviewJob validJob{
        1U,ownedPrepared,*manualGain.value(),gainBindings,reg,{}};
    const auto ownedWorkerRender=m::render_m12_exact_product_preview_job(
        *workerTicket.value(),validJob,*maxBlock.value());
    QVERIFY(ownedWorkerRender);
    QCOMPARE(ownedWorkerRender.value()->view().frame_count().value(),
             std::int64_t{48000});
    auto staleJob=validJob;
    staleJob.generation=0;
    QVERIFY(!m::render_m12_exact_product_preview_job(
        *workerTicket.value(),staleJob,*maxBlock.value()));
    staleJob=validJob;
    staleJob.registry=std::make_shared<const dsp::ModuleRegistry>(*reg);
    QVERIFY(!m::render_m12_exact_product_preview_job(
        *workerTicket.value(),staleJob,*maxBlock.value()));
    staleJob=validJob;
    const auto alternateParams=dsp::GainParameters::create(-5.0);
    QVERIFY(alternateParams);
    staleJob.bindings[0].parameters=*alternateParams.value();
    QVERIFY(!m::render_m12_exact_product_preview_job(
        *workerTicket.value(),staleJob,*maxBlock.value()));
    staleJob=validJob;
    QVERIFY(staleJob.chain.set_user_bypass(gainId,true));
    QVERIFY(!m::render_m12_exact_product_preview_job(
        *workerTicket.value(),staleJob,*maxBlock.value()));
    auto newPrepared=render::render_preview(
        *preparedRequest.value(),*reg);
    QVERIFY(newPrepared);
    staleJob=validJob;
    staleJob.preparedSnapshot=std::make_shared<const render::RenderResult>(
        std::move(*newPrepared.value()));
    QVERIFY(!m::render_m12_exact_product_preview_job(
        *workerTicket.value(),staleJob,*maxBlock.value()));

    // Even a BYPASSED P2 stays in the actual terminal position after
    // REF_LINKED; the saved Manual value, not an M10 transient override,
    // remains the replay truth.
    auto p2Registration=dsp::ModuleRegistry::with_streaming_true_peak_preview(
        *reg);
    QVERIFY(p2Registration);
    auto optRegistry=std::make_shared<const dsp::ModuleRegistry>(
        std::move(*p2Registration.value()));
    auto manualWithP2=dsp::ProcessingChain::create(*optRegistry,{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(manualWithP2);
    QVERIFY(manualWithP2.value()->add(gainId,"rgsml.dsp.gain",0));
    const auto p2Id=id("bf100000-0000-4000-8000-000000000002");
    QVERIFY(manualWithP2.value()->add(
        p2Id,"rgsml.dsp.streaming-true-peak-limiter",1));
    const auto manualP2Params=dsp::StreamingTruePeakLimiterParameters::create(
        -1.0,0.0,2);
    QVERIFY(manualP2Params);
    const std::vector<dsp::ModuleExecutionBinding> manualP2Bindings{
        {gainId,*gainParams.value()},{p2Id,*manualP2Params.value()}};
    const auto p2Json=dsp::encode_module_parameters_json(
        manualP2Bindings.back().parameters);
    QVERIFY(p2Json);
    const auto p2Opaque=project::OpaqueJsonValue::parse(*p2Json.value());
    QVERIFY(p2Opaque);
    auto p2Doc=replayDocument;
    project::Module persistedP2;
    persistedP2.instanceId=p2Id.uuid();
    persistedP2.typeId="rgsml.dsp.streaming-true-peak-limiter";
    persistedP2.parameters=*p2Opaque.value();
    const auto p2Descriptor=optRegistry->find_descriptor(persistedP2.typeId);
    QVERIFY(p2Descriptor);
    persistedP2.algorithmVersion=p2Descriptor.value()->get().algorithm_version();
    persistedP2.parameterSchemaId=p2Descriptor.value()->get().parameter_schema_id();
    p2Doc.chains.front().modules.push_back(persistedP2);
    p2Doc.chains.front().revision=manualWithP2.value()->revision();
    const auto reopenedP2=m::decode_exact_m12_ref_linked_product_state(
        p2Doc,optRegistry);
    QVERIFY(reopenedP2);
    const auto p2Plan=m::prepare_restored_m12_native_replay_plan(
        p2Doc,*reopenedP2.value(),manualChain.chainId,
        *manualWithP2.value(),manualP2Bindings,true);
    QVERIFY(p2Plan);
    QVERIFY(p2Plan.value()->terminal_p2.has_value());
    QCOMPARE(p2Plan.value()->manual_prefix.instances().size(),
             std::size_t{1});
    QCOMPARE(p2Plan.value()->terminal_p2_bindings.size(),std::size_t{1});
    m::M12NativeStageExecutionWitness completeStageWitness;
    const auto p2RealReplay=m::render_restored_m12_full_native_pcm_once(
        *p2Plan.value(),*prepared.value(),*maxBlock.value(),
        &completeStageWitness);
    QVERIFY(p2RealReplay);
    QCOMPARE(completeStageWitness.protected_manual.size(),std::size_t{1});
    QCOMPARE(completeStageWitness.reference_linked.size(),std::size_t{3});
    QCOMPARE(completeStageWitness.user_terminal_p2.size(),std::size_t{1});
    QCOMPARE(completeStageWitness.user_terminal_p2.front().instance_id,p2Id);
    QVERIFY(!completeStageWitness.is_m12_committed());
    QVERIFY(!completeStageWitness.is_m13_verified());
    QCOMPARE(p2RealReplay.value()->signatures().back().type_id,
             std::string("rgsml.dsp.streaming-true-peak-limiter"));
    QCOMPARE(p2RealReplay.value()->view().frame_count().value(),
             std::int64_t{48000});
    // Independent native oracle: explicitly render each real DSP
    // segment in the frozen order, then compare *every* output sample
    // bit-for-bit. A signature alone cannot prove the true audio.
    const auto manualReq=render::RenderRequest::create(
        prepared.value()->view(),prepared.value()->view().absolute_range(),
        p2Plan.value()->manual_prefix,p2Plan.value()->manual_bindings,
        *maxBlock.value());
    QVERIFY(manualReq);
    const auto manualPCM=render::render_preview(
        *manualReq.value(),*optRegistry);
    QVERIFY(manualPCM);
    const auto refReq=render::RenderRequest::create(
        manualPCM.value()->view(),manualPCM.value()->view().absolute_range(),
        p2Plan.value()->reference_linked,p2Plan.value()->reference_bindings,
        *maxBlock.value());
    QVERIFY(refReq);
    const auto refPCM=render::render_preview(
        *refReq.value(),*optRegistry);
    QVERIFY(refPCM);
    const auto p2Req=render::RenderRequest::create(
        refPCM.value()->view(),refPCM.value()->view().absolute_range(),
        *p2Plan.value()->terminal_p2,p2Plan.value()->terminal_p2_bindings,
        *maxBlock.value());
    QVERIFY(p2Req);
    const auto oraclePCM=render::render_preview(
        *p2Req.value(),*optRegistry);
    QVERIFY(oraclePCM);
    const auto actualView=p2RealReplay.value()->view();
    const auto oracleView=oraclePCM.value()->view();
    QCOMPARE(actualView.absolute_range(),oracleView.absolute_range());
    QCOMPARE(actualView.format(),oracleView.format());
    for(std::size_t ch=0;ch<actualView.format().channel_count();++ch) {
        const auto actualSamples=actualView.channel(ch);
        const auto oracleSamples=oracleView.channel(ch);
        QVERIFY(actualSamples&&oracleSamples);
        QCOMPARE(actualSamples.value()->size(),oracleSamples.value()->size());
        for(std::size_t frame=0;frame<actualSamples.value()->size();++frame)
            QCOMPARE(std::bit_cast<std::uint64_t>(
                         (*actualSamples.value())[frame]),
                     std::bit_cast<std::uint64_t>(
                         (*oracleSamples.value())[frame]));
    }
    // A user-bypassed terminal P2 is still structurally AFTER the
    // linked artist DSP. The bypass must not move the node, alter
    // its saved Ceiling/Drive or reinstate limiter processing.
    auto manualBypassed=*manualWithP2.value();
    QVERIFY(manualBypassed.set_user_bypass(p2Id,true));
    auto bypassedDocument=p2Doc;
    bypassedDocument.chains.front().modules.back().userBypass=true;
    bypassedDocument.chains.front().revision=manualBypassed.revision();
    const auto restoredBypass=m::decode_exact_m12_ref_linked_product_state(
        bypassedDocument,optRegistry);
    QVERIFY(restoredBypass);
    const auto bypassPlan=m::prepare_restored_m12_native_replay_plan(
        bypassedDocument,*restoredBypass.value(),manualChain.chainId,
        manualBypassed,manualP2Bindings,true);
    QVERIFY(bypassPlan);
    QVERIFY(bypassPlan.value()->terminal_p2);
    QVERIFY(!bypassPlan.value()->terminal_p2->instances().front().active());
    const auto bypassSound=m::render_restored_m12_full_native_pcm_once(
        *bypassPlan.value(),*prepared.value(),*maxBlock.value());
    QVERIFY(bypassSound);
    const auto bypassView=bypassSound.value()->view();
    const auto withoutP2=realReplay.value()->view();
    QCOMPARE(bypassView.absolute_range(),withoutP2.absolute_range());
    for(std::size_t ch=0;ch<bypassView.format().channel_count();++ch) {
        const auto bypassSamples=bypassView.channel(ch);
        const auto plainSamples=withoutP2.channel(ch);
        QVERIFY(bypassSamples&&plainSamples);
        QCOMPARE(bypassSamples.value()->size(),plainSamples.value()->size());
        for(std::size_t frame=0;frame<bypassSamples.value()->size();++frame)
            QCOMPARE(std::bit_cast<std::uint64_t>(
                         (*bypassSamples.value())[frame]),
                     std::bit_cast<std::uint64_t>(
                         (*plainSamples.value())[frame]));
    }

    // P2 parameters are part of the same immutable MANUAL worker key:
    // a later dial adjustment invalidates the old linked replay ticket.
    const auto p2SharedPlan=
        std::make_shared<const m::RestoredM12NativeReplayPlan>(
            *p2Plan.value());
    const auto p2Ticket=m::prepare_m12_immutable_product_preview_ticket(
        p2SharedPlan,ownedPrepared,*manualWithP2.value(),manualP2Bindings);
    QVERIFY(p2Ticket);
    rgsml::app::MasteringPreviewController::PreviewJob p2Job{
        2U,ownedPrepared,*manualWithP2.value(),manualP2Bindings,optRegistry,{}};
    QVERIFY(m::render_m12_exact_product_preview_job(
        *p2Ticket.value(),p2Job,*maxBlock.value()));
    const auto alternateP2=dsp::StreamingTruePeakLimiterParameters::create(
        -3.0,4.0,2);
    QVERIFY(alternateP2);
    p2Job.bindings.back().parameters=*alternateP2.value();
    QVERIFY(!m::render_m12_exact_product_preview_job(
        *p2Ticket.value(),p2Job,*maxBlock.value()));
    // The opt-in runtime factory must still exist when reopened:
    // do not silently drop a required calculated module.
    // Project decoding must refuse foreign-owned nodes masquerading
    // as MANUAL and any extra REF_LINKED segment in another stage.
    auto contaminatedManual=proposed;
    auto fakeManualNode=carrier.value()->native_project_chain.modules.front();
    fakeManualNode.instanceId=uuid(
        "bd000000-0000-4000-8000-000000000001");
    contaminatedManual.chains.front().modules.push_back(fakeManualNode);
    QVERIFY(!m::decode_exact_m12_ref_linked_product_state(
        contaminatedManual,reg));
    auto foreignLinkedStage=proposed;
    project::Chain foreignStage;
    foreignStage.chainId=uuid(
        "be000000-0000-4000-8000-000000000001");
    foreignStage.stage="RESTORE";
    foreignStage.segment="REF_LINKED";
    foreignLinkedStage.chains.push_back(foreignStage);
    QVERIFY(!m::decode_exact_m12_ref_linked_product_state(
        foreignLinkedStage,reg));
    const auto minimal=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(minimal);
    const auto noOptin=std::make_shared<const dsp::ModuleRegistry>(
        std::move(*minimal.value()));
    QVERIFY(!m::decode_exact_m12_ref_linked_product_state(
        proposed,noOptin));
    QVERIFY(m::exact_project_chain_equal(
        proposed.chains.front(),manualChain));
    QCOMPARE(proposed.chains.back().segment,std::string("REF_LINKED"));
    QCOMPARE(proposed.pipeline.masteringChainId,
             doc.pipeline.masteringChainId);
    QCOMPARE(currentBefore.value()->document().chains.size(),
             std::size_t{1});
    QVERIFY(proposed.referenceMatch.canonical_utf8().find(
        "\"verificationState\":\"NOT_VERIFIED\"")!=std::string::npos);
    // A stale manual revision or changed Reference must refuse M12.
    // An independently chosen WAV ceiling has no bearing on this gate.
    auto stale=identity;
    ++stale.exact_protected_manual_snapshot.revision;
    QVERIFY(!m::prepare_exact_m12_project_transaction(
        *currentBefore.value(),*carrier.value(),*stage.value(),
        stale));
    stale=identity;
    stale.exact_live_reference_id=uuid(
        "b7000000-0000-4000-8000-000000000004");
    QVERIFY(!m::prepare_exact_m12_project_transaction(
        *currentBefore.value(),*carrier.value(),*stage.value(),
        stale));
    // Exact same-controller update may replace only its existing
    // segment; its immutable native manual/terminal scope survives.
    auto update=identity;
    update.exact_existing_linked_snapshot=proposed.chains.back();
    const auto updated=m::prepare_exact_m12_project_transaction(
        transaction.value()->candidate_not_yet_committed,
        *carrier.value(),*stage.value(),update);
    QVERIFY(updated);
    QCOMPARE(updated.value()->candidate_not_yet_committed.document().
             chains.size(),std::size_t{2});
    // Amount=0 preserves the actual LINKED node, the unrelated MANUAL
    // chain, and all user/domains/bypass flags independently.
    const auto zero=m::prepare_m12_owned_controller_suspension(
        transaction.value()->candidate_not_yet_committed,
        carrier.value()->exact_owner_controller,
        carrier.value()->native_project_chain.chainId,
        m::ControllerSuspensionCause::amount_zero,true);
    QVERIFY(zero);
    QVERIFY(!zero.value()->authorizes_persisted_commit());
    const auto& suspended=
        zero.value()->updated_project_not_committed.document();
    QCOMPARE(suspended.chains.size(),std::size_t{2});
    QVERIFY(m::exact_project_chain_equal(
        suspended.chains.front(),manualChain));
    QCOMPARE(suspended.chains.back().modules.size(),std::size_t{3});
    for(std::size_t i=0;i<3U;++i) {
        const auto& original=proposed.chains.back().modules[i];
        const auto& now=suspended.chains.back().modules[i];
        QVERIFY(now.controllerSuspended);
        QCOMPARE(now.userBypass,original.userBypass);
        QCOMPARE(now.enabled,original.enabled);
        QCOMPARE(now.domainSuspended,original.domainSuspended);
        QCOMPARE(now.semanticNodeId,original.semanticNodeId);
        QCOMPARE(now.parameters.canonical_utf8(),
                 original.parameters.canonical_utf8());
    }
    const auto restored=m::prepare_m12_owned_controller_suspension(
        zero.value()->updated_project_not_committed,
        carrier.value()->exact_owner_controller,
        carrier.value()->native_project_chain.chainId,
        m::ControllerSuspensionCause::current_amount_restored,true);
    QVERIFY(restored);
    for(const auto& module:restored.value()->
            updated_project_not_committed.document().chains.back().modules)
        QVERIFY(!module.controllerSuspended);
    const auto stale_suspension=m::prepare_m12_owned_controller_suspension(
        zero.value()->updated_project_not_committed,
        carrier.value()->exact_owner_controller,
        carrier.value()->native_project_chain.chainId,
        m::ControllerSuspensionCause::plan_stale,true);
    QVERIFY(stale_suspension);
    QVERIFY(stale_suspension.value()->updated_project_not_committed.document().
        referenceMatch.canonical_utf8().find(
          "\"freshnessState\":\"STALE\"")!=std::string::npos);
    QVERIFY(!m::prepare_m12_owned_controller_suspension(
        stale_suspension.value()->updated_project_not_committed,
        carrier.value()->exact_owner_controller,
        carrier.value()->native_project_chain.chainId,
        m::ControllerSuspensionCause::current_amount_restored,true));
    QVERIFY(!m::prepare_m12_owned_controller_suspension(
        transaction.value()->candidate_not_yet_committed,
        uuid("bf000000-0000-4000-8000-000000000001"),
        carrier.value()->native_project_chain.chainId,
        m::ControllerSuspensionCause::amount_zero,true));
    // A caller may rewrite its own expected Project snapshot, but
    // cannot rewrite the M11 frozen typed manual DSP inside the
    // accepted M10 plan. The source truth is the accepted stage.
    auto laundered=currentBefore.value()->document();
    project::Module illicit_gain;
    illicit_gain.instanceId=uuid(
        "bb000000-0000-4000-8000-000000000001");
    illicit_gain.typeId="rgsml.dsp.gain";
    illicit_gain.parameters=*project::OpaqueJsonValue::parse(
        "{\"gainDb\":0.0}").value();
    laundered.chains.front().modules.push_back(illicit_gain);
    auto illicit_snapshot=project::ProjectSnapshot::create(
        laundered);
    QVERIFY(illicit_snapshot);
    auto forged_expected=identity;
    forged_expected.exact_protected_manual_snapshot=
        laundered.chains.front();
    QVERIFY(!m::prepare_exact_m12_project_transaction(
        *illicit_snapshot.value(),*carrier.value(),*stage.value(),
        forged_expected));
    auto corrupted=proposed;
    corrupted.chains.front().revision+=1;
    const auto changed=project::ProjectSnapshot::create(corrupted);
    QVERIFY(changed);
    QVERIFY(!m::prepare_exact_m12_project_transaction(
        *changed.value(),*carrier.value(),*stage.value(),
        identity));
    doc.chains.push_back(carrier.value()->native_project_chain);
    const auto snapshot=project::ProjectSnapshot::create(doc);
    QVERIFY(snapshot);
    auto encoded=project::internal::write_project_json(
        snapshot.value()->document());
    QVERIFY(encoded);
    auto decoded=project::internal::parse_project_json(*encoded.value());
    QVERIFY(decoded);
    QCOMPARE(decoded.value()->chains.size(),std::size_t{2});
    QCOMPARE(decoded.value()->pipeline.masteringChainId,
             doc.pipeline.masteringChainId);
    const auto& old=decoded.value()->chains[0];
    QCOMPARE(old.segment,std::string("MANUAL"));
    QVERIFY(old.modules.empty());
    const auto& linked=decoded.value()->chains[1];
    QCOMPARE(linked.segment,std::string("REF_LINKED"));
    QCOMPARE(linked.modules.size(),std::size_t{3});
    for(std::size_t i=0;i<3U;++i)
        QCOMPARE(linked.modules[i].parameters.canonical_utf8(),
                 carrier.value()->native_project_chain.modules[i].
                    parameters.canonical_utf8());
    auto again=project::internal::write_project_json(*decoded.value());
    QVERIFY(again);
    QCOMPARE(*again.value(),*encoded.value());
    QVERIFY(!stage.value()->authorizes_project_mutation());
}
void M12NativeProjectCarrierTest::
optimizedManualP2CannotBeSilentlyPersistedAsDifferentSound() {
    auto base=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(base);
    auto opted=dsp::ModuleRegistry::with_streaming_true_peak_preview(
        *base.value());
    QVERIFY(opted);
    auto registry=std::make_shared<const dsp::ModuleRegistry>(
        std::move(*opted.value()));
    auto manual=dsp::ProcessingChain::create(*registry,{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(manual);
    const auto p2Id=id("c1000000-0000-4000-8000-000000000001");
    QVERIFY(manual.value()->add(
        p2Id,"rgsml.dsp.streaming-true-peak-limiter",0));
    const auto manualP2=dsp::StreamingTruePeakLimiterParameters::create(
        -1.0,0.0,2);
    const auto optimizedP2=dsp::StreamingTruePeakLimiterParameters::create(
        -3.0,6.0,2);
    auto eq=dsp::ParametricEqParameters::create_legacy_default();
    QVERIFY(manualP2&&optimizedP2&&eq);
    const std::vector<dsp::ModuleExecutionBinding> manualBindings{{
        p2Id,*manualP2.value()}};
    const auto originalAccepted=accepted();
    const std::vector<m::ExactLinkedNode> artistic{{
        id("c1000000-0000-4000-8000-000000000002"),
        uuid("c2000000-0000-4000-8000-000000000001"),
        "rgsml.dsp.parametric-eq",*eq.value()
    }};
    const auto staged=m::stage_exact_m12_native_ref_linked(
        originalAccepted,*manual.value(),manualBindings,
        registry,artistic);
    QVERIFY(staged);
    const m::StagedExactP2Winner changed{
        p2Id,*optimizedP2.value(),
        originalAccepted.plan_id,
        originalAccepted.exact_m10_result_and_topology_checksum,
        originalAccepted.exact_source_reference_amount_scope_revision,
        "real-m10-p2-winner-ticket",manual.value()->revision(),
        {manual.value()->instances().begin(),
         manual.value()->instances().end()},manualBindings};
    QVERIFY(changed.matches(*staged.value(),*manual.value(),
                            manualBindings));
    const auto json=dsp::encode_module_parameters_json(
        manualBindings.back().parameters);
    QVERIFY(json);
    const auto canonical=project::OpaqueJsonValue::parse(*json.value());
    QVERIFY(canonical);
    project::Module persistedP2;
    persistedP2.instanceId=p2Id.uuid();
    persistedP2.typeId="rgsml.dsp.streaming-true-peak-limiter";
    persistedP2.provenance="MANUAL";
    persistedP2.owner="USER";
    persistedP2.linkState="UNLINKED";
    persistedP2.enabled=true;
    persistedP2.parameters=*canonical.value();
    project::Chain persisted;
    persisted.chainId=uuid("c3000000-0000-4000-8000-000000000001");
    persisted.stage="MASTER";
    persisted.segment="MANUAL";
    persisted.revision=manual.value()->revision();
    persisted.modules.push_back(persistedP2);
    auto audit=m::audit_exact_m12_p2_persisted_sound(
        *staged.value(),changed,*manual.value(),manualBindings,
        persisted,true,true);
    QCOMPARE(audit.reason,m::P2PersistedSoundReason::
        manual_p2_sound_conflict_requires_user_decision);
    QVERIFY(!audit.keeps_exact_frozen_sound());
    QVERIFY(!audit.authorizes_manual_p2_mutation());
    QVERIFY(!audit.authorizes_project_commit());

    auto fixed=changed;
    fixed.winning_parameters=*manualP2.value();
    QCOMPARE(m::audit_exact_m12_p2_persisted_sound(
        *staged.value(),fixed,*manual.value(),manualBindings,
        persisted,true,true).reason,
        m::P2PersistedSoundReason::keep_fixed_manual_p2);
    QCOMPARE(m::audit_exact_m12_p2_persisted_sound(
        *staged.value(),fixed,*manual.value(),manualBindings,
        persisted,true,false).reason,
        m::P2PersistedSoundReason::
            unverified_p2_terminal_after_linked_execution_order);
    // A forged on-disk value cannot validate against the live DSP.
    const auto changedJson=dsp::encode_module_parameters_json(
        dsp::ModuleParameterPayload{*optimizedP2.value()});
    QVERIFY(changedJson);
    const auto changedPayload=
        project::OpaqueJsonValue::parse(*changedJson.value());
    QVERIFY(changedPayload);
    persisted.modules.back().parameters=*changedPayload.value();
    QCOMPARE(m::audit_exact_m12_p2_persisted_sound(
        *staged.value(),fixed,*manual.value(),manualBindings,
        persisted,true,true).reason,
        m::P2PersistedSoundReason::
            persisted_manual_p2_conflicts_with_live_product);
    persisted.modules.back().parameters=*canonical.value();
    QCOMPARE(m::audit_exact_m12_p2_persisted_sound(
        *staged.value(),fixed,*manual.value(),manualBindings,
        persisted,false,true).reason,
        m::P2PersistedSoundReason::missing_or_stale_m10_m12_winner);

    // A genuinely serializable Product Project receives ordinary
    // MANUAL P2 + ordinary REF_LINKED EQ. An optimized P2 which
    // changes protected sound is refused BEFORE draft materialization.
    // An unchanged P2 can be carried through .rgsml byte-identically.
    const auto descriptor=registry->find_descriptor(
        "rgsml.dsp.streaming-true-peak-limiter");
    QVERIFY(descriptor);
    QVERIFY(descriptor.value()->get().algorithm_version());
    QVERIFY(descriptor.value()->get().parameter_schema_id());
    persisted.modules.back().algorithmVersion=std::string{
        *descriptor.value()->get().algorithm_version()};
    persisted.modules.back().parameterSchemaId=std::string{
        *descriptor.value()->get().parameter_schema_id()};
    project::ProjectDocument doc;
    doc.projectId=uuid("c4000000-0000-4000-8000-000000000001");
    doc.displayName="M12 P2 preserved ordinary DSP state";
    doc.sourceResourceId=uuid("c5000000-0000-4000-8000-000000000001");
    const auto referenceResource=uuid(
        "c5000000-0000-4000-8000-000000000002");
    const auto referenceId=uuid(
        "c5000000-0000-4000-8000-000000000003");
    doc.resources.push_back({
        doc.sourceResourceId,"AUDIO",
        {{"rgsml.windows.local-file","C:/p2-source.wav","source.wav"}},
        std::nullopt});
    doc.resources.push_back({
        referenceResource,"AUDIO",
        {{"rgsml.windows.local-file","C:/p2-reference.wav","ref.wav"}},
        std::nullopt});
    doc.references.activeReferenceId=referenceId;
    doc.references.items.push_back({
        referenceId,"GOLD",referenceResource});
    doc.pipeline.masteringChainId=persisted.chainId;
    doc.chains.push_back(persisted);
    const auto prior=project::ProjectSnapshot::create(doc);
    QVERIFY(prior);
    const auto carrier=m::encode_exact_ref_linked_project_carrier(
        *staged.value(),
        uuid("c6000000-0000-4000-8000-000000000001"));
    QVERIFY(carrier);
    m::M12ProjectTransactionExpected expected{
        doc.sourceResourceId,referenceId,persisted.chainId,
        persisted,std::nullopt,true,true};
    const auto changedDraft=
        m::prepare_exact_m12_project_transaction_with_p2_sound_guard(
            *prior.value(),*carrier.value(),*staged.value(),
            expected,changed,*manual.value(),manualBindings,true);
    QVERIFY(!changedDraft);
    QCOMPARE(prior.value()->document().chains.size(),std::size_t{1});
    const auto keepFixedDraft=
        m::prepare_exact_m12_project_transaction_with_p2_sound_guard(
            *prior.value(),*carrier.value(),*staged.value(),
            expected,fixed,*manual.value(),manualBindings,true);
    QVERIFY(keepFixedDraft);
    QVERIFY(!keepFixedDraft.value()->authorizes_project_commit());
    QVERIFY(!keepFixedDraft.value()->is_materialization_receipt());
    QCOMPARE(keepFixedDraft.value()->
             candidate_not_yet_committed.document().chains.size(),
             std::size_t{2});
    const auto& newManual=keepFixedDraft.value()->
        candidate_not_yet_committed.document().chains.front();
    QVERIFY(m::exact_project_chain_equal(newManual,persisted));
    const auto jsonAfter=project::internal::write_project_json(
        keepFixedDraft.value()->candidate_not_yet_committed.document());
    QVERIFY(jsonAfter);
    const auto reopened=project::internal::parse_project_json(
        *jsonAfter.value());
    QVERIFY(reopened);
    QCOMPARE(reopened.value()->chains.size(),std::size_t{2});
    QVERIFY(m::exact_project_chain_equal(
        reopened.value()->chains.front(),persisted));
    QCOMPARE(reopened.value()->chains.front().modules.back().
             parameters.canonical_utf8(),
             canonical.value()->canonical_utf8());
    // The persisted REF_LINKED chain is decoded into the SAME native
    // ModuleRegistry/ProcessingChain/typed bindings as M12 stage.
    // This is a read-only decoder; ProductSession must still adopt it
    // atomically with MANUAL to make a reopened project audible.
    const auto restored=m::decode_exact_m12_ref_linked_product_state(
        *reopened.value(),registry);
    QVERIFY(restored);
    QCOMPARE(restored.value()->plan_id,originalAccepted.plan_id);
    QCOMPARE(restored.value()->controller_id,
             originalAccepted.reference_match_controller_id);
    QCOMPARE(restored.value()->exact_plan_checksum,
             originalAccepted.exact_plan_content_checksum);
    QCOMPARE(restored.value()->exact_native_chain.instances().size(),
             staged.value()->chain.instances().size());
    QVERIFY(std::equal(
        restored.value()->exact_native_chain.instances().begin(),
        restored.value()->exact_native_chain.instances().end(),
        staged.value()->chain.instances().begin()));
    QCOMPARE(restored.value()->exact_native_bindings,
             staged.value()->exact_bindings);
    QVERIFY(!restored.value()->authorizes_project_adoption());
    QVERIFY(!restored.value()->is_m12_materialization_receipt());
    QVERIFY(!restored.value()->authorizes_m13_verification());
    auto wrongOwner=*reopened.value();
    wrongOwner.chains.back().modules.front().owner="USER";
    QVERIFY(!m::decode_exact_m12_ref_linked_product_state(
        wrongOwner,registry));
    auto wrongSchema=*reopened.value();
    wrongSchema.chains.back().modules.front().parameterSchemaId=
        "rgsml.invalid.schema/1.0.0";
    QVERIFY(!m::decode_exact_m12_ref_linked_product_state(
        wrongSchema,registry));
    auto noMaterialization=*reopened.value();
    noMaterialization.referenceMatch=
        project::OpaqueJsonValue::empty_object();
    QVERIFY(!m::decode_exact_m12_ref_linked_product_state(
        noMaterialization,registry));
    // Readback without a REAL Product writer lease is NOT a receipt,
    // even when its bytes happen to equal the prepared draft.
    const auto parsedReadback=project::ProjectSnapshot::create(
        *reopened.value());
    QVERIFY(parsedReadback);
    const auto noRealCommit=m::audit_exact_m12_postcommit_project_readback(
        *prior.value(),*keepFixedDraft.value(),*parsedReadback.value(),
        false);
    QCOMPARE(noRealCommit.reason,m::PostCommitReadbackReason::
        missing_real_owner_transaction);
    QVERIFY(!noRealCommit.is_product_materialization_receipt());
    QVERIFY(!noRealCommit.authorizes_m13_verification());
    // This test simulates the readback AFTER a hypothetical real
    // atomic Qt owner commit, without pretending to perform one.
    const auto exactReadback=m::audit_exact_m12_postcommit_project_readback(
        *prior.value(),*keepFixedDraft.value(),*parsedReadback.value(),
        true);
    QCOMPARE(exactReadback.reason,m::PostCommitReadbackReason::
        exact_after_state);
    QVERIFY(exactReadback.exact_after_state_observed());
    QVERIFY(!exactReadback.is_product_materialization_receipt());
    auto corruptedAfter=*reopened.value();
    corruptedAfter.chains.front().modules.back().parameters=
        *changedPayload.value();
    const auto corruptedAfterSnapshot=
        project::ProjectSnapshot::create(corruptedAfter);
    QVERIFY(corruptedAfterSnapshot);
    QCOMPARE(m::audit_exact_m12_postcommit_project_readback(
        *prior.value(),*keepFixedDraft.value(),
        *corruptedAfterSnapshot.value(),true).reason,
        m::PostCommitReadbackReason::
            incomplete_or_noncanonical_readback);
    const std::vector<project::OptionalEntry> unrelatedFiles{{
        "artifacts/unrelated/file.txt","text/plain",{
            std::byte{0x61},std::byte{0x62}}}};
    const auto alteredAttachment=project::ProjectSnapshot::create(
        *reopened.value(),unrelatedFiles);
    QVERIFY(alteredAttachment);
    QCOMPARE(m::audit_exact_m12_postcommit_project_readback(
        *prior.value(),*keepFixedDraft.value(),
        *alteredAttachment.value(),true).reason,
        m::PostCommitReadbackReason::
            unrelated_optional_entries_changed);
}
void M12NativeProjectCarrierTest::
userManualChainAndSingleModulePresetsAreStrictlyTyped() {
    namespace p=rgsml::app::dsp_presets;
    auto baseline=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(baseline);
    auto opted=dsp::ModuleRegistry::with_streaming_true_peak_preview(
        std::move(*baseline.value()));
    QVERIFY(opted);
    const auto registry=std::make_shared<const dsp::ModuleRegistry>(
        std::move(*opted.value()));
    auto manual=dsp::ProcessingChain::create(*registry,{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(manual);
    const auto gain=id("cb000000-0000-4000-8000-000000000001");
    const auto p2=id("cb000000-0000-4000-8000-000000000002");
    QVERIFY(manual.value()->add(gain,"rgsml.dsp.gain",0));
    QVERIFY(manual.value()->add(
        p2,"rgsml.dsp.streaming-true-peak-limiter",1));
    QVERIFY(manual.value()->set_user_bypass(gain,true));
    auto gainParams=dsp::GainParameters::create(-4.0);
    auto p2Params=dsp::StreamingTruePeakLimiterParameters::create(
        -2.0,4.0,2);
    QVERIFY(gainParams&&p2Params);
    const std::vector<dsp::ModuleExecutionBinding> bindings{{
        gain,*gainParams.value()},{p2,*p2Params.value()}};
    const auto all=p::capture(
        *manual.value(),bindings,*registry);
    QVERIFY(all);
    QVERIFY(!all.value()->authorizes_project_commit());
    QVERIFY(!all.value()->authorizes_automatch_overwrite());
    QCOMPARE(all.value()->modules.size(),std::size_t{2});
    QVERIFY(all.value()->modules.front().user_bypass);
    const auto bytes=p::encode_json(*all.value());
    QVERIFY(bytes);
    const auto committedBefore=*manual.value();
    const auto draft=p::prepare_explicit_manual_preset_restore(
        *all.value(),*manual.value(),bindings,*registry);
    QVERIFY(draft);
    QCOMPARE(draft.value()->proposed_manual_chain.revision(),
             manual.value()->revision()+1U);
    QCOMPARE(draft.value()->proposed_bindings,bindings);
    QVERIFY(!draft.value()->authorizes_project_commit());
    QVERIFY(!draft.value()->authorizes_automatch_takeover());
    QCOMPARE(manual.value()->revision(),committedBefore.revision());
    QCOMPARE(manual.value()->instances().size(),
             committedBefore.instances().size());
    const auto reopened=p::decode_json(*bytes.value(),*registry);
    QVERIFY(reopened);
    QVERIFY(p::matches_exact_manual_topology(
        *reopened.value(),*manual.value()));
    QCOMPARE(reopened.value()->modules[0].parameters,bindings[0].parameters);
    QCOMPARE(reopened.value()->modules[1].parameters,bindings[1].parameters);
    QVERIFY(!p::matches_exact_manual_topology(
        *reopened.value(),*manual.value(),std::size_t{1}));
    const auto single=p::capture(
        *manual.value(),bindings,*registry,std::size_t{1});
    QVERIFY(single);
    const auto oneJson=p::encode_json(*single.value());
    QVERIFY(oneJson);
    const auto oneReopen=p::decode_json(*oneJson.value(),*registry);
    QVERIFY(oneReopen);
    QCOMPARE(oneReopen.value()->modules.size(),std::size_t{1});
    // A single-DSP preset is applied to the CURRENT typed instance,
    // without copying an old instanceId or touching the other modules.
    auto differentP2=*oneReopen.value();
    auto newP2=dsp::StreamingTruePeakLimiterParameters::create(
        -5.0,8.0,2);
    QVERIFY(newP2);
    differentP2.modules[0].parameters=*newP2.value();
    differentP2.modules[0].user_bypass=true;
    const auto proposedSingle=p::prepare_explicit_manual_preset_restore(
        differentP2,*manual.value(),bindings,*registry,std::size_t{1});
    QVERIFY(proposedSingle);
    QCOMPARE(proposedSingle.value()->proposed_bindings[0].parameters,
             bindings[0].parameters);
    QCOMPARE(proposedSingle.value()->proposed_bindings[1].parameters,
             dsp::ModuleParameterPayload{*newP2.value()});
    QVERIFY(proposedSingle.value()->proposed_manual_chain.
        instances()[1].user_bypass());
    QVERIFY(!manual.value()->instances()[1].user_bypass());
    QVERIFY(p::matches_exact_manual_topology(
        *oneReopen.value(),*manual.value(),std::size_t{1}));
    QVERIFY(!p::matches_exact_manual_topology(
        *oneReopen.value(),*manual.value(),std::size_t{0}));
    // A preset cannot install an unregistered P2 on a normal Product.
    auto noOptin=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(noOptin);
    QVERIFY(!p::decode_json(*bytes.value(),*noOptin.value()));
    // Schema, kind and parameter payload must never be coerced.
    auto wrongSchema=*bytes.value();
    const auto found=wrongSchema.find("rgsml.dsp.manual-preset/1.0.0");
    QVERIFY(found!=std::string::npos);
    wrongSchema.replace(found,
        std::string("rgsml.dsp.manual-preset/1.0.0").size(),
        "rgsml.dsp.manual-preset/999.0.0");
    QVERIFY(!p::decode_json(wrongSchema,*registry));
    auto incompatible=dsp::ProcessingChain::create(*registry,{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(incompatible);
    QVERIFY(incompatible.value()->add(gain,"rgsml.dsp.gain",0));
    QVERIFY(incompatible.value()->add(
        id("cb000000-0000-4000-8000-000000000003"),
        "rgsml.dsp.parametric-eq",1));
    QVERIFY(!p::matches_exact_manual_topology(
        *reopened.value(),*incompatible.value()));
    QVERIFY(!p::prepare_explicit_manual_preset_restore(
        *reopened.value(),*incompatible.value(),bindings,*registry));
    auto invalidScope=*oneReopen.value();
    invalidScope.modules[0].parameter_schema_id=
        "rgsml.dsp.invalid.parameters/0";
    QVERIFY(!p::prepare_explicit_manual_preset_restore(
        invalidScope,*manual.value(),bindings,*registry,
        std::size_t{1}));
}
void M12NativeProjectCarrierTest::
wrongProjectChainOrUnavailableFactoryCannotBePublished(){
    auto base=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(base);
    auto reg=std::make_shared<const dsp::ModuleRegistry>(
        std::move(*base.value()));
    auto manual=dsp::ProcessingChain::create(*reg,{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(manual);
    auto ts=dsp::TransientShaperParameters::create_default();
    QVERIFY(ts);
    const std::vector<dsp::ModuleExecutionBinding> none;
    const m::ExactLinkedNode node{
        id("b9000000-0000-4000-8000-000000000001"),
        uuid("ba000000-0000-4000-8000-000000000001"),
        "rgsml.dsp.transient-shaper",*ts.value()};
    // Type descriptor exists but the actual Product runtime factory
    // was NOT opted in: never silently drop this required plan node.
    QVERIFY(!m::stage_exact_m12_native_ref_linked(
        accepted(),*manual.value(),none,reg,{node}));
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M12NativeProjectCarrierTest)
#include "test_l3_m12_ref_linked_project_carrier.moc"
