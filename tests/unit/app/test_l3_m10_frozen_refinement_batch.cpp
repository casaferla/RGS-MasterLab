#include "internal/l3_m10_frozen_refinement_batch.hpp"
#include "internal/l3_m10_frozen_total_cost.hpp"
#include <QtTest/QTest>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
namespace rgsml::tests {
namespace {
namespace m10=rgsml::app::l3_automatch::m10_frozen;
[[nodiscard]] m10::MeasuredOrderKey score(
    double total,std::uint8_t id) {
    return {total,total,0.0,1,{id},{0x71}};
}
[[nodiscard]] m10::SeedTrial trial(
    std::size_t ordinal,double value,std::uint8_t id) {
    return {ordinal,m10::SeedTrialKind::attempted_complete,
            score(value,id),true,true};
}
[[nodiscard]] std::vector<std::vector<m10::Coordinate>> inventories(
    std::size_t n) {
    std::vector<std::vector<m10::Coordinate>> out;
    for(std::size_t i=0;i<n;++i)
        out.push_back({{{static_cast<std::uint8_t>(i+1)},
                        {0x11},0.5}});
    return out;
}
class M10RefinementBatchTest final:public QObject {
    Q_OBJECT
private slots:
    void sameFrozenStepsAcrossMeasuredTop2AndSharedBudget();
    void exactNativeLowerBoundNeverPrunesEqualQuantizedCost();
    void partialBudgetKeepsMeasuredWinnerNo801stAttempt();
    void staleInventoryOrUnreadySeedNeverCallsEvaluator();
    void cancellationNeverClaimsCompletedMaster();
    void twoRunsReuseOnlyExactlyIdenticalCompletedCoordinates();
    void exhaustedSeedBudgetAndInvalidTrialRemainDistinct();
};
void M10RefinementBatchTest::exactNativeLowerBoundNeverPrunesEqualQuantizedCost()
{
    const std::vector<m10::ActiveReferenceCoordinate> moved{{0.75,0.5,1.0}};
    const std::vector<m10::ActiveReferenceCoordinate> seed{{0.5,0.5,1.0}};
    const auto current=m10::MeasuredOrderKey{0.000625,0.0,0.0,1U,{1},{2}};
    QVERIFY(m10::native_cost_bound_cannot_beat(moved,1U,0U,current));
    QVERIFY(!m10::native_cost_bound_cannot_beat(seed,1U,0U,current));
    auto tied=current;
    tied.total_j+=0.02*0.0625;
    QVERIFY(!m10::native_cost_bound_cannot_beat(moved,1U,0U,tied));
    tied.total_j-=0.1e-9;
    QVERIFY(!m10::native_cost_bound_cannot_beat(moved,1U,0U,tied));
    QVERIFY(!m10::native_cost_bound_cannot_beat(moved,9U,0U,current));
    QVERIFY(m10::native_cost_bound_cannot_beat(seed,2U,0U,current));
}
void M10RefinementBatchTest::
sameFrozenStepsAcrossMeasuredTop2AndSharedBudget() {
    const std::vector<m10::SeedTrial> seeds{
        trial(7,0.7,7),trial(9,0.8,9)};
    const auto coords=inventories(2);
    std::vector<std::size_t> called_ordinals;
    const auto outcome=m10::run_frozen_top2_refinements(
        seeds,coords,0,true,
        [&](std::size_t ordinal,const std::vector<double>& u,bool allow) {
            if(!allow)
                return m10::Evaluation{
                    m10::EvaluationKind::needs_new_attempt,{}};
            called_ordinals.push_back(ordinal);
            const double target=ordinal==7?0.75:0.25;
            const double d=u[0]-target;
            return m10::Evaluation{
                m10::EvaluationKind::attempted_complete,
                score(d*d,static_cast<std::uint8_t>(ordinal))};
        });
    QCOMPARE(outcome.reason,m10::RefinementBatchReason::complete);
    QCOMPARE(outcome.candidate_refinements.size(),std::size_t{2});
    QCOMPARE(outcome.total_attempted,std::uint32_t{18});
    QCOMPARE(called_ordinals.size(),std::size_t{16});
    QCOMPARE(called_ordinals.front(),std::size_t{7});
    QCOMPARE(called_ordinals.back(),std::size_t{9});
    QVERIFY(outcome.winning_refinement_index);
    QVERIFY(!outcome.authorizes_product_publication());
}
void M10RefinementBatchTest::
partialBudgetKeepsMeasuredWinnerNo801stAttempt() {
    const std::vector<m10::SeedTrial> seeds{trial(7,1.0,7)};
    const auto coords=inventories(1);
    std::uint32_t fresh_attempts=0U;
    const auto outcome=m10::run_frozen_top2_refinements(
        seeds,coords,799,true,
        [&](std::size_t,const std::vector<double>& u,bool allow) {
            if(allow) {
                ++fresh_attempts;
                return m10::Evaluation{
                    m10::EvaluationKind::attempted_failure,{}};
            }
            if(u[0]==0.25)
                return m10::Evaluation{
                    m10::EvaluationKind::completed_cache_hit,
                    score(0.1,7)};
            return m10::Evaluation{
                m10::EvaluationKind::needs_new_attempt,{}};
        });
    QCOMPARE(outcome.total_attempted,std::uint32_t{800});
    QCOMPARE(fresh_attempts,std::uint32_t{0});
    QCOMPARE(outcome.reason,m10::RefinementBatchReason::budget_exhausted);
    QVERIFY(outcome.winning_refinement_index);
    QCOMPARE(outcome.candidate_refinements[0].numerical.winner.total_j,
             0.1);
}
void M10RefinementBatchTest::
staleInventoryOrUnreadySeedNeverCallsEvaluator() {
    const std::vector<m10::SeedTrial> seeds{trial(7,0.7,7)};
    auto coords=inventories(1);
    std::uint32_t calls=0;
    const auto evaluate=[&](std::size_t,const std::vector<double>&,
                            bool) {
        ++calls;
        return m10::Evaluation{m10::EvaluationKind::attempted_failure,{}};
    };
    QCOMPARE(m10::run_frozen_top2_refinements(
        seeds,coords,0,false,evaluate).reason,
        m10::RefinementBatchReason::invalid_inventory);
    coords.clear();
    QCOMPARE(m10::run_frozen_top2_refinements(
        seeds,coords,0,true,evaluate).reason,
        m10::RefinementBatchReason::invalid_inventory);
    auto incomplete=seeds;
    incomplete[0].kind=m10::SeedTrialKind::not_evaluated;
    incomplete[0].frozen_measured.reset();
    coords=inventories(1);
    QCOMPARE(m10::run_frozen_top2_refinements(
        incomplete,coords,0,true,evaluate).reason,
        m10::RefinementBatchReason::no_complete_seed_or_stale_source);
    QCOMPARE(calls,std::uint32_t{0});
}
void M10RefinementBatchTest::cancellationNeverClaimsCompletedMaster() {
    const std::vector<m10::SeedTrial> seeds{trial(7,0.7,7)};
    const auto coords=inventories(1);
    std::atomic<bool> cancelled{true};
    const auto outcome=m10::run_frozen_top2_refinements(
        seeds,coords,0,true,
        [](std::size_t,const std::vector<double>&,bool) {
            return m10::Evaluation{
                m10::EvaluationKind::attempted_failure,{}};
        },&cancelled);
    QCOMPARE(outcome.reason,m10::RefinementBatchReason::cancelled);
    QVERIFY(!outcome.winning_refinement_index);
}
void M10RefinementBatchTest::
twoRunsReuseOnlyExactlyIdenticalCompletedCoordinates() {
    const std::vector<m10::SeedTrial> seeds{trial(7,1.0,7)};
    const auto coords=inventories(1);
    m10::ExactCompletedEvaluationCache cache{{0xAF,0x71}};
    std::uint32_t physical_renders=0U;
    const auto make_key=[](std::size_t ordinal,
                           const std::vector<double>& u)
        -> std::optional<m10::ExactExecutionCacheKey> {
        const auto bits=std::bit_cast<std::uint64_t>(u[0]);
        std::vector<std::uint8_t> exact{
            static_cast<std::uint8_t>(ordinal)};
        for(unsigned i=0;i<8U;++i)
            exact.push_back(static_cast<std::uint8_t>(
                (bits>>(8U*i))&0xffU));
        return m10::ExactExecutionCacheKey{
            {0xAF,0x71},exact,exact,{0x88}};
    };
    const auto render=[&](std::size_t ordinal,
                          const std::vector<double>& u) {
        ++physical_renders;
        const double d=u[0]-0.75;
        const auto key=make_key(ordinal,u);
        return m10::FreshCompleteEvidence{
            m10::MeasuredOrderKey{d*d,d*d,0.0,1,
                                  key->canonical_current_tuple,{0x88}},
            true};
    };
    const auto allowed=[](std::size_t,const std::vector<double>&) {
        return true;
    };
    const auto first=m10::run_frozen_top2_refinements_with_exact_cache(
        seeds,coords,0,true,cache,make_key,allowed,render);
    QCOMPARE(first.reason,m10::RefinementBatchReason::complete);
    const auto attempted_first=first.total_attempted;
    QVERIFY(attempted_first>1U);
    const auto rendered_first=physical_renders;
    const auto second=m10::run_frozen_top2_refinements_with_exact_cache(
        seeds,coords,0,true,cache,make_key,allowed,render);
    QCOMPARE(second.reason,m10::RefinementBatchReason::complete);
    QCOMPARE(second.total_attempted,std::uint32_t{1});
    QCOMPARE(physical_renders,rendered_first);
    QVERIFY(second.winning_refinement_index);
    QVERIFY(!second.authorizes_product_publication());
}
void M10RefinementBatchTest::
exhaustedSeedBudgetAndInvalidTrialRemainDistinct() {
    const std::vector<m10::SeedTrial> seeds{
        trial(7,0.7,7),trial(9,0.8,9)};
    const auto coords=inventories(2);
    const auto never=[](std::size_t,const std::vector<double>&,bool) {
        return m10::Evaluation{
            m10::EvaluationKind::attempted_failure,{}};
    };
    const auto over=m10::run_frozen_top2_refinements(
        seeds,coords,799,true,never);
    QCOMPARE(over.reason,
             m10::RefinementBatchReason::seed_attempt_budget_exceeded);
    QVERIFY(over.candidate_refinements.empty());
    auto malformed=seeds;
    malformed[1].frozen_measured.reset();
    const auto bad=m10::run_frozen_top2_refinements(
        malformed,coords,0,true,never);
    QCOMPARE(bad.reason,m10::RefinementBatchReason::invalid_seed_trial);
    QVERIFY(bad.candidate_refinements.empty());
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M10RefinementBatchTest)
#include "test_l3_m10_frozen_refinement_batch.moc"
