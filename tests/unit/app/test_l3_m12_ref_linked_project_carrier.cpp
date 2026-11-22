#include "internal/l3_m12_ref_linked_project_carrier.hpp"
#include "internal/l3_m12_project_transaction_draft.hpp"
#include "internal/l3_m12_owned_controller_suspension.hpp"
#include <rgsml/dsp/true_peak_limiter_parameters.hpp>
#include "canonical_json.hpp"
#include <rgsml/project/project_snapshot.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>
#include <rgsml/dsp/dynamic_eq_parameters.hpp>
#include <rgsml/dsp/transient_shaper_parameters.hpp>
#include <QtTest/QTest>
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
