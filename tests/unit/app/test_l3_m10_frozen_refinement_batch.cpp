#include "internal/l3_m10_frozen_refinement_batch.hpp"
#include <QtTest/QTest>
#include <atomic>
#include <cstddef>
#include <cstdint>
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
    void partialBudgetKeepsMeasuredWinnerNo801stAttempt();
    void staleInventoryOrUnreadySeedNeverCallsEvaluator();
    void cancellationNeverClaimsCompletedMaster();
};
void M10RefinementBatchTest::
sameFrozenStepsAcrossMeasuredTop2AndSharedBudget() {
    const std::vector<m10::SeedTrial> seeds{
        trial(7,0.7,7),trial(9,0.8,9)};
    const auto coords=inventories(2);
    std::vector<std::size_t> called_ordinals;
    const auto outcome=m10::run_frozen_top2_refinements(
        seeds,coords,0,true,
        [&](std::size_t ordinal,const std::vector<double>& u,bool allow) {
            QVERIFY(allow);
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
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M10RefinementBatchTest)
#include "test_l3_m10_frozen_refinement_batch.moc"
