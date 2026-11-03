#include "../render/render_test_support.hpp"

#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/stereo_ms_module.hpp>

#include <QtTest/QTest>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <span>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace rgsml::dsp;
using namespace render_support;

// These test-only measurements are NOT a production true-peak meter,
// not a BS.1770 compliance claim and not a source of live telemetry.
// Their purpose is objective evidence that spatial DSP can change
// intersample peak risk and correlation without hidden gain limiting.

[[nodiscard]] double correlation(
    std::span<const double> left, std::span<const double> right)
{
    double a = 0.0, b = 0.0, aa = 0.0, bb = 0.0, ab = 0.0;
    for (std::size_t i = 0; i < left.size(); ++i) {
        a += left[i];
        b += right[i];
    }
    const double lm = a / static_cast<double>(left.size());
    const double rm = b / static_cast<double>(right.size());
    for (std::size_t i = 0; i < left.size(); ++i) {
        const double l = left[i] - lm, r = right[i] - rm;
        aa += l * l; bb += r * r; ab += l * r;
    }
    return ab / std::sqrt(aa * bb);
}

[[nodiscard]] double sample_peak(std::span<const double> samples)
{
    double peak = 0.0;
    for (double v : samples) peak = std::max(peak, std::abs(v));
    return peak;
}

[[nodiscard]] double sinc(double x)
{
    if (x == 0.0) return 1.0;
    const double angle = std::numbers::pi_v<double> * x;
    return std::sin(angle) / angle;
}

[[nodiscard]] double windowed_sinc_peak_indicator(
    std::span<const double> samples)
{
    // Independently windowed 8x band-limited reconstruction, 24 samples
    // on either side; exclude boundaries. This is a qualitative test
    // indicator, not the canonical production true-peak measurement.
    constexpr std::size_t radius = 24;
    constexpr int factor = 8;
    double peak = 0.0;
    for (std::size_t n = radius; n + radius < samples.size(); ++n) {
        for (int phase = 0; phase < factor; ++phase) {
            const double fraction = static_cast<double>(phase) / factor;
            double sum = 0.0, normalization = 0.0;
            for (int tap = -static_cast<int>(radius);
                 tap <= static_cast<int>(radius); ++tap) {
                const double x = fraction - static_cast<double>(tap);
                const double taper =
                    0.5 + 0.5 * std::cos(
                        std::numbers::pi_v<double> * x /
                        static_cast<double>(radius + 1));
                const double coeff = sinc(x) * taper;
                sum += coeff * samples[static_cast<std::size_t>(
                    static_cast<std::ptrdiff_t>(n) + tap)];
                normalization += coeff;
            }
            peak = std::max(peak, std::abs(sum / normalization));
        }
    }
    return peak;
}

[[nodiscard]] std::unique_ptr<StereoMsModule> module(
    const ModuleRegistry& registry, StereoMsParameters p)
{
    const auto d = registry.find_descriptor("rgsml.dsp.stereo-ms");
    Q_ASSERT(d);
    auto result = StereoMsModule::create(d.value()->get(), p);
    Q_ASSERT(result);
    return std::move(*result.value());
}

class StereoMsGateGSpatialEvidenceTest final : public QObject {
    Q_OBJECT
private slots:
    void sideMuteCorrelationAndAntiPhaseFoldDown();
    void intersamplePeakRiskHasNoHiddenLimiter();
};

