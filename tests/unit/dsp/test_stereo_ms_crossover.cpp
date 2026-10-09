#include "test_support.hpp"

#include <rgsml/dsp/stereo_ms_crossover.hpp>

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
using rgsml::dsp::StereoMsCrossoverDesign;
using rgsml::dsp::StereoMsFilterSection;
using rgsml::dsp::design_stereo_ms_crossover;

[[nodiscard]] std::complex<double> response(
    StereoMsFilterSection s, double omega)
{
    using C = std::complex<double>;
    const C q = std::exp(C{0.0, -omega});
    return (s.b0 + s.b1 * q + s.b2 * q * q)
         / (1.0 + s.a1 * q + s.a2 * q * q);
}

[[nodiscard]] std::complex<double> branch(
    StereoMsCrossoverDesign d, bool high, double omega)
{
    auto h = response(high ? d.high_section : d.low_section, omega);
    h *= h;  // Exactly two independently realized sections per branch.
    return high && d.invert_high_branch ? -h : h;
}

[[nodiscard]] double frequency_omega(double frequency_hz, double sample_rate_hz)
{
    return 2.0 * std::numbers::pi_v<double> * frequency_hz / sample_rate_hz;
}

class StereoMsCrossoverTest final : public QObject {
    Q_OBJECT

private slots:
    void frozenCoefficientsAt48k120();
    void fcPhaseAndAllpassVsOff();
    void poleDerivedSettlingReferenceVectors();
    void cutoffAndModeFailClosed();
    void independentSectionAndEndpointOracles();
};

void StereoMsCrossoverTest::frozenCoefficientsAt48k120()
{
    const auto lr12 = design_stereo_ms_crossover(MonoBassMode::LR12, 120.0, 48000.0);
    const auto lr24 = design_stereo_ms_crossover(MonoBassMode::LR24, 120.0, 48000.0);
    QVERIFY(lr12);
    QVERIFY(lr24);
    QCOMPARE(lr12.value()->sections_per_branch, std::uint32_t{2});
    QCOMPARE(lr24.value()->sections_per_branch, std::uint32_t{2});
    QVERIFY(lr12.value()->invert_high_branch);
    QVERIFY(!lr24.value()->invert_high_branch);

    auto near = [](double actual, double expected) {
        return std::abs(actual - expected) <= 8e-15;
    };
    const auto l1 = lr12.value()->low_section;
    const auto h1 = lr12.value()->high_section;
    QVERIFY(near(l1.b0, 0.007792936291951552));
    QVERIFY(near(l1.b1, l1.b0));
    QVERIFY(near(h1.b0, 0.9922070637080484477));
    QVERIFY(near(h1.b1, -h1.b0));
    QVERIFY(near(l1.a1, -0.984414127416096895));
    QVERIFY(near(h1.a1, l1.a1));
    QCOMPARE(l1.a2, 0.0);
    QCOMPARE(h1.b2, 0.0);

    const auto l2 = lr24.value()->low_section;
    const auto h2 = lr24.value()->high_section;
    QVERIFY(near(l2.b0, 0.00006100617875806426));
    QVERIFY(near(l2.b1, 2.0 * l2.b0));
    QVERIFY(near(l2.b2, l2.b0));
    QVERIFY(near(h2.b0, 0.9889542480671399351));
    QVERIFY(near(h2.b1, -2.0 * h2.b0));
    QVERIFY(near(h2.b2, h2.b0));
    QVERIFY(near(l2.a1, -1.9777864837767637416));
    QVERIFY(near(l2.a2, 0.9780305084917959987));
    QVERIFY(near(h2.a1, l2.a1));
    QVERIFY(near(h2.a2, l2.a2));
}

void StereoMsCrossoverTest::fcPhaseAndAllpassVsOff()
{
    const double fc = 120.0;
    const double fs = 48000.0;
    const double omega = frequency_omega(fc, fs);
    const std::complex<double> neg_i{0.0, -1.0};
    const std::complex<double> neg_one{-1.0, 0.0};
    for (auto mode : {MonoBassMode::LR12, MonoBassMode::LR24}) {
        auto d = design_stereo_ms_crossover(mode, fc, fs);
        QVERIFY(d);
        const auto low = branch(*d.value(), false, omega);
        const auto high = branch(*d.value(), true, omega);
        const auto total = low + high;
        const auto expected_half = mode == MonoBassMode::LR12
            ? 0.5 * neg_i : 0.5 * neg_one;
        const auto expected_full = mode == MonoBassMode::LR12
            ? neg_i : neg_one;
        QVERIFY(std::abs(low - expected_half) < 1e-10);
        QVERIFY(std::abs(high - expected_half) < 1e-10);
        QVERIFY(std::abs(total - expected_full) < 1e-10);
        QVERIFY(std::abs(total - std::complex<double>{1.0, 0.0}) > 1.0);

        // This sum MUST NOT be replaced with identity at 100% low Side.
        for (double hz : {0.0, 40.0, 60.0, 120.0, 240.0, 1000.0, 20000.0}) {
            const auto h = branch(*d.value(), false, frequency_omega(hz, fs))
                + branch(*d.value(), true, frequency_omega(hz, fs));
            QVERIFY(std::abs(std::abs(h) - 1.0) < 1e-9);
        }
    }
}

