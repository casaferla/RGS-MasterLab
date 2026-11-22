#include "internal/l3_m12_ref_linked_project_carrier.hpp"
#include "internal/l3_m12_project_transaction_draft.hpp"
#include "internal/l3_m12_p2_persisted_sound_guard.hpp"
#include "internal/l3_m12_exact_postcommit_readback.hpp"
#include "internal/l3_m12_ref_linked_project_restore.hpp"
#include "internal/l3_m12_owned_controller_suspension.hpp"
#include <rgsml/dsp/true_peak_limiter_parameters.hpp>
#include "canonical_json.hpp"
#include <rgsml/project/project_snapshot.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>
#include <rgsml/dsp/dynamic_eq_parameters.hpp>
#include <rgsml/dsp/transient_shaper_parameters.hpp>
#include <QtTest/QTest>
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
    // Dynamic EQ / Transient Shaper / Parametric EQ are real
    // registered DSPs, not opaque plan-only placeholders. Rebuild
    // them from the actual persisted REF_LINKED project segment.
    const auto productReopen=m::decode_exact_m12_ref_linked_product_state(
        proposed,reg);
    QVERIFY(productReopen);
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
    // The opt-in runtime factory must still exist when reopened:
    // do not silently drop a required calculated module.
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
        "unrelated/file.txt","text/plain",{
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
