#include "internal/l3_automatch_typed_p2_trial.hpp"
#include "internal/l3_m10_p2_canonical_coordinate_port.hpp"
#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <QtTest/QTest>
#include <cstdint>
#include <memory>
#include <limits>
#include <optional>
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
    void frozenM10CanonicalPairDecodesAndRejectsInvalidInventory();
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
    const auto identity=a::make_p2_joint_trial(
        *chain.value(),binds,p2,-1.0,0.0,"same-but-other-dsp-varies",
        true);
    QVERIFY(identity);
    const auto* same=identity->chosen_p2_parameters();
    QVERIFY(same);
    QCOMPARE(*same,*original.value());
    QVERIFY(identity->still_matches(*chain.value(),binds));
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

void L3P2TypedTrialTest::
frozenM10CanonicalPairDecodesAndRejectsInvalidInventory() {
    namespace m10=a::m10_frozen;
    auto base=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(base);
    auto opt=dsp::ModuleRegistry::with_streaming_true_peak_preview(
        std::move(*base.value()));
    QVERIFY(opt);
    auto chain=dsp::ProcessingChain::create(*opt.value(),{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(chain);
    const auto gainId=id("de000000-0000-4000-8000-000000000001");
    const auto p2id=id("de000000-0000-4000-8000-000000000002");
    QVERIFY(chain.value()->add(gainId,"rgsml.dsp.gain",0));
    QVERIFY(chain.value()->add(
        p2id,"rgsml.dsp.streaming-true-peak-limiter",1));
    auto gain=dsp::GainParameters::create(0.0);
    auto p2=dsp::StreamingTruePeakLimiterParameters::create(-1.0,0.0,5);
    QVERIFY(gain&&p2);
    const std::vector<dsp::ModuleExecutionBinding> bindings{
        {gainId,*gain.value()},{p2id,*p2.value()}};
    const m10::ExactP2CoordinateNames names{
        {0x70,0x32},{0x43},{0x44}};
    const std::vector<m10::Coordinate> inventory{
        {{0x70,0x32},{0x44},0.5},
        {{0x70,0x33},{0x41},0.3},
        {{0x70,0x32},{0x43},0.75}};
    const std::vector<double> proposed{0.625,0.4,0.25};
    std::size_t calls=0;
    const auto authorized_m09_inverse=[&](
        const m10::Coordinate& c,double u)->std::optional<double> {
        ++calls;
        // A fixture mapping, NOT a production normalization policy.
        if(c.stable_parameter_id==names.ceiling_parameter_id)
            return -12.0+u*12.0;
        if(c.stable_parameter_id==names.drive_parameter_id)
            return -24.0+u*48.0;
        return std::nullopt;
    };
    auto bound=m10::bind_m10_p2_joint_coordinates(
        inventory,proposed,names,*chain.value(),bindings,p2id,
        "exact-current-m10-typed-ticket",authorized_m09_inverse);
    QVERIFY(bound);
    QCOMPARE(calls,std::size_t{2});
    QCOMPARE(bound->normalized_ceiling_u,0.25);
    QCOMPARE(bound->normalized_drive_u,0.625);
    const auto* typed=bound->typed.chosen_p2_parameters();
    QVERIFY(typed);
    QCOMPARE(typed->ceiling_db_tp(),-9.0);
    QCOMPARE(typed->drive_db(),6.0);
    QCOMPARE(typed->margin_percent(),5);
    QVERIFY(bound->typed.still_matches(*chain.value(),bindings));
    QVERIFY(!bound->authorizes_product_publication());
    const auto previous_calls=calls;
    auto missing=inventory;
    missing[2].stable_parameter_id={0x45};
    QVERIFY(!m10::bind_m10_p2_joint_coordinates(
        missing,proposed,names,*chain.value(),bindings,p2id,
        "ticket",authorized_m09_inverse));
    QCOMPARE(calls,previous_calls);
    auto duplicate=inventory;
    duplicate[1].semantic_node_id={0x70,0x32};
    duplicate[1].stable_parameter_id=names.ceiling_parameter_id;
    QVERIFY(!m10::bind_m10_p2_joint_coordinates(
        duplicate,proposed,names,*chain.value(),bindings,p2id,
        "ticket",authorized_m09_inverse));
    auto u_nan=proposed;
    u_nan[0]=std::numeric_limits<double>::quiet_NaN();
    QVERIFY(!m10::bind_m10_p2_joint_coordinates(
        inventory,u_nan,names,*chain.value(),bindings,p2id,
        "ticket",authorized_m09_inverse));
    auto out_of_range=proposed;
    out_of_range[2]=1.01;
    QVERIFY(!m10::bind_m10_p2_joint_coordinates(
        inventory,out_of_range,names,*chain.value(),bindings,p2id,
        "ticket",authorized_m09_inverse));
    QVERIFY(!m10::bind_m10_p2_joint_coordinates(
        inventory,proposed,names,*chain.value(),bindings,p2id,
        "",authorized_m09_inverse));
    QVERIFY(chain.value()->set_user_bypass(p2id,true));
    QVERIFY(!m10::bind_m10_p2_joint_coordinates(
        inventory,proposed,names,*chain.value(),bindings,p2id,
        "ticket",authorized_m09_inverse));
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::L3P2TypedTrialTest)
#include "test_l3_p2_typed_trial.moc"
