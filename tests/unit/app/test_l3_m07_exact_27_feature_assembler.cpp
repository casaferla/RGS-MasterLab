#include "internal/l3_m07_exact_27_feature_assembler.hpp"
#include <QtTest/QTest>
#include <cstddef>
#include <string>
namespace rgsml::tests {
namespace {
namespace m07=rgsml::app::l3_automatch::m07_frozen;
namespace m08=rgsml::app::l3_automatch::m08_frozen;
[[nodiscard]] m08::AuthenticatedPairSlot example(
    const m07::Binding& binding)
{
    m08::AuthenticatedPairSlot slot{
        std::string{binding.binding_id},
        std::string{binding.source_feature_id},
        "verified-phase6-native-feature-content-sha",true,{}};
    const std::size_t n=binding.component_share==
            m07::ComponentShare::thirds?3U:1U;
    for(std::size_t i=0;i<n;++i)
        slot.configured.push_back({
            true,1.0,{10.0,m08::Endpoint::ordinary},
            {12.0,m08::Endpoint::ordinary},11.0,true});
    return slot;
}
class M07ExactAssemblerTest final:public QObject {
    Q_OBJECT
private slots:
    void partialRealPopulationCannotPretendTwentySevenReady();
    void canonicalCompleteOrderOnlyAfterAllExactSources();
    void duplicateOrInvalidSourcePoisonsWholeGeneration();
    void missingEntireSourceAuthorityCannotAcceptAnyFeature();
};
void M07ExactAssemblerTest::
partialRealPopulationCannotPretendTwentySevenReady() {
    m07::ExactTwentySevenFeatureAssembler gen{
        "exact-M05-M06-M07-generation",true};
    QCOMPARE(gen.add(example(m07::kBindings[8])),
             m07::FullFeatureAssemblerReason::added);
    QCOMPARE(gen.add(example(m07::kBindings[9])),
             m07::FullFeatureAssemblerReason::added);
    QCOMPARE(gen.current_inventory().populated_count,std::size_t{2});
    QCOMPARE(gen.current_inventory().missing_original_binding_ids.size(),
             std::size_t{25});
    QVERIFY(!gen.complete_canonical_population());
    QVERIFY(!gen.authorizes_product_publication());
}
void M07ExactAssemblerTest::
canonicalCompleteOrderOnlyAfterAllExactSources() {
    m07::ExactTwentySevenFeatureAssembler gen{
        "exact-M05-M06-M07-generation",true};
    // Input order deliberately opposite canonical order.
    for(std::size_t i=m07::kBindings.size();i>0U;--i)
        QCOMPARE(gen.add(example(m07::kBindings[i-1U])),
                 m07::FullFeatureAssemblerReason::added);
    QCOMPARE(gen.current_inventory().populated_count,std::size_t{27});
    QVERIFY(gen.current_inventory().missing_original_binding_ids.empty());
    const auto full=gen.complete_canonical_population();
    QVERIFY(full);
    QCOMPARE(full->size(),std::size_t{27});
    for(std::size_t i=0;i<full->size();++i)
        QCOMPARE((*full)[i].binding_id,
                 std::string{m07::kBindings[i].binding_id});
}
void M07ExactAssemblerTest::
duplicateOrInvalidSourcePoisonsWholeGeneration() {
    m07::ExactTwentySevenFeatureAssembler gen{"exact-gen",true};
    QCOMPARE(gen.add(example(m07::kBindings[0])),
             m07::FullFeatureAssemblerReason::added);
    QCOMPARE(gen.add(example(m07::kBindings[0])),
             m07::FullFeatureAssemblerReason::
                duplicate_or_conflicting_binding);
    QVERIFY(gen.current_inventory().entire_generation_poisoned);
    QCOMPARE(gen.add(example(m07::kBindings[1])),
             m07::FullFeatureAssemblerReason::poisoned);
    QVERIFY(!gen.complete_canonical_population());
    m07::ExactTwentySevenFeatureAssembler invalid{"exact-gen",true};
    auto bad=example(m07::kBindings[6]);
    bad.exact_feature_dependency.clear();
    QCOMPARE(invalid.add(std::move(bad)),
             m07::FullFeatureAssemblerReason::invalid_feature_binding);
    QVERIFY(!invalid.complete_canonical_population());
}
void M07ExactAssemblerTest::
missingEntireSourceAuthorityCannotAcceptAnyFeature() {
    m07::ExactTwentySevenFeatureAssembler missing{"",true};
    QCOMPARE(missing.add(example(m07::kBindings[0])),
             m07::FullFeatureAssemblerReason::poisoned);
    m07::ExactTwentySevenFeatureAssembler unattested{
        "nonempty",false};
    QCOMPARE(unattested.add(example(m07::kBindings[0])),
             m07::FullFeatureAssemblerReason::poisoned);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M07ExactAssemblerTest)
#include "test_l3_m07_exact_27_feature_assembler.moc"
