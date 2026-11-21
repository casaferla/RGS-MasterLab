#include "internal/l3_m09_p01_weighted_atom_fit.hpp"
#include <QtTest/QTest>
#include <array>
#include <cmath>
#include <limits>
#include <vector>
namespace rgsml::tests {
namespace {
namespace m09=rgsml::app::l3_automatch::m09_frozen;
[[nodiscard]] std::array<m09::AtomProfileCell,3> example() {
    return {{{2.0,1.0,0.25},{4.0,2.0,0.50},{2.0,1.0,0.25}}};
}
[[nodiscard]] m09::AtomFitEvidence atom(
    m09::AtomFilter filter,double freq,double q,
    m09::AtomProjection projection,std::vector<double> response) {
    return {filter,freq,q,std::move(projection),
            std::move(response),true};
}
class M09WeightedAtomTest final:public QObject {
    Q_OBJECT
private slots:
    void frozenUnweightedG0AndLimits();
    void wholePopulationActualResponseWeightedFit();
    void errorStopsAndUnusableAtoms();
    void deterministicSelectionRanking();
};
void M09WeightedAtomTest::frozenUnweightedG0AndLimits() {
    auto cells=example();
    const auto p=m09::project_atom_unweighted_g0(cells,false);
    QCOMPARE(p.reason,m09::AtomSeedReason::ready);
    QCOMPARE(*p.raw_gain_db,2.0);
    QCOMPARE(*p.limited_gain_db,2.0);
    cells[0].residual_db=20.0;
    cells[1].residual_db=40.0;
    cells[2].residual_db=20.0;
    const auto limited=m09::project_atom_unweighted_g0(cells,false);
    QCOMPARE(limited.reason,m09::AtomSeedReason::seed_parameter_limited);
    QCOMPARE(*limited.limited_gain_db,3.0);
    QCOMPARE(*m09::project_atom_unweighted_g0(cells,true)
             .limited_gain_db,1.5);
    QVERIFY(!limited.authorizes_product_publication());
}
void M09WeightedAtomTest::wholePopulationActualResponseWeightedFit() {
    const auto cells=example();
    const auto g=m09::project_atom_unweighted_g0(cells,false);
    const auto fit=m09::evaluate_weighted_atom(cells,
        atom(m09::AtomFilter::bell,1000,0.707,g,{2.0,3.9,2.0}));
    QCOMPARE(fit.reason,m09::AtomSeedReason::ready);
    QVERIFY(fit.before_error&&fit.after_error);
    QVERIFY(std::abs(*fit.before_error-10.0)<1e-12);
    QVERIFY(std::abs(*fit.after_error-0.005)<1e-12);
    QVERIFY(*fit.relative_improvement>0.99);
    QVERIFY(!fit.authorizes_master_publication());
}
void M09WeightedAtomTest::errorStopsAndUnusableAtoms() {
    auto cells=example();
    for(auto& c:cells)c.atom_plus_one_db_response=0.0;
    QCOMPARE(m09::project_atom_unweighted_g0(cells,false).reason,
             m09::AtomSeedReason::unusable_zero_denominator);
    cells=example();
    const auto p=m09::project_atom_unweighted_g0(cells,false);
    auto missing=atom(m09::AtomFilter::bell,1000,1.0,p,{2.0,3.9});
    QCOMPARE(m09::evaluate_weighted_atom(cells,missing).reason,
             m09::AtomSeedReason::missing_exact_static_response);
    missing.exact_canonical_response_at_limited_gain={0.0,0.0,0.0};
    QCOMPARE(m09::evaluate_weighted_atom(cells,missing).reason,
             m09::AtomSeedReason::below_minimum_relative_improvement);
    missing.exact_canonical_response_at_limited_gain={-1,-1,-1};
    QCOMPARE(m09::evaluate_weighted_atom(cells,missing).reason,
             m09::AtomSeedReason::error_not_improved);
    missing.exact_canonical_response_at_limited_gain={2,4,2};
    missing.exact_response_content_and_gain_attested=false;
    QCOMPARE(m09::evaluate_weighted_atom(cells,missing).reason,
             m09::AtomSeedReason::missing_exact_static_response);
}
void M09WeightedAtomTest::deterministicSelectionRanking() {
    const auto cells=example();
    const auto p=m09::project_atom_unweighted_g0(cells,false);
    std::vector<m09::AtomFitResult> fits;
    for(const auto f:{m09::AtomFilter::high_shelf,
                      m09::AtomFilter::bell,
                      m09::AtomFilter::low_shelf}) {
        fits.push_back(m09::evaluate_weighted_atom(cells,
            atom(f,1000.0,0.5,p,{2.0,3.0,2.0})));
    }
    const auto best=m09::best_weighted_eligible_atom(fits);
    QVERIFY(best);
    QCOMPARE(*best,std::size_t{2}); // LOW_SHELF wins equal E
    fits[0].atom.exact_canonical_response_at_limited_gain={2,4,2};
    fits[0]=m09::evaluate_weighted_atom(cells,fits[0].atom);
    QCOMPARE(*m09::best_weighted_eligible_atom(fits),std::size_t{0});
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M09WeightedAtomTest)
#include "test_l3_m09_p01_weighted_atom_fit.moc"
