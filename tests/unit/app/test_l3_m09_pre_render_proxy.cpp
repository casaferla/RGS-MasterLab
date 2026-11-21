#include "internal/l3_m09_pre_render_proxy.hpp"
#include <QtTest/QTest>
#include <cmath>
#include <cstdint>
#include <vector>
namespace rgsml::tests {
namespace {
namespace m09=rgsml::app::l3_automatch::m09_frozen;
[[nodiscard]] m09::ProxyOperands bound(
    double target,double displacement,double complexity,
    std::uint32_t modules,std::uint32_t bands,
    std::vector<std::uint8_t> key) {
    return {true,target,displacement,complexity,
            modules,bands,std::move(key)};
}
class M09ProxyTest final:public QObject {
    Q_OBJECT
private slots:
    void exactFrozenProxyFormulaAndQuantization();
    void missingOperandsStayUnboundNotZero();
    void deterministicTieOrderAndUnsignedKey();
    void noM10CapsOrProductPublication();
};
void M09ProxyTest::exactFrozenProxyFormulaAndQuantization() {
    const auto p=m09::make_pre_render_proxy(bound(
        0.75,0.50,1.0,1,2,{0x02}));
    QVERIFY(p.is_bound());
    QVERIFY(std::abs(*p.raw_value-0.77)<1e-12);
    QCOMPARE(*p.q1e9,std::int64_t{770000000});
    QVERIFY(!p.authorizes_seed_render_or_publishing());
}
void M09ProxyTest::missingOperandsStayUnboundNotZero() {
    auto in=bound(0.75,0.5,1,1,2,{0x01});
    in.m10_topology_complexity.reset();
    const auto missing=m09::make_pre_render_proxy(in);
    QCOMPARE(missing.reason,m09::ProxyReason::unbound_missing_exact_operand);
    QVERIFY(!missing.raw_value);
    in=bound(0.75,0.5,1,1,2,{0x01});
    in.exact_m05_m08_and_m10_operands_attested=false;
    QVERIFY(!m09::make_pre_render_proxy(in).is_bound());
    in=bound(-1.0,0.5,1,1,2,{0x01});
    QCOMPARE(m09::make_pre_render_proxy(in).reason,
             m09::ProxyReason::invalid_numeric_or_canonical_identity);
    in=bound(0.75,0.5,1,1,2,{});
    QCOMPARE(m09::make_pre_render_proxy(in).reason,
             m09::ProxyReason::invalid_numeric_or_canonical_identity);
}
void M09ProxyTest::deterministicTieOrderAndUnsignedKey() {
    std::vector<m09::PreRenderTopologyProxy> p;
    p.push_back(m09::make_pre_render_proxy(
        bound(0.75,0.50,1.0,2,3,{0xFF})));
    p.push_back(m09::make_pre_render_proxy(
        bound(0.75,0.50,1.0,1,3,{0xFF})));
    p.push_back(m09::make_pre_render_proxy(
        bound(0.75,0.50,1.0,1,2,{0xFF})));
    p.push_back(m09::make_pre_render_proxy(
        bound(0.75,0.50,1.0,1,2,{0x80})));
    p.push_back(m09::make_pre_render_proxy(
        bound(0.75,0.50,1.0,1,2,{0x01})));
    p.push_back(m09::make_pre_render_proxy(
        bound(0.75,0.50,1.0,1,2,{0x01})));
    auto unbound=bound(0.75,0.5,1.0,1,2,{0x00});
    unbound.m10_seed_normalized_parameter_displacement.reset();
    p.push_back(m09::make_pre_render_proxy(unbound));
    const auto ordered=m09::partition_and_rank_proxy(p);
    const std::vector<std::size_t> expected{4,5,3,2,1,0};
    QCOMPARE(ordered.fully_bound_sorted_indices,expected);
    const std::vector<std::size_t> missing{6};
    QCOMPARE(ordered.unbound_original_order_indices,missing);
}
void M09ProxyTest::noM10CapsOrProductPublication() {
    std::vector<m09::PreRenderTopologyProxy> p;
    for(std::uint32_t i=0;i<40U;++i)
        p.push_back(m09::make_pre_render_proxy(
            bound(static_cast<double>(40-i),0.0,0.0,1,0,
                  {static_cast<std::uint8_t>(i)})));
    const auto result=m09::partition_and_rank_proxy(p);
    QCOMPARE(result.fully_bound_sorted_indices.size(),std::size_t{40});
    QVERIFY(!result.authorizes_product_publication());
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M09ProxyTest)
#include "test_l3_m09_pre_render_proxy.moc"
