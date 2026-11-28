#include "internal/l3_m12_ref_linked_project_carrier.hpp"
#include "internal/l3_m12_project_transaction_draft.hpp"
#include "internal/l3_m12_p2_persisted_sound_guard.hpp"
#include "internal/l3_m12_exact_postcommit_readback.hpp"
#include "internal/l3_m12_ref_linked_project_restore.hpp"
#include "internal/l3_m12_ref_linked_native_replay_plan.hpp"
#include "internal/l3_m12_restored_native_render.hpp"
#include "internal/l3_m12_product_preview_worker_ticket.hpp"
#include "internal/l3_manual_match_transfer_project.hpp"
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
#include <rgsml/project/project_repository.hpp>
#include <rgsml/core/resource_reference.hpp>
#include <rgsml/core/resource_io.hpp>
#include <span>
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
// Real RGSML ZIP producer/consumer with an entirely in-memory resource;
// never use a made-up text serializer as evidence of durable M12 sound.
struct M12ZipStorage final {std::vector<std::byte> bytes;};
class M12ZipEndpoint final:public core::IResourceReader,
                           public core::IResourceWriter {
public:
    explicit M12ZipEndpoint(std::shared_ptr<M12ZipStorage> storage)
        :storage_(std::move(storage)),
         ref_(*core::ResourceReference::create(
             "test","m12-zip",true,true).value()) {}
    [[nodiscard]] const core::ResourceReference& reference()
        const noexcept override{return ref_;}
    [[nodiscard]] core::ResourceCapabilities capabilities()
        const noexcept override {
        return core::ResourceCapabilities::create(true,true,true,true);
    }
    [[nodiscard]] core::Result<std::uint64_t> size_bytes() const override {
        return core::Result<std::uint64_t>::success(storage_->bytes.size());
    }
    [[nodiscard]] core::Result<std::uint64_t> position_bytes() const override {
        return core::Result<std::uint64_t>::success(position_);
    }
    [[nodiscard]] core::Result<std::size_t> read(
        std::span<std::byte> dst) override {
        if(closed_||position_>storage_->bytes.size())
            return core::Result<std::size_t>::failure(
                core::Error{core::ErrorCode::InvalidState});
        const auto available=storage_->bytes.size()-position_;
        const auto count=std::min(dst.size(),available);
        std::copy_n(storage_->bytes.data()+position_,count,dst.data());
        position_+=count;
        return core::Result<std::size_t>::success(count);
    }
    [[nodiscard]] core::Result<std::size_t> write(
        std::span<const std::byte> src) override {
        if(closed_)return core::Result<std::size_t>::failure(
            core::Error{core::ErrorCode::InvalidState});
        if(position_+src.size()>storage_->bytes.size())
            storage_->bytes.resize(position_+src.size());
        std::copy(src.begin(),src.end(),
                  storage_->bytes.begin()+static_cast<std::ptrdiff_t>(position_));
        position_+=src.size();
        return core::Result<std::size_t>::success(src.size());
    }
    [[nodiscard]] core::Status seek_bytes(std::uint64_t offset) override {
        if(closed_||offset>storage_->bytes.size())
            return core::Status::failure(
                core::Error{core::ErrorCode::OutOfRange});
        position_=static_cast<std::size_t>(offset);
        return core::Status::success();
    }
    [[nodiscard]] core::Status resize_bytes(std::uint64_t size) override {
        if(closed_)return core::Status::failure(
            core::Error{core::ErrorCode::InvalidState});
        storage_->bytes.resize(static_cast<std::size_t>(size));
        position_=std::min(position_,storage_->bytes.size());
        return core::Status::success();
    }
    [[nodiscard]] core::Status flush() override {
        return core::Status::success();
    }
    [[nodiscard]] core::Status close() override {
        closed_=true;
        return core::Status::success();
    }
private:
    std::shared_ptr<M12ZipStorage> storage_;
    core::ResourceReference ref_;
    std::size_t position_{};
    bool closed_{};
};
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
    void ownedP2AndGainRoundtripUpdateAndSuspend();
};
void M12NativeProjectCarrierTest::ownedP2AndGainRoundtripUpdateAndSuspend() {
    auto base=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(base);
    auto expanded=dsp::ModuleRegistry::with_streaming_true_peak_automatch(*base.value());
    QVERIFY(expanded);
    auto registry=std::make_shared<const dsp::ModuleRegistry>(std::move(*expanded.value()));
    auto manual=dsp::ProcessingChain::create(*registry,
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(manual);
    const auto gain=dsp::GainParameters::create(1.0);
    const auto p2=dsp::StreamingTruePeakLimiterParameters::create(-2.0,3.0,5);
    QVERIFY(gain&&p2);
    std::vector<m::ExactLinkedNode> nodes{
        {id("b3000000-0000-4000-8000-000000000081"),
         uuid("b4000000-0000-4000-8000-000000000081"),"rgsml.dsp.gain",*gain.value()},
        {id("b3000000-0000-4000-8000-000000000082"),
         uuid("b4000000-0000-4000-8000-000000000082"),"rgsml.dsp.streaming-true-peak-limiter",*p2.value()}};
    const std::vector<dsp::ModuleExecutionBinding> none;
    auto stage=m::stage_exact_m12_native_ref_linked(accepted(),*manual.value(),none,registry,nodes);
    QVERIFY(stage);
    const auto linkedId=uuid("b5000000-0000-4000-8000-000000000081");
    auto carrier=m::encode_exact_ref_linked_project_carrier(*stage.value(),linkedId);
    QVERIFY(carrier);
    project::ProjectDocument doc;
    doc.projectId=uuid("b6000000-0000-4000-8000-000000000081");
    doc.displayName="Owned P2 roundtrip fixture";
    doc.sourceResourceId=uuid("b7000000-0000-4000-8000-000000000081");
    const auto refResource=uuid("b7000000-0000-4000-8000-000000000082");
    const auto refId=uuid("b7000000-0000-4000-8000-000000000083");
    doc.resources.push_back({doc.sourceResourceId,"AUDIO",
        {{"rgsml.windows.local-file","C:/source.wav","source.wav"}},std::nullopt});
    doc.resources.push_back({refResource,"AUDIO",
        {{"rgsml.windows.local-file","C:/reference.wav","reference.wav"}},std::nullopt});
    doc.references.activeReferenceId=refId;
    doc.references.items.push_back({refId,"GOLD",refResource});
    project::Chain fixed;
    fixed.chainId=uuid("b8000000-0000-4000-8000-000000000081");
    fixed.stage="MASTER";fixed.segment="MANUAL";fixed.revision=0;
    doc.pipeline.masteringChainId=fixed.chainId;doc.chains.push_back(fixed);
    const auto before=project::ProjectSnapshot::create(doc);
    QVERIFY(before);
    m::M12ProjectTransactionExpected expected{doc.sourceResourceId,refId,fixed.chainId,
        fixed,std::nullopt,true,true};
    auto transaction=m::prepare_exact_m12_project_transaction(*before.value(),*carrier.value(),
        *stage.value(),expected);
    QVERIFY(transaction);
    const auto& candidate=transaction.value()->candidate_not_yet_committed;
    const auto zipped=std::make_shared<M12ZipStorage>();
    QVERIFY(project::ProjectRepository::save_create_new(candidate,
        std::make_unique<M12ZipEndpoint>(zipped),
        [zipped]()->core::Result<std::unique_ptr<core::IResourceReader>>{
            return core::Result<std::unique_ptr<core::IResourceReader>>::
                success(std::make_unique<M12ZipEndpoint>(zipped));}));
    auto reopened=project::ProjectRepository::open(std::make_unique<M12ZipEndpoint>(zipped));
    QVERIFY(reopened);
    auto restored=m::decode_exact_m12_ref_linked_product_state(reopened.value()->document(),registry);
    QVERIFY(restored);
    QVERIFY(restored.value()->exact_native_bindings==stage.value()->exact_bindings);
    auto unavailable=std::make_shared<const dsp::ModuleRegistry>(*base.value());
    QVERIFY(!m::decode_exact_m12_ref_linked_product_state(reopened.value()->document(),unavailable));
    const auto suspension=m::prepare_m12_owned_controller_suspension(*reopened.value(),
        accepted().reference_match_controller_id,linkedId,m::ControllerSuspensionCause::amount_zero,true);
    QVERIFY(suspension);
    const auto& suspended=suspension.value()->updated_project_not_committed;
    auto off=m::decode_exact_m12_ref_linked_product_state(suspended.document(),registry);
    QVERIFY(off);
    for(const auto& node:off.value()->exact_native_chain.instances())QVERIFY(!node.active());
    QVERIFY(off.value()->exact_native_bindings==stage.value()->exact_bindings);
    const auto resumed=m::prepare_m12_owned_controller_suspension(suspended,
        accepted().reference_match_controller_id,linkedId,m::ControllerSuspensionCause::current_amount_restored,true);
    QVERIFY(resumed);
    auto next=accepted();next.exact_accepted_plan_revision++;
    next.exact_plan_content_checksum="new-reviewed-owned-p2-plan";
    const auto changed=dsp::StreamingTruePeakLimiterParameters::create(-3.0,2.0,5);
    QVERIFY(changed);nodes.back().exact_m10_winning_parameters=*changed.value();
    auto nextStage=m::stage_exact_m12_native_ref_linked(next,*manual.value(),none,registry,nodes);
    QVERIFY(nextStage);
    auto nextCarrier=m::encode_exact_ref_linked_project_carrier(*nextStage.value(),linkedId);
    QVERIFY(nextCarrier);
    expected.exact_existing_linked_snapshot=reopened.value()->document().chains.back();
    auto update=m::prepare_exact_m12_project_transaction(*reopened.value(),*nextCarrier.value(),
        *nextStage.value(),expected);
    QVERIFY(update);
    auto updated=m::decode_exact_m12_ref_linked_product_state(
        update.value()->candidate_not_yet_committed.document(),registry);
    QVERIFY(updated);
    QVERIFY(updated.value()->exact_native_bindings==nextStage.value()->exact_bindings);
    QCOMPARE(updated.value()->exact_native_chain.instances().back().instance_id(),nodes.back().allocated_instance_id);
    auto wrongOrder=reopened.value()->document();
    std::reverse(wrongOrder.chains.back().modules.begin(),wrongOrder.chains.back().modules.end());
    QVERIFY(!m::decode_exact_m12_ref_linked_product_state(wrongOrder,registry));
    auto manualP2=dsp::ProcessingChain::create(*registry,
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(manualP2);
    const auto manualId=id("b3000000-0000-4000-8000-000000000083");
    QVERIFY(manualP2.value()->add(manualId,"rgsml.dsp.streaming-true-peak-limiter",0));
    const std::vector<dsp::ModuleExecutionBinding> manualBindings{{manualId,*p2.value()}};
    QVERIFY(!m::stage_exact_m12_native_ref_linked(accepted(),*manualP2.value(),manualBindings,registry,nodes));
}
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
    QVERIFY(transaction.value()->matches_exact_precommit(*currentBefore.value()));
    // P04: revision-only checks are insufficient. Even an unrelated
    // extension/output/optional file mutation invalidates the accepted
    // full precommit owner without changing MANUAL's revision.
    auto modifiedBefore=doc;
    modifiedBefore.displayName="a-renamed-project";
    auto renamed=project::ProjectSnapshot::create(modifiedBefore);
    QVERIFY(renamed);
    QVERIFY(!transaction.value()->matches_exact_precommit(*renamed.value()));
    modifiedBefore=doc;
    modifiedBefore.chains.front().revision++;
    auto revisedManual=project::ProjectSnapshot::create(modifiedBefore);
    QVERIFY(revisedManual);
    QVERIFY(!transaction.value()->matches_exact_precommit(
        *revisedManual.value()));
    modifiedBefore=doc;
    const auto futureOutput=project::OpaqueJsonValue::parse(
        "{\"unrelatedExportChoice\":true}");
    QVERIFY(futureOutput);
    modifiedBefore.output=*futureOutput.value();
    auto changedOutput=project::ProjectSnapshot::create(modifiedBefore);
    QVERIFY(changedOutput);
    QVERIFY(!transaction.value()->matches_exact_precommit(
        *changedOutput.value()));
    modifiedBefore=doc;
    project::OptionalEntry extraEntry;
    extraEntry.path="artifacts/unrelated.bin";
    extraEntry.mediaType="application/octet-stream";
    extraEntry.bytes={std::byte{0x21},std::byte{0x42}};
    auto changedOptional=project::ProjectSnapshot::create(
        modifiedBefore,{extraEntry});
    QVERIFY(changedOptional);
    QVERIFY(!transaction.value()->matches_exact_precommit(
        *changedOptional.value()));
    const auto& proposed=
        transaction.value()->candidate_not_yet_committed.document();
    QCOMPARE(proposed.chains.size(),std::size_t{2});
    // Persist actual native .rgsml ZIP bytes and reopen them through the
    // normal ProjectRepository. The precommit draft remains unadopted,
    // but the persisted format and exact linked identities are genuine.
    const auto zipped=std::make_shared<M12ZipStorage>();
    const auto saved=project::ProjectRepository::save_create_new(
        transaction.value()->candidate_not_yet_committed,
        std::make_unique<M12ZipEndpoint>(zipped),
        [zipped]()->core::Result<std::unique_ptr<core::IResourceReader>>{
            return core::Result<std::unique_ptr<core::IResourceReader>>::
                success(std::make_unique<M12ZipEndpoint>(zipped));
        });
    QVERIFY(saved);
    QVERIFY(zipped->bytes.size()>100U);
    const auto reopenedZip=project::ProjectRepository::open(
        std::make_unique<M12ZipEndpoint>(zipped));
    QVERIFY(reopenedZip);
    const auto& reopenedDocument=reopenedZip.value()->document();
    QCOMPARE(reopenedDocument.projectId,proposed.projectId);
    QCOMPARE(reopenedDocument.referenceMatch.canonical_utf8(),
             proposed.referenceMatch.canonical_utf8());
    QCOMPARE(reopenedDocument.chains.size(),std::size_t{2});
    QVERIFY(m::exact_project_chain_equal(
        reopenedDocument.chains.front(),manualChain));
    QVERIFY(m::exact_project_chain_equal(
        reopenedDocument.chains.back(),proposed.chains.back()));
    const auto zippedLinked=m::decode_exact_m12_ref_linked_product_state(
        reopenedDocument,stage.value()->registry_owner);
    QVERIFY(zippedLinked);
    QCOMPARE(zippedLinked.value()->plan_id,accepted().plan_id);
    QCOMPARE(zippedLinked.value()->exact_plan_checksum,
             accepted().exact_plan_content_checksum);
    QVERIFY(!zippedLinked.value()->is_m12_materialization_receipt());
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
    QVERIFY(m::m12_native_stage_witness_matches_replay_plan(
        *replayPlan.value(),firstStageWitness));
    auto forgedWitness=firstStageWitness;
    forgedWitness.reference_linked.front().instance_id=gainId;
    QVERIFY(!m::m12_native_stage_witness_matches_replay_plan(
        *replayPlan.value(),forgedWitness));
    forgedWitness=firstStageWitness;
    forgedWitness.protected_manual.front().algorithm_version="bogus-version";
    QVERIFY(!m::m12_native_stage_witness_matches_replay_plan(
        *replayPlan.value(),forgedWitness));
    forgedWitness=firstStageWitness;
    forgedWitness.reference_linked.front().disposition=
        firstStageWitness.reference_linked.front().disposition==
            render::ModuleExecutionDisposition::PROCESSED
            ?render::ModuleExecutionDisposition::BYPASS_IDENTITY
            :render::ModuleExecutionDisposition::PROCESSED;
    QVERIFY(!m::m12_native_stage_witness_matches_replay_plan(
        *replayPlan.value(),forgedWitness));
    QCOMPARE(realReplay.value()->view().frame_count().value(),
             std::int64_t{48000});
    QCOMPARE(realReplay.value()->view().format(),input.value()->view().format());
    QCOMPARE(realReplay.value()->chain_revision(),
             replayPlan.value()->reference_linked.revision());
    // This second ZIP fixture includes an ACTUAL persisted Input Gain.
    // Rehydrate MANUAL and REF_LINKED from genuine ProjectRepository
    // bytes and compare every resulting PCM sample to the pre-save native
    // realization. Preserved IDs/metadata alone are not audio evidence.
    const auto persistedReplay=project::ProjectSnapshot::create(replayDocument);
    QVERIFY(persistedReplay);
    const auto audioZip=std::make_shared<M12ZipStorage>();
    const auto persistedStatus=project::ProjectRepository::save_create_new(
        *persistedReplay.value(),std::make_unique<M12ZipEndpoint>(audioZip),
        [audioZip]()->core::Result<std::unique_ptr<core::IResourceReader>>{
            return core::Result<std::unique_ptr<core::IResourceReader>>::
                success(std::make_unique<M12ZipEndpoint>(audioZip));
        });
    QVERIFY(persistedStatus);
    const auto onDisk=project::ProjectRepository::open(
        std::make_unique<M12ZipEndpoint>(audioZip));
    QVERIFY(onDisk);
    const auto diskLinked=m::decode_exact_m12_ref_linked_product_state(
        onDisk.value()->document(),reg);
    QVERIFY(diskLinked);
    const auto diskPlan=m::prepare_restored_m12_native_replay_plan(
        onDisk.value()->document(),*diskLinked.value(),
        manualChain.chainId,*manualGain.value(),gainBindings,true);
    QVERIFY(diskPlan);
    const auto diskPCM=m::render_restored_m12_full_native_pcm_once(
        *diskPlan.value(),*prepared.value(),*maxBlock.value());
    QVERIFY(diskPCM);
    const auto beforeZip=realReplay.value()->view();
    const auto afterZip=diskPCM.value()->view();
    QCOMPARE(beforeZip.absolute_range(),afterZip.absolute_range());
    QCOMPARE(beforeZip.format(),afterZip.format());
    for(std::size_t ch=0;ch<beforeZip.format().channel_count();++ch) {
        const auto beforeSamples=beforeZip.channel(ch);
        const auto afterSamples=afterZip.channel(ch);
        QVERIFY(beforeSamples&&afterSamples);
        QCOMPARE(beforeSamples.value()->size(),afterSamples.value()->size());
        for(std::size_t i=0;i<beforeSamples.value()->size();++i)
            QCOMPARE(std::bit_cast<std::uint64_t>((*beforeSamples.value())[i]),
                     std::bit_cast<std::uint64_t>((*afterSamples.value())[i]));
    }
    // All six Product rates must preserve genuine C00 PCM across
    // persisted REF_LINKED native Project roundtrip (mono and stereo).
    // No DSP bypass, proxy, SRC or M17 export may stand in for C00.
    for(const int hz:{44100,48000,88200,96000,176400,192000}) {
        const auto fs=core::SampleRate::create(hz);
        QVERIFY(fs);
        const auto frames=core::FrameCount::create(hz/8);
        QVERIFY(frames);
        for(const auto layout:{audio::ChannelLayout::MONO_C,
                               audio::ChannelLayout::STEREO_LR}) {
            const auto format=audio::AudioFormat::create(*fs.value(),layout);
            QVERIFY(format);
            auto programme=audio::AudioBuffer::create(
                *format.value(),
                audio::FrameDomainId::SOURCE_PROCESSING_RATE,
                core::FrameIndex{0},*frames.value());
            QVERIFY(programme);
            auto writable=programme.value()->mutable_view();
            for(std::size_t ch=0;ch<format.value()->channel_count();++ch) {
                const auto channel=writable.channel(ch);
                QVERIFY(channel);
                for(std::size_t i=0;i<channel.value()->size();++i)
                    (*channel.value())[i]=0.35*std::sin(
                        2.0*std::numbers::pi*(440.0+55.0*ch)*
                        static_cast<double>(i)/hz);
            }
            const auto preRequest=render::RenderRequest::create(
                programme.value()->view(),
                programme.value()->view().absolute_range(),
                *empty.value(),{},*maxBlock.value());
            QVERIFY(preRequest);
            const auto sourcePCM=render::render_preview(
                *preRequest.value(),*reg);
            QVERIFY(sourcePCM);
            const auto originalPCM=m::render_restored_m12_full_native_pcm_once(
                *replayPlan.value(),*sourcePCM.value(),*maxBlock.value());
            const auto savedPCM=m::render_restored_m12_full_native_pcm_once(
                *diskPlan.value(),*sourcePCM.value(),*maxBlock.value());
            const auto supportedCase=std::to_string(hz)+" Hz / "+
                std::to_string(format.value()->channel_count())+" ch";
            QVERIFY2(originalPCM,supportedCase.c_str());
            QVERIFY2(savedPCM,supportedCase.c_str());
            QCOMPARE(originalPCM.value()->view().absolute_range(),
                     savedPCM.value()->view().absolute_range());
            QCOMPARE(originalPCM.value()->view().format(),
                     savedPCM.value()->view().format());
            for(std::size_t ch=0;ch<format.value()->channel_count();++ch) {
                const auto originalChannel=originalPCM.value()->view().channel(ch);
                const auto savedChannel=savedPCM.value()->view().channel(ch);
                QVERIFY(originalChannel&&savedChannel);
                QCOMPARE(originalChannel.value()->size(),
                         savedChannel.value()->size());
                for(std::size_t i=0;i<originalChannel.value()->size();++i)
                    QCOMPARE(std::bit_cast<std::uint64_t>(
                                 (*originalChannel.value())[i]),
                             std::bit_cast<std::uint64_t>(
                                 (*savedChannel.value())[i]));
            }
        }
    }
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
    // A real PREPARED owner and typed manual slots permit a provisional
    // transfer without mutating the accepted ticket or unrelated DSPs.
    namespace transfer=rgsml::app::l3_automatch;
    auto transferManual=*manualGain.value();
    const auto manualDynamic=id("b3000000-0000-4000-8000-000000000091");
    const auto manualTransient=id("b3000000-0000-4000-8000-000000000092");
    QVERIFY(transferManual.add(manualDynamic,"rgsml.dsp.dynamic-eq",transferManual.instances().size()));
    QVERIFY(transferManual.set_user_bypass(manualDynamic,true));
    QVERIFY(transferManual.add(manualTransient,"rgsml.dsp.transient-shaper",transferManual.instances().size()));
    QVERIFY(transferManual.set_user_bypass(manualTransient,true));
    auto transferBindings=gainBindings;
    const auto neutralTransient=dsp::TransientShaperParameters::create_default();
    QVERIFY(neutralTransient);
    transferBindings.push_back({manualDynamic,*deq.value()});
    transferBindings.push_back({manualTransient,*neutralTransient.value()});
    auto transferReplay=*replayPlan.value();
    transferReplay.manual_prefix=transferManual;
    transferReplay.manual_bindings=transferBindings;
    auto transferTicket=m::prepare_m12_immutable_product_preview_ticket(
        std::make_shared<const m::RestoredM12NativeReplayPlan>(transferReplay),
        ownedPrepared,transferManual,transferBindings);
    QVERIFY(transferTicket);
    auto exactTransferOwner=std::make_shared<const m::M12ImmutableProductPreviewTicket>(*transferTicket.value());
    auto draft=transfer::prepare_manual_match_transfer(exactTransferOwner,transferManual,
        transferBindings,*reg,nodes[1].allocated_instance_id);
    QVERIFY(draft);
    QVERIFY(draft.value()->requires_explicit_confirmation());
    QVERIFY(draft.value()->requires_fresh_verification());
    QVERIFY(!draft.value()->authorizes_project_commit());
    QVERIFY(draft.value()->manual_instance==manualTransient);
    const auto manualAfter=draft.value()->manual_after.proposed_manual_chain.instances();
    QVERIFY(manualAfter.back().active());
    QVERIFY(std::get<dsp::TransientShaperParameters>(
        draft.value()->manual_after.proposed_bindings.back().parameters)==*ts.value());
    const auto linkedAfter=draft.value()->linked_after.instances();
    const auto linkedBefore=exactTransferOwner->replay->reference_linked.instances();
    QCOMPARE(linkedAfter.size(),linkedBefore.size());
    QVERIFY(!linkedAfter[1].active());
    QVERIFY(linkedBefore[1].active());
    QVERIFY(linkedAfter[0]==linkedBefore[0]);
    QVERIFY(linkedAfter[2]==linkedBefore[2]);
    QVERIFY(draft.value()->linked_bindings_after==exactTransferOwner->replay->reference_bindings);
    QVERIFY(!transferManual.instances().back().active());
    const auto transferIdentity=transfer::identify_manual_match_transfer(*draft.value());
    QVERIFY(transferIdentity);
    QCOMPARE(transferIdentity.value()->sha256.size(),std::size_t{64});
    QVERIFY(!transferIdentity.value()->is_solver_winner());
    QVERIFY(!transferIdentity.value()->is_verified());
    QVERIFY(!transferIdentity.value()->authorizes_project_commit());
    const auto repeat=transfer::identify_manual_match_transfer(*draft.value());
    QVERIFY(repeat);
    QCOMPARE(repeat.value()->sha256,transferIdentity.value()->sha256);
    const auto other=transfer::prepare_manual_match_transfer(exactTransferOwner,transferManual,
        transferBindings,*reg,nodes[0].allocated_instance_id);
    QVERIFY(other);
    const auto otherIdentity=transfer::identify_manual_match_transfer(*other.value());
    QVERIFY(otherIdentity);
    QVERIFY(otherIdentity.value()->sha256!=transferIdentity.value()->sha256);
    QVERIFY(transferIdentity.value()->sha256!=exactTransferOwner->replay->exact_plan_checksum);
    auto forgedTransfer=*draft.value();
    forgedTransfer.manual_after.proposed_bindings.front().parameters=
        *dsp::GainParameters::create(7.0).value();
    QVERIFY(!transfer::identify_manual_match_transfer(forgedTransfer));
    // Construct the exact persisted before-owner with the two manual slots.
    auto transferDocument=replayDocument;
    auto& manualCarrier=transferDocument.chains.front();
    manualCarrier.revision=transferManual.revision();
    for(std::size_t i=1;i<transferManual.instances().size();++i) {
        const auto& n=transferManual.instances()[i];
        project::Module module;
        module.instanceId=n.instance_id().uuid();
        module.typeId=std::string(n.module_type_id());
        module.enabled=n.enabled();module.userBypass=n.user_bypass();
        const auto descriptor=reg->find_descriptor(module.typeId);
        QVERIFY(descriptor);
        module.algorithmVersion=descriptor.value()->get().algorithm_version();
        module.parameterSchemaId=descriptor.value()->get().parameter_schema_id();
        const auto json=dsp::encode_module_parameters_json(transferBindings[i].parameters);
        QVERIFY(json);
        const auto opaque=project::OpaqueJsonValue::parse(*json.value());
        QVERIFY(opaque);
        module.parameters=*opaque.value();
        manualCarrier.modules.push_back(std::move(module));
    }
    const auto beforeTransfer=project::ProjectSnapshot::create(transferDocument);
    QVERIFY(beforeTransfer);
    const auto projectDraft=transfer::prepare_manual_match_transfer_project(
        *beforeTransfer.value(),*draft.value());
    QVERIFY(projectDraft);
    QVERIFY(projectDraft.value()->matches_before(*beforeTransfer.value()));
    QVERIFY(!projectDraft.value()->matches_after(*beforeTransfer.value()));
    QVERIFY(!projectDraft.value()->authorizes_project_commit());
    QVERIFY(transfer::validate_manual_match_transfer_project_readback(
        projectDraft.value()->after,*projectDraft.value(),*draft.value()));
    QVERIFY(!transfer::validate_manual_match_transfer_project_readback(
        *beforeTransfer.value(),*projectDraft.value(),*draft.value()));
    auto tampered=projectDraft.value()->after.document();
    tampered.chains.front().modules.front().userBypass=true;
    const auto tamperedSnapshot=project::ProjectSnapshot::create(tampered);
    QVERIFY(tamperedSnapshot);
    QVERIFY(!transfer::validate_manual_match_transfer_project_readback(
        *tamperedSnapshot.value(),*projectDraft.value(),*draft.value()));
    auto forgedExpected=*projectDraft.value();
    forgedExpected.after=*tamperedSnapshot.value();
    QVERIFY(!transfer::validate_manual_match_transfer_project_readback(
        *tamperedSnapshot.value(),forgedExpected,*draft.value()));
    forgedExpected=*projectDraft.value();
    forgedExpected.identity.sha256=std::string(64,'0');
    QVERIFY(!transfer::validate_manual_match_transfer_project_readback(
        projectDraft.value()->after,forgedExpected,*draft.value()));

    const auto& afterDocument=projectDraft.value()->after.document();
    QVERIFY(!afterDocument.chains.front().modules.back().userBypass);
    QVERIFY(afterDocument.chains.back().modules[1].userBypass);
    QVERIFY(afterDocument.chains.front().modules.back().parameters==
        afterDocument.chains.back().modules[1].parameters);
    QVERIFY(m::exact_project_module_equal(
        afterDocument.chains.back().modules[0],transferDocument.chains.back().modules[0]));
    QVERIFY(!m::decode_exact_m12_ref_linked_product_state(afterDocument,reg));
    const auto recovered=transfer::recover_manual_match_transfer_before(projectDraft.value()->after);
    QVERIFY(recovered);
    QVERIFY(projectDraft.value()->matches_before(*recovered.value()));
    const auto recoveredForward=transfer::validate_recovered_manual_match_transfer(
        projectDraft.value()->after,*draft.value());
    QVERIFY(recoveredForward);
    QVERIFY(recoveredForward.value()->matches_before(*beforeTransfer.value()));
    QVERIFY(recoveredForward.value()->matches_after(projectDraft.value()->after));
    QVERIFY(!transfer::validate_recovered_manual_match_transfer(
        *tamperedSnapshot.value(),*draft.value()));
    QVERIFY(!transfer::validate_recovered_manual_match_transfer(
        projectDraft.value()->after,*other.value()));

    QVERIFY(m::decode_exact_m12_ref_linked_product_state(recovered.value()->document(),reg));
    QVERIFY(!transfer::recover_manual_match_transfer_before(*beforeTransfer.value()));
    auto corruptedCandidate=afterDocument;
    auto candidateMeta=QJsonDocument::fromJson(QByteArray::fromStdString(
        corruptedCandidate.referenceMatch.canonical_utf8())).object();
    auto candidateRecord=candidateMeta.value(QStringLiteral("manualTransferCandidate")).toObject();
    candidateRecord.insert(QStringLiteral("identitySha256"),QString(64,QLatin1Char('0')));
    candidateMeta.insert(QStringLiteral("manualTransferCandidate"),candidateRecord);
    const auto corruptMeta=project::OpaqueJsonValue::parse(
        QJsonDocument(candidateMeta).toJson(QJsonDocument::Compact).toStdString());
    QVERIFY(corruptMeta);
    corruptedCandidate.referenceMatch=*corruptMeta.value();
    const auto corruptCandidateSnapshot=project::ProjectSnapshot::create(corruptedCandidate);
    QVERIFY(corruptCandidateSnapshot);
    QVERIFY(!transfer::recover_manual_match_transfer_before(*corruptCandidateSnapshot.value()));
    auto wrongRevision=afterDocument;
    ++wrongRevision.chains.front().revision;
    const auto wrongRevisionSnapshot=project::ProjectSnapshot::create(wrongRevision);
    QVERIFY(wrongRevisionSnapshot);
    QVERIFY(!transfer::recover_manual_match_transfer_before(*wrongRevisionSnapshot.value()));

    auto staleDocument=transferDocument;
    staleDocument.chains.front().modules.front().userBypass=true;
    const auto staleSnapshot=project::ProjectSnapshot::create(staleDocument);
    QVERIFY(staleSnapshot);
    QVERIFY(!transfer::prepare_manual_match_transfer_project(*staleSnapshot.value(),*draft.value()));

    const auto audition=transfer::render_manual_match_transfer_candidate(
        *draft.value(),*transferIdentity.value(),*maxBlock.value());
    QVERIFY(audition);
    QCOMPARE(audition.value()->view().absolute_range(),ownedPrepared->view().absolute_range());
    QCOMPARE(audition.value()->view().format(),ownedPrepared->view().format());
    QVERIFY(!transfer::render_manual_match_transfer_candidate(
        *draft.value(),*otherIdentity.value(),*maxBlock.value()));
    std::atomic<bool> cancelledTransfer{true};
    QVERIFY(!transfer::render_manual_match_transfer_candidate(
        *draft.value(),*transferIdentity.value(),*maxBlock.value(),&cancelledTransfer));

    // The worker entry point must retain the same exact native and
    // persisted owner. A cancelled or forged capture cannot publish PCM.
    const transfer::ManualMatchTransferCapture capture{
        1U,*draft.value(),*projectDraft.value()};
    const auto capturedAudition=transfer::render_captured_manual_match_transfer(
        capture,*maxBlock.value());
    QVERIFY(capturedAudition);
    QCOMPARE(capturedAudition.value()->view().absolute_range(),
        audition.value()->view().absolute_range());
    QCOMPARE(capturedAudition.value()->view().format(),
        audition.value()->view().format());
    auto invalidCapture=capture;
    invalidCapture.product_epoch=0U;
    QVERIFY(!transfer::render_captured_manual_match_transfer(
        invalidCapture,*maxBlock.value()));
    invalidCapture=capture;
    invalidCapture.project.identity.sha256=std::string(64,'0');
    QVERIFY(!transfer::render_captured_manual_match_transfer(
        invalidCapture,*maxBlock.value()));
    invalidCapture=capture;
    invalidCapture.project.after=*tamperedSnapshot.value();
    QVERIFY(!transfer::render_captured_manual_match_transfer(
        invalidCapture,*maxBlock.value()));
    QVERIFY(!transfer::render_captured_manual_match_transfer(
        capture,*maxBlock.value(),&cancelledTransfer));

    const auto immutableCapture=std::make_shared<const transfer::ManualMatchTransferCapture>(capture);
    const auto provisional=transfer::ManualMatchTransferPreviewTicket::create(immutableCapture);
    QVERIFY(provisional);
    QVERIFY(!provisional.value()->is_verified());
    QVERIFY(provisional.value()->matches_prepared(ownedPrepared));
    QVERIFY(!provisional.value()->matches_prepared({}));
    QVERIFY(provisional.value()->matches_project(projectDraft.value()->after));
    QVERIFY(!provisional.value()->matches_project(projectDraft.value()->before));
    QVERIFY(provisional.value()->matches_manual(
        draft.value()->manual_after.proposed_manual_chain,
        draft.value()->manual_after.proposed_bindings));
    QVERIFY(!provisional.value()->matches_manual(transferManual,transferBindings));
    auto staleBindings=draft.value()->manual_after.proposed_bindings;
    staleBindings.front().parameters=*dsp::GainParameters::create(9.0).value();
    QVERIFY(!provisional.value()->matches_manual(
        draft.value()->manual_after.proposed_manual_chain,staleBindings));
    const auto& stagedManual=draft.value()->manual_after.proposed_manual_chain;
    const auto& stagedBindings=draft.value()->manual_after.proposed_bindings;
    const auto tuned=transfer::prepare_transferred_manual_parameter_edit(
        *provisional.value(),stagedManual,stagedBindings,ownedPrepared,manualTransient,
        *neutralTransient.value());
    QVERIFY(tuned);
    QCOMPARE(tuned.value()->proposed_manual_chain.revision(),stagedManual.revision()+1U);
    QVERIFY(tuned.value()->proposed_bindings.back().parameters==
        dsp::ModuleParameterPayload{*neutralTransient.value()});
    QVERIFY(tuned.value()->proposed_bindings.front()==stagedBindings.front());
    QVERIFY(tuned.value()->proposed_bindings[1]==stagedBindings[1]);
    QVERIFY(std::equal(tuned.value()->proposed_manual_chain.instances().begin(),
        tuned.value()->proposed_manual_chain.instances().end(),stagedManual.instances().begin()));
    QVERIFY(!tuned.value()->authorizes_project_commit());
    QVERIFY(!transfer::prepare_transferred_manual_parameter_edit(
        *provisional.value(),stagedManual,stagedBindings,ownedPrepared,manualDynamic,*deq.value()));
    QVERIFY(!transfer::prepare_transferred_manual_parameter_edit(
        *provisional.value(),stagedManual,stagedBindings,ownedPrepared,manualTransient,*deq.value()));
    QVERIFY(!transfer::prepare_transferred_manual_parameter_edit(
        *provisional.value(),stagedManual,stagedBindings,{},manualTransient,*neutralTransient.value()));
    QVERIFY(!transfer::prepare_transferred_manual_parameter_edit(
        *provisional.value(),stagedManual,staleBindings,ownedPrepared,manualTransient,*neutralTransient.value()));
    const auto fineTune=transfer::prepare_transferred_manual_edit_project(
        *provisional.value(),projectDraft.value()->after,stagedManual,stagedBindings,
        ownedPrepared,manualTransient,*neutralTransient.value());
    QVERIFY(fineTune);
    QCOMPARE(fineTune.value()->identity_sha256.size(),std::size_t{64});
    QVERIFY(fineTune.value()->identity_sha256!=transferIdentity.value()->sha256);
    QVERIFY(projectDraft.value()->matches_after(fineTune.value()->before));
    QVERIFY(!projectDraft.value()->matches_after(fineTune.value()->after));
    const auto& tunedDoc=fineTune.value()->after.document();
    QCOMPARE(tunedDoc.chains.front().revision,stagedManual.revision()+1U);
    QVERIFY(m::exact_project_module_equal(tunedDoc.chains.front().modules.front(),
        projectDraft.value()->after.document().chains.front().modules.front()));
    QVERIFY(m::exact_project_module_equal(tunedDoc.chains.back().modules[1],
        projectDraft.value()->after.document().chains.back().modules[1]));
    const auto fineMeta=QJsonDocument::fromJson(QByteArray::fromStdString(
        tunedDoc.referenceMatch.canonical_utf8())).object();
    const auto transferBaseMeta=QJsonDocument::fromJson(QByteArray::fromStdString(
        projectDraft.value()->after.document().referenceMatch.canonical_utf8())).object();
    QVERIFY(fineMeta.value(QStringLiteral("manualTransferCandidate"))==
        transferBaseMeta.value(QStringLiteral("manualTransferCandidate")));
    QVERIFY(fineMeta.contains(QStringLiteral("manualFineTune")));
    const auto recoveredFineBase=transfer::recover_transferred_manual_edit_base(fineTune.value()->after);
    QVERIFY(recoveredFineBase);
    QVERIFY(projectDraft.value()->matches_after(*recoveredFineBase.value()));
    const auto noFineBase=transfer::recover_transferred_manual_edit_base(projectDraft.value()->after);
    QVERIFY(noFineBase&&projectDraft.value()->matches_after(*noFineBase.value()));
    auto corruptedFine=tunedDoc;
    auto corruptedFineMeta=fineMeta;
    auto corruptedFineRecord=corruptedFineMeta.value(QStringLiteral("manualFineTune")).toObject();
    corruptedFineRecord.insert(QStringLiteral("identitySha256"),QString(64,QLatin1Char('0')));
    corruptedFineMeta.insert(QStringLiteral("manualFineTune"),corruptedFineRecord);
    auto corruptedFineOpaque=project::OpaqueJsonValue::parse(
        QJsonDocument(corruptedFineMeta).toJson(QJsonDocument::Compact).toStdString());
    QVERIFY(corruptedFineOpaque);
    corruptedFine.referenceMatch=*corruptedFineOpaque.value();
    const auto corruptedFineSnapshot=project::ProjectSnapshot::create(corruptedFine);
    QVERIFY(corruptedFineSnapshot);
    QVERIFY(!transfer::recover_transferred_manual_edit_base(*corruptedFineSnapshot.value()));
    auto mismatchedFine=tunedDoc;
    mismatchedFine.chains.front().modules.back().parameters=
        projectDraft.value()->after.document().chains.front().modules.back().parameters;
    const auto mismatchedFineSnapshot=project::ProjectSnapshot::create(mismatchedFine);
    QVERIFY(mismatchedFineSnapshot);
    QVERIFY(!transfer::recover_transferred_manual_edit_base(*mismatchedFineSnapshot.value()));

    QVERIFY(!fineMeta.contains(QStringLiteral("materialization")));
    QVERIFY(!transfer::prepare_transferred_manual_edit_project(
        *provisional.value(),*tamperedSnapshot.value(),stagedManual,stagedBindings,
        ownedPrepared,manualTransient,*neutralTransient.value()));
    const auto editedTicket=transfer::ManualMatchTransferPreviewTicket::create_edited(
        immutableCapture,fineTune.value()->after);
    QVERIFY(editedTicket);
    QVERIFY(editedTicket.value()->matches_manual(tuned.value()->proposed_manual_chain,
        tuned.value()->proposed_bindings));
    QVERIFY(!editedTicket.value()->matches_manual(stagedManual,stagedBindings));
    QVERIFY(!editedTicket.value()->is_verified());
    QVERIFY(editedTicket.value()->matches_project(fineTune.value()->after));
    QVERIFY(!editedTicket.value()->matches_project(projectDraft.value()->after));
    QVERIFY(!transfer::ManualMatchTransferPreviewTicket::create_edited(
        immutableCapture,*corruptedFineSnapshot.value()));
    const auto finePcm=editedTicket.value()->render(*maxBlock.value());
    QVERIFY(finePcm);
    QVERIFY(!finePcm.value()->linked_activity_sidecar());
    QVERIFY(!editedTicket.value()->render(*maxBlock.value(),&cancelledTransfer));
    // C01 also uses the same immutable overlay, including exact decimals.
    const auto dynamicProject=transfer::prepare_manual_match_transfer_project(
        *beforeTransfer.value(),*other.value());
    QVERIFY(dynamicProject);
    const auto dynamicCapture=std::make_shared<const transfer::ManualMatchTransferCapture>(
        transfer::ManualMatchTransferCapture{1U,*other.value(),*dynamicProject.value()});
    const auto dynamicTicket=transfer::ManualMatchTransferPreviewTicket::create(dynamicCapture);
    QVERIFY(dynamicTicket);
    auto bandSpec=deq.value()->bands().front().spec();
    bandSpec.control.base_gain_db=-1.23456789;
    auto band=dsp::DynamicEqBandParameters::create(bandSpec);
    QVERIFY(band);
    auto editedEq=dsp::DynamicEqParameters::create({*band.value()});
    QVERIFY(editedEq);
    const auto& dynamicState=dynamicTicket.value()->manual_state();
    const auto dynamicEdit=transfer::prepare_transferred_manual_edit_project(
        *dynamicTicket.value(),dynamicProject.value()->after,dynamicState.proposed_manual_chain,
        dynamicState.proposed_bindings,ownedPrepared,manualDynamic,*editedEq.value());
    QVERIFY(dynamicEdit);
    const auto dynamicEditedTicket=transfer::ManualMatchTransferPreviewTicket::create_edited(
        dynamicCapture,dynamicEdit.value()->after);
    QVERIFY(dynamicEditedTicket);
    QVERIFY(dynamicEditedTicket.value()->matches_manual(dynamicEdit.value()->staged.proposed_manual_chain,
        dynamicEdit.value()->staged.proposed_bindings));
    const auto dynamicPcm=dynamicEditedTicket.value()->render(*maxBlock.value());
    QVERIFY(dynamicPcm);
    QVERIFY(!dynamicPcm.value()->linked_activity_sidecar());
    const auto dynamicRecovered=transfer::recover_transferred_manual_edit_base(dynamicEdit.value()->after);
    QVERIFY(dynamicRecovered&&dynamicProject.value()->matches_after(*dynamicRecovered.value()));
    auto forgedFinePeer=dynamicEdit.value()->after.document();
    forgedFinePeer.chains.front().modules.front().userBypass=true;
    const auto forgedFinePeerSnapshot=project::ProjectSnapshot::create(forgedFinePeer);
    QVERIFY(forgedFinePeerSnapshot);
    QVERIFY(!transfer::ManualMatchTransferPreviewTicket::create_edited(dynamicCapture,*forgedFinePeerSnapshot.value()));
    const auto provisionalPcm=provisional.value()->render(*maxBlock.value());
    QVERIFY(provisionalPcm);
    QCOMPARE(provisionalPcm.value()->view().absolute_range(),
        audition.value()->view().absolute_range());
    QVERIFY(!provisional.value()->render(*maxBlock.value(),&cancelledTransfer));
    QVERIFY(!transfer::ManualMatchTransferPreviewTicket::create({}));
    const auto workerAudition=transfer::ManualMatchTransferAudition::create(
        immutableCapture,*maxBlock.value());
    QVERIFY(workerAudition);
    QVERIFY(&workerAudition.value()->capture()==immutableCapture.get());
    QCOMPARE(workerAudition.value()->rendered().view().absolute_range(),
        audition.value()->view().absolute_range());
    QVERIFY(!workerAudition.value()->is_verified());
    QVERIFY(!workerAudition.value()->authorizes_project_commit());
    const transfer::ManualMatchTransferConfirmation confirmation{
        capture.product_epoch,capture.project.identity.sha256};
    QVERIFY(transfer::manual_match_transfer_confirmation_matches(
        *workerAudition.value(),confirmation));
    auto expiredConfirmation=confirmation;
    ++expiredConfirmation.product_epoch;
    QVERIFY(!transfer::manual_match_transfer_confirmation_matches(
        *workerAudition.value(),expiredConfirmation));
    auto differentConfirmation=confirmation;
    differentConfirmation.identity_sha256=otherIdentity.value()->sha256;
    QVERIFY(!transfer::manual_match_transfer_confirmation_matches(
        *workerAudition.value(),differentConfirmation));
    QVERIFY(!transfer::manual_match_transfer_confirmation_matches(
        *workerAudition.value(),{}));

    QVERIFY(!transfer::ManualMatchTransferAudition::create({},*maxBlock.value()));
    QVERIFY(!transfer::ManualMatchTransferAudition::create(
        immutableCapture,*maxBlock.value(),&cancelledTransfer));


    QVERIFY(!transfer::prepare_manual_match_transfer(exactTransferOwner,transferManual,
        transferBindings,*reg,nodes[2].allocated_instance_id));
    QVERIFY(transferManual.set_user_bypass(manualTransient,false));
    QVERIFY(!transfer::prepare_manual_match_transfer(exactTransferOwner,transferManual,
        transferBindings,*reg,nodes[1].allocated_instance_id));

    // Exercise the combined nonpublishing Product precommit bridge:
    // exact M11 protected MANUAL, immutable M10/M12 winning plan,
    // canonical Project after-state, real PREPARED PCM and one registry.
    auto productBeforeDocument=replayDocument;
    productBeforeDocument.chains.pop_back();
    productBeforeDocument.referenceMatch=
        project::OpaqueJsonValue::empty_object();
    const auto productBefore=project::ProjectSnapshot::create(
        productBeforeDocument);
    QVERIFY(productBefore);
    const auto exactProductStage=m::stage_exact_m12_native_ref_linked(
        accepted(),*manualGain.value(),gainBindings,reg,nodes);
    QVERIFY(exactProductStage);
    const auto exactProductCarrier=m::encode_exact_ref_linked_project_carrier(
        *exactProductStage.value(),refChainId);
    QVERIFY(exactProductCarrier);
    m::M12ProjectTransactionExpected productExpected{
        doc.sourceResourceId,activeReference,manualChain.chainId,
        productBeforeDocument.chains.front(),std::nullopt,true,true};
    const auto productDraft=m::prepare_exact_m12_project_transaction(
        *productBefore.value(),*exactProductCarrier.value(),
        *exactProductStage.value(),productExpected);
    QVERIFY(productDraft);
    const auto combinedTicket=m::prepare_m12_product_prepublication_preview_ticket(
        *productBefore.value(),*productDraft.value(),reg,
        manualChain.chainId,*manualGain.value(),gainBindings,ownedPrepared,
        true);
    QVERIFY(combinedTicket);
    QVERIFY(!combinedTicket.value()->authorizes_product_installation());
    QVERIFY(!combinedTicket.value()->authorizes_m12_receipt());
    QVERIFY(!combinedTicket.value()->authorizes_m13_verified());
    QVERIFY(combinedTicket.value()->replay);
    QCOMPARE(combinedTicket.value()->replay->plan_id,accepted().plan_id);
    // The Reference Match loudness gain is a distinct owned DSP,
    // not an edit/reownership of the protected Input Gain.
    const auto linkedGainId=id("bf100000-0000-4000-8000-000000000002");
    const auto loudnessGain=dsp::GainParameters::create(2.0);
    QVERIFY(loudnessGain);
    std::vector<m::ExactLinkedNode> loudnessNodes{{
        linkedGainId,uuid("bf100000-0000-4000-8000-000000000003"),
        "rgsml.dsp.gain",*loudnessGain.value()}};
    const auto gainStage=m::stage_exact_m12_native_ref_linked(
        accepted(),*manualGain.value(),gainBindings,reg,loudnessNodes);
    QVERIFY(gainStage);
    const auto gainCarrier=m::encode_exact_ref_linked_project_carrier(
        *gainStage.value(),refChainId);
    QVERIFY(gainCarrier);
    const auto gainDraft=m::prepare_exact_m12_project_transaction(
        *productBefore.value(),*gainCarrier.value(),*gainStage.value(),productExpected);
    QVERIFY(gainDraft);
    const auto gainTicket=m::prepare_m12_product_prepublication_preview_ticket(
        *productBefore.value(),*gainDraft.value(),reg,
        manualChain.chainId,*manualGain.value(),gainBindings,ownedPrepared,true);
    QVERIFY(gainTicket&&gainTicket.value()->replay);
    m::M12NativeStageExecutionWitness gainWitness;
    const auto gainReplay=m::render_restored_m12_full_native_pcm_once(
        *gainTicket.value()->replay,*ownedPrepared,*maxBlock.value(),&gainWitness);
    QVERIFY(gainReplay);
    QCOMPARE(gainWitness.protected_manual.front().instance_id,gainId);
    QCOMPARE(gainWitness.reference_linked.front().instance_id,linkedGainId);
    QVERIFY(m::m12_native_stage_witness_matches_replay_plan(
        *gainTicket.value()->replay,gainWitness));
    loudnessNodes.front()={gainId,uuid("bf100000-0000-4000-8000-000000000003"),
                           "rgsml.dsp.gain",*loudnessGain.value()};
    QVERIFY(!m::stage_exact_m12_native_ref_linked(
        accepted(),*manualGain.value(),gainBindings,reg,loudnessNodes));
    // A same-controller UPDATE with a fresh immutable plan revision
    // must remain admissible after an earlier REF segment was saved.
    auto nextProductPlan=accepted();
    ++nextProductPlan.exact_accepted_plan_revision;
    nextProductPlan.exact_plan_content_checksum="product-plan-v2-checksum";
    nextProductPlan.exact_m10_result_and_topology_checksum=
        "product-winning-v2-checksum";
    const auto nextProductStage=m::stage_exact_m12_native_ref_linked(
        nextProductPlan,*manualGain.value(),gainBindings,reg,nodes);
    QVERIFY(nextProductStage);
    const auto nextProductCarrier=m::encode_exact_ref_linked_project_carrier(
        *nextProductStage.value(),refChainId);
    QVERIFY(nextProductCarrier);
    auto currentLinkedExpected=productExpected;
    currentLinkedExpected.exact_existing_linked_snapshot=
        productDraft.value()->candidate_not_yet_committed.document().
            chains.back();
    const auto acceptedNextDraft=m::prepare_exact_m12_project_transaction(
        productDraft.value()->candidate_not_yet_committed,
        *nextProductCarrier.value(),*nextProductStage.value(),
        currentLinkedExpected);
    QVERIFY(acceptedNextDraft);
    const auto updatedProductTicket=
        m::prepare_m12_product_prepublication_preview_ticket(
            productDraft.value()->candidate_not_yet_committed,
            *acceptedNextDraft.value(),reg,manualChain.chainId,
            *manualGain.value(),gainBindings,ownedPrepared,true);
    QVERIFY(updatedProductTicket);
    QCOMPARE(updatedProductTicket.value()->replay->plan_id,
             nextProductPlan.plan_id);
    QVERIFY(!updatedProductTicket.value()->authorizes_m12_receipt());
    QVERIFY(!m::prepare_m12_product_prepublication_preview_ticket(
        *productBefore.value(),*productDraft.value(),reg,
        manualChain.chainId,*manualGain.value(),gainBindings,ownedPrepared,
        false));
    auto changedProductBefore=productBeforeDocument;
    changedProductBefore.displayName="changed while calculating";
    auto staleProductBefore=project::ProjectSnapshot::create(
        changedProductBefore);
    QVERIFY(staleProductBefore);
    QVERIFY(!m::prepare_m12_product_prepublication_preview_ticket(
        *staleProductBefore.value(),*productDraft.value(),reg,
        manualChain.chainId,*manualGain.value(),gainBindings,ownedPrepared,
        true));
    auto falsifiedDraft=*productDraft.value();
    falsifiedDraft.accepted_plan_checksum="different-accepted-checksum";
    QVERIFY(!m::prepare_m12_product_prepublication_preview_ticket(
        *productBefore.value(),falsifiedDraft,reg,
        manualChain.chainId,*manualGain.value(),gainBindings,ownedPrepared,
        true));
    // A manufactured M12 C++ candidate must not alter unrelated
    // Project fields or optional bytes under a genuine before-snapshot.
    auto alteredProductOutput=productDraft.value()->candidate_not_yet_committed.document();
    auto outputChoice=project::OpaqueJsonValue::parse(
        "{\"unauthorizedOutputChoice\":true}");
    QVERIFY(outputChoice);
    alteredProductOutput.output=*outputChoice.value();
    auto changedOutputSnapshot=project::ProjectSnapshot::create(alteredProductOutput);
    QVERIFY(changedOutputSnapshot);
    auto changedDraft=*productDraft.value();
    changedDraft.candidate_not_yet_committed=
        std::move(*changedOutputSnapshot.value());
    QVERIFY(!m::prepare_m12_product_prepublication_preview_ticket(
        *productBefore.value(),changedDraft,reg,
        manualChain.chainId,*manualGain.value(),gainBindings,ownedPrepared,
        true));
    auto changedMetadata=productDraft.value()->candidate_not_yet_committed.document();
    auto metaObj=QJsonDocument::fromJson(
        QByteArray::fromStdString(
            changedMetadata.referenceMatch.canonical_utf8())).object();
    metaObj.insert(QStringLiteral("foreignFutureState"),true);
    auto foreignJson=project::OpaqueJsonValue::parse(
        QJsonDocument(metaObj).toJson(QJsonDocument::Compact).toStdString());
    QVERIFY(foreignJson);
    changedMetadata.referenceMatch=*foreignJson.value();
    auto changedMetaSnapshot=project::ProjectSnapshot::create(changedMetadata);
    QVERIFY(changedMetaSnapshot);
    changedDraft=*productDraft.value();
    changedDraft.candidate_not_yet_committed=
        std::move(*changedMetaSnapshot.value());
    QVERIFY(!m::prepare_m12_product_prepublication_preview_ticket(
        *productBefore.value(),changedDraft,reg,
        manualChain.chainId,*manualGain.value(),gainBindings,ownedPrepared,
        true));
    const std::vector<project::OptionalEntry> extraneousFile{{
        "artifacts/unrelated.txt","text/plain",{std::byte{0x42}}}};
    auto foreignOptional=project::ProjectSnapshot::create(
        productDraft.value()->candidate_not_yet_committed.document(),
        extraneousFile);
    QVERIFY(foreignOptional);
    changedDraft=*productDraft.value();
    changedDraft.candidate_not_yet_committed=
        std::move(*foreignOptional.value());
    QVERIFY(!m::prepare_m12_product_prepublication_preview_ticket(
        *productBefore.value(),changedDraft,reg,
        manualChain.chainId,*manualGain.value(),gainBindings,ownedPrepared,
        true));
    // A half-persisted old plan with metadata but no linked segment
    // cannot be treated as a fresh unowned project.
    auto orphanBefore=productBeforeDocument;
    orphanBefore.referenceMatch=
        productDraft.value()->candidate_not_yet_committed.document().
            referenceMatch;
    auto orphanSnapshot=project::ProjectSnapshot::create(orphanBefore);
    QVERIFY(orphanSnapshot);
    auto orphanDraft=*productDraft.value();
    orphanDraft.exact_precommit_project=*orphanSnapshot.value();
    QVERIFY(!m::prepare_m12_product_prepublication_preview_ticket(
        *orphanSnapshot.value(),orphanDraft,reg,
        manualChain.chainId,*manualGain.value(),gainBindings,ownedPrepared,
        true));
    // Even a syntactically valid existing REF graph belongs to its
    // original controller. A different controller cannot take it over.
    auto foreignOwnerBefore=
        productDraft.value()->candidate_not_yet_committed.document();
    auto oldOwnerMeta=QJsonDocument::fromJson(QByteArray::fromStdString(
        foreignOwnerBefore.referenceMatch.canonical_utf8())).object();
    auto oldOwnerRecord=oldOwnerMeta.value(
        QStringLiteral("materialization")).toObject();
    oldOwnerRecord.insert(QStringLiteral("controllerId"),
        QStringLiteral("cf000000-0000-4000-8000-000000000099"));
    oldOwnerMeta.insert(QStringLiteral("materialization"),oldOwnerRecord);
    auto otherOwnerJson=project::OpaqueJsonValue::parse(
        QJsonDocument(oldOwnerMeta).toJson(
            QJsonDocument::Compact).toStdString());
    QVERIFY(otherOwnerJson);
    foreignOwnerBefore.referenceMatch=*otherOwnerJson.value();
    auto foreignOwnerSnapshot=project::ProjectSnapshot::create(
        foreignOwnerBefore);
    QVERIFY(foreignOwnerSnapshot);
    auto ownerHijackDraft=*productDraft.value();
    ownerHijackDraft.exact_precommit_project=*foreignOwnerSnapshot.value();
    QVERIFY(!m::prepare_m12_product_prepublication_preview_ticket(
        *foreignOwnerSnapshot.value(),ownerHijackDraft,reg,
        manualChain.chainId,*manualGain.value(),gainBindings,ownedPrepared,
        true));
    auto changedManualBindings=gainBindings;
    changedManualBindings.front().parameters=
        *dsp::GainParameters::create(-8.0).value();
    QVERIFY(!m::prepare_m12_product_prepublication_preview_ticket(
        *productBefore.value(),*productDraft.value(),reg,
        manualChain.chainId,*manualGain.value(),changedManualBindings,
        ownedPrepared,true));
    // A successful preflight remains just a worker candidate: real Qt
    // installation, persistent commit/readback and M12 receipt are
    // separate transactions, and M13 has not run.
    rgsml::app::MasteringPreviewController::PreviewJob validJob{
        1U,ownedPrepared,*manualGain.value(),gainBindings,reg,{}};
    const auto ownedWorkerRender=m::render_m12_exact_product_preview_job(
        *workerTicket.value(),validJob,*maxBlock.value());
    QVERIFY(ownedWorkerRender);
    const auto combinedWorkerRender=m::render_m12_exact_product_preview_job(
        *combinedTicket.value(),validJob,*maxBlock.value());
    QVERIFY(combinedWorkerRender);
    const auto combinedPCM=combinedWorkerRender.value()->view();
    const auto separatePCM=ownedWorkerRender.value()->view();
    QCOMPARE(combinedPCM.absolute_range(),separatePCM.absolute_range());
    QCOMPARE(combinedPCM.format(),separatePCM.format());
    for(std::size_t ch=0;ch<combinedPCM.format().channel_count();++ch) {
        const auto actual=combinedPCM.channel(ch);
        const auto original=separatePCM.channel(ch);
        QVERIFY(actual&&original);
        QCOMPARE(actual.value()->size(),original.value()->size());
        for(std::size_t i=0;i<actual.value()->size();++i)
            QCOMPARE(std::bit_cast<std::uint64_t>((*actual.value())[i]),
                     std::bit_cast<std::uint64_t>((*original.value())[i]));
    }
    QCOMPARE(ownedWorkerRender.value()->view().frame_count().value(),
             std::int64_t{48000});
    QCOMPARE(workerTicket.value()->exact_manual_chain_revision,
             manualGain.value()->revision());
    // Same DSP nodes and parameters are NOT sufficient when the
    // underlying MANUAL chain's structural revision has advanced.
    std::vector<dsp::ModuleInstance> identicalNodes{
        manualGain.value()->instances().begin(),
        manualGain.value()->instances().end()};
    auto revisedChain=dsp::ProcessingChain::restore(
        *reg,manualGain.value()->context(),
        manualGain.value()->revision()+1U,std::move(identicalNodes));
    QVERIFY(revisedChain);
    QVERIFY(!m::prepare_m12_immutable_product_preview_ticket(
        ownedReplayPlan,ownedPrepared,
        *revisedChain.value(),gainBindings));
    auto revisedOnly=validJob;
    revisedOnly.chain=*revisedChain.value();
    QVERIFY(!m::render_m12_exact_product_preview_job(
        *workerTicket.value(),revisedOnly,*maxBlock.value()));
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
    QVERIFY(m::m12_native_stage_witness_matches_replay_plan(
        *p2Plan.value(),completeStageWitness));
    auto forgedP2Witness=completeStageWitness;
    forgedP2Witness.user_terminal_p2.front().type_id="forged-limiter";
    QVERIFY(!m::m12_native_stage_witness_matches_replay_plan(
        *p2Plan.value(),forgedP2Witness));
    QCOMPARE(p2RealReplay.value()->signatures().back().type_id,
             std::string("rgsml.dsp.streaming-true-peak-limiter"));
    QCOMPARE(p2RealReplay.value()->view().frame_count().value(),
             std::int64_t{48000});
    QCOMPARE(p2RealReplay.value()->chain_revision(),
             p2Plan.value()->terminal_p2->revision());
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
    QCOMPARE(p2Ticket.value()->exact_manual_chain_revision,
             manualWithP2.value()->revision());
    QCOMPARE(p2Plan.value()->terminal_p2->revision(),
             manualWithP2.value()->revision());
    // The protected user P2 terminal shares MANUAL's structural epoch.
    // Even identical Ceiling/Drive/Margin must not bypass it after a
    // new MANUAL revision; no reoptimization or silent P2 replacement.
    std::vector<dsp::ModuleInstance> laterP2Nodes{
        manualWithP2.value()->instances().begin(),
        manualWithP2.value()->instances().end()};
    auto laterP2Chain=dsp::ProcessingChain::restore(
        *optRegistry,manualWithP2.value()->context(),
        manualWithP2.value()->revision()+1U,std::move(laterP2Nodes));
    QVERIFY(laterP2Chain);
    QVERIFY(!m::prepare_m12_immutable_product_preview_ticket(
        p2SharedPlan,ownedPrepared,
        *laterP2Chain.value(),manualP2Bindings));
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
    // A REF_LINKED segment disguised as a different processing stage
    // cannot coexist with a freshly materialized current-controller REF.
    auto foreignStageDoc=doc;
    project::Chain foreignLinked;
    foreignLinked.chainId=uuid(
        "b7000000-0000-4000-8000-000000000009");
    foreignLinked.stage="CONDITIONING";
    foreignLinked.segment="REF_LINKED";
    foreignStageDoc.chains.push_back(foreignLinked);
    const auto foreignStageSnapshot=
        project::ProjectSnapshot::create(foreignStageDoc);
    if(foreignStageSnapshot) {
        QVERIFY(!m::prepare_exact_m12_project_transaction(
            *foreignStageSnapshot.value(),*carrier.value(),*stage.value(),
            identity));
    }
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
    // A genuine UPDATE can change DSP parameters without losing the
    // same semantic identity, instance ID, or protected MANUAL snapshot.
    auto adjustedNodes=nodes;
    auto adjustedTransient=dsp::TransientShaperParameters::create(4.0,-2.0);
    QVERIFY(adjustedTransient);
    adjustedNodes[1].exact_m10_winning_parameters=*adjustedTransient.value();
    // An immutable plan cannot carry retuned parameters under its old
    // plan revision/checksum. Refuse such a forged M10/M12 handoff.
    auto staleAdjustedStage=m::stage_exact_m12_native_ref_linked(
        accepted(),*manual.value(),none,reg,adjustedNodes);
    QVERIFY(staleAdjustedStage);
    auto staleAdjustedCarrier=m::encode_exact_ref_linked_project_carrier(
        *staleAdjustedStage.value(),refChainId);
    QVERIFY(staleAdjustedCarrier);
    QVERIFY(!m::prepare_exact_m12_project_transaction(
        transaction.value()->candidate_not_yet_committed,
        *staleAdjustedCarrier.value(),*staleAdjustedStage.value(),update));
    auto nextPlan=accepted();
    ++nextPlan.exact_accepted_plan_revision;
    nextPlan.exact_plan_content_checksum="full-current-plan-sha-v2";
    nextPlan.exact_m10_result_and_topology_checksum="full-m10-winning-sha-v2";
    // Changing checksum alone, while reusing the existing planId and
    // revision, is still an invalid immutable-plan transition.
    auto badRevisionPlan=nextPlan;
    --badRevisionPlan.exact_accepted_plan_revision;
    auto badRevisionStage=m::stage_exact_m12_native_ref_linked(
        badRevisionPlan,*manual.value(),none,reg,adjustedNodes);
    QVERIFY(badRevisionStage);
    auto badRevisionCarrier=m::encode_exact_ref_linked_project_carrier(
        *badRevisionStage.value(),refChainId);
    QVERIFY(badRevisionCarrier);
    QVERIFY(!m::prepare_exact_m12_project_transaction(
        transaction.value()->candidate_not_yet_committed,
        *badRevisionCarrier.value(),*badRevisionStage.value(),update));
    auto adjustedStage=m::stage_exact_m12_native_ref_linked(
        nextPlan,*manual.value(),none,reg,adjustedNodes);
    QVERIFY(adjustedStage);
    auto adjustedCarrier=m::encode_exact_ref_linked_project_carrier(
        *adjustedStage.value(),refChainId);
    QVERIFY(adjustedCarrier);
    const auto adjusted=m::prepare_exact_m12_project_transaction(
        transaction.value()->candidate_not_yet_committed,
        *adjustedCarrier.value(),*adjustedStage.value(),update);
    QVERIFY(adjusted);
    const auto& beforeUpdate=proposed.chains.back().modules[1];
    const auto& afterUpdate=adjusted.value()->
        candidate_not_yet_committed.document().chains.back().modules[1];
    QCOMPARE(afterUpdate.instanceId,beforeUpdate.instanceId);
    QCOMPARE(afterUpdate.semanticNodeId,beforeUpdate.semanticNodeId);
    QVERIFY(afterUpdate.parameters.canonical_utf8()!=
            beforeUpdate.parameters.canonical_utf8());
    QVERIFY(m::exact_project_chain_equal(
        adjusted.value()->candidate_not_yet_committed.document().chains.front(),
        manualChain));
    // P02/P04: UPDATE retains both semantic and instance identities.
    // Reusing an old instance for a different semantic node is a
    // forbidden disguised REPLACE, even with the same type and payload.
    auto stolenConcrete=nodes;
    stolenConcrete.front().semantic_node_id=uuid(
        "b4000000-0000-4000-8000-000000000099");
    auto stolenStage=m::stage_exact_m12_native_ref_linked(
        nextPlan,*manual.value(),none,reg,stolenConcrete);
    QVERIFY(stolenStage);
    auto stolenCarrier=m::encode_exact_ref_linked_project_carrier(
        *stolenStage.value(),refChainId);
    QVERIFY(stolenCarrier);
    QVERIFY(!m::prepare_exact_m12_project_transaction(
        transaction.value()->candidate_not_yet_committed,
        *stolenCarrier.value(),*stolenStage.value(),update));
    // Conversely, re-allocating a compatible UPDATE's instance is
    // not a permissible identity rewrite: the stable ID must survive.
    auto recreatedSameSemantic=nodes;
    recreatedSameSemantic.front().allocated_instance_id=id(
        "b3000000-0000-4000-8000-000000000099");
    auto recreatedStage=m::stage_exact_m12_native_ref_linked(
        nextPlan,*manual.value(),none,reg,recreatedSameSemantic);
    QVERIFY(recreatedStage);
    auto recreatedCarrier=m::encode_exact_ref_linked_project_carrier(
        *recreatedStage.value(),refChainId);
    QVERIFY(recreatedCarrier);
    QVERIFY(!m::prepare_exact_m12_project_transaction(
        transaction.value()->candidate_not_yet_committed,
        *recreatedCarrier.value(),*recreatedStage.value(),update));
    // A genuinely new semantic node carrying a fresh concrete ID may
    // enter only as an explicit replacement in a newly accepted plan.
    recreatedSameSemantic.front().semantic_node_id=uuid(
        "b4000000-0000-4000-8000-000000000099");
    auto freshStage=m::stage_exact_m12_native_ref_linked(
        nextPlan,*manual.value(),none,reg,recreatedSameSemantic);
    QVERIFY(freshStage);
    auto freshCarrier=m::encode_exact_ref_linked_project_carrier(
        *freshStage.value(),refChainId);
    QVERIFY(freshCarrier);
    QVERIFY(m::prepare_exact_m12_project_transaction(
        transaction.value()->candidate_not_yet_committed,
        *freshCarrier.value(),*freshStage.value(),update));
    // A self-consistent caller-supplied before-snapshot cannot launder
    // a corrupted old REF owner into an accepted current-controller UPDATE.
    auto foreignPrior=proposed;
    foreignPrior.chains.back().modules.front().owner="USER";
    auto foreignSnapshot=project::ProjectSnapshot::create(foreignPrior);
    QVERIFY(foreignSnapshot);
    auto foreignExpected=update;
    foreignExpected.exact_existing_linked_snapshot=
        foreignPrior.chains.back();
    QVERIFY(!m::prepare_exact_m12_project_transaction(
        *foreignSnapshot.value(),*carrier.value(),*stage.value(),
        foreignExpected));
    // Duplicate old semantic identities are likewise not a valid
    // replaceable linked segment, even with distinct concrete IDs.
    auto duplicatePrior=proposed;
    duplicatePrior.chains.back().modules[1].semanticNodeId=
        duplicatePrior.chains.back().modules[0].semanticNodeId;
    auto duplicateSnapshot=project::ProjectSnapshot::create(duplicatePrior);
    QVERIFY(duplicateSnapshot);
    auto duplicateExpected=update;
    duplicateExpected.exact_existing_linked_snapshot=
        duplicatePrior.chains.back();
    QVERIFY(!m::prepare_exact_m12_project_transaction(
        *duplicateSnapshot.value(),*carrier.value(),*stage.value(),
        duplicateExpected));
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
    // RESUME is a guarded transition, not a generic command that
    // happens to set every controllerSuspended bit to false.
    QVERIFY(!m::prepare_m12_owned_controller_suspension(
        transaction.value()->candidate_not_yet_committed,
        carrier.value()->exact_owner_controller,
        carrier.value()->native_project_chain.chainId,
        m::ControllerSuspensionCause::current_amount_restored,true));
    auto incompleteSuspension=suspended;
    incompleteSuspension.chains.back().modules.front().
        controllerSuspended=false;
    auto incompleteSnapshot=project::ProjectSnapshot::create(
        incompleteSuspension);
    QVERIFY(incompleteSnapshot);
    QVERIFY(!m::prepare_m12_owned_controller_suspension(
        *incompleteSnapshot.value(),carrier.value()->exact_owner_controller,
        carrier.value()->native_project_chain.chainId,
        m::ControllerSuspensionCause::current_amount_restored,true));
    auto unresolvedSuspension=suspended;
    auto unresolvedMeta=QJsonDocument::fromJson(QByteArray::fromStdString(
        unresolvedSuspension.referenceMatch.canonical_utf8())).object();
    auto unresolvedRecord=unresolvedMeta.value(
        QStringLiteral("materialization")).toObject();
    unresolvedRecord.insert(QStringLiteral("freshnessState"),
                            QStringLiteral("UNRESOLVED_DEPENDENCIES"));
    unresolvedMeta.insert(QStringLiteral("materialization"),
                          unresolvedRecord);
    auto unresolvedJson=project::OpaqueJsonValue::parse(
        QJsonDocument(unresolvedMeta).toJson(
            QJsonDocument::Compact).toStdString());
    QVERIFY(unresolvedJson);
    unresolvedSuspension.referenceMatch=*unresolvedJson.value();
    auto unresolvedSnapshot=project::ProjectSnapshot::create(
        unresolvedSuspension);
    QVERIFY(unresolvedSnapshot);
    QVERIFY(!m::prepare_m12_owned_controller_suspension(
        *unresolvedSnapshot.value(),carrier.value()->exact_owner_controller,
        carrier.value()->native_project_chain.chainId,
        m::ControllerSuspensionCause::current_amount_restored,true));
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
    // P04: even Amount-zero must not change a stale/foreign stored
    // REF graph, since restoring positive Amount would revive it.
    auto obsoleteRevision=proposed;
    auto oldMeta=QJsonDocument::fromJson(QByteArray::fromStdString(
        obsoleteRevision.referenceMatch.canonical_utf8())).object();
    auto oldRecord=oldMeta.value(QStringLiteral("materialization")).toObject();
    oldRecord.insert(QStringLiteral("linkedChainRevision"),
                     QStringLiteral("999"));
    oldMeta.insert(QStringLiteral("materialization"),oldRecord);
    auto oldJson=project::OpaqueJsonValue::parse(
        QJsonDocument(oldMeta).toJson(QJsonDocument::Compact).toStdString());
    QVERIFY(oldJson);
    obsoleteRevision.referenceMatch=*oldJson.value();
    auto obsoleteSnapshot=project::ProjectSnapshot::create(obsoleteRevision);
    QVERIFY(obsoleteSnapshot);
    QVERIFY(!m::prepare_m12_owned_controller_suspension(
        *obsoleteSnapshot.value(),carrier.value()->exact_owner_controller,
        carrier.value()->native_project_chain.chainId,
        m::ControllerSuspensionCause::amount_zero,true));

    auto duplicateSuspension=proposed;
    duplicateSuspension.chains.back().modules[1].semanticNodeId=
        duplicateSuspension.chains.back().modules[0].semanticNodeId;
    auto duplicatedSnapshot=project::ProjectSnapshot::create(
        duplicateSuspension);
    QVERIFY(duplicatedSnapshot);
    QVERIFY(!m::prepare_m12_owned_controller_suspension(
        *duplicatedSnapshot.value(),carrier.value()->exact_owner_controller,
        carrier.value()->native_project_chain.chainId,
        m::ControllerSuspensionCause::amount_zero,true));

    auto doubleLinked=proposed;
    project::Chain extra;
    extra.chainId=uuid("cf000000-0000-4000-8000-000000000001");
    extra.stage="MASTER";
    extra.segment="REF_LINKED";
    doubleLinked.chains.push_back(extra);
    auto doubleSnapshot=project::ProjectSnapshot::create(doubleLinked);
    QVERIFY(doubleSnapshot);
    QVERIFY(!m::prepare_m12_owned_controller_suspension(
        *doubleSnapshot.value(),carrier.value()->exact_owner_controller,
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
