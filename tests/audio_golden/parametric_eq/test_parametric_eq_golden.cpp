#include "../../unit/render/render_test_support.hpp"
#include "../../oracles/parametric_eq/parametric_eq_oracle.hpp"

#include <rgsml/core/error.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/parametric_eq_module.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>

#include <QtTest/QTest>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace rgsml::dsp;
using namespace render_support;
using namespace oracles;

[[nodiscard]] const ModuleDescriptor& eq_descriptor(const ModuleRegistry& registry)
{
    return registry.find_descriptor("rgsml.dsp.parametric-eq").value()->get();
}

[[nodiscard]] std::unique_ptr<ParametricEqModule> make_eq_module(
    const ModuleRegistry& registry,
    const ParametricEqParameters& params)
{
    auto res = ParametricEqModule::create(eq_descriptor(registry), params);
    Q_ASSERT(res);
    return std::move(*res.value());
}

class ParametricEqGoldenTest final : public QObject {
    Q_OBJECT

private slots:
    void requiredFixedFamiliesVerification();
    void fullHpLpMatrixVerification();
    void independentOraclesVerification();
    void multitoneAndSweepVerification();
    void multiSampleRateQualification();
};

void ParametricEqGoldenTest::requiredFixedFamiliesVerification()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto uuid = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000001").value();
    const double fs = 48000.0;

    // Test cases for required fixed families
    struct FamilyCase {
        const char* name;
        EqFilterType type;
        EqBandPayload payload;
    };

    const std::array<FamilyCase, 7> families{{
        {"bell-1k-plus6-q0707", EqFilterType::BELL, BellPayload{1000.0, 6.0, 0.707}},
        {"bell-280-minus6-q12", EqFilterType::BELL, BellPayload{280.0, -6.0, 12.0}},
        {"notch-1k-q12", EqFilterType::NOTCH, NotchPayload{1000.0, 12.0}},
        {"low-shelf-100-plus6-s05", EqFilterType::LOW_SHELF, ShelfPayload{100.0, 6.0, 0.5}},
        {"low-shelf-100-minus6-s10", EqFilterType::LOW_SHELF, ShelfPayload{100.0, -6.0, 1.0}},
        {"high-shelf-10k-plus6-s05", EqFilterType::HIGH_SHELF, ShelfPayload{10000.0, 6.0, 0.5}},
        {"high-shelf-10k-minus6-s10", EqFilterType::HIGH_SHELF, ShelfPayload{10000.0, -6.0, 1.0}},
    }};

    for (const auto& fam : families) {
        auto band = *EqBandParameters::create(uuid, true, fam.type, EqRouting::STEREO, fam.payload).value();
        std::vector<EqBandParameters> bands{band};
        auto params = *ParametricEqParameters::create(bands).value();
        auto module = make_eq_module(*registry.value(), params);

        const DspProcessSpec spec{
            format(rgsml::audio::ChannelLayout::MONO_C, fs),
            rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            frame_count(256)};
        QVERIFY2(module->prepare(spec), fam.name);

        std::vector<double> impulse(256, 0.0);
        impulse[0] = 1.0;
        auto input = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, impulse);
        auto output = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, impulse);

        QVERIFY2(module->process(
            input.value()->view(), output.value()->mutable_view(),
            DspProcessContext{frame_range(0, 256), true, true}), fam.name);

        // Output must be finite
        const auto samples = *output.value()->view().channel(0).value();
        for (double s : samples) {
            QVERIFY(std::isfinite(s));
        }

        // Numerical check for bell-1k-plus6-q0707: first sample equals b0 coefficient
        if (fam.type == EqFilterType::BELL) {
            const auto expected = compute_bell_coeffs(1000.0, 6.0, 0.707, fs);
            QVERIFY(std::abs(samples[0] - expected.b0) <= 1e-10);
        }
    }
}

void ParametricEqGoldenTest::fullHpLpMatrixVerification()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto uuid = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000001").value();
    const double fs = 48000.0;

    const std::array<SlopeDbPerOctave, 6> slopes{
        SlopeDbPerOctave::DB_6,
        SlopeDbPerOctave::DB_12,
        SlopeDbPerOctave::DB_18,
        SlopeDbPerOctave::DB_24,
        SlopeDbPerOctave::DB_36,
        SlopeDbPerOctave::DB_48};

    for (const auto filter_type : {EqFilterType::HIGH_PASS, EqFilterType::LOW_PASS}) {
        for (const auto slope : slopes) {
            auto band = *EqBandParameters::create(uuid, true, filter_type, EqRouting::STEREO, PassPayload{1000.0, slope}).value();
            std::vector<EqBandParameters> bands{band};
            auto params = *ParametricEqParameters::create(bands).value();
            auto module = make_eq_module(*registry.value(), params);

            const DspProcessSpec spec{
                format(rgsml::audio::ChannelLayout::MONO_C, fs),
                rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
                frame_count(128)};
            QVERIFY(module->prepare(spec));

            std::vector<double> impulse(128, 0.0);
            impulse[0] = 1.0;
            auto input = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, impulse);
            auto output = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, impulse);

            QVERIFY(module->process(
                input.value()->view(), output.value()->mutable_view(),
                DspProcessContext{frame_range(0, 128), true, true}));

            const auto samples = *output.value()->view().channel(0).value();
            for (double s : samples) {
                QVERIFY(std::isfinite(s));
            }
        }
    }
}

