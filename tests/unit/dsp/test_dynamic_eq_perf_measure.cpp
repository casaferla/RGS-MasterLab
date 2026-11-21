#include "../../../dsp/src/internal/dynamic_eq_c01_engine.hpp"
#include <rgsml/dsp/dynamic_eq_parameters.hpp>
#include <QtTest/QTest>
#include <QElapsedTimer>
#include <QDebug>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <utility>
#include <limits>

namespace rgsml::tests {
namespace {
using namespace rgsml::dsp;
using namespace rgsml::dsp::internal;

class DynamicEqPerformanceMeasurement final : public QObject {
    Q_OBJECT
private slots:
    void threeBands48kStereoMeasurementOnly();
    void sixBands96kStereoMeasurementOnly();
};
[[nodiscard]] DynamicEqParameters workload(std::size_t count) {
    const std::vector<const char*> ids={
        "11111111-1111-4111-8111-111111111111",
        "22222222-2222-4222-8222-222222222222",
        "33333333-3333-4333-8333-333333333333",
        "44444444-4444-4444-8444-444444444444",
        "55555555-5555-4555-8555-555555555555",
        "66666666-6666-4666-8666-666666666666"};
    std::vector<DynamicEqBandParameters> bands;
    for(std::size_t i=0;i<count;++i) {
        DynamicEqBandSpec s;
        s.band_id=*core::Uuid::parse(ids[i]).value();
        s.filter_type=(i%3)==0 ?DynamicEqFilterType::LOW_SHELF:
                      (i%3)==1 ?DynamicEqFilterType::BELL:
                                 DynamicEqFilterType::HIGH_SHELF;
        s.routing=i%5==0?EqRouting::STEREO:
                  i%5==1?EqRouting::MID:
                  i%5==2?EqRouting::SIDE:
                  i%5==3?EqRouting::LEFT:EqRouting::RIGHT;
        const double freq=200.0+static_cast<double>(i)*440.0;
        s.shape=s.filter_type==DynamicEqFilterType::BELL
            ?DynamicEqShape{DynamicEqBellShape{freq,0.707}}
            :DynamicEqShape{DynamicEqShelfShape{freq,0.8}};
        s.control.base_gain_db=0.0;
        s.control.range_db=i%2==0?-6.0:6.0;
        s.control.threshold_dbfs=-24.0;
        auto b=DynamicEqBandParameters::create(s);
        Q_ASSERT(b);
        bands.push_back(*b.value());
    }
    auto all=DynamicEqParameters::create(std::move(bands));
    Q_ASSERT(all);
    return *all.value();
}
void measure(std::uint32_t fs,std::size_t count) {
    // Accepted M02 workload families; no numeric Q5 throughput gate exists.
    // This fixture is measurements-only on ephemeral CI machines and never
    // advertises audio callback/deadline conformance or an E1-E6 seal.
    const auto params=workload(count);
    constexpr std::size_t seconds=1;
    const auto n=static_cast<std::size_t>(fs)*seconds;
    std::vector<double> l(n),r(n),out_l(n),out_r(n);
    for(std::size_t i=0;i<n;++i) {
        const double t=static_cast<double>(i);
        l[i]=0.25*std::sin(0.012*t)+0.12*std::cos(0.042*t);
        r[i]=0.22*std::sin(0.019*t)-0.11*std::cos(0.023*t);
    }
    QElapsedTimer timer;
    timer.start();
    auto engine=C01FrameEngine::create(params,fs,2);
    QVERIFY(engine);
    const auto setup_ns=timer.nsecsElapsed();
    // Warm process (not counted) then explicit reset.
    QVERIFY(engine.value()->process_block(l,r,out_l,out_r));
    engine.value()->reset();
    timer.restart();
    QVERIFY(engine.value()->process_block(l,r,out_l,out_r));
    const auto core_ns=timer.nsecsElapsed();
    QVERIFY(core_ns>0);
    QVERIFY(std::isfinite(out_l[n-1]));
    QVERIFY(std::isfinite(out_r[n-1]));
    const double programme_s=static_cast<double>(n)/static_cast<double>(fs);
    const double core_wall_s=static_cast<double>(core_ns)*1e-9;
    qInfo().nospace()<<"L3_M02_MEASUREMENT_ONLY fs="<<fs
        <<" stereo enabled_bands="<<count
        <<" core_wall_seconds="<<core_wall_s
        <<" core_programme_over_wall="<<(programme_s/core_wall_s)
        <<" prepare_wall_seconds="<<(static_cast<double>(setup_ns)*1e-9)
        <<" host_end_to_end=NOT_MEASURED"
        <<" device_deadlines=NOT_MEASURED q5=NOT_EVALUABLE_PENDING_REQUIREMENT";
}
void DynamicEqPerformanceMeasurement::threeBands48kStereoMeasurementOnly() {
    measure(48000,3);
}
void DynamicEqPerformanceMeasurement::sixBands96kStereoMeasurementOnly() {
    measure(96000,6);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::DynamicEqPerformanceMeasurement)
#include "test_dynamic_eq_perf_measure.moc"