void StereoMsCrossoverTest::poleDerivedSettlingReferenceVectors()
{
    struct Row { double fs; double fc; std::int64_t lr12; std::int64_t lr24; };
    constexpr std::array<Row, 6> cases{{
        {44100.0, 40.0, 4850, 6858},
        {44100.0, 120.0, 1618, 2286},
        {48000.0, 40.0, 5278, 7464},
        {48000.0, 120.0, 1760, 2488},
        {48000.0, 300.0, 704, 996},
        {88200.0, 120.0, 3234, 4572},
    }};
    for (const auto& row : cases) {
        auto a = design_stereo_ms_crossover(MonoBassMode::LR12, row.fc, row.fs);
        auto b = design_stereo_ms_crossover(MonoBassMode::LR24, row.fc, row.fs);
        QVERIFY(a);
        QVERIFY(b);
        QCOMPARE(a.value()->settling_frames, row.lr12);
        QCOMPARE(b.value()->settling_frames, row.lr24);
        QVERIFY(a.value()->maximum_pole_magnitude > 0.0);
        QVERIFY(b.value()->maximum_pole_magnitude < 1.0);
    }
}

void StereoMsCrossoverTest::cutoffAndModeFailClosed()
{
    QVERIFY(!design_stereo_ms_crossover(MonoBassMode::OFF, 120.0, 48000.0));
    QVERIFY(!design_stereo_ms_crossover(static_cast<MonoBassMode>(255), 120.0, 48000.0));
    for (double invalid : {
        0.0, -1.0, 39.999, 300.001,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()}) {
        QVERIFY(!design_stereo_ms_crossover(MonoBassMode::LR12, invalid, 48000.0));
        QVERIFY(!design_stereo_ms_crossover(MonoBassMode::LR24, invalid, 48000.0));
    }
    QVERIFY(design_stereo_ms_crossover(MonoBassMode::LR12, 40.0, 48000.0));
    QVERIFY(design_stereo_ms_crossover(MonoBassMode::LR24, 300.0, 48000.0));
    for (double invalid_fs : {
        0.0, -48000.0,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity()}) {
        QVERIFY(!design_stereo_ms_crossover(MonoBassMode::LR24, 120.0, invalid_fs));
    }
    // Strict Fs=500 boundary: 225 == 0.45*Fs must be rejected.
    const double limit = 225.0;
    QVERIFY(design_stereo_ms_crossover(
        MonoBassMode::LR12,
        std::nextafter(limit, -std::numeric_limits<double>::infinity()),
        500.0));
    QVERIFY(!design_stereo_ms_crossover(MonoBassMode::LR12, limit, 500.0));
}

void StereoMsCrossoverTest::independentSectionAndEndpointOracles()
{
    // Frozen interpretation: one high-polarity reversal in LR12 only;
    // DC Side ratio beta, high-frequency ratio -> 1.
    for (auto mode : {MonoBassMode::LR12, MonoBassMode::LR24}) {
        auto d = design_stereo_ms_crossover(mode, 120.0, 48000.0);
        QVERIFY(d);
        const auto low_dc = branch(*d.value(), false, 0.0);
        const auto high_dc = branch(*d.value(), true, 0.0);
        QVERIFY(std::abs(low_dc - std::complex<double>{1.0, 0.0}) < 1e-9);
        QVERIFY(std::abs(high_dc) < 1e-9);
        for (double beta : {0.0, 0.25, 0.5, 0.75, 1.0}) {
            const auto side_dc = beta * low_dc + high_dc;
            QVERIFY(std::abs(side_dc - std::complex<double>{beta, 0.0}) < 1e-9);
            const auto low_fc = branch(*d.value(), false,
                frequency_omega(120.0, 48000.0));
            const auto high_fc = branch(*d.value(), true,
                frequency_omega(120.0, 48000.0));
            const auto ratio = (beta * low_fc + high_fc) / (low_fc + high_fc);
            QVERIFY(std::abs(ratio - std::complex<double>{(1.0 + beta) / 2.0, 0.0})
                    < 1e-9);
        }
        // A unit-magnitude all-pass summed branch is not 0dB/0phase bypass.
        const double omega = frequency_omega(120.0, 48000.0);
        const auto allpass = branch(*d.value(), false, omega)
                           + branch(*d.value(), true, omega);
        QVERIFY(std::abs(allpass - std::complex<double>{1.0, 0.0}) > 1.0);
    }
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::StereoMsCrossoverTest)

#include "test_stereo_ms_crossover.moc"
