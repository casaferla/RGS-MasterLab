#include "internal/l3_m10_p04_numeric_domain_consumer.hpp"
#include <QtTest/QTest>
#include <array>
#include <cstdint>
#include <optional>
#include <vector>
namespace rgsml::tests {
namespace {
namespace m09=rgsml::app::l3_automatch::m09_frozen;
namespace m10=rgsml::app::l3_automatch::m10_frozen;
[[nodiscard]] m10::ConsumerInput candidate(
    double proxy_value,std::uint8_t key,bool available=true)
{
    m09::ProxyOperands input{true,proxy_value,0.0,0.0,1,0,{key}};
    return {m09::make_pre_render_proxy(input),available};
}
class M10DomainConsumerTest final:public QObject {
    Q_OBJECT
private slots:
    void capEightDoesNotReserveNoneSlot();
    void unavailableWinnerRemainsSelectedNoBackfill();
    void anyUnboundCandidateBlocksEntireNumericRanking();
    void emptyAndInvalidStreamRemainDistinct();
};
void M10DomainConsumerTest::capEightDoesNotReserveNoneSlot() {
    std::vector<m10::ConsumerInput> items;
    for(std::uint8_t i=0;i<12;++i)
        items.push_back(candidate(12.0-i,i));
    const auto result=m10::select_domain_top8(items);
    QCOMPARE(result.status,m10::ConsumerStatus::ready);
    QCOMPARE(result.numeric_selected_indices.size(),std::size_t{8});
    QCOMPARE(result.ledger.size(),std::size_t{12});
    for(std::size_t i=0;i<8;++i) {
        QCOMPARE(result.numeric_selected_indices[i],11U-i);
        QCOMPARE(result.ledger[11U-i].reason,m10::ConsumerReason::selected);
    }
    for(std::size_t i=0;i<4;++i)
        QCOMPARE(result.ledger[i].reason,
                 m10::ConsumerReason::domain_consumer_cap_excluded);
    QVERIFY(!result.authorizes_audible_master());
}
void M10DomainConsumerTest::unavailableWinnerRemainsSelectedNoBackfill() {
    std::vector<m10::ConsumerInput> items;
    for(std::uint8_t i=0;i<9;++i)
        items.push_back(candidate(static_cast<double>(i),i,i!=0));
    const auto result=m10::select_domain_top8(items);
    QCOMPARE(result.status,m10::ConsumerStatus::ready);
    QCOMPARE(result.numeric_selected_indices.size(),std::size_t{8});
    QCOMPARE(result.numeric_selected_indices.front(),std::size_t{0});
    QVERIFY(result.ledger[0].selected_but_unavailable);
    QCOMPARE(result.ledger[8].reason,
             m10::ConsumerReason::domain_consumer_cap_excluded);
}
void M10DomainConsumerTest::anyUnboundCandidateBlocksEntireNumericRanking() {
    std::vector<m10::ConsumerInput> items;
    for(std::uint8_t i=0;i<9;++i)
        items.push_back(candidate(static_cast<double>(i),i));
    items[8].proxy.reason=m09::ProxyReason::unbound_missing_exact_operand;
    items[8].proxy.q1e9.reset();
    const auto result=m10::select_domain_top8(items);
    QCOMPARE(result.status,m10::ConsumerStatus::ranking_unready);
    QVERIFY(result.numeric_selected_indices.empty());
    QCOMPARE(result.ledger.size(),items.size());
    for(const auto& item:result.ledger)
        QCOMPARE(item.reason,m10::ConsumerReason::ranking_unready);
}
void M10DomainConsumerTest::emptyAndInvalidStreamRemainDistinct() {
    const std::vector<m10::ConsumerInput> empty{};
    QCOMPARE(m10::select_domain_top8(empty).status,
             m10::ConsumerStatus::empty_stream);
    auto invalid=candidate(0.0,2);
    invalid.proxy.unsigned_topology_key.clear();
    QCOMPARE(m10::select_domain_top8(
        std::array<m10::ConsumerInput,1>{invalid}).status,
        m10::ConsumerStatus::invalid_canonical_stream);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M10DomainConsumerTest)
#include "test_l3_m10_p04_numeric_domain_consumer.moc"
