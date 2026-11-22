#include "internal/l3_m12_ref_linked_native_stage.hpp"
#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>
#include <rgsml/dsp/stereo_ms_parameters.hpp>
#include <QtTest/QTest>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace rgsml::tests {
namespace {
namespace a=rgsml::app::l3_automatch;
namespace m=a::m12_frozen;
[[nodiscard]] core::Uuid uuid(const char* text) {
    auto x=core::Uuid::parse(text);Q_ASSERT(x);return *x.value();
}
[[nodiscard]] dsp::ModuleInstanceId id(const char* text) {
    auto x=dsp::ModuleInstanceId::from_uuid(uuid(text));
    Q_ASSERT(x);return *x.value();
}
[[nodiscard]] m::AcceptedPlanAndOwner approved() {
    return {
        uuid("a1000000-0000-4000-8000-000000000011"),
        uuid("a1000000-0000-4000-8000-000000000012"),
        "actual-plan-canonical-sha256",
        "actual-original-winner-topology-sha256",
        "actual-manual-fixed-and-protected-sha256",
        "actual-source-ref-amount-scope-revision",
        7,true,true,true,false};
}
class M12NativeStageTest final:public QObject {
    Q_OBJECT
private slots:
    void realRegistryStagesTypedEqAndProtectsManualInputGain();
    void forbiddenInputGainCollisionsStalenessAndUnavailableAreAtomic();
    void noCorrectionEmptyArtisticSectionMustBeExplicit();
    void linkedSuspensionAndUserBypassAreIndependent();
};
void M12NativeStageTest::
realRegistryStagesTypedEqAndProtectsManualInputGain() {
    auto base=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(base);
    auto registry=std::make_shared<const dsp::ModuleRegistry>(
        std::move(*base.value()));
    auto manual=dsp::ProcessingChain::create(*registry,{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(manual);
    const auto gainId=id("a2000000-0000-4000-8000-000000000011");
    QVERIFY(manual.value()->add(gainId,"rgsml.dsp.gain",0));
    auto gain=dsp::GainParameters::create(0.0);QVERIFY(gain);
    const std::vector<dsp::ModuleExecutionBinding> protectedParams{
        {gainId,*gain.value()}};
    auto eq=dsp::ParametricEqParameters::create_legacy_default();
    QVERIFY(eq);
    const auto linkId=id("a3000000-0000-4000-8000-000000000011");
    const auto sem=uuid("a4000000-0000-4000-8000-000000000011");
    const std::vector<m::ExactLinkedNode> nodes{
        {linkId,sem,"rgsml.dsp.parametric-eq",*eq.value(),
         true,false,false,false}};
    auto staged=m::stage_exact_m12_native_ref_linked(
        approved(),*manual.value(),protectedParams,registry,nodes);
    QVERIFY(staged);
    QCOMPARE(staged.value()->chain.context().segment,
             dsp::ChainSegment::REF_LINKED);
    QCOMPARE(staged.value()->chain.revision(),std::uint64_t{7});
    QCOMPARE(staged.value()->chain.instances().size(),std::size_t{1});
    QCOMPARE(staged.value()->chain.instances()[0].instance_id(),linkId);
    QCOMPARE(staged.value()->chain.instances()[0].semantic_node_id(),sem);
    QCOMPARE(staged.value()->chain.instances()[0].provenance(),
             dsp::ModuleProvenance::REFERENCE_MATCH);
    QCOMPARE(staged.value()->exact_bindings.size(),std::size_t{1});
    QVERIFY(std::get_if<dsp::ParametricEqParameters>(
        &staged.value()->exact_bindings[0].parameters));
    QCOMPARE(manual.value()->revision(),std::uint64_t{1});
    QCOMPARE(manual.value()->instances()[0].instance_id(),gainId);
    QVERIFY(!staged.value()->authorizes_project_mutation());
    QVERIFY(!staged.value()->authorizes_m13_verified());
}
void M12NativeStageTest::
forbiddenInputGainCollisionsStalenessAndUnavailableAreAtomic() {
    auto base=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(base);
    auto registry=std::make_shared<const dsp::ModuleRegistry>(
        std::move(*base.value()));
    auto manual=dsp::ProcessingChain::create(*registry,{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(manual);
    auto gain=dsp::GainParameters::create(0.0);
    auto eq=dsp::ParametricEqParameters::create_legacy_default();
    QVERIFY(gain&&eq);
    const auto manualId=id("a5000000-0000-4000-8000-000000000011");
    QVERIFY(manual.value()->add(manualId,"rgsml.dsp.gain",0));
    const std::vector<dsp::ModuleExecutionBinding> protectedParams{
        {manualId,*gain.value()}};
    m::ExactLinkedNode node{
        manualId,uuid("a6000000-0000-4000-8000-000000000011"),
        "rgsml.dsp.parametric-eq",*eq.value()};
    QVERIFY(!m::stage_exact_m12_native_ref_linked(
        approved(),*manual.value(),protectedParams,registry,{node}));
    node.allocated_instance_id=
        id("a5000000-0000-4000-8000-000000000022");
    node.exact_registered_type_id="rgsml.dsp.gain";
    node.exact_m10_winning_parameters=*gain.value();
    QVERIFY(!m::stage_exact_m12_native_ref_linked(
        approved(),*manual.value(),protectedParams,registry,{node}));
    node.exact_registered_type_id="rgsml.dsp.parametric-eq";
    node.exact_m10_winning_parameters=*eq.value();
    auto stale=approved();
    stale.current_source_reference_owner_and_plan_attested=false;
    QVERIFY(!m::stage_exact_m12_native_ref_linked(
        stale,*manual.value(),protectedParams,registry,{node}));
    stale=approved();
    stale.anchor_and_separate_terminal_limiter_admissible=false;
    QVERIFY(!m::stage_exact_m12_native_ref_linked(
        stale,*manual.value(),protectedParams,registry,{node}));
    QCOMPARE(manual.value()->revision(),std::uint64_t{1});
    QCOMPARE(manual.value()->instances().size(),std::size_t{1});
}
void M12NativeStageTest::
noCorrectionEmptyArtisticSectionMustBeExplicit() {
    auto base=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(base);
    auto registry=std::make_shared<const dsp::ModuleRegistry>(
        std::move(*base.value()));
    auto manual=dsp::ProcessingChain::create(*registry,{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(manual);
    auto yes=approved();yes.no_correction_required=true;
    const std::vector<dsp::ModuleExecutionBinding> none;
    auto empty=m::stage_exact_m12_native_ref_linked(
        yes,*manual.value(),none,registry,{});
    QVERIFY(empty);
    QVERIFY(empty.value()->chain.instances().empty());
    auto no=approved();
    QVERIFY(!m::stage_exact_m12_native_ref_linked(
        no,*manual.value(),none,registry,{}));
}
void M12NativeStageTest::
linkedSuspensionAndUserBypassAreIndependent() {
    auto base=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(base);
    auto registry=std::make_shared<const dsp::ModuleRegistry>(
        std::move(*base.value()));
    auto manual=dsp::ProcessingChain::create(*registry,{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(manual);
    auto eq=dsp::ParametricEqParameters::create_legacy_default();
    QVERIFY(eq);
    m::ExactLinkedNode node{
        id("a7000000-0000-4000-8000-000000000011"),
        uuid("a8000000-0000-4000-8000-000000000011"),
        "rgsml.dsp.parametric-eq",*eq.value(),
        true,false,true,false};
    const std::vector<dsp::ModuleExecutionBinding> none;
    auto staged=m::stage_exact_m12_native_ref_linked(
        approved(),*manual.value(),none,registry,{node});
    QVERIFY(staged);
    auto& linked=staged.value()->chain;
    QVERIFY(!linked.instances()[0].active());
    QVERIFY(!linked.instances()[0].user_bypass());
    QVERIFY(linked.instances()[0].controller_suspended());
    QVERIFY(linked.set_user_bypass(node.allocated_instance_id,true));
    QVERIFY(linked.instances()[0].user_bypass());
    QVERIFY(linked.instances()[0].controller_suspended());
    QVERIFY(!linked.instances()[0].active());
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M12NativeStageTest)
#include "test_l3_m12_ref_linked_native_stage.moc"
