#include "internal/l3_m10_cache_aware_evaluator.hpp"
#include <QtTest/QTest>
#include <cstdint>
namespace rgsml::tests {
namespace {
namespace m10=rgsml::app::l3_automatch::m10_frozen;
[[nodiscard]] m10::ExactExecutionCacheKey key() {
    return {{0x01},{0x31},{0x31},{0x41}};
}
[[nodiscard]] m10::MeasuredOrderKey measured() {
    return {0.1,0.08,0.05,1,{0x31},{0x41}};
}
class M10CacheEvaluatorTest final:public QObject {
    Q_OBJECT
private slots:
    void firstPhysicalAttemptThenFreeExactHit();
    void staleOrPreflightRejectedNeverRenders();
    void invalidRealEvidenceConsumesAttemptButIsNotCached();
    void budgetRequiresHitWhenAttemptIsNotPermitted();
    void fullOptionalCacheNeverDowngradesRealCompleteMeasurement();
};
void M10CacheEvaluatorTest::firstPhysicalAttemptThenFreeExactHit() {
    m10::ExactCompletedEvaluationCache cache{{0x01}};
    std::uint32_t renders=0U;
    const auto fresh=[&]() {
        ++renders;
        return m10::FreshCompleteEvidence{measured(),true};
    };
    const auto first=m10::evaluate_with_exact_completed_cache(
        cache,key(),true,true,fresh);
    QCOMPARE(first.kind,m10::EvaluationKind::attempted_complete);
    QCOMPARE(renders,std::uint32_t{1});
    const auto repeat=m10::evaluate_with_exact_completed_cache(
        cache,key(),true,false,fresh);
    QCOMPARE(repeat.kind,m10::EvaluationKind::completed_cache_hit);
    QVERIFY(repeat.measured);
    QCOMPARE(repeat.measured->total_j,0.1);
    QCOMPARE(renders,std::uint32_t{1});
}
void M10CacheEvaluatorTest::
staleOrPreflightRejectedNeverRenders() {
    m10::ExactCompletedEvaluationCache cache{{0x01}};
    std::uint32_t renders=0U;
    const auto fresh=[&]() {
        ++renders;
        return m10::FreshCompleteEvidence{measured(),true};
    };
    QCOMPARE(m10::evaluate_with_exact_completed_cache(
        cache,key(),false,true,fresh).kind,
        m10::EvaluationKind::preflight_rejected);
    auto stale=key();
    stale.immutable_generation_and_policy={0x02};
    QCOMPARE(m10::evaluate_with_exact_completed_cache(
        cache,stale,true,true,fresh).kind,
        m10::EvaluationKind::preflight_rejected);
    QCOMPARE(renders,std::uint32_t{0});
}
void M10CacheEvaluatorTest::
invalidRealEvidenceConsumesAttemptButIsNotCached() {
    m10::ExactCompletedEvaluationCache cache{{0x01}};
    const auto incomplete=[]() {
        return m10::FreshCompleteEvidence{measured(),false};
    };
    QCOMPARE(m10::evaluate_with_exact_completed_cache(
        cache,key(),true,true,incomplete).kind,
        m10::EvaluationKind::attempted_failure);
    QVERIFY(!cache.find(key()));
    const auto invalid=[]() {
        auto score=measured();
        score.canonical_current_tuple={0x77};
        return m10::FreshCompleteEvidence{score,true};
    };
    QCOMPARE(m10::evaluate_with_exact_completed_cache(
        cache,key(),true,true,invalid).kind,
        m10::EvaluationKind::attempted_failure);
    QCOMPARE(cache.size(),std::size_t{0});
}
void M10CacheEvaluatorTest::
budgetRequiresHitWhenAttemptIsNotPermitted() {
    m10::ExactCompletedEvaluationCache cache{{0x01}};
    std::uint32_t renders=0;
    const auto fresh=[&]() {
        ++renders;
        return m10::FreshCompleteEvidence{measured(),true};
    };
    const auto before=m10::evaluate_with_exact_completed_cache(
        cache,key(),true,false,fresh);
    QCOMPARE(before.kind,m10::EvaluationKind::needs_new_attempt);
    QCOMPARE(renders,std::uint32_t{0});
}
void M10CacheEvaluatorTest::
fullOptionalCacheNeverDowngradesRealCompleteMeasurement() {
    m10::ExactCompletedEvaluationCache cache{{0x01}};
    for(std::uint32_t i=1;i<=800U;++i) {
        m10::ExactExecutionCacheKey candidate{
            {0x01},{static_cast<std::uint8_t>(i>>8),
                    static_cast<std::uint8_t>(i&0xffU)},
            {static_cast<std::uint8_t>(i>>8),
             static_cast<std::uint8_t>(i&0xffU)},
            {0x41}};
        const m10::MeasuredOrderKey measure{
            0.2,0.2,0.0,1,candidate.canonical_current_tuple,{0x41}};
        QCOMPARE(cache.insert_completed(candidate,measure,true),
                 m10::CacheInsert::inserted);
    }
    const auto new_key=key(); // distinct 1-byte execution tuple
    bool rendered=false;
    const auto result=m10::evaluate_with_exact_completed_cache(
        cache,new_key,true,true,[&]() {
            rendered=true;
            return m10::FreshCompleteEvidence{measured(),true};
        });
    QCOMPARE(result.kind,m10::EvaluationKind::attempted_complete);
    QVERIFY(result.measured);
    QVERIFY(rendered);
    QCOMPARE(cache.size(),std::size_t{800});
    QVERIFY(!cache.find(new_key));
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M10CacheEvaluatorTest)
#include "test_l3_m10_cache_aware_evaluator.moc"
