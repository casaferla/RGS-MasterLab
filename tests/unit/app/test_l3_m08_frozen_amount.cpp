#include "internal/l3_m08_frozen_amount_target.hpp"
#include "internal/l3_m08_frozen_proximity_reporting.hpp"
#include "internal/l3_m08_frozen_targeted_population.hpp"
#include "internal/l3_m08_native_gold_proximity.hpp"
#include <QtTest/QTest>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string_view>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::tests {
namespace {
namespace m08=rgsml::app::l3_automatch::m08_frozen;
namespace m07=rgsml::app::l3_automatch::m07_frozen;
namespace m10=rgsml::app::l3_automatch::m10_frozen;
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
    void p02AlreadyMatchedDropoutAndRawNegativeLaw();
    void p02ConditionalRequestedGlobalDoesNotFabricateFull();
    void exact27M07M08M10PopulationUsesActualAmountAndEndpoint();
    void nativeGoldProximityUsesFrozenMassAndCoverage();
    void nativeGoldProximityHandlesClampingAndAlreadyClose();
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
void FrozenM08AmountTest::
p02AlreadyMatchedDropoutAndRawNegativeLaw() {
    m08::DomainDistanceEvidence evidence{
        true,true,false,false,3.0,1.5};
    const auto normal=m08::compute_domain_proximity(evidence);
    QVERIFY(normal.comparable());
    QCOMPARE(*normal.raw,0.5);
    evidence.candidate_to_gold_distance=1.5; // overshoot same distance
    QCOMPARE(*m08::compute_domain_proximity(evidence).raw,0.5);
    evidence.baseline_to_gold_distance=1.0;
    evidence.candidate_to_gold_distance=2.0;
    const auto worse=m08::compute_domain_proximity(evidence);
    QVERIFY(worse.comparable());
    QCOMPARE(*worse.raw,-1.0); // raw worsening is NEVER UI clipped
    evidence.prepared_already_within_frozen_close=true;
    evidence.candidate_within_frozen_close=true;
    const auto matched=m08::compute_domain_proximity(evidence);
    QVERIFY(matched.comparable());
    QCOMPARE(*matched.raw,1.0);
    QCOMPARE(matched.reason,
             m08::ProximityReason::proximity_already_matched);
    evidence.candidate_within_frozen_close=false;
    const auto departed=m08::compute_domain_proximity(evidence);
    QVERIFY(!departed.comparable());
    QVERIFY(!departed.raw);
    QCOMPARE(departed.reason,
             m08::ProximityReason::
             proximity_undefined_initial_already_matched_candidate_departure);
    evidence.required_candidate_components_present=false;
    const auto dropped=m08::compute_domain_proximity(evidence);
    QVERIFY(!dropped.raw);
    QCOMPARE(dropped.reason,
             m08::ProximityReason::candidate_required_component_missing);
    evidence.producer_current=false;
    const auto stale=m08::compute_domain_proximity(evidence);
    QVERIFY(!stale.raw);
    QCOMPARE(stale.reason,
             m08::ProximityReason::source_evidence_unavailable);
    evidence={true,true,false,false,0.0,0.0};
    QCOMPARE(m08::compute_domain_proximity(evidence).reason,
             m08::ProximityReason::invalid_distance);
}
void FrozenM08AmountTest::
p02ConditionalRequestedGlobalDoesNotFabricateFull() {
    std::array<m08::RequestedDomainReport,4> full{{
        {true,{0.8,m08::ProximityReason::comparable},1.0},
        {true,{0.4,m08::ProximityReason::comparable},0.8},
        {true,{{},m08::ProximityReason::
            candidate_required_component_missing},0.5},
        {true,{{},m08::ProximityReason::
            source_evidence_unavailable},{}}
    }};
    const auto partial=m08::report_requested_domains(full);
    QCOMPARE(partial.completeness,
             m08::GlobalCompleteness::partial);
    QVERIFY(partial.conditional_comparable_proximity);
    QVERIFY(std::abs(*partial.conditional_comparable_proximity-0.6)<1e-12);
    QCOMPARE(partial.requested_domain_mass,1.0);
    QCOMPARE(partial.comparable_domain_mass,0.5);
    QCOMPARE(partial.structural_coverage,0.5);
    QVERIFY(std::abs(*partial.fixed_contributions[0]-0.2)<1e-12);
    QVERIFY(std::abs(*partial.fixed_contributions[1]-0.1)<1e-12);
    QVERIFY(!partial.fixed_contributions[2]);
    QVERIFY(!partial.fixed_contributions[3]);
    QVERIFY(!partial.effective_coverage);
    QCOMPARE(partial.unknown_effective_mass,0.25);
    QVERIFY(!partial.authorizes_full_match_publication());
    full[3].effective_coverage=0.4;
    const auto covered=m08::report_requested_domains(full);
    QVERIFY(covered.effective_coverage);
    QVERIFY(std::abs(*covered.effective_coverage-0.675)<1e-12);
    full[0].proximity={};
    full[1].proximity={};
    const auto none=m08::report_requested_domains(full);
    QCOMPARE(none.completeness,
             m08::GlobalCompleteness::no_comparable_domain);
    QVERIFY(!none.conditional_comparable_proximity);
    // Request set membership is explicit; an Amount=0 elsewhere does
    // NOT remove a requested Domain from the global denominator.
    full[0].requested=false;
    full[1].requested=false;
    full[2].requested=false;
    full[3].requested=false;
    const auto empty=m08::report_requested_domains(full);
    QCOMPARE(empty.completeness,
             m08::GlobalCompleteness::no_requested_domains);
    QVERIFY(!empty.conditional_comparable_proximity);
    // Full scope current/raw evidence also never authorizes a master.
    for(auto& d:full) {
        d.requested=true;
        d.effective_coverage=1.0;
        d.proximity={0.75,m08::ProximityReason::comparable};
    }
    const auto complete=m08::report_requested_domains(full);
    QCOMPARE(complete.completeness,
             m08::GlobalCompleteness::complete);
    QCOMPARE(complete.comparable_domain_mass,1.0);
    QVERIFY(!complete.authorizes_full_match_publication());
}
void FrozenM08AmountTest::
exact27M07M08M10PopulationUsesActualAmountAndEndpoint() {
    std::vector<m08::AuthenticatedPairSlot> paired;
    paired.reserve(m07::kBindings.size());
    for(const auto& b:m07::kBindings) {
        m08::AuthenticatedPairSlot slot;
        slot.binding_id=std::string{b.binding_id};
        slot.source_feature_id=std::string{b.source_feature_id};
        slot.exact_feature_dependency="test-attested-exact-content";
        slot.feature_authority_attested=true;
        const auto n=b.component_share==m07::ComponentShare::thirds
            ?std::size_t{3}:
            b.component_share==m07::ComponentShare::configured_n
            ?std::size_t{2}:std::size_t{1};
        for(std::size_t i=0;i<n;++i)
            slot.configured.push_back({
                true,1.0,
                {12.0,m08::Endpoint::ordinary},
                {9.0,m08::Endpoint::ordinary},10.5,true});
        paired.push_back(std::move(slot));
    }
    const auto amounts=m08::create_amount_config(
        0.5,ordered(),19);
    QVERIFY(amounts.config);
    auto built=m08::build_targeted_population(
        *amounts.config,paired,{true,true,true,true});
    QVERIFY(built.value);
    QCOMPARE(built.failure,m08::PopulationFailure::none);
    QVERIFY(!built.value->authorizes_product_master());
    QCOMPARE(built.value->desired_by_canonical_slot.size(),
             std::size_t{27});
    for(const auto& slot:built.value->desired_by_canonical_slot)
        for(const auto& t:slot) {
            QVERIFY(t);
            QCOMPARE(t->desired_z,10.5);
            QCOMPARE(t->effective_amount,0.5);
            QVERIFY(!t->achieved_z);
            QVERIFY(!t->constraint_adjusted_z);
        }
    const auto objective=m10::compute_frozen_target_cost(
        built.value->objective_input);
    QVERIFY(objective.valid_measured());
    QVERIFY(std::abs(*objective.j_target)<1e-12);

    // One missing native ST selector is not dropped from the D01 mass.
    paired[6].configured[1].current_valid=false;
    paired[6].configured[1].prepared.z.reset();
    paired[6].configured[1].reference.z.reset();
    paired[6].configured[1].current_candidate_z.reset();
    built=m08::build_targeted_population(
        *amounts.config,paired,{true,true,true,true});
    QVERIFY(built.value);
    QVERIFY(!built.value->desired_by_canonical_slot[6][1]);
    const auto partial=m10::compute_frozen_target_cost(
        built.value->objective_input);
    QVERIFY(partial.valid_measured());
    QVERIFY(partial.effective_coverage[1]);
    QVERIFY(std::abs(*partial.effective_coverage[1]-0.95)<1e-12);
    paired[6].configured[1].current_valid=true;
    const auto invalid=m08::build_targeted_population(
        *amounts.config,paired,{true,true,true,true});
    QVERIFY(!invalid.value);
    QCOMPARE(invalid.failure,
             m08::PopulationFailure::amount_or_target_invalid);
    paired[6].configured[1].prepared.z=12.0;
    paired[6].configured[1].reference.z=9.0;
    paired[6].configured[1].current_candidate_z=10.5;
    paired[0].source_feature_id=
        std::string{m07::kBindings[0].exclusive_fallback_feature_id};
    const auto alternative=m08::build_targeted_population(
        *amounts.config,paired,{true,true,true,true});
    QVERIFY(alternative.value); // T1 is one slot, not a 28th
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
void FrozenM08AmountTest::nativeGoldProximityUsesFrozenMassAndCoverage()
{
    std::vector<m08::AuthenticatedPairSlot> paired;
    for(const auto& b:m07::kBindings) {
        m08::AuthenticatedPairSlot slot{
            std::string(b.binding_id),std::string(b.source_feature_id),
            "test-definition",true,{}};
        const auto n=b.component_share==m07::ComponentShare::thirds?
            std::size_t{3}:std::size_t{1};
        for(std::size_t i=0;i<n;++i)
            slot.configured.push_back({
                true,1.0,{2.0*b.scale},{0.0},b.scale,true});
        paired.push_back(std::move(slot));
    }
    const std::array<bool,4> requested{{true,true,true,true}};
    const auto half=m08::measure_native_gold_proximity(paired,requested,true);
    QVERIFY(half.canonical_population_current);
    for(std::size_t d=0;d<4;++d) {
        QVERIFY(half.domains[d].proximity.comparable());
        QVERIFY(std::abs(*half.domains[d].proximity.raw-0.5)<1e-12);
        QCOMPARE(*half.domains[d].effective_coverage,1.0);
    }
    // Support changes the effective weight exactly once.
    paired[0].configured[0].paired_native_support=0.75;
    paired[0].configured[0].current_candidate_z=0.0;
    const auto weighted=m08::measure_native_gold_proximity(
        paired,requested,true);
    QVERIFY(weighted.domains[0].proximity.comparable());
    QVERIFY(std::abs(*weighted.domains[0].effective_coverage-0.8)<1e-12);
    QVERIFY(std::abs(*weighted.domains[0].proximity.raw-0.75)<1e-12);
    paired[0].configured[0].current_candidate_z.reset();
    const auto dropout=m08::measure_native_gold_proximity(paired,requested,true);
    QCOMPARE(dropout.domains[0].proximity.reason,
        m08::ProximityReason::candidate_required_component_missing);
    QVERIFY(!dropout.domains[0].proximity.raw);
    QCOMPARE(dropout.domains[0].effective_coverage,
             weighted.domains[0].effective_coverage);
    QVERIFY(dropout.domains[1].proximity.comparable());
    paired[0].configured[0].paired_native_support=0.0;
    const auto insufficient=m08::measure_native_gold_proximity(
        paired,requested,true);
    QVERIFY(!insufficient.domains[0].proximity.comparable());
    QVERIFY(*insufficient.domains[0].effective_coverage<0.60);
    QVERIFY(!m08::measure_native_gold_proximity(
        paired,requested,false).canonical_population_current);
    paired.back().binding_id="wrong-binding";
    QVERIFY(!m08::measure_native_gold_proximity(
        paired,requested,true).canonical_population_current);
}
void FrozenM08AmountTest::nativeGoldProximityHandlesClampingAndAlreadyClose()
{
    std::vector<m08::AuthenticatedPairSlot> paired;
    for(const auto& b:m07::kBindings) {
        m08::AuthenticatedPairSlot slot{
            std::string(b.binding_id),std::string(b.source_feature_id),
            "test-definition",true,{}};
        const auto n=b.component_share==m07::ComponentShare::thirds?
            std::size_t{3}:std::size_t{1};
        for(std::size_t i=0;i<n;++i)
            slot.configured.push_back({
                true,1.0,{12.0*b.scale},{0.0},6.0*b.scale,true});
        paired.push_back(std::move(slot));
    }
    const std::array<bool,4> requested{{true,false,false,false}};
    auto report=m08::measure_native_gold_proximity(paired,requested,true);
    QVERIFY(report.domains[0].proximity.comparable());
    QCOMPARE(*report.domains[0].proximity.raw,0.0);
    QVERIFY(!report.domains[1].requested);
    for(auto& slot:paired)
        for(auto& c:slot.configured) {
            c.prepared.z=0.0;c.current_candidate_z=0.0;
        }
    report=m08::measure_native_gold_proximity(paired,requested,true);
    QCOMPARE(report.domains[0].proximity.reason,
        m08::ProximityReason::proximity_already_matched);
    QCOMPARE(*report.domains[0].proximity.raw,1.0);
    paired[0].configured[0].current_candidate_z=1.0;
    report=m08::measure_native_gold_proximity(paired,requested,true);
    QCOMPARE(report.domains[0].proximity.reason,
        m08::ProximityReason::proximity_undefined_initial_already_matched_candidate_departure);
    QVERIFY(!report.domains[0].proximity.raw);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::FrozenM08AmountTest)
#include "test_l3_m08_frozen_amount.moc"
