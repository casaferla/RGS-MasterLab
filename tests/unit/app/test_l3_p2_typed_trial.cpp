#include "internal/l3_automatch_typed_p2_trial.hpp"
#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <QtTest/QTest>
#include <cstdint>
#include <memory>
#include <vector>

namespace rgsml::tests {
namespace {
namespace a=rgsml::app::l3_automatch;
[[nodiscard]] core::Uuid uuid(const char* text) {
    auto u=core::Uuid::parse(text);Q_ASSERT(u);return *u.value();
}
[[nodiscard]] dsp::ModuleInstanceId id(const char* text) {
    auto u=dsp::ModuleInstanceId::from_uuid(uuid(text));
    Q_ASSERT(u);return *u.value();
}
class L3P2TypedTrialTest final:public QObject {
    Q_OBJECT
private slots:
    void ceilingAndDriveAreRealTypedIndependentCandidates();
    void wrongBindingBypassOrGraphMustRefuse();
};
void L3P2TypedTrialTest::
ceilingAndDriveAreRealTypedIndependentCandidates() {
    auto base=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(base);
    auto opt=dsp::ModuleRegistry::with_streaming_true_peak_preview(
        std::move(*base.value()));
    QVERIFY(opt);
    auto chain=dsp::ProcessingChain::create(*opt.value(),{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(chain);
    const auto p2=id("dd000000-0000-4000-8000-000000000001");
    QVERIFY(chain.value()->add(p2,
        "rgsml.dsp.streaming-true-peak-limiter",0));
    auto original=dsp::StreamingTruePeakLimiterParameters::create(-1.0,0.0,5);
    QVERIFY(original);
    const std::vector<dsp::ModuleExecutionBinding> binds{
        {p2,*original.value()}};
    const auto ceiling=a::make_p2_scalar_trial(
        *chain.value(),binds,p2,a::P2ScalarCoordinate::ceiling_db_tp,
        -2.0,"exact-m10-ceiling");
    QVERIFY(ceiling);
    QVERIFY(ceiling->still_matches(*chain.value(),binds));
    const auto* c=ceiling->chosen_p2_parameters();
    QVERIFY(c);
    QCOMPARE(c->ceiling_db_tp(),-2.0);
    QCOMPARE(c->drive_db(),0.0);
    QCOMPARE(c->margin_percent(),5);
    QCOMPARE(binds.front().parameters,
             dsp::ModuleParameterPayload{*original.value()});
    const auto drive=a::make_p2_scalar_trial(
        *chain.value(),binds,p2,a::P2ScalarCoordinate::drive_db,
        6.0,"exact-m10-drive");
    QVERIFY(drive);
    const auto* d=drive->chosen_p2_parameters();
    QVERIFY(d);
    QCOMPARE(d->drive_db(),6.0);
    QCOMPARE(d->ceiling_db_tp(),-1.0);
    QCOMPARE(d->margin_percent(),5);
    a::Ticket ticket{1U,0U,"exact-m10-drive"};
    QVERIFY(drive->matches_ticket(ticket));
    ticket.typed_dsp_fingerprint="wrong";
    QVERIFY(!drive->matches_ticket(ticket));
    // Same setting is not a new render attempt.
    QVERIFY(!a::make_p2_scalar_trial(
        *chain.value(),binds,p2,a::P2ScalarCoordinate::drive_db,
        0.0,"same"));
    QVERIFY(!a::make_p2_scalar_trial(
        *chain.value(),binds,p2,a::P2ScalarCoordinate::drive_db,
        25.0,"outside"));
    QVERIFY(!a::make_p2_scalar_trial(
        *chain.value(),binds,p2,a::P2ScalarCoordinate::ceiling_db_tp,
        0.1,"outside"));
    QVERIFY(!a::make_p2_scalar_trial(
        *chain.value(),binds,p2,a::P2ScalarCoordinate::drive_db,
        5.0,""));
    const auto joint=a::make_p2_joint_trial(
        *chain.value(),binds,p2,-3.0,9.0,"exact-m10-joint");
    QVERIFY(joint);
    QVERIFY(joint->still_matches(*chain.value(),binds));
    const auto* chosen=joint->chosen_p2_parameters();
    QVERIFY(chosen);
    QCOMPARE(chosen->ceiling_db_tp(),-3.0);
    QCOMPARE(chosen->drive_db(),9.0);
    QCOMPARE(chosen->margin_percent(),5);
    a::Ticket jointTicket{2U,1U,"exact-m10-joint"};
    QVERIFY(joint->matches_ticket(jointTicket));
    QVERIFY(!a::make_p2_joint_trial(
        *chain.value(),binds,p2,-1.0,0.0,"same"));
    QVERIFY(!a::make_p2_joint_trial(
        *chain.value(),binds,p2,-13.0,9.0,"out-of-range"));

}
void L3P2TypedTrialTest::wrongBindingBypassOrGraphMustRefuse() {
    auto base=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(base);
    auto opt=dsp::ModuleRegistry::with_streaming_true_peak_preview(
        std::move(*base.value()));
    QVERIFY(opt);
    auto chain=dsp::ProcessingChain::create(*opt.value(),{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(chain);
    const auto p2=id("dd000000-0000-4000-8000-000000000002");
    const auto gainId=id("dd000000-0000-4000-8000-000000000003");
    QVERIFY(chain.value()->add(p2,
        "rgsml.dsp.streaming-true-peak-limiter",0));
    auto old=dsp::StreamingTruePeakLimiterParameters::create_default();
    QVERIFY(old);
    const std::vector<dsp::ModuleExecutionBinding> p2only{{p2,*old.value()}};
    auto trial=a::make_p2_scalar_trial(
        *chain.value(),p2only,p2,a::P2ScalarCoordinate::drive_db,
        2.0,"right");
    QVERIFY(trial);
    // The Product descriptor requires Gain before P2.  Adding Gain
    // after P2 is invalid; create a genuinely legal changed graph.
    QVERIFY(!chain.value()->add(gainId,"rgsml.dsp.gain",1));
    QVERIFY(chain.value()->add(gainId,"rgsml.dsp.gain",0));
    QVERIFY(!chain.value()->move(p2,0));
    QVERIFY(!trial->still_matches(*chain.value(),p2only));
    QVERIFY(!a::make_p2_scalar_trial(
        *chain.value(),p2only,p2,a::P2ScalarCoordinate::drive_db,
        2.0,"old-bindings"));
    auto gain=dsp::GainParameters::create(0.0);
    QVERIFY(gain);
    // A syntactically valid binding vector with the wrong order must
    // not be accepted against a legally ordered current Product graph.
    const std::vector<dsp::ModuleExecutionBinding> wrongOrder{
        {p2,*old.value()},{gainId,*gain.value()}};
    QVERIFY(!a::make_p2_scalar_trial(
        *chain.value(),wrongOrder,p2,a::P2ScalarCoordinate::drive_db,
        2.0,"wrong-bindings"));
    const std::vector<dsp::ModuleExecutionBinding> ordered{
        {gainId,*gain.value()},{p2,*old.value()}};
    const auto legal=a::make_p2_scalar_trial(
        *chain.value(),ordered,p2,a::P2ScalarCoordinate::drive_db,
        2.0,"valid-p2-final");
    QVERIFY(legal);
    QVERIFY(legal->still_matches(*chain.value(),ordered));
    QVERIFY(chain.value()->set_user_bypass(p2,true));
    QVERIFY(!legal->still_matches(*chain.value(),ordered));
    QVERIFY(!a::make_p2_scalar_trial(
        *chain.value(),ordered,p2,a::P2ScalarCoordinate::drive_db,
        2.0,"bypass"));
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::L3P2TypedTrialTest)
#include "test_l3_p2_typed_trial.moc"
