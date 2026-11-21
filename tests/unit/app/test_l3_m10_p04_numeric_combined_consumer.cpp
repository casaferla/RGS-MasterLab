#include "internal/l3_m10_p04_numeric_domain_consumer.hpp"
#include <QtTest/QTest>
#include <cstdint>
#include <vector>
namespace rgsml::tests {
namespace {
namespace m09=rgsml::app::l3_automatch::m09_frozen;
namespace m10=rgsml::app::l3_automatch::m10_frozen;
[[nodiscard]] m10::CombinedConsumerInput combined(
    double score,std::uint8_t id,std::size_t n=20,bool available=true) {
    m09::ProxyOperands proxy{true,score,0.0,0.0,2,1,{id}};
    return {{m09::make_pre_render_proxy(proxy),available},n,true};
}
class M10CombinedConsumerTest final:public QObject {
    Q_OBJECT
private slots:
    void cap32AndOver48ExclusionPrecedeRanking();
    void missingProxyPreventsPartialTop32();
    void noAvailabilityBackfillOfCandidate33();
    void invalidInventoryRemainsInvalidNotNoCorrection();
};
void M10CombinedConsumerTest::cap32AndOver48ExclusionPrecedeRanking() {
    std::vector<m10::CombinedConsumerInput> input;
    for(std::uint8_t i=0;i<40;++i)
        input.push_back(combined(40.0-i,i));
    input[39].exact_active_coordinate_count=49;
    const auto r=m10::select_combined_top32(input);
    QCOMPARE(r.status,m10::ConsumerStatus::ready);
    QCOMPARE(r.numeric_selected_indices.size(),std::size_t{32});
    QCOMPARE(r.numeric_selected_indices[0],std::size_t{38});
    QCOMPARE(r.ledger[39].reason,
             m10::ConsumerReason::topology_parameter_budget_exceeded);
    QCOMPARE(r.ledger[0].reason,
             m10::ConsumerReason::combined_consumer_cap_excluded);
    QVERIFY(!r.authorizes_seed_render());
}
void M10CombinedConsumerTest::missingProxyPreventsPartialTop32() {
    std::vector<m10::CombinedConsumerInput> input;
    for(std::uint8_t i=0;i<40;++i)
        input.push_back(combined(40.0-i,i));
    input[39].candidate.proxy.q1e9.reset();
    input[39].candidate.proxy.reason=
        m09::ProxyReason::unbound_missing_exact_operand;
    const auto r=m10::select_combined_top32(input);
    QCOMPARE(r.status,m10::ConsumerStatus::ranking_unready);
    QVERIFY(r.numeric_selected_indices.empty());
    QCOMPARE(r.ledger[39].reason,m10::ConsumerReason::ranking_unready);
}
void M10CombinedConsumerTest::noAvailabilityBackfillOfCandidate33() {
    std::vector<m10::CombinedConsumerInput> input;
    for(std::uint8_t i=0;i<36;++i)
        input.push_back(combined(static_cast<double>(i),i));
    input[0].candidate.available_currently=false;
    const auto r=m10::select_combined_top32(input);
    QCOMPARE(r.status,m10::ConsumerStatus::ready);
    QCOMPARE(r.numeric_selected_indices.size(),std::size_t{32});
    QCOMPARE(r.numeric_selected_indices.front(),std::size_t{0});
    QCOMPARE(r.ledger[0].reason,m10::ConsumerReason::selected);
    QVERIFY(r.ledger[0].selected_but_unavailable);
    QCOMPARE(r.ledger[32].reason,
             m10::ConsumerReason::combined_consumer_cap_excluded);
}
void M10CombinedConsumerTest::
invalidInventoryRemainsInvalidNotNoCorrection() {
    std::vector<m10::CombinedConsumerInput> input{
        combined(0.1,1)};
    input[0].coordinate_inventory_and_legal_bounds_attested=false;
    const auto r=m10::select_combined_top32(input);
    QCOMPARE(r.status,m10::ConsumerStatus::invalid_canonical_stream);
    QVERIFY(r.numeric_selected_indices.empty());
    QVERIFY(r.ledger.empty());
    input[0].coordinate_inventory_and_legal_bounds_attested=true;
    input[0].exact_active_coordinate_count=49;
    const auto excluded=m10::select_combined_top32(input);
    QCOMPARE(excluded.status,m10::ConsumerStatus::empty_stream);
    QCOMPARE(excluded.ledger.front().reason,
             m10::ConsumerReason::topology_parameter_budget_exceeded);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M10CombinedConsumerTest)
#include "test_l3_m10_p04_numeric_combined_consumer.moc"
