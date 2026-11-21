#include "internal/l3_m06_d07_d09_native_binding.hpp"
#include <QtTest/QTest>
#include <cstddef>
#include <cstdint>
#include <string>
namespace rgsml::tests {
namespace {
namespace real=rgsml::app::l3_automatch::m05m06_real;
[[nodiscard]] real::TransientTripleEvidence example() {
    real::TransientTripleEvidence e;
    e.same_exact_p04_policy_context_and_generation_attested=true;
    for(std::size_t d=0;d<3U;++d) {
        e.exact_feature_definition_digests[d]=
            "exact-Phase6-P04-native-event-feature-"+std::to_string(d);
        e.per_descriptor_source_grade[d]={true,true,true};
    }
    const std::array<std::string,3> ids{"PREP","GOLD","CAND"};
    for(std::size_t j=0;j<3U;++j) {
        auto& source=e.current_sources[j];
        source.owner={ids[j],static_cast<std::uint64_t>(j+1),true};
        for(std::size_t d=0;d<3U;++d) {
            auto& descriptor=source.measured.d07_d08_d09[d];
            descriptor.reason=real::TransientDescriptorFailure::ready;
            descriptor.native_support=j==0?0.9:j==1?0.75:0.6;
            for(std::size_t k=0;k<3U;++k)
                descriptor.canonical_type7_p10_p50_p90[k]=
                    static_cast<double>(d+k+j);
        }
    }
    return e;
}
class M06TransientBindingTest final:public QObject {
    Q_OBJECT
private slots:
    void actualThreeCanonicalBindingsWithIndependentSupport();
    void oneDescriptorMissingLeavesThreeOriginalComponents();
    void staleP04OrMissingFeatureVersionRejectsGeneration();
};
void M06TransientBindingTest::
actualThreeCanonicalBindingsWithIndependentSupport() {
    auto e=example();
    const auto bound=real::bind_real_d07_d09(e);
    QCOMPARE(bound.failure,real::TransientBindingFailure::none);
    QVERIFY(bound.slots);
    QCOMPARE(bound.slots->size(),std::size_t{3});
    for(std::size_t d=0;d<3U;++d) {
        const auto& b=(*bound.slots)[d];
        QCOMPARE(b.configured.size(),std::size_t{3});
        for(const auto& cell:b.configured) {
            QVERIFY(cell.current_valid);
            QCOMPARE(cell.paired_native_support,0.75);
            QVERIFY(cell.current_candidate_z);
        }
    }
    QVERIFY(!bound.authorizes_match_or_publication());
}
void M06TransientBindingTest::
oneDescriptorMissingLeavesThreeOriginalComponents() {
    auto e=example();
    e.current_sources[1].measured.d07_d08_d09[1].reason=
        real::TransientDescriptorFailure::insufficient_events_or_active_scope;
    const auto bound=real::bind_real_d07_d09(e);
    QVERIFY(bound.slots);
    const auto& missing=(*bound.slots)[1];
    QCOMPARE(missing.configured.size(),std::size_t{3});
    for(const auto& cell:missing.configured) {
        QVERIFY(!cell.current_valid);
        QCOMPARE(cell.paired_native_support,0.0);
    }
    QVERIFY((*bound.slots)[0].configured[0].current_valid);
}
void M06TransientBindingTest::
staleP04OrMissingFeatureVersionRejectsGeneration() {
    auto e=example();
    e.same_exact_p04_policy_context_and_generation_attested=false;
    QVERIFY(!real::bind_real_d07_d09(e).slots);
    e=example();
    e.exact_feature_definition_digests[2].clear();
    QCOMPARE(real::bind_real_d07_d09(e).failure,
             real::TransientBindingFailure::
                unresolved_native_descriptor_policy);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M06TransientBindingTest)
#include "test_l3_m06_d07_d09_native_binding.moc"
