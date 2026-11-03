#include <rgsml/dsp/stereo_ms_crossover.hpp>
#include <rgsml/dsp/stereo_ms_crossover_runtime.hpp>

#include <QtTest/QTest>

#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>
#include <numbers>

namespace rgsml::tests {
namespace {

using rgsml::dsp::MonoBassMode;
using rgsml::dsp::StereoMsCrossoverRuntime;
using rgsml::dsp::design_stereo_ms_crossover;

class StereoMsCrossoverRuntimeTest final : public QObject {
    Q_OBJECT

private slots:
    void signedAllpassImpulseMatchesIndependentRecurrences();
    void frequencyPhaseAtFcAndBetaLaw();
    void branchIndependenceAndReset();
    void responseIsInvariantToBlockPartition();
};

void StereoMsCrossoverRuntimeTest::signedAllpassImpulseMatchesIndependentRecurrences()
{
    for (const double fc : {40.0, 120.0, 300.0}) {
        constexpr double fs = 48000.0;
        const double k = std::tan(std::numbers::pi_v<double> * fc / fs);
        for (auto mode : {MonoBassMode::LR12, MonoBassMode::LR24}) {
            const auto design = design_stereo_ms_crossover(mode, fc, fs);
            QVERIFY(design);
            StereoMsCrossoverRuntime filter{*design.value()};
            double h_prev2 = 0.0;
            double h_prev = 0.0;
            const double r = (1.0 - k) / (1.0 + k);
            const double root2 = std::numbers::sqrt2_v<double>;
            const double kk = k * k;
            const double denom = 1.0 + root2 * k + kk;
            const double a1 = (2.0 * (kk - 1.0)) / denom;
            const double a2 = (1.0 - root2 * k + kk) / denom;

            for (int n = 0; n < 128; ++n) {
                const auto frame = filter.process(n == 0 ? 1.0 : 0.0, 0.0, 1.0);
                double oracle;
                if (mode == MonoBassMode::LR12) {
                    oracle = n == 0 ? -r
                        : (1.0 - r * r) * std::pow(r, n - 1);
                } else {
                    oracle = n == 0 ? a2
                        : n == 1 ? a1 * (1.0 - a2)
                        : n == 2 ? 1.0 - a1 * h_prev - a2 * h_prev2
                        : -a1 * h_prev - a2 * h_prev2;
                }
                // Independently generated all-pass difference-equation oracle,
                // not the production four-cascade implementation.
                QVERIFY2(std::abs(frame.mid - oracle) < 2e-9,
                         "Signed LR impulse differs from the frozen all-pass oracle.");
                QCOMPARE(frame.side, 0.0);
                QVERIFY(filter.finite());
                h_prev2 = h_prev;
                h_prev = oracle;
            }
        }
    }
}

void StereoMsCrossoverRuntimeTest::frequencyPhaseAtFcAndBetaLaw()
{
    constexpr double fs = 48000.0;
    constexpr double fc = 120.0;
    const double omega = 2.0 * std::numbers::pi_v<double> * fc / fs;
    const std::complex<double> q = std::exp(std::complex<double>{0.0, -omega});
    std::complex<double> oscillator{1.0, 0.0};

    for (const auto mode : {MonoBassMode::LR12, MonoBassMode::LR24}) {
        const auto design = design_stereo_ms_crossover(mode, fc, fs);
        QVERIFY(design);
        const auto expected_mid = mode == MonoBassMode::LR12
            ? std::complex<double>{0.0, -1.0}
            : std::complex<double>{-1.0, 0.0};
        for (const double beta : {0.0, 0.25, 0.5, 0.75, 1.0}) {
            StereoMsCrossoverRuntime filter{*design.value()};
            std::complex<double> mid{0.0, 0.0};
            std::complex<double> side{0.0, 0.0};
            oscillator = {1.0, 0.0};
            // Impulse DFT. 24k samples fully includes the frozen settling
            // span at 48k/120 and exposes phase, not just magnitude.
            for (int n = 0; n < 24000; ++n) {
                const auto f = filter.process(n == 0 ? 1.0 : 0.0,
                                              n == 0 ? 1.0 : 0.0, beta);
                mid += f.mid * oscillator;
                side += f.side * oscillator;
                oscillator *= q;
            }
            QVERIFY(std::abs(mid - expected_mid) < 5e-8);
            QVERIFY(std::abs(side / mid -
                std::complex<double>{(1.0 + beta) / 2.0, 0.0}) < 5e-8);
            QVERIFY(filter.finite());
        }
    }
}

void StereoMsCrossoverRuntimeTest::branchIndependenceAndReset()
{
    const auto design = design_stereo_ms_crossover(MonoBassMode::LR24, 120.0, 48000.0);
    QVERIFY(design);
    StereoMsCrossoverRuntime a{*design.value()};
    StereoMsCrossoverRuntime b{*design.value()};
    for (int n = 0; n < 2048; ++n) {
        const auto mid_only = a.process(n == 0 ? 1.0 : 0.0, 0.0, 0.0);
        const auto side_only = b.process(0.0, n == 0 ? 1.0 : 0.0, 1.0);
        QCOMPARE(mid_only.side, 0.0);
        QCOMPARE(side_only.mid, 0.0);
        QVERIFY(std::abs(mid_only.mid - side_only.side) < 1e-12);
    }
    a.reset();
    b.reset();
    const auto origin1 = a.process(1.0, 1.0, 1.0);
    b.reset();
    const auto origin2 = b.process(1.0, 1.0, 1.0);
    QCOMPARE(origin1.mid, origin2.mid);
    QCOMPARE(origin1.side, origin2.side);
}

void StereoMsCrossoverRuntimeTest::responseIsInvariantToBlockPartition()
{
    for (const auto mode : {MonoBassMode::LR12, MonoBassMode::LR24}) {
        const auto design = design_stereo_ms_crossover(mode, 120.0, 48000.0);
        QVERIFY(design);
        StereoMsCrossoverRuntime continuous{*design.value()};
        StereoMsCrossoverRuntime chunked{*design.value()};
        std::array<double, 1024> mid{};
        std::array<double, 1024> side{};
        for (std::size_t n = 0; n < mid.size(); ++n) {
            mid[n] = std::sin(static_cast<double>(n) * 0.123)
                + 0.2 * std::cos(static_cast<double>(n) * 0.031);
            side[n] = std::cos(static_cast<double>(n) * 0.083) * 0.6;
        }
        std::array<double, 1024> mid_expected{};
        std::array<double, 1024> side_expected{};
        for (std::size_t n = 0; n < mid.size(); ++n) {
            const auto f = continuous.process(mid[n], side[n], 0.25);
            mid_expected[n] = f.mid;
            side_expected[n] = f.side;
        }
        // A new chunk boundary NEVER creates fresh crossover states.
        constexpr std::array<std::size_t, 9> boundaries{
            1, 7, 33, 34, 255, 512, 513, 1000, 1024};
        std::size_t start = 0;
        for (std::size_t end : boundaries) {
            for (std::size_t n = start; n < end; ++n) {
                const auto f = chunked.process(mid[n], side[n], 0.25);
                QCOMPARE(f.mid, mid_expected[n]);
                QCOMPARE(f.side, side_expected[n]);
            }
            start = end;
        }
        QVERIFY(chunked.finite());
    }
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::StereoMsCrossoverRuntimeTest)

#include "test_stereo_ms_crossover_runtime.moc"
