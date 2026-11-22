#include "internal/l3_m06_p04_real_product_to_m07.hpp"
#include <QtTest/QTest>
#include <cstdint>
#include <string>
namespace rgsml::tests {
namespace {
namespace m=rgsml::app::l3_automatch::m05m06_real;
[[nodiscard]] m::CurrentP04TripleProduct test_pair() {
    m::CurrentP04TripleProduct x;
    x.exact_current_three_source_epochs_and_p04_policy_attested=true;
    x.exact_source_compatibility_and_grade={true,true,true};
    x.exact_phase6_feature_definition_sha={
        "exact-P04-d07-algorithmSHA","exact-P04-d08-algorithmSHA",
        "exact-P04-d09-algorithmSHA"};
    const std::array<std::string,3> names{"PREP","GOLD","CAND"};
    for(std::size_t j=0;j<3U;++j) {
        x.independent_live_product_owners[j]={
            names[j],static_cast<std::uint64_t>(j+1),true};
        auto& sample=x.true_pcm_product_measurements[j];
        sample.reason=m::P04ProductObservationReason::ready;
        sample.detector.reason=m::P04SelectionReason::ready;
        sample.actual_d07_d08_d09_events.
            exact_source_and_canonical_selection_current=true;
        for(auto& descriptor:sample.original_d07_d08_d09_type7.
                d07_d08_d09) {
            descriptor.reason=m::TransientDescriptorFailure::ready;
            descriptor.native_support=0.75;
            for(std::size_t k=0;k<3U;++k)
                descriptor.canonical_type7_p10_p50_p90[k]=
                    static_cast<double>(j+k);
        }
    }
    return x;
}
class P04CurrentM07BindingTest final:public QObject {
    Q_OBJECT
private slots:
    void exactOriginalThreeProductBindingsAndNativeSupport();
    void missingDetectorOrCandidateGradeNeverClaimsMatch();
};
void P04CurrentM07BindingTest::
exactOriginalThreeProductBindingsAndNativeSupport() {
    const auto mapped=m::bind_current_product_p04_d07_d09(
        test_pair());
    QCOMPARE(mapped.failure,m::TransientBindingFailure::none);
    QVERIFY(mapped.feature_slots);
    for(const auto& source:*mapped.feature_slots) {
        QCOMPARE(source.configured.size(),std::size_t{3});
        for(const auto& component:source.configured) {
            QVERIFY(component.current_valid);
            QCOMPARE(component.paired_native_support,0.75);
        }
    }
    QVERIFY(!mapped.authorizes_match_or_publication());
}
void P04CurrentM07BindingTest::
missingDetectorOrCandidateGradeNeverClaimsMatch(){
    auto input=test_pair();
    input.true_pcm_product_measurements[1].reason=
        m::P04ProductObservationReason::invalid_detector;
    QVERIFY(!m::bind_current_product_p04_d07_d09(input).feature_slots);
    input=test_pair();
    input.exact_source_compatibility_and_grade[2]=false;
    const auto mapped=m::bind_current_product_p04_d07_d09(input);
    QVERIFY(mapped.feature_slots);
    QVERIFY((*mapped.feature_slots)[0].configured[0].current_valid);
    QVERIFY(!(*mapped.feature_slots)[0].configured[0].
        current_candidate_z);
    input=test_pair();
    input.independent_live_product_owners[1].resource_identity="PREP";
    QVERIFY(!m::bind_current_product_p04_d07_d09(input).feature_slots);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::P04CurrentM07BindingTest)
#include "test_l3_m06_p04_real_product_to_m07.moc"