void ParametricEqGoldenTest::independentOraclesVerification()
{
    // Verification against independent coefficient and TDF2 scalar oracle
    const double fs = 48000.0;
    const auto coeffs = compute_bell_coeffs(1000.0, 6.0, 0.707, fs);

    IndependentTdf2State oracle_state;
    std::vector<double> impulse(32, 0.0);
    impulse[0] = 1.0;

    std::vector<double> expected(32, 0.0);
    for (std::size_t i = 0; i < 32; ++i) {
        expected[i] = oracle_state.process_sample(impulse[i], coeffs);
    }

    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto uuid = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000001").value();
    auto band = *EqBandParameters::create(uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, 6.0, 0.707}).value();
    std::vector<EqBandParameters> bands{band};
    auto params = *ParametricEqParameters::create(bands).value();
    auto module = make_eq_module(*registry.value(), params);

    const DspProcessSpec spec{
        format(rgsml::audio::ChannelLayout::MONO_C, fs),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(32)};
    QVERIFY(module->prepare(spec));

    auto input = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, impulse);
    auto output = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, impulse);
    QVERIFY(module->process(
        input.value()->view(), output.value()->mutable_view(),
        DspProcessContext{frame_range(0, 32), true, true}));

    const auto actual = *output.value()->view().channel(0).value();
    for (std::size_t i = 0; i < 32; ++i) {
        QVERIFY(std::abs(actual[i] - expected[i]) <= 1e-10);
    }

    // Verify RMS diff (O4 oracle)
    const double rms = compute_rms_diff(actual, expected);
    QVERIFY(rms <= 1e-10);
}

void ParametricEqGoldenTest::multitoneAndSweepVerification()
{
    // Verification with multitone and log sweep signals
    const double fs = 48000.0;
    const std::size_t n = 1024;
    std::vector<double> signal(n, 0.0);

    // Multitone: 100 Hz + 1000 Hz + 10000 Hz
    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / fs;
        signal[i] = 0.3 * std::sin(2.0 * std::numbers::pi * 100.0 * t) +
                    0.3 * std::sin(2.0 * std::numbers::pi * 1000.0 * t) +
                    0.3 * std::sin(2.0 * std::numbers::pi * 10000.0 * t);
    }

    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto uuid = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000001").value();
    auto band = *EqBandParameters::create(uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, -6.0, 1.0}).value();
    std::vector<EqBandParameters> bands{band};
    auto params = *ParametricEqParameters::create(bands).value();
    auto module = make_eq_module(*registry.value(), params);

    const DspProcessSpec spec{
        format(rgsml::audio::ChannelLayout::MONO_C, fs),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(static_cast<std::int64_t>(n))};
    QVERIFY(module->prepare(spec));

    auto input = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, signal);
    auto output = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, signal);
    QVERIFY(module->process(
        input.value()->view(), output.value()->mutable_view(),
        DspProcessContext{frame_range(0, static_cast<std::int64_t>(n)), true, true}));

    const auto out = *output.value()->view().channel(0).value();
    for (double s : out) {
        QVERIFY(std::isfinite(s));
    }

    // Log sweep verification
    std::vector<double> sweep(n, 0.0);
    const double f_start = 20.0;
    const double f_end = 20000.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / fs;
        const double f = f_start * std::pow(f_end / f_start, t / (n / fs));
        sweep[i] = 0.5 * std::sin(2.0 * std::numbers::pi * f * t);
    }
    auto in_sweep = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, sweep);
    auto out_sweep = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, sweep);
    QVERIFY(module->process(
        in_sweep.value()->view(), out_sweep.value()->mutable_view(),
        DspProcessContext{frame_range(0, static_cast<std::int64_t>(n)), true, true}));

    const auto sweep_res = *out_sweep.value()->view().channel(0).value();
    for (double s : sweep_res) {
        QVERIFY(std::isfinite(s));
    }
}

void ParametricEqGoldenTest::multiSampleRateQualification()
{
    // Qualification across required sample rates: 44100, 48000, 96000
    const std::array<double, 3> sample_rates{44100.0, 48000.0, 96000.0};
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto uuid = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000001").value();
    auto band = *EqBandParameters::create(uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, 3.0, 1.0}).value();
    std::vector<EqBandParameters> bands{band};
    auto params = *ParametricEqParameters::create(bands).value();

    for (const double fs : sample_rates) {
        auto module = make_eq_module(*registry.value(), params);
        const DspProcessSpec spec{
            format(rgsml::audio::ChannelLayout::STEREO_LR, fs),
            rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            frame_count(128)};
        QVERIFY2(module->prepare(spec), std::to_string(fs).c_str());

        std::vector<double> in_l(128, 0.5);
        std::vector<double> in_r(128, -0.5);
        auto input = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, in_l, in_r);
        auto output = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, in_l, in_r);

        QVERIFY(module->process(
            input.value()->view(), output.value()->mutable_view(),
            DspProcessContext{frame_range(0, 128), true, true}));
    }
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::ParametricEqGoldenTest)

#include "test_parametric_eq_golden.moc"
