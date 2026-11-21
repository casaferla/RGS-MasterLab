#include "internal/l3_m07_frozen_binding_projection.hpp"
#include "internal/l3_m07_authoritative_m10_bridge.hpp"
#include <rgsml/dsp/module_registry.hpp>
#include <QtTest/QTest>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::tests {
namespace {
namespace m07=rgsml::app::l3_automatch::m07_frozen;
namespace m10=rgsml::app::l3_automatch::m10_frozen;
[[nodiscard]] std::vector<m07::ResolvedSlot> fixture() {
    std::vector<m07::ResolvedSlot> rows;
    rows.reserve(m07::kBindings.size());
    for(const auto& b:m07::kBindings) {
        const auto n=b.component_share==m07::ComponentShare::thirds?
            std::size_t{3}:
            b.component_share==m07::ComponentShare::configured_n?
                std::size_t{2}:std::size_t{1};
        m07::ResolvedSlot s;
        s.binding_id=std::string{b.binding_id};
        s.resolved_source_feature_id=std::string{b.source_feature_id};
        s.exact_feature_dependency="fixture-exact-artifact-digest";
        s.feature_authority_attested=true;
        s.configured_components.reserve(n);
        for(std::size_t i=0;i<n;++i)
            s.configured_components.push_back({
                true,1.0,1.0,0.0,true});
        rows.push_back(std::move(s));
    }
    return rows;
}
class FrozenM07RegistryTest final:public QObject {
    Q_OBJECT
private slots:
    void frozen27SlotsAndExactRationalMasses();
    void realCanonicalDspAvailabilityDoesNotChangeStaticPolicy();
    void m07ToM10FullyResolvedFixture();
    void missingSourceIdentityOrPopulationNeverInventsJ();
};
void FrozenM07RegistryTest::frozen27SlotsAndExactRationalMasses() {
    static_assert(m07::static_structure_valid());
    QCOMPARE(m07::kBindings.size(),std::size_t{27});
    QCOMPARE(m07::kIntents.size(),std::size_t{8});
    QCOMPARE(m07::kNonartistic.size(),std::size_t{13});
    std::array<int,4> sum{},count{};
    for(const auto& b:m07::kBindings) {
        const auto d=static_cast<std::size_t>(b.domain);
        sum[d]+=b.domain_mass.in_units_8000();
        ++count[d];
        QVERIFY(m07::find_binding(b.binding_id)==&b);
    }
    for(const auto x:sum)QCOMPARE(x,8000);
    QCOMPARE(count[0],6);
    QCOMPARE(count[1],12);
    QCOMPARE(count[2],6);
    QCOMPARE(count[3],3);
    QVERIFY(!m07::find_binding(
        "rgsml.reference-match.binding.tonal.t07"));
    QCOMPARE(m07::kBindings[0].binding_id,
             std::string_view{"rgsml.reference-match.binding.tonal.t01"});
    QCOMPARE(m07::kBindings[0].domain_mass.in_units_8000(),6400);
    QVERIFY(!m07::kBindings[0].exclusive_fallback_feature_id.empty());
    for(std::size_t i=1;i<m07::kBindings.size();++i)
        QVERIFY(m07::kBindings[i].exclusive_fallback_feature_id.empty());
    // L2 and D01 share ONE actual ST series source ID; distinct selector,
    // support/identity/mass, no invented source alias or 28th slot.
    QCOMPARE(m07::kBindings[6].source_feature_id,
             m07::kBindings[25].source_feature_id);
    QCOMPARE(m07::kBindings[6].component_share,
             m07::ComponentShare::thirds);
    QCOMPARE(m07::kBindings[25].component_share,
             m07::ComponentShare::one);
    for(const auto& r:m07::kNonartistic) {
        QCOMPARE(r.artistic_mass.numerator,0);
        QCOMPARE(r.artistic_mass.denominator,1);
    }
}
void FrozenM07RegistryTest::
realCanonicalDspAvailabilityDoesNotChangeStaticPolicy() {
    const auto native=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(native);
    const auto first=m07::assess_canonical_dsp_snapshot(*native.value());
    QCOMPARE(first.size(),std::size_t{8});
    for(const auto& record:first) {
        QVERIFY(record.factory_state!=m07::FactoryEvidence::unknown_type);
        QVERIFY(!record.authorizes_match_planning());
    }
    QCOMPARE(first[2].factory_state,
             m07::FactoryEvidence::descriptor_only);
    QCOMPARE(first[3].factory_state,
             m07::FactoryEvidence::descriptor_only);
    QCOMPARE(first[1].factory_state,
             m07::FactoryEvidence::factory_present_unattested);
    // Two optical-intent records must point to the SAME Parametric EQ
    // descriptor, not clone a second registry/feature authority.
    QCOMPARE(first[0].intent.dsp_type_id,first[5].intent.dsp_type_id);
    auto eq2=dsp::ModuleRegistry::with_dynamic_eq_offline(*native.value());
    QVERIFY(eq2);
    auto trans=dsp::ModuleRegistry::with_transient_shaper_offline(
        std::move(*eq2.value()));
    QVERIFY(trans);
    const auto activated=m07::assess_canonical_dsp_snapshot(
        *trans.value());
    QCOMPARE(activated[2].factory_state,
             m07::FactoryEvidence::factory_present_unattested);
    QCOMPARE(activated[3].factory_state,
             m07::FactoryEvidence::factory_present_unattested);
    QVERIFY(activated[2].intrinsic_context_matches);
    QVERIFY(activated[3].intrinsic_context_matches);
    QCOMPARE(m07::kBindings.size(),std::size_t{27});
    QCOMPARE(native.value()->factory_count(),
             std::size_t{3});
    // The original L1 package factory-less placeholders are immutable.
    QVERIFY(!native.value()->has_factory("rgsml.dsp.dynamic-eq"));
    QVERIFY(!native.value()->has_factory("rgsml.dsp.transient-shaper"));
}
void FrozenM07RegistryTest::m07ToM10FullyResolvedFixture() {
    auto sourced=fixture();
    const auto built=m07::to_m10_population(sourced,{
        true,true,true,true});
    QVERIFY(built.populations);
    QCOMPARE(built.failure,m07::BridgeFailure::none);
    QVERIFY(!built.populations->authorizes_master_publication());
    std::array<std::size_t,4> expected{{
        7,25,14,3}};
    // T01 two bands + 5 scalars=7, D01/D04/D07/D08/D09
    // vector3, D10/11/12 vector2; stereo vector and low-end share
    // are NEVER contracted to one scalar for missing support.
    for(std::size_t i=0;i<4;++i)
        QCOMPARE(built.populations->domain[i].configured.size(),
                 expected[i]);
    const auto score=m10::compute_frozen_target_cost(
        built.populations->domain);
    QVERIFY(score.valid_measured());
    for(const auto& c:score.effective_coverage) {
        QVERIFY(c);
        QCOMPARE(*c,1.0);
    }
    QVERIFY(score.j_target);
    QVERIFY(*score.j_target>0.0);
    QCOMPARE(m07::kBindings[6].domain_mass.numerator,3);
    QCOMPARE(m07::kBindings[6].domain_mass.denominator,20);
    QVERIFY(std::abs(
        built.populations->domain[1].configured[0].configured_mass-
        0.05)<1e-12); // D01 mass .15 distributed 1/3, no triple count

    sourced[0].resolved_source_feature_id=
        std::string{m07::kBindings[0].exclusive_fallback_feature_id};
    const auto fallback=m07::to_m10_population(sourced,{
        true,true,true,true});
    QVERIFY(fallback.populations);
    QVERIFY(!fallback.populations->authorizes_master_publication());
}
void FrozenM07RegistryTest::missingSourceIdentityOrPopulationNeverInventsJ() {
    auto rows=fixture();
    rows.pop_back();
    QCOMPARE(m07::to_m10_population(rows,{true,true,true,true}).failure,
             m07::BridgeFailure::wrong_slot_count);
    rows=fixture();
    std::swap(rows[0],rows[1]);
    QCOMPARE(m07::to_m10_population(rows,{true,true,true,true}).failure,
             m07::BridgeFailure::binding_order_or_identity_mismatch);
    rows=fixture();
    rows[0].feature_authority_attested=false;
    QCOMPARE(m07::to_m10_population(rows,{true,true,true,true}).failure,
             m07::BridgeFailure::exact_feature_authority_unresolved);
    rows=fixture();
    rows[0].resolved_source_feature_id=
        "rgsml.tonal.fake-source";
    QCOMPARE(m07::to_m10_population(rows,{true,true,true,true}).failure,
             m07::BridgeFailure::incompatible_feature_variant);
    rows=fixture();
    rows[6].configured_components.pop_back();
    QCOMPARE(m07::to_m10_population(rows,{true,true,true,true}).failure,
             m07::BridgeFailure::invalid_component_population);
    rows=fixture();
    rows[2].configured_components[0].paired_support=1.01;
    QCOMPARE(m07::to_m10_population(rows,{true,true,true,true}).failure,
             m07::BridgeFailure::illegal_support_or_numeric);
    rows=fixture();
    rows[2].configured_components[0].optimization_target_z.reset();
    QCOMPARE(m07::to_m10_population(rows,{true,true,true,true}).failure,
             m07::BridgeFailure::exact_feature_authority_unresolved);
    // Disabled feature is not silently removed from configured domain
    // mass, and the existing frozen inclusive 0.60 gate still applies.
    rows=fixture();
    for(auto& slot:rows)
        if(slot.binding_id.rfind(
               "rgsml.reference-match.binding.tonal.",0)==0)
            for(auto& c:slot.configured_components)
                c.valid_current=false;
    const auto built=m07::to_m10_population(rows,{
        true,true,true,true});
    QVERIFY(built.populations);
    const auto score=m10::compute_frozen_target_cost(
        built.populations->domain);
    QVERIFY(!score.valid_measured());
    QCOMPARE(score.status,
             m10::TargetCostStatus::insufficient_effective_coverage);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::FrozenM07RegistryTest)
#include "test_l3_m07_frozen_registry.moc"
