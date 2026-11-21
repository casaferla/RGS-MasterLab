#include "internal/l3_m10_measured_seed_shortlist.hpp"
#include <QtTest/QTest>
#include <vector>
namespace rgsml::tests {
namespace {
namespace m10=rgsml::app::l3_automatch::m10_frozen;
[[nodiscard]] m10::MeasuredOrderKey key(
    double j,double target,std::uint8_t b) {
    return {j,target,0.0,1,{b},{99}};
}
[[nodiscard]] m10::SeedTrial complete(
    std::size_t id,double total,double target,std::uint8_t byte,
    m10::SeedTrialKind kind=m10::SeedTrialKind::attempted_complete) {
    return {id,kind,key(total,target,byte),true,true};
}
class M10ShortlistTest final:public QObject {
    Q_OBJECT
private slots:
    void measuredTargetRAndCanonicalOrderWithoutProxy();
    void cacheHitDoesNotSpendSolverBudget();
    void hardConstraintAndUnfinishedResultFailClosed();
    void failurePreflightAndBudgetAccounting();
};
void M10ShortlistTest::measuredTargetRAndCanonicalOrderWithoutProxy() {
    const std::vector<m10::SeedTrial> input{
        complete(0,1.0,0.5,10),
        complete(1,0.5,0.5,10),
        complete(2,0.5,0.4,99),
        complete(3,0.5,0.4,2),
        complete(4,0.8,0.8,2)};
    const auto r=m10::select_measured_seed_top2(input,0);
    QCOMPARE(r.reason,m10::ShortlistReason::ready);
    QCOMPARE(r.best_trial_indices.size(),std::size_t{2});
    QCOMPARE(r.best_trial_indices[0],std::size_t{3});
    QCOMPARE(r.best_trial_indices[1],std::size_t{2});
    QCOMPARE(r.attempted_total,std::uint32_t{5});
    QVERIFY(!r.authorizes_publish_or_verify());
}
void M10ShortlistTest::cacheHitDoesNotSpendSolverBudget() {
    std::vector<m10::SeedTrial> input{
        complete(0,0.2,0.2,1,m10::SeedTrialKind::completed_cache_hit),
        complete(1,0.3,0.3,2)};
    const auto r=m10::select_measured_seed_top2(input,799);
    QCOMPARE(r.reason,m10::ShortlistReason::ready);
    QCOMPARE(r.attempted_total,std::uint32_t{800});
    QCOMPARE(r.completed_cache_hits,std::uint32_t{1});
    QCOMPARE(r.best_trial_indices.front(),std::size_t{0});
}
void M10ShortlistTest::hardConstraintAndUnfinishedResultFailClosed() {
    std::vector<m10::SeedTrial> input{
        complete(0,0.2,0.2,1),complete(1,0.3,0.3,2)};
    input[1].kind=m10::SeedTrialKind::not_evaluated;
    input[1].frozen_measured.reset();
    QCOMPARE(m10::select_measured_seed_top2(input,0).reason,
             m10::ShortlistReason::unready_missing_trial);
    input[1]=complete(1,0.3,0.3,2);
    input[1].all_hard_constraints_passed=false;
    QCOMPARE(m10::select_measured_seed_top2(input,0).reason,
             m10::ShortlistReason::invalid_selected_identity_or_result);
    input[1]=complete(0,0.3,0.3,2);
    QCOMPARE(m10::select_measured_seed_top2(input,0).reason,
             m10::ShortlistReason::invalid_selected_identity_or_result);
}
void M10ShortlistTest::failurePreflightAndBudgetAccounting() {
    const std::vector<m10::SeedTrial> input{
        complete(0,0.2,0.2,1),
        {1,m10::SeedTrialKind::attempted_failure,{},false,false},
        {2,m10::SeedTrialKind::preflight_rejected,{},false,false}};
    const auto r=m10::select_measured_seed_top2(input,798);
    QCOMPARE(r.reason,m10::ShortlistReason::ready);
    QCOMPARE(r.attempted_total,std::uint32_t{800});
    QCOMPARE(r.attempted_failures,std::uint32_t{1});
    QCOMPARE(r.preflight_rejections,std::uint32_t{1});
    QCOMPARE(r.best_trial_indices.size(),std::size_t{1});
    const auto over=m10::select_measured_seed_top2(input,799);
    QCOMPARE(over.reason,m10::ShortlistReason::budget_exceeded);
    QVERIFY(over.best_trial_indices.empty());
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M10ShortlistTest)
#include "test_l3_m10_measured_seed_shortlist.moc"
