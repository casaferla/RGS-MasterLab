#include "internal/l3_m08_frozen_amount_target.hpp"
#include <QtTest/QTest>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string_view>

namespace rgsml::tests {
namespace {
namespace m08=rgsml::app::l3_automatch::m08_frozen;
namespace m07=rgsml::app::l3_automatch::m07_frozen;
[[nodiscard]] std::array<m08::DomainAmount,4> ordered(
    double t=1.0,double d=1.0,double s=1.0,double l=1.0) {
    return {{
        {m07::kDomainIds[0],t},{m07::kDomainIds[1],d},
        {m07::kDomainIds[2],s},{m07::kDomainIds[3],l}}};
}
class FrozenM08AmountTest final:public QObject {
    Q_OBJECT
private slots:
    void frozenFourDomainsAndAmountValidation();
    void canonicalDesiredEndpointAndInterior();
    void positiveEffectiveAmountUnderflowAndInvalidEvidence();
    void noClaimOfProductTargetOrM13();
};
void FrozenM08AmountTest::frozenFourDomainsAndAmountValidation() {
    auto a=m08::create_amount_config(0.5,ordered(0.8,0.4,0.1,0.0),1);
    QVERIFY(a.config);
    QCOMPARE(a.failure,m08::AmountFailure::none);
    QCOMPARE(a.config->independent_amount_revision,std::uint64_t{1});
    QCOMPARE(a.config->global,0.5);
    QCOMPARE(a.config->domains[0],0.8);
    QCOMPARE(a.config->domains[3],0.0);
    QVERIFY(!a.config->authorizes_persistence());
    const auto neg=m08::create_amount_config(-0.0,
        ordered(-0.0,1.0,0.0,1.0),5);
    QVERIFY(neg.config);
    QCOMPARE(std::bit_cast<std::uint64_t>(neg.config->global),
             std::uint64_t{0});
    QCOMPARE(std::bit_cast<std::uint64_t>(neg.config->domains[0]),
             std::uint64_t{0});
    QVERIFY(!m08::create_amount_config(1.01,ordered(),1).config);
    QVERIFY(!m08::create_amount_config(-0.01,ordered(),1).config);
    QVERIFY(!m08::create_amount_config(
        std::numeric_limits<double>::quiet_NaN(),ordered(),1).config);
    auto bad=ordered();
    bad[2].stored=std::numeric_limits<double>::infinity();
    QCOMPARE(m08::create_amount_config(1,bad,1).failure,
             m08::AmountFailure::invalid_number);
    bad=ordered();
    bad[1].stored=1.001;
    QCOMPARE(m08::create_amount_config(1,bad,1).failure,
             m08::AmountFailure::out_of_range);
    bad=ordered();
    bad[1].domain_id=m07::kDomainIds[0]; // duplicate, no drop/sort
    QCOMPARE(m08::create_amount_config(1,bad,1).failure,
             m08::AmountFailure::domain_set_invalid);
    QCOMPARE(m08::create_amount_config(1,ordered(),0).failure,
             m08::AmountFailure::domain_set_invalid);
}
void FrozenM08AmountTest::canonicalDesiredEndpointAndInterior() {
    const auto amounts=m08::create_amount_config(
        0.5,ordered(0.8,1.0,1.0,1.0),3);
    QVERIFY(amounts.config);
    const auto target=m08::desired_for_component(*amounts.config,0,
        {12.0,m08::Endpoint::ordinary},
        {9.0,m08::Endpoint::ordinary});
    QVERIFY(target.value);
    QVERIFY(std::abs(target.value->effective_amount-0.4)<1e-15);
    QVERIFY(std::abs(target.value->desired_z-10.8)<1e-12);
    QCOMPARE(target.value->endpoint,
             m08::Endpoint::intermediary_not_endpoint_attested);
    QVERIFY(!target.value->authorizes_solver_or_publication());
    QVERIFY(!target.value->achieved_z);
    QVERIFY(!target.value->constraint_adjusted_z);

    const auto zero=m08::create_amount_config(
        1.0,ordered(0.0,1.0,1.0,1.0),4);
    QVERIFY(zero.config);
    const auto exactP=m08::desired_for_component(*zero.config,0,
        {-0.0,m08::Endpoint::mono_side_muted},
        {-24.0,m08::Endpoint::positive_width_floor});
    QVERIFY(exactP.value);
    QCOMPARE(std::bit_cast<std::uint64_t>(exactP.value->desired_z),
             std::bit_cast<std::uint64_t>(-0.0));
    QCOMPARE(exactP.value->endpoint,m08::Endpoint::mono_side_muted);
    const auto one=m08::create_amount_config(
        1.0,ordered(),5);
    QVERIFY(one.config);
    const auto exactG=m08::desired_for_component(*one.config,0,
        {-24.0,m08::Endpoint::mono_side_muted},
        {-24.0,m08::Endpoint::positive_width_floor});
    QVERIFY(exactG.value);
    QCOMPARE(exactG.value->desired_z,-24.0);
    QCOMPARE(exactG.value->endpoint,m08::Endpoint::positive_width_floor);
    const auto mid=m08::create_amount_config(
        0.5,ordered(),6);
    QVERIFY(mid.config);
    const auto coincident=m08::desired_for_component(*mid.config,0,
        {-24.0,m08::Endpoint::mono_side_muted},
        {-24.0,m08::Endpoint::positive_width_floor});
    QVERIFY(coincident.value);
    QCOMPARE(coincident.value->desired_z,-24.0);
    QCOMPARE(coincident.value->endpoint,
             m08::Endpoint::intermediary_not_endpoint_attested);
}
void FrozenM08AmountTest::
positiveEffectiveAmountUnderflowAndInvalidEvidence() {
    const auto tiny=std::numeric_limits<double>::denorm_min();
    const auto amounts=m08::create_amount_config(
        tiny,ordered(tiny,1.0,1.0,1.0),9);
    QVERIFY(amounts.config);
    const auto a=m08::effective_amount(*amounts.config,0);
    QVERIFY(a.positive_underflow_to_zero);
    QCOMPARE(a.rounded,0.0);
    QCOMPARE(amounts.config->global,tiny);
    QCOMPARE(amounts.config->domains[0],tiny);
    const auto result=m08::desired_for_component(*amounts.config,0,
        {3.0,m08::Endpoint::ordinary},
        {5.0,m08::Endpoint::ordinary});
    QVERIFY(!result.value);
    QVERIFY(result.positive_effective_underflow);
    QCOMPARE(result.failure,
             m08::TargetFailure::effective_amount_underflow_to_zero);
    const auto zero=m08::create_amount_config(
        0.0,ordered(tiny,1.0,1.0,1.0),10);
    QVERIFY(zero.config);
    const auto normal=m08::desired_for_component(*zero.config,0,
        {3.0,m08::Endpoint::ordinary},
        {5.0,m08::Endpoint::ordinary});
    QVERIFY(normal.value);
    QVERIFY(!normal.positive_effective_underflow);
    const auto missing=m08::desired_for_component(*zero.config,0,
        {{},m08::Endpoint::ordinary},
        {5.0,m08::Endpoint::ordinary});
    QVERIFY(!missing.value); // Amount=0 does not conceal missing P
    QCOMPARE(missing.failure,
             m08::TargetFailure::missing_or_nonfinite_input);
    const auto invalid=m08::desired_for_component(*zero.config,4,
        {0.0,m08::Endpoint::ordinary},
        {1.0,m08::Endpoint::ordinary});
    QVERIFY(!invalid.value);
    QCOMPARE(invalid.failure,
             m08::TargetFailure::invalid_amount_domain);
}
void FrozenM08AmountTest::noClaimOfProductTargetOrM13() {
    const auto amounts=m08::create_amount_config(
        1.0,ordered(0.25,0.5,0.75,1.0),11);
    QVERIFY(amounts.config);
    for(std::size_t i=0;i<4;++i) {
        const auto target=m08::desired_for_component(
            *amounts.config,i,
            {12.0,m08::Endpoint::ordinary},
            {9.0,m08::Endpoint::ordinary});
        QVERIFY(target.value);
        QVERIFY(!target.value->authorizes_solver_or_publication());
        QVERIFY(!target.value->achieved_z);
    }
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::FrozenM08AmountTest)
#include "test_l3_m08_frozen_amount.moc"
