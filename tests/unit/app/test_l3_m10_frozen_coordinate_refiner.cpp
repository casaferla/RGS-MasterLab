#include "internal/l3_m10_frozen_coordinate_refiner.hpp"
#include "internal/l3_m10_frozen_coordinate_transform.hpp"
#include <QtTest/QTest>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace rgsml::tests {
namespace {
using namespace app::l3_automatch::m10_frozen;
[[nodiscard]] MeasuredOrderKey score(double total,double target=0.0,
                                     double regularization=0.0,
                                     std::uint32_t modules=1,
                                     std::uint8_t canonical_byte=7) {
    return MeasuredOrderKey{total,target,regularization,modules,
                            {canonical_byte},{99}};
}
[[nodiscard]] Coordinate coord(std::uint8_t node,std::uint8_t field,
                                double value=0.5) {
    return Coordinate{{node},{field},value};
}
class FrozenM10RefinerTest final:public QObject {
    Q_OBJECT
private slots:
    void quantizationTieAndFrozenTotalOrder();
    void exactP01TransformsAndNoClamps();
    void sameCenterMinusThenPlusAndExactlyFourSteps();
    void exhaustedBudgetRetainsBestAndAllowsExactCache();
    void malformedEvidenceAndCancellation();
    void maximum48CoordinatesDeterministicOrder();
    void invalidDirectionsNeverSpendBudgetAndExactCacheIsFree();
};
void FrozenM10RefinerTest::exactP01TransformsAndNoClamps() {
    const std::array<std::tuple<Transform,double,double>,5> specs{{
        {Transform::additive_or_linear,-3.0,3.0},
        {Transform::frequency_log2,40.0,12000.0},
        {Transform::positive_time_or_q_log,0.1,500.0},
        {Transform::ratio_log_from_one,1.0,20.0},
        {Transform::zero_inclusive_time,0.0,20.0}
    }};
    for(const auto& [kind,lo,hi]:specs) {
        const auto x=CoordinateTransform::create(kind,lo,hi);
        QVERIFY(x);
        QCOMPARE(*x->to_normalized(lo),0.0);
        QCOMPARE(*x->to_normalized(hi),1.0);
        QCOMPARE(*x->from_normalized(0.0),lo);
        QCOMPARE(*x->from_normalized(1.0),hi);
        QVERIFY(!x->from_normalized(-0.001));
        QVERIFY(!x->from_normalized(1.001));
        QVERIFY(!x->to_normalized(lo-0.01));
        QVERIFY(!x->to_normalized(hi+0.01));
        for(const double u:{0.125,0.25,0.5,0.75,0.875}) {
            const auto value=x->from_normalized(u);
            QVERIFY(value);
            const auto roundtrip=x->to_normalized(*value);
            QVERIFY(roundtrip);
            QVERIFY(std::abs(*roundtrip-u)<1e-12);
        }
    }
    QVERIFY(!CoordinateTransform::create(
        Transform::frequency_log2,0.0,20000.0));
    QVERIFY(!CoordinateTransform::create(
        Transform::ratio_log_from_one,0.5,20.0));
    QVERIFY(!CoordinateTransform::create(
        Transform::positive_time_or_q_log,-1.0,10.0));
    QVERIFY(!CoordinateTransform::create(
        Transform::zero_inclusive_time,1.0,20.0));
    QVERIFY(!CoordinateTransform::create(
        Transform::additive_or_linear,1.0,1.0));
    QVERIFY(!CoordinateTransform::create(
        Transform::additive_or_linear,
        std::numeric_limits<double>::quiet_NaN(),1.0));
}
void FrozenM10RefinerTest::quantizationTieAndFrozenTotalOrder() {
    QCOMPARE(*quantize_q1e9(2.5e-9),std::int64_t{2});
    QCOMPARE(*quantize_q1e9(3.5e-9),std::int64_t{4});
    QCOMPARE(*quantize_q1e9(0.5e-9),std::int64_t{0});
    QVERIFY(!quantize_q1e9(-1.0));
    QVERIFY(!quantize_q1e9(
        std::numeric_limits<double>::infinity()));
    QVERIFY(!quantize_q1e9(1e60));
    const auto a=score(1.0,0.5,0.3,2,22);
    const auto b=score(1.0000000001,0.6,0.2,1,7);
    QVERIFY(compare_measured(a,b)<0); // equal quantized J, Jtarget
    QVERIFY(compare_measured(b,a)>0);
    QVERIFY(compare_measured(a,a)==0);
    auto c=a;
    c.processing_r=0.4;
    QVERIFY(compare_measured(a,c)<0);
    c=a;c.active_owned_modules=3;
    QVERIFY(compare_measured(a,c)<0);
    c=a;c.canonical_current_tuple={254};
    QVERIFY(compare_measured(a,c)<0); // unsigned 22 < 254
    c=a;c.canonical_current_tuple={22};
    c.topology_evidence_identity={100};
    QVERIFY(compare_measured(a,c)<0);
}
void FrozenM10RefinerTest::sameCenterMinusThenPlusAndExactlyFourSteps() {
    std::vector<std::vector<double>> observed;
    const auto eval=[&](const std::vector<double>& u,bool can_render) {
        if(!can_render)
            return Evaluation{EvaluationKind::needs_new_attempt,{}};
        observed.push_back(u);
        const double d=u[0]-0.75;
        return Evaluation{EvaluationKind::attempted_complete,
                          score(d*d)};
    };
    auto result=CoordinateRefiner::run({coord(1,1)},score(1.0),
                                        1,eval);
    QVERIFY(result);
    QVERIFY(result->end==End::complete);
    QCOMPARE(result->attempted_total,9U); // seed 1 + 4*1*2
    QCOMPARE(result->evaluated_complete,8U);
    QCOMPARE(observed.size(),std::size_t{8});
    QCOMPARE(observed[0][0],0.25);
    QCOMPARE(observed[1][0],0.75);
    QCOMPARE(observed[2][0],0.625); // after BOTH first directions
    QCOMPARE(observed[3][0],0.875);
    QCOMPARE(observed[4][0],0.6875);
    QCOMPARE(observed[5][0],0.8125);
    QCOMPARE(observed[6][0],0.71875);
    QCOMPARE(observed[7][0],0.78125);
    QCOMPARE(result->coordinates[0].normalized_u,0.75);
    QVERIFY(result->winner.total_j==0.0);
    QVERIFY(!result->authorizes_product_publication());
}
void FrozenM10RefinerTest::exhaustedBudgetRetainsBestAndAllowsExactCache() {
    std::uint32_t calls=0;
    const auto eval=[&](const std::vector<double>& u,bool allow) {
        ++calls;
        if(!allow) {
            if(u[0]==0.375)
                return Evaluation{EvaluationKind::completed_cache_hit,
                                  score(0.0)};
            return Evaluation{EvaluationKind::needs_new_attempt,{}};
        }
        return Evaluation{EvaluationKind::attempted_complete,score(0.1)};
    };
    auto r=CoordinateRefiner::run({coord(1,1)},score(1.0),799,eval);
    QVERIFY(r);
    QVERIFY(r->end==End::budget_exhausted);
    QCOMPARE(r->attempted_total,800U);
    QCOMPARE(r->completed_cache_hits,1U);
    QCOMPARE(r->coordinates[0].normalized_u,0.375);
    QCOMPARE(r->winner.total_j,0.0);
    QVERIFY(calls>2);
    const auto fail=[](const std::vector<double>&,bool allow) {
        return allow?
            Evaluation{EvaluationKind::attempted_complete,score(
                std::numeric_limits<double>::quiet_NaN())}:
            Evaluation{EvaluationKind::needs_new_attempt,{}};
    };
    auto failed=CoordinateRefiner::run({coord(3,1)},score(2.0),799,fail);
    QVERIFY(failed);
    QCOMPARE(failed->attempted_total,800U);
    QCOMPARE(failed->attempted_failures,1U);
    QVERIFY(failed->end==End::budget_exhausted);
    QCOMPARE(failed->winner.total_j,2.0);
}
void FrozenM10RefinerTest::malformedEvidenceAndCancellation() {
    const auto complete=[](const std::vector<double>&,bool) {
        return Evaluation{EvaluationKind::completed_cache_hit,score(0.5)};
    };
    QVERIFY(!CoordinateRefiner::run(
        {coord(2,1),coord(2,1)},score(2),0,complete));
    QVERIFY(!CoordinateRefiner::run(
        {coord(2,1,1.1)},score(2),0,complete));
    QVERIFY(!CoordinateRefiner::run(
        {coord(2,1)},score(
            std::numeric_limits<double>::infinity()),0,complete));
    QVERIFY(!CoordinateRefiner::run({coord(2,1)},score(2),801,complete));
    std::atomic<bool> cancelled{true};
    auto never=[](const std::vector<double>&,bool) {
        Q_ASSERT(false);
        return Evaluation{EvaluationKind::preflight_rejected,{}};
    };
    auto out=CoordinateRefiner::run(
        {coord(2,1)},score(1.0),1,never,&cancelled);
    QVERIFY(out);
    QVERIFY(out->end==End::cancelled);
    QCOMPARE(out->attempted_total,1U);
    QCOMPARE(out->winner.total_j,1.0);
}
void FrozenM10RefinerTest::
invalidDirectionsNeverSpendBudgetAndExactCacheIsFree() {
    const auto preflight=[](const std::vector<double>&,bool) {
        return Evaluation{EvaluationKind::preflight_rejected,{}};
    };
    auto p=CoordinateRefiner::run({coord(4,1,0.95)},
                                   score(1.0),17,preflight);
    QVERIFY(p);
    QVERIFY(p->end==End::complete);
    QCOMPARE(p->attempted_total,17U);
    QCOMPARE(p->preflight_rejections,5U);
    QCOMPARE(p->discarded_out_of_bounds,3U);
    QCOMPARE(p->coordinates[0].normalized_u,0.95);
    QCOMPARE(p->winner.total_j,1.0);

    // Even with no remaining attempted-render budget, the executor can
    // legally reuse fresh exact complete evidence across all four steps.
    const auto cached=[](const std::vector<double>&,bool allowed) {
        if(allowed)return Evaluation{EvaluationKind::preflight_rejected,{}};
        return Evaluation{EvaluationKind::completed_cache_hit,score(0.0)};
    };
    auto c=CoordinateRefiner::run({coord(5,1)},score(1.0),800,cached);
    QVERIFY(c);
    QCOMPARE(c->end,End::complete);
    QCOMPARE(c->attempted_total,800U);
    QCOMPARE(c->completed_cache_hits,8U);
    QCOMPARE(c->evaluated_complete,8U);
    QCOMPARE(c->winner.total_j,0.0);
    QCOMPARE(c->coordinates[0].normalized_u,0.25);
}
void FrozenM10RefinerTest::maximum48CoordinatesDeterministicOrder() {
    std::vector<Coordinate> coordinates;
    for(int i=48;i>=1;--i)
        coordinates.push_back(coord(static_cast<std::uint8_t>(i),1,0.5));
    std::vector<std::pair<double,double>> visits;
    const auto attempted=[&](const std::vector<double>& u,bool allow) {
        if(!allow)
            return Evaluation{EvaluationKind::needs_new_attempt,{}};
        visits.emplace_back(u[0],u[47]);
        return Evaluation{EvaluationKind::attempted_complete,score(1.0)};
    };
    auto r=CoordinateRefiner::run(coordinates,score(1.0),416,attempted);
    QVERIFY(r);
    QVERIFY(r->end==End::complete);
    QCOMPARE(r->attempted_total,800U); // 416 + 48*4*2
    QCOMPARE(r->evaluated_complete,384U);
    QCOMPARE(r->coordinates.size(),std::size_t{48});
    QCOMPARE(r->coordinates[0].semantic_node_id[0],std::uint8_t{1});
    QCOMPARE(r->coordinates[47].semantic_node_id[0],std::uint8_t{48});
    QCOMPARE(visits.size(),std::size_t{384});
    QCOMPARE(visits[0].first,0.25);
    QCOMPARE(visits[1].first,0.75);
    for(const auto& c:r->coordinates)
        QCOMPARE(c.normalized_u,0.5); // equal costs retain CURRENT
    coordinates.push_back(coord(49,1));
    QVERIFY(!CoordinateRefiner::run(coordinates,score(1.0),0,attempted));
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::FrozenM10RefinerTest)
#include "test_l3_m10_frozen_coordinate_refiner.moc"
