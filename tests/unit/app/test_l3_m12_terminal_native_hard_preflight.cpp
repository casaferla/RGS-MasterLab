#include "internal/l3_m12_terminal_native_hard_preflight.hpp"
#include <rgsml/dsp/module_registry.hpp>
#include <QtTest/QTest>
#include <cstddef>
namespace rgsml::tests {
namespace {
namespace m=rgsml::app::l3_automatch::m12_frozen;
[[nodiscard]] dsp::ModuleInstanceId id() {
    auto uid=core::Uuid::parse(
        "d1000000-0000-4000-8000-000000000001");
    Q_ASSERT(uid);
    auto out=dsp::ModuleInstanceId::from_uuid(*uid.value());
    Q_ASSERT(out);return *out.value();
}
class TerminalM12SafetyTest final:public QObject {
    Q_OBJECT
private slots:
    void actualTerminalSingleLimiterExactFrozenCeiling();
    void missingBypassCeilingOrStaleOwnerIsHardVeto();
};
void TerminalM12SafetyTest::
actualTerminalSingleLimiterExactFrozenCeiling() {
    auto r=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(r);
    auto chain=dsp::ProcessingChain::create(*r.value(),{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::TERMINAL});
    QVERIFY(chain);
    const auto limiter=id();
    QVERIFY(chain.value()->add(
        limiter,"rgsml.dsp.true-peak-limiter",0));
    auto p=dsp::TruePeakLimiterParameters::create(-1.0);
    QVERIFY(p);
    const std::vector<dsp::ModuleExecutionBinding> binding{
        {limiter,*p.value()}};
    const auto v=m::verify_current_m12_terminal_limiter_composition(
        *chain.value(),binding,-1.0,true);
    QCOMPARE(v.reason,m::TerminalNativeReason::pass);
    QVERIFY(v.valid_existing_limiter);
    QVERIFY(*v.valid_existing_limiter==limiter);
    QVERIFY(!v.authorizes_master_export());
}
void TerminalM12SafetyTest::
missingBypassCeilingOrStaleOwnerIsHardVeto() {
    auto r=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(r);
    auto chain=dsp::ProcessingChain::create(*r.value(),{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::TERMINAL});
    QVERIFY(chain);
    const auto limiter=id();
    QCOMPARE(m::verify_current_m12_terminal_limiter_composition(
        *chain.value(),{},-1.0,true).reason,
        m::TerminalNativeReason::
            missing_or_multiple_terminal_true_peak);
    QVERIFY(chain.value()->add(
        limiter,"rgsml.dsp.true-peak-limiter",0));
    auto p=dsp::TruePeakLimiterParameters::create(-1.0);
    QVERIFY(p);
    const std::vector<dsp::ModuleExecutionBinding> binding{
        {limiter,*p.value()}};
    QCOMPARE(m::verify_current_m12_terminal_limiter_composition(
        *chain.value(),binding,0.0,true).reason,
        m::TerminalNativeReason::incompatible_terminal_ceiling);
    QCOMPARE(m::verify_current_m12_terminal_limiter_composition(
        *chain.value(),binding,-1.0,false).reason,
        m::TerminalNativeReason::stale_or_unverified_owner);
    QVERIFY(chain.value()->set_user_bypass(limiter,true));
    QCOMPARE(m::verify_current_m12_terminal_limiter_composition(
        *chain.value(),binding,-1.0,true).reason,
        m::TerminalNativeReason::
            bypassed_or_suspended_terminal_safety);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::TerminalM12SafetyTest)
#include "test_l3_m12_terminal_native_hard_preflight.moc"
