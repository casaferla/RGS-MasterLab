#include "internal/l3_m10_p04_exact_combined_binding.hpp"
#include <QtTest/QTest>
#include <cstdint>
#include <string>
#include <vector>

namespace rgsml::tests {
namespace {
namespace m09=rgsml::app::l3_automatch::m09_frozen;
namespace m10=rgsml::app::l3_automatch::m10_frozen;
[[nodiscard]] m09::ExactBytes bytes(const char* x) {
    m09::ExactBytes b;
    for(const auto* p=reinterpret_cast<const unsigned char*>(x);*p;++p)
        b.push_back(*p);
    return b;
}
[[nodiscard]] m10::BoundLegalCombinedCandidate candidate(
    std::uint8_t key,double proxy,bool available=true)
{
    m09::CandidateCarrier desc{{key},{0x51},{0x71},
                                bytes("snapshot-A"),{"source"}};
    m09::ProxyOperands p{true,proxy,0.0,0.0,1,1,{key}};
    return {desc,m09::make_pre_render_proxy(p),8,true,available};
}
class M10ExactCombinedTest final:public QObject {
    Q_OBJECT
private slots:
    void exactDuplicateProvenanceAndNoAvailabilityBackfill();
    void sameTopologyDifferentSeedPoisonsWholeGeneration();
    void inconsistentDuplicateNumericAnnotationFails();
    void missingProxyOrStaleSnapshotCannotProduceRankedSubset();
    void exactCoordinate48ExclusionRetainsOriginalLedger();
};
void M10ExactCombinedTest::
exactDuplicateProvenanceAndNoAvailabilityBackfill() {
    std::vector<m10::BoundLegalCombinedCandidate> input;
    for(std::uint8_t i=1;i<=35;++i)
        input.push_back(candidate(i,static_cast<double>(i),i!=1));
    auto dup=input[0];
    dup.description.ordered_provenance={"duplicate"};
    input.push_back(dup);
    const auto result=m10::bind_exact_legal_combined_and_select_top32(
        bytes("snapshot-A"),input,true);
    QCOMPARE(result.status,m10::BoundLegalStatus::selected);
    QCOMPARE(result.exact_descriptions.size(),std::size_t{35});
    QCOMPARE(result.exact_descriptions[0].ordered_provenance.size(),
             std::size_t{2});
    QCOMPARE(result.combined_consumer.numeric_selected_indices.size(),
             std::size_t{32});
    QCOMPARE(result.combined_consumer.numeric_selected_indices.front(),
             std::size_t{0});
    QVERIFY(result.combined_consumer.ledger[0].selected_but_unavailable);
    QCOMPARE(result.combined_consumer.ledger[32].reason,
             m10::ConsumerReason::combined_consumer_cap_excluded);
    QVERIFY(!result.authorizes_product_publication());
}
void M10ExactCombinedTest::
sameTopologyDifferentSeedPoisonsWholeGeneration() {
    auto first=candidate(1,0.1);
    auto collision=first;
    collision.description.seed_exact_content={0x52};
    const std::vector<m10::BoundLegalCombinedCandidate> input{
        first,collision};
    const auto r=m10::bind_exact_legal_combined_and_select_top32(
        bytes("snapshot-A"),input,true);
    QCOMPARE(r.status,m10::BoundLegalStatus::candidate_key_seed_collision);
    QVERIFY(r.exact_descriptions.empty());
    QVERIFY(r.combined_consumer.numeric_selected_indices.empty());
}
void M10ExactCombinedTest::
inconsistentDuplicateNumericAnnotationFails() {
    auto x=candidate(1,0.1);
    auto y=x;
    y.pre_render_proxy.raw_value=2.0;
    y.pre_render_proxy.q1e9=2000000000LL;
    const std::vector<m10::BoundLegalCombinedCandidate> input{x,y};
    const auto r=m10::bind_exact_legal_combined_and_select_top32(
        bytes("snapshot-A"),input,true);
    QCOMPARE(r.status,m10::BoundLegalStatus::inconsistent_exact_annotation);
    QVERIFY(r.exact_descriptions.empty());
}
void M10ExactCombinedTest::
missingProxyOrStaleSnapshotCannotProduceRankedSubset() {
    auto good=candidate(1,0.1);
    auto missing=candidate(2,0.2);
    missing.pre_render_proxy.reason=
        m09::ProxyReason::unbound_missing_exact_operand;
    missing.pre_render_proxy.raw_value.reset();
    missing.pre_render_proxy.q1e9.reset();
    const std::vector<m10::BoundLegalCombinedCandidate> input{good,missing};
    const auto result=m10::bind_exact_legal_combined_and_select_top32(
        bytes("snapshot-A"),input,true);
    QCOMPARE(result.status,m10::BoundLegalStatus::unbound_ranking);
    QVERIFY(result.combined_consumer.numeric_selected_indices.empty());
    const auto stale=m10::bind_exact_legal_combined_and_select_top32(
        bytes("snapshot-B"),input,true);
    QCOMPARE(stale.status,m10::BoundLegalStatus::invalid_or_stale_generation);
    QVERIFY(stale.exact_descriptions.empty());
    const auto untrusted=m10::bind_exact_legal_combined_and_select_top32(
        bytes("snapshot-A"),input,false);
    QCOMPARE(untrusted.status,
             m10::BoundLegalStatus::invalid_or_stale_generation);
}
void M10ExactCombinedTest::
exactCoordinate48ExclusionRetainsOriginalLedger() {
    auto a=candidate(1,0.1);
    auto b=candidate(2,0.2);
    a.exact_active_coordinate_count=49;
    const std::vector<m10::BoundLegalCombinedCandidate> input{a,b};
    const auto r=m10::bind_exact_legal_combined_and_select_top32(
        bytes("snapshot-A"),input,true);
    QCOMPARE(r.status,m10::BoundLegalStatus::selected);
    QCOMPARE(r.combined_consumer.numeric_selected_indices.size(),
             std::size_t{1});
    QCOMPARE(r.combined_consumer.numeric_selected_indices[0],
             std::size_t{1});
    QCOMPARE(r.combined_consumer.ledger[0].reason,
             m10::ConsumerReason::topology_parameter_budget_exceeded);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M10ExactCombinedTest)
#include "test_l3_m10_p04_exact_combined_binding.moc"
