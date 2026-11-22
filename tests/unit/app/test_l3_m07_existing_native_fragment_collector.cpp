#include "internal/l3_m07_existing_native_fragment_collector.hpp"
#include <QtTest/QTest>
#include <cstddef>
#include <string>
namespace rgsml::tests {
namespace {
namespace m07=rgsml::app::l3_automatch::m07_frozen;
namespace m05=rgsml::app::l3_automatch::m05m06_real;
namespace m08=rgsml::app::l3_automatch::m08_frozen;
[[nodiscard]] m08::AuthenticatedPairSlot binding(
    const m07::Binding& b) {
    m08::AuthenticatedPairSlot output{
        std::string{b.binding_id},std::string{b.source_feature_id},
        "exact-independent-FeatureDefinition-sha",true,{}};
    const auto n=b.component_share==m07::ComponentShare::thirds?
        std::size_t{3}:std::size_t{1};
    for(std::size_t i=0;i<n;++i)
        output.configured.push_back({
            true,1.0,{1.0,m08::Endpoint::ordinary},
            {2.0,m08::Endpoint::ordinary},1.5,true});
    return output;
}
class M07NativeFragmentCollectorTest final:public QObject {
    Q_OBJECT
private slots:
    void existingNativeSubsetRemainsExplicitlyIncomplete();
    void duplicateCannotOverwriteOrDoubleCountFeature();
    void missingAllFragmentsReportsExactMissingIds();
    void certifiedNativeEventAndSpectralFragmentsKeepFourStereoMissing();
};
void M07NativeFragmentCollectorTest::
existingNativeSubsetRemainsExplicitlyIncomplete() {
    m07::ExactTwentySevenFeatureAssembler assembler{
        "exact-native-reference-generation",true};
    m07::ExistingNativeSourceFragments selected;
    selected.tonal_primary=m05::T01BindingProjection{
        binding(m07::kBindings[0]),m05::T01BindingFailure::none};
    selected.macro_d01=m05::D01BindingProjection{
        binding(m07::kBindings[6]),m05::D01BindingFailure::none};
    std::array<m08::AuthenticatedPairSlot,2> dynamics{
        binding(m07::kBindings[8]),binding(m07::kBindings[9])};
    selected.macro_windowed=m05::D03D04SlotProjection{
        dynamics,m05::D03D04SlotFailure::none};
    const auto result=m07::collect_existing_m05m06_native_fragments(
        assembler,selected);
    QCOMPARE(result.status,m07::NativeFragmentAdmission::accepted_partial);
    QCOMPARE(result.inventory.populated_count,std::size_t{4});
    QCOMPARE(result.inventory.missing_original_binding_ids.size(),
             std::size_t{23});
    QVERIFY(!assembler.complete_canonical_population());
    QVERIFY(!result.authorizes_solver());
}
void M07NativeFragmentCollectorTest::
duplicateCannotOverwriteOrDoubleCountFeature() {
    m07::ExactTwentySevenFeatureAssembler assembler{
        "exact-native-reference-generation",true};
    m07::ExistingNativeSourceFragments selected;
    selected.tonal_primary=m05::T01BindingProjection{
        binding(m07::kBindings[0]),m05::T01BindingFailure::none};
    QCOMPARE(m07::collect_existing_m05m06_native_fragments(
        assembler,selected).status,
        m07::NativeFragmentAdmission::accepted_partial);
    QCOMPARE(m07::collect_existing_m05m06_native_fragments(
        assembler,selected).status,
        m07::NativeFragmentAdmission::conflicting_or_invalid_source);
    QVERIFY(!assembler.complete_canonical_population());
}
void M07NativeFragmentCollectorTest::
missingAllFragmentsReportsExactMissingIds() {
    m07::ExactTwentySevenFeatureAssembler assembler{
        "exact-native-reference-generation",true};
    const auto result=m07::collect_existing_m05m06_native_fragments(
        assembler,{});
    QCOMPARE(result.status,
             m07::NativeFragmentAdmission::missing_upstream_authority);
    QCOMPARE(result.inventory.missing_original_binding_ids.size(),
             std::size_t{27});
    QCOMPARE(result.inventory.missing_original_binding_ids.front(),
             m07::kBindings[0].binding_id);
}
void M07NativeFragmentCollectorTest::
certifiedNativeEventAndSpectralFragmentsKeepFourStereoMissing() {
    m07::ExactTwentySevenFeatureAssembler g{
        "authentic-native-Phase6-generation",true};
    m07::ExistingNativeSourceFragments fragments;
    std::array<m08::AuthenticatedPairSlot,3> transient{
        binding(m07::kBindings[12]),binding(m07::kBindings[13]),
        binding(m07::kBindings[14])};
    std::array<m08::AuthenticatedPairSlot,3> spectral{
        binding(m07::kBindings[15]),binding(m07::kBindings[16]),
        binding(m07::kBindings[17])};
    fragments.micro_events=m05::TransientBindingProjection{
        transient,m05::TransientBindingFailure::none};
    fragments.spectral_frame_energy=m05::SpectralSlotProjection{
        spectral,m05::SpectralSlotFailure::none};
    const auto out=m07::collect_existing_m05m06_native_fragments(
        g,fragments);
    QCOMPARE(out.status,m07::NativeFragmentAdmission::accepted_partial);
    QCOMPARE(out.inventory.populated_count,std::size_t{6});
    QCOMPARE(out.inventory.missing_original_binding_ids.size(),
             std::size_t{21});
    QVERIFY(!g.complete_canonical_population());
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M07NativeFragmentCollectorTest)
#include "test_l3_m07_existing_native_fragment_collector.moc"
