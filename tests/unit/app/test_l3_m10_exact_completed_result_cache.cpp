#include "internal/l3_m10_exact_completed_result_cache.hpp"
#include <QtTest/QTest>
#include <cstdint>
namespace rgsml::tests {
namespace {
namespace m10=rgsml::app::l3_automatch::m10_frozen;
[[nodiscard]] m10::ExactExecutionCacheKey key(
    std::uint8_t candidate=1) {
    return {{0xAF,0x51},{candidate},{candidate},{0x81}};
}
[[nodiscard]] m10::MeasuredOrderKey score(
    std::uint8_t candidate=1,double j=0.2) {
    return {j,j,0.0,2,{candidate},{0x81}};
}
class M10CompletedCacheTest final:public QObject {
    Q_OBJECT
private slots:
    void exactOnlyAndNoPreviousOwnerSnapshotReuse();
    void noPreflightOrPartialEvidenceCaching();
    void deterministicConflictPoisonsWholeGeneration();
    void boundedStorageAndTieBytes();
};
void M10CompletedCacheTest::
exactOnlyAndNoPreviousOwnerSnapshotReuse() {
    m10::ExactCompletedEvaluationCache cache{{0xAF,0x51}};
    QCOMPARE(cache.insert_completed(key(),score(),true),
             m10::CacheInsert::inserted);
    const auto result=cache.find(key());
    QVERIFY(result);
    QCOMPARE(result->total_j,0.2);
    auto changed=key();
    changed.candidate_execution_content={0x02};
    QVERIFY(!cache.find(changed));
    changed=key();
    changed.immutable_generation_and_policy={0xAF,0x52};
    QVERIFY(!cache.find(changed));
    QVERIFY(!cache.authorizes_product_publication());
}
void M10CompletedCacheTest::noPreflightOrPartialEvidenceCaching() {
    m10::ExactCompletedEvaluationCache cache{{0xAF,0x51}};
    QCOMPARE(cache.insert_completed(key(),score(),false),
             m10::CacheInsert::invalid_or_unattested);
    QVERIFY(!cache.find(key()));
    auto mismatch=score();
    mismatch.canonical_current_tuple={0x02};
    QCOMPARE(cache.insert_completed(key(),mismatch,true),
             m10::CacheInsert::invalid_or_unattested);
    auto malformed=key();
    malformed.candidate_execution_content.clear();
    QCOMPARE(cache.insert_completed(malformed,score(),true),
             m10::CacheInsert::invalid_or_unattested);
}
void M10CompletedCacheTest::
deterministicConflictPoisonsWholeGeneration() {
    m10::ExactCompletedEvaluationCache cache{{0xAF,0x51}};
    QCOMPARE(cache.insert_completed(key(),score(),true),
             m10::CacheInsert::inserted);
    QCOMPARE(cache.insert_completed(key(),score(),true),
             m10::CacheInsert::already_exact);
    QCOMPARE(cache.insert_completed(key(),score(1,0.1),true),
             m10::CacheInsert::deterministic_result_collision);
    QVERIFY(cache.poisoned());
    QVERIFY(!cache.find(key()));
    QCOMPARE(cache.insert_completed(key(),score(),true),
             m10::CacheInsert::generation_poisoned);
}
void M10CompletedCacheTest::boundedStorageAndTieBytes() {
    m10::ExactCompletedEvaluationCache cache{{0xAF,0x51}};
    for(std::uint32_t i=1;i<=800U;++i) {
        m10::ExactExecutionCacheKey k{
            {0xAF,0x51},{
                static_cast<std::uint8_t>(i>>8),
                static_cast<std::uint8_t>(i&0xFF)},
            {static_cast<std::uint8_t>(i>>8),
             static_cast<std::uint8_t>(i&0xFF)},
            {0x81}};
        m10::MeasuredOrderKey score{
            0.2,0.2,0.0,1,k.canonical_current_tuple,{0x81}};
        QCOMPARE(cache.insert_completed(k,score,true),
                 m10::CacheInsert::inserted);
    }
    QCOMPARE(cache.size(),std::size_t{800});
    auto extra=key(2);
    QCOMPARE(cache.insert_completed(extra,score(2),true),
             m10::CacheInsert::full_capacity);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M10CompletedCacheTest)
#include "test_l3_m10_exact_completed_result_cache.moc"
