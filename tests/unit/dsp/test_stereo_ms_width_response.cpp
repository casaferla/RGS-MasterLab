#include <rgsml/dsp/stereo_ms_width_response.hpp>
#include <rgsml/dsp/stereo_ms_crossover.hpp>
#include <rgsml/dsp/stereo_ms_crossover_runtime.hpp>

#include <QTest>

#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

namespace rgsml::dsp::tests {
namespace {

class StereoMsWidthResponseTest final : public QObject {
    Q_OBJECT
private slots:
    void offAndExactSideMute();
    void activeBetaOneDoesNotImplyOffPhase();
    void analyticalResponseMatchesActualCrossoverRuntime();
    void invalidFrequencyAndPreparePredicateFailClosed();
};

void StereoMsWidthResponseTest::offAndExactSideMute()
{
    const std::array<double, 3> frequencies{20.0, 120.0, 4000.0};
    auto p = StereoMsParameters::create(
        -3.0, 3.0, false, MonoBassMode::OFF, 150.0, 25.0);
    QVERIFY(p);
    auto r = stereo_ms_width_response_at(*p.value(), 48000.0, frequencies);
    QVERIFY(r);
    QCOMPARE(r.value()->size(), frequencies.size());
    for (const auto& x : *r.value()) {
        QVERIFY(std::abs(x.effective_width_percent
                         - 100.0 * std::pow(10.0, 6.0 / 20.0)) < 1e-9);
    }
    auto muted = StereoMsParameters::create(
        -3.0, 3.0, true, MonoBassMode::LR24, 150.0, 0.0);
    QVERIFY(muted);
    auto zero = stereo_ms_width_response_at(*muted.value(), 48000.0, frequencies);
    QVERIFY(zero);
    for (const auto& x : *zero.value())
        QCOMPARE(x.effective_width_percent, 0.0);
}

void StereoMsWidthResponseTest::activeBetaOneDoesNotImplyOffPhase()
{
    for (const auto mode : {MonoBassMode::LR12, MonoBassMode::LR24}) {
        auto p = StereoMsParameters::create(0, 0, false, mode, 140.0, 100.0);
        QVERIFY(p);
        auto r = stereo_ms_width_response_grid(*p.value(), 48000.0);
        QVERIFY(r);
        QCOMPARE(r.value()->size(), std::size_t{129});
        for (const auto& point : *r.value()) {
            // Ratio of transfer magnitudes is exactly 1 when beta=1.
            // This does NOT assert phase equivalence to Mono Bass OFF.
            QVERIFY(std::abs(point.effective_width_percent - 100.0) < 1e-8);
        }
    }
}

void StereoMsWidthResponseTest::analyticalResponseMatchesActualCrossoverRuntime()
{
    constexpr double sampleRate = 48000.0;
    constexpr double frequency = 150.0; // integer cycles in the measurement window
    const std::array<double, 1> frequencies{frequency};
    constexpr double beta = 0.35;
    for (const auto mode : {MonoBassMode::LR12, MonoBassMode::LR24}) {
        auto params = StereoMsParameters::create(
            0, 0, false, mode, 180.0, beta * 100.0);
        QVERIFY(params);
        auto graph = stereo_ms_width_response_at(*params.value(), sampleRate, frequencies);
        QVERIFY(graph);
        auto design = design_stereo_ms_crossover(mode, 180.0, sampleRate);
        QVERIFY(design);
        StereoMsCrossoverRuntime runtime(*design.value());
        double midSin = 0, midCos = 0, sideSin = 0, sideCos = 0;
        // Observe the actual streaming kernel after a complete settle;
        // no independent or alternate filter mathematics in the test.
        for (int n = 0; n < 96000; ++n) {
            const double angle = 2.0 * std::numbers::pi_v<double>
                * frequency * static_cast<double>(n) / sampleRate;
            const double s = std::sin(angle);
            const double c = std::cos(angle);
            const auto frame = runtime.process(s, s, beta);
            if (n >= 48000) {
                midSin += frame.mid * s;
                midCos += frame.mid * c;
                sideSin += frame.side * s;
                sideCos += frame.side * c;
            }
        }
        const double measured = 100.0 * std::hypot(sideSin, sideCos)
            / std::hypot(midSin, midCos);
        QVERIFY(std::abs(measured - graph.value()->front().effective_width_percent)
                < 1e-6);
    }
}

void StereoMsWidthResponseTest::invalidFrequencyAndPreparePredicateFailClosed()
{
    auto p = StereoMsParameters::create_default();
    QVERIFY(p);
    const std::array<double, 1> invalid{-1.0};
    QVERIFY(!stereo_ms_width_response_at(*p.value(), 48000.0, invalid));
    const std::array<double, 1> nyquist{24000.0};
    QVERIFY(!stereo_ms_width_response_at(*p.value(), 48000.0, nyquist));
    QVERIFY(!stereo_ms_width_response_grid(*p.value(), 48000.0, 1));
    QVERIFY(!stereo_ms_width_response_grid(*p.value(),
        std::numeric_limits<double>::quiet_NaN()));
    auto active = StereoMsParameters::create(
        0, 0, false, MonoBassMode::LR12, 300.0, 0.0);
    QVERIFY(active);
    const std::array<double, 1> legalFrequency{100.0};
    // 300 Hz is not strictly below 0.45 * 600 Hz:
    // use the same prepare-time predicate as the DSP.
    QVERIFY(!stereo_ms_width_response_at(*active.value(), 600.0, legalFrequency));
}

} // namespace
} // namespace rgsml::dsp::tests

QTEST_APPLESS_MAIN(rgsml::dsp::tests::StereoMsWidthResponseTest)
#include "test_stereo_ms_width_response.moc"
