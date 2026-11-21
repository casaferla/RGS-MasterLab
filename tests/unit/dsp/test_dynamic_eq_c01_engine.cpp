#include "../../../dsp/src/internal/dynamic_eq_c01_engine.hpp"
#include <rgsml/dsp/dynamic_eq_parameters.hpp>
#include <QtTest/QTest>
#include <cmath>
#include <limits>
#include <vector>
#include <array>
#include <algorithm>

namespace rgsml::tests {
using namespace rgsml::dsp;
using namespace rgsml::dsp::internal;
class C01EngineTest final:public QObject {
    Q_OBJECT
private slots:
    void zeroAndDynamicMono();
    void stereoPartitionInvariantAndBandOrder();
    void finiteFailurePreservesPCMAndState();
    void disabledAndRoutingValidation();
};
namespace {
[[nodiscard]] core::Uuid id(int i) {
    return *core::Uuid::parse(
        i==1?"11111111-1111-4111-8111-111111111111":
             "22222222-2222-4222-8222-222222222222").value();
}
[[nodiscard]] DynamicEqParameters params(
    EqRouting route=EqRouting::STEREO,bool enabled=true,
    double range=-9.0)
{
    DynamicEqBandSpec spec;
    spec.band_id=id(1);
    spec.routing=route;
    spec.enabled=enabled;
    spec.control.threshold_dbfs=-60.0;
    spec.control.range_db=range;
    auto b=DynamicEqBandParameters::create(spec);
    return *DynamicEqParameters::create({*b.value()}).value();
}
}
void C01EngineTest::zeroAndDynamicMono() {
    auto neutral=C01FrameEngine::create(params(EqRouting::STEREO,true,0.0),48000,1);
    auto dynamic=C01FrameEngine::create(params(),48000,1);
    QVERIFY(neutral&&dynamic);
    std::vector<double> x(4096);
    for(std::size_t i=0;i<x.size();++i)
        x[i]=0.5*std::sin(2.0*3.141592653589793*1000.0*i/48000.0);
    std::vector<double> a(x.size()),b(x.size());
    QVERIFY(neutral.value()->process_block(x,{},a,{}));
    QVERIFY(dynamic.value()->process_block(x,{},b,{}));
    QVERIFY(dynamic.value()->states()[0].unsigned_magnitude_db>0.0);
    double delta=0.0;
    for(std::size_t i=0;i<a.size();++i) {
        QVERIFY(std::isfinite(b[i]));
        delta=std::max(delta,std::abs(a[i]-b[i]));
    }
    QVERIFY(delta>1e-6);
    dynamic.value()->reset();
    QCOMPARE(dynamic.value()->states()[0].unsigned_magnitude_db,0.0);
    QCOMPARE(dynamic.value()->states()[0].lanes[0].power,0.0);
}
void C01EngineTest::stereoPartitionInvariantAndBandOrder() {
    DynamicEqBandSpec one;
    one.band_id=id(1);
    one.routing=EqRouting::MID;
    one.filter_type=DynamicEqFilterType::LOW_SHELF;
    one.shape=DynamicEqShelfShape{800.0,0.8};
    one.control.range_db=-6.0;
    one.control.threshold_dbfs=-45.0;
    DynamicEqBandSpec two;
    two.band_id=id(2);
    // MID and SIDE are orthogonal and can commute; LEFT crosses the basis
    // and makes persisted order observationally testable.
    two.routing=EqRouting::LEFT;
    two.filter_type=DynamicEqFilterType::BELL;
    two.shape=DynamicEqBellShape{1500.0,2.0};
    two.control.range_db=6.0;
    two.control.threshold_dbfs=-50.0;
    auto a=DynamicEqBandParameters::create(one);
    auto b=DynamicEqBandParameters::create(two);
    QVERIFY(a&&b);
    const auto p=DynamicEqParameters::create({*a.value(),*b.value()});
    QVERIFY(p);
    auto full=C01FrameEngine::create(*p.value(),44100,2);
    auto split=C01FrameEngine::create(*p.value(),44100,2);
    QVERIFY(full&&split);
    std::vector<double> l(2048),r(2048);
    for(std::size_t i=0;i<l.size();++i) {
        l[i]=0.3*std::sin(0.013*i)+0.1*std::cos(0.032*i);
        r[i]=0.2*std::cos(0.019*i)-0.1*std::sin(0.021*i);
    }
    std::vector<double> l_full(l.size()),r_full(l.size());
    std::vector<double> l_split(l.size()),r_split(l.size());
    QVERIFY(full.value()->process_block(l,r,l_full,r_full));
    for(std::size_t start=0;start<l.size();) {
        const auto n=std::min(std::size_t{127},l.size()-start);
        QVERIFY(split.value()->process_block(
            std::span<const double>(l).subspan(start,n),
            std::span<const double>(r).subspan(start,n),
            std::span<double>(l_split).subspan(start,n),
            std::span<double>(r_split).subspan(start,n)));
        start+=n;
    }
    QVERIFY(l_full==l_split);
    QVERIFY(r_full==r_split);
    QVERIFY(full.value()->states()==split.value()->states());
    auto reversed=DynamicEqParameters::create({*b.value(),*a.value()});
    QVERIFY(reversed);
    auto reverse_engine=C01FrameEngine::create(*reversed.value(),44100,2);
    QVERIFY(reverse_engine);
    std::vector<double> l_rev(l.size()),r_rev(l.size());
    QVERIFY(reverse_engine.value()->process_block(l,r,l_rev,r_rev));
    double difference=0.0;
    for(std::size_t i=0;i<l.size();++i)
        difference=std::max(difference,std::abs(l_rev[i]-l_full[i]));
    QVERIFY(difference>1e-10);
}
void C01EngineTest::finiteFailurePreservesPCMAndState() {
    auto eng=C01FrameEngine::create(params(),48000,2);
    QVERIFY(eng);
    std::vector<double> l(40,0.2),r(40,0.4),ol(40,777.0),orr(40,888.0);
    QVERIFY(eng.value()->process_block(l,r,ol,orr));
    const auto checkpoint=eng.value()->states();
    l[23]=std::numeric_limits<double>::quiet_NaN();
    std::fill(ol.begin(),ol.end(),777.0);
    std::fill(orr.begin(),orr.end(),888.0);
    QVERIFY(!eng.value()->process_block(l,r,ol,orr));
    QVERIFY(checkpoint==eng.value()->states());
    QVERIFY(std::all_of(ol.begin(),ol.end(),[](double v){return v==777.0;}));
    QVERIFY(std::all_of(orr.begin(),orr.end(),[](double v){return v==888.0;}));
    l[23]=0.2;
    QVERIFY(!eng.value()->process_block(
        l,r,std::span<double>(l),std::span<double>(orr)));
    QCOMPARE(checkpoint,eng.value()->states());
    QVERIFY(eng.value()->process_block(l,r,ol,orr));
}
void C01EngineTest::disabledAndRoutingValidation() {
    auto off=C01FrameEngine::create(params(EqRouting::SIDE,false),48000,1);
    QVERIFY(off);
    std::vector<double> x{0.1,0.2,-0.3},y(3);
    QVERIFY(off.value()->process_block(x,{},y,{}));
    QVERIFY(x==y);
    QVERIFY(off.value()->states().empty());
    // SIDE is valid for stereo and uses only the dedicated Side lane.
    auto side=C01FrameEngine::create(params(EqRouting::SIDE),48000,2);
    QVERIFY(side);
    std::vector<double> left(256,0.4),right(256,-0.4);
    std::vector<double> out_l(256),out_r(256);
    QVERIFY(side.value()->process_block(left,right,out_l,out_r));
    QVERIFY(side.value()->states()[0].lanes[1].power>0.0);
    QVERIFY(!C01FrameEngine::create(params(EqRouting::SIDE),48000,1));
}
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::C01EngineTest)
#include "test_dynamic_eq_c01_engine.moc"
