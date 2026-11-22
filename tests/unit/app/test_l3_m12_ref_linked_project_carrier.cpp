#include "internal/l3_m12_ref_linked_project_carrier.hpp"
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
    project::Chain manualChain;
    manualChain.chainId=uuid("b8000000-0000-4000-8000-000000000001");
    manualChain.stage="MASTER";
    manualChain.segment="MANUAL";
    manualChain.revision=0;
    doc.pipeline.masteringChainId=manualChain.chainId;
    doc.chains.push_back(manualChain);
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
