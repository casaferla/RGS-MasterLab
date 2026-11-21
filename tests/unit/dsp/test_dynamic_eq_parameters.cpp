#include <rgsml/dsp/dynamic_eq_parameters.hpp>
#include <QtTest/QTest>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace rgsml::tests {

class DynamicEqParametersTest final:public QObject {
    Q_OBJECT
private slots:
    void defaultsAndIdentities();
    void shapeAndEnvelopeRejection();
    void sampleRateMonoAndDisabledExecution();
};
using namespace rgsml::dsp;
namespace {
[[nodiscard]] core::Uuid id(int n) {
    return *core::Uuid::parse(
        n==1?"11111111-1111-4111-8111-111111111111":
        "22222222-2222-4222-8222-222222222222").value();
}
[[nodiscard]] DynamicEqBandSpec bellSpec(int n=1) {
    DynamicEqBandSpec s;
    s.band_id=id(n);
    return s;
}
}
void DynamicEqParametersTest::defaultsAndIdentities() {
    const auto p=DynamicEqParameters::create_default(id(1));
    QVERIFY(p);
    QCOMPARE(p.value()->bands().size(),std::size_t{1});
    const auto& b=p.value()->bands().front().spec();
    QVERIFY(b.enabled);
    QCOMPARE(b.filter_type,DynamicEqFilterType::BELL);
    QCOMPARE(b.routing,EqRouting::STEREO);
    QCOMPARE(std::get<DynamicEqBellShape>(b.shape).frequency_hz,1000.0);
    QCOMPARE(std::get<DynamicEqBellShape>(b.shape).q,0.707);
    QCOMPARE(b.control.range_db,0.0);
    QCOMPARE(b.control.rms_time_constant_ms,10.0);
    QVERIFY(p.value()->validate_for_audio(44100,1));
    QVERIFY(p.value()->validate_for_audio(48000,2));
    QVERIFY(!DynamicEqParameters::create_default(core::Uuid{}));
    auto b1=DynamicEqBandParameters::create(bellSpec(1));
    auto b2=DynamicEqBandParameters::create(bellSpec(2));
    QVERIFY(b1&&b2);
    QVERIFY(!DynamicEqParameters::create({*b1.value(),*b1.value()}));
    QVERIFY(!DynamicEqParameters::create({}));
    QVERIFY(DynamicEqParameters::create({*b2.value(),*b1.value()}));
    std::vector<DynamicEqBandParameters> tooMany(7,*b1.value());
    QVERIFY(!DynamicEqParameters::create(std::move(tooMany)));
}
void DynamicEqParametersTest::shapeAndEnvelopeRejection() {
    auto x=bellSpec();
    x.shape=DynamicEqShelfShape{};
    QVERIFY(!DynamicEqBandParameters::create(x));
    x.filter_type=DynamicEqFilterType::HIGH_SHELF;
    QVERIFY(DynamicEqBandParameters::create(x));
    x.shape=DynamicEqBellShape{};
    QVERIFY(!DynamicEqBandParameters::create(x));
    x.shape=DynamicEqShelfShape{20000.0,0.10};
    QVERIFY(DynamicEqBandParameters::create(x));
    x.shape=DynamicEqShelfShape{20000.0,0.09};
    QVERIFY(!DynamicEqBandParameters::create(x));
    x=bellSpec();
    x.filter_type=static_cast<DynamicEqFilterType>(55);
    QVERIFY(!DynamicEqBandParameters::create(x));
    x=bellSpec();
    x.routing=static_cast<EqRouting>(75);
    QVERIFY(!DynamicEqBandParameters::create(x));
    x=bellSpec();
    x.control.base_gain_db=17.0;
    x.control.range_db=2.0; // invalid endpoint, not clipped or reduced
    QVERIFY(!DynamicEqBandParameters::create(x));
    x.control.range_db=1.0;
    QVERIFY(DynamicEqBandParameters::create(x));
    x.control.range_db=-18.0;
    QVERIFY(DynamicEqBandParameters::create(x));
    x.control.range_db=std::numeric_limits<double>::quiet_NaN();
    QVERIFY(!DynamicEqBandParameters::create(x));
    x=bellSpec();
    x.control.rms_time_constant_ms=0.99;
    QVERIFY(!DynamicEqBandParameters::create(x));
    x=bellSpec();
    x.control.attack_ms=0.09;
    QVERIFY(!DynamicEqBandParameters::create(x));
    x=bellSpec();
    x.control.release_ms=5000.01;
    QVERIFY(!DynamicEqBandParameters::create(x));
    x=bellSpec();
    x.control.ratio=20.0;
    QVERIFY(DynamicEqBandParameters::create(x));
    x.control.ratio=20.01;
    QVERIFY(!DynamicEqBandParameters::create(x));
}
void DynamicEqParametersTest::sampleRateMonoAndDisabledExecution() {
    auto x=bellSpec();
    x.shape=DynamicEqBellShape{20000.0,0.707};
    auto b=DynamicEqBandParameters::create(x);
    QVERIFY(b);
    QVERIFY(b.value()->validate_for_audio(48000,2));
    QVERIFY(!b.value()->validate_for_audio(44100,2)); // 0.45 Fs=19845
    x.shape=DynamicEqBellShape{1000.0,0.707};
    x.routing=EqRouting::SIDE;
    b=DynamicEqBandParameters::create(x);
    QVERIFY(b);
    QVERIFY(!b.value()->validate_for_audio(48000,1));
    QVERIFY(b.value()->validate_for_audio(48000,2));
    x.enabled=false;
    b=DynamicEqBandParameters::create(x);
    QVERIFY(b);
    QVERIFY(b.value()->validate_for_audio(48000,1));
    QVERIFY(!b.value()->validate_for_audio(0,2));
    auto p=DynamicEqParameters::create({*b.value()});
    QVERIFY(p);
    QVERIFY(p.value()->validate_for_audio(48000,1));
    QVERIFY(!p.value()->validate_for_audio(0,1));
}

} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::DynamicEqParametersTest)
#include "test_dynamic_eq_parameters.moc"