void StereoMsGateGSpatialEvidenceTest::sideMuteCorrelationAndAntiPhaseFoldDown()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    constexpr std::size_t count = 1024;
    std::array<double, count> left{}, right{};
    for (std::size_t n = 0; n < count; ++n) {
        left[n] = 0.4 * std::sin(0.083 * static_cast<double>(n))
                + 0.1 * std::cos(0.22 * static_cast<double>(n));
        right[n] = -left[n];
    }
    const auto layout = rgsml::audio::ChannelLayout::STEREO_LR;
    const DspProcessSpec spec{
        format(layout), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(static_cast<std::int64_t>(count))};
    auto input = make_buffer(layout, 0, left, right);
    QVERIFY(input);

    auto mute = StereoMsParameters::create(
        -3.0, 12.0, true, MonoBassMode::LR24, 120.0, 0.0);
    QVERIFY(mute);
    auto muted = module(*registry.value(), *mute.value());
    QVERIFY(muted->prepare(spec));
    auto output = make_buffer(layout, 0, left, right);
    QVERIFY(output);
    QVERIFY(muted->process(input.value()->view(), output.value()->mutable_view(),
        DspProcessContext{frame_range(0, static_cast<std::int64_t>(count)), true, true}));
    const auto a = *output.value()->view().channel(0).value();
    const auto b = *output.value()->view().channel(1).value();
    for (std::size_t i = 0; i < count; ++i) {
        QCOMPARE(a[i], b[i]);
        QVERIFY(std::abs(a[i]) < 2e-15);
    }

    // For a non-zero centered source, exact Side mute must create L=R
    // and correlation +1; an anti-phase unmuted OFF path remains -1.
    for (std::size_t n = 0; n < count; ++n) {
        right[n] = left[n] * 0.15 + 0.05 * std::cos(0.17 * static_cast<double>(n));
    }
    auto input2 = make_buffer(layout, 0, left, right);
    auto output2 = make_buffer(layout, 0, left, right);
    QVERIFY(input2);
    QVERIFY(output2);
    muted->reset();
    QVERIFY(muted->process(input2.value()->view(), output2.value()->mutable_view(),
        DspProcessContext{frame_range(0, static_cast<std::int64_t>(count)), true, true}));
    const auto l2 = *output2.value()->view().channel(0).value();
    const auto r2 = *output2.value()->view().channel(1).value();
    QVERIFY(std::abs(correlation(l2, r2) - 1.0) < 1e-13);

    for (std::size_t n = 0; n < count; ++n) right[n] = -left[n];
    auto unmuted_p = StereoMsParameters::create(
        0.0, 12.0, false, MonoBassMode::OFF, 120.0, 100.0);
    QVERIFY(unmuted_p);
    auto unmuted = module(*registry.value(), *unmuted_p.value());
    QVERIFY(unmuted->prepare(spec));
    auto source3 = make_buffer(layout, 0, left, right);
    auto out3 = make_buffer(layout, 0, left, right);
    QVERIFY(source3);
    QVERIFY(out3);
    QVERIFY(unmuted->process(source3.value()->view(), out3.value()->mutable_view(),
        DspProcessContext{frame_range(0, static_cast<std::int64_t>(count)), true, true}));
    const auto l3 = *out3.value()->view().channel(0).value();
    const auto r3 = *out3.value()->view().channel(1).value();
    QVERIFY(std::abs(correlation(l3, r3) + 1.0) < 1e-13);
    for (std::size_t n = 0; n < count; ++n) {
        QVERIFY(std::abs(0.5 * (l3[n] + r3[n])) < 2e-15);
    }
}

void StereoMsGateGSpatialEvidenceTest::intersamplePeakRiskHasNoHiddenLimiter()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    constexpr std::size_t count = 1024;
    std::array<double, count> left{}, right{};
    for (std::size_t n = 0; n < count; ++n) {
        // Quarter-Nyquist periodic signal with extrema BETWEEN samples:
        // expected ideal amplitude is 0.45, sample extrema 0.45/sqrt(2).
        const double value = 0.45 * std::sin(
            0.5 * std::numbers::pi_v<double> * n +
            0.25 * std::numbers::pi_v<double>);
        left[n] = value;
        right[n] = -value;
    }
    const auto layout = rgsml::audio::ChannelLayout::STEREO_LR;
    const DspProcessSpec spec{
        format(layout), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(static_cast<std::int64_t>(count))};
    auto params = StereoMsParameters::create(
        0.0, 12.0, false, MonoBassMode::OFF, 120.0, 100.0);
    QVERIFY(params);
    auto instance = module(*registry.value(), *params.value());
    QVERIFY(instance->prepare(spec));
    auto in = make_buffer(layout, 0, left, right);
    auto out = make_buffer(layout, 0, left, right);
    QVERIFY(in);
    QVERIFY(out);
    QVERIFY(instance->process(in.value()->view(), out.value()->mutable_view(),
        DspProcessContext{frame_range(0, static_cast<std::int64_t>(count)), true, true}));
    const auto processed = *out.value()->view().channel(0).value();
    const double original_sample_peak = sample_peak(left);
    const double output_sample_peak = sample_peak(processed);
    const double original_inter = windowed_sinc_peak_indicator(left);
    const double output_inter = windowed_sinc_peak_indicator(processed);
    QVERIFY(std::isfinite(original_inter));
    QVERIFY(std::isfinite(output_inter));
    QVERIFY(original_inter > 1.25 * original_sample_peak);
    QVERIFY(output_inter > 1.25 * output_sample_peak);
    QVERIFY(output_sample_peak > 1.0);
    QVERIFY(output_inter > 1.4);
    QVERIFY(output_inter > original_inter * 3.9);
    // An intentional warning-only policy: no DSP clipping or makeup.
    // Production true-peak metering remains a separately qualified seam.
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::StereoMsGateGSpatialEvidenceTest)
#include "test_stereo_ms_gate_g_spatial_evidence.moc"
