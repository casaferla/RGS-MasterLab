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
using namespace oracles::ref_constants;

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

void verify_rendered_against_o2(
    std::span<const double> actual,
    std::span<const double> expected)
{
    QCOMPARE(actual.size(), expected.size());
    const double max_abs = compute_max_abs_diff(actual, expected);
    const double rms = compute_rms_diff(actual, expected);

    QVERIFY2(max_abs <= 2e-6, std::to_string(max_abs).c_str());
    QVERIFY2(rms <= 5e-7, std::to_string(rms).c_str());

    for (std::size_t i = 0; i < actual.size(); ++i) {
        if (std::abs(expected[i]) >= 1e-4) {
            const double rel = std::abs(actual[i] - expected[i]) / std::abs(expected[i]);
            QVERIFY2(rel <= 2e-5, std::to_string(rel).c_str());
        }
    }
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

    struct FamilyCase {
        const char* name;
        EqFilterType type;
        EqBandPayload payload;
        IndependentBiquadCoeffs expected_coeffs;
    };

    const std::array<FamilyCase, 12> families{{
        {"bell-1k-plus6-q0707", EqFilterType::BELL, BellPayload{1000.0, 6.0, 0.707}, BELL_1K_PLUS6_Q0707},
        {"bell-280-minus6-q12", EqFilterType::BELL, BellPayload{280.0, -6.0, 12.0}, BELL_280_MINUS6_Q12},
        {"bell-1k-plus6-q010", EqFilterType::BELL, BellPayload{1000.0, 6.0, 0.10}, BELL_1K_PLUS6_Q010},
        {"notch-1k-q12", EqFilterType::NOTCH, NotchPayload{1000.0, 12.0}, NOTCH_1K_Q12},
        {"low-shelf-100-plus6-s05", EqFilterType::LOW_SHELF, ShelfPayload{100.0, 6.0, 0.5}, LOW_SHELF_100_PLUS6_S05},
        {"low-shelf-100-plus6-s10", EqFilterType::LOW_SHELF, ShelfPayload{100.0, 6.0, 1.0}, LOW_SHELF_100_PLUS6_S10},
        {"low-shelf-100-minus6-s05", EqFilterType::LOW_SHELF, ShelfPayload{100.0, -6.0, 0.5}, LOW_SHELF_100_MINUS6_S05},
        {"low-shelf-100-minus6-s10", EqFilterType::LOW_SHELF, ShelfPayload{100.0, -6.0, 1.0}, LOW_SHELF_100_MINUS6_S10},
        {"high-shelf-10k-plus6-s05", EqFilterType::HIGH_SHELF, ShelfPayload{10000.0, 6.0, 0.5}, HIGH_SHELF_10K_PLUS6_S05},
        {"high-shelf-10k-plus6-s10", EqFilterType::HIGH_SHELF, ShelfPayload{10000.0, 6.0, 1.0}, HIGH_SHELF_10K_PLUS6_S10},
        {"high-shelf-10k-minus6-s05", EqFilterType::HIGH_SHELF, ShelfPayload{10000.0, -6.0, 0.5}, HIGH_SHELF_10K_MINUS6_S05},
        {"high-shelf-10k-minus6-s10", EqFilterType::HIGH_SHELF, ShelfPayload{10000.0, -6.0, 1.0}, HIGH_SHELF_10K_MINUS6_S10},
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

        const auto actual = *output.value()->view().channel(0).value();

        // O2 reference render from authoritative constants
        std::vector<double> expected(256, 0.0);
        IndependentCascadeTdf2State o2_cascade({fam.expected_coeffs});
        o2_cascade.process_block(impulse, expected);

        verify_rendered_against_o2(actual, expected);

        // O3 Analytic Transfer Verification
        if (fam.type == EqFilterType::BELL) {
            const auto bell = std::get<BellPayload>(fam.payload);
            const auto h = biquad_transfer_function(fam.expected_coeffs, bell.frequency_hz, fs);
            const double gain_lin = std::pow(10.0, bell.gain_db / 20.0);
            QVERIFY(std::abs(std::abs(h) - gain_lin) <= 1e-10);
        } else if (fam.type == EqFilterType::NOTCH) {
            const auto notch = std::get<NotchPayload>(fam.payload);
            const auto h = biquad_transfer_function(fam.expected_coeffs, notch.frequency_hz, fs);
            QVERIFY(std::abs(h) <= 1e-10);
        } else if (fam.type == EqFilterType::LOW_SHELF) {
            const auto shelf = std::get<ShelfPayload>(fam.payload);
            const auto h_dc = biquad_transfer_function(fam.expected_coeffs, 0.0, fs);
            const auto h_nyq = biquad_transfer_function(fam.expected_coeffs, fs / 2.0, fs);
            const double gain_lin = std::pow(10.0, shelf.gain_db / 20.0);
            QVERIFY(std::abs(std::abs(h_dc) - gain_lin) <= 1e-10);
            QVERIFY(std::abs(std::abs(h_nyq) - 1.0) <= 1e-10);
        } else if (fam.type == EqFilterType::HIGH_SHELF) {
            const auto shelf = std::get<ShelfPayload>(fam.payload);
            const auto h_dc = biquad_transfer_function(fam.expected_coeffs, 0.0, fs);
            const auto h_nyq = biquad_transfer_function(fam.expected_coeffs, fs / 2.0, fs);
            const double gain_lin = std::pow(10.0, shelf.gain_db / 20.0);
            QVERIFY(std::abs(std::abs(h_dc) - 1.0) <= 1e-10);
            QVERIFY(std::abs(std::abs(h_nyq) - gain_lin) <= 1e-10);
        }
    }
}

void ParametricEqGoldenTest::fullHpLpMatrixVerification()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto uuid = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000001").value();
    const double fs = 48000.0;

    struct SlopeCase {
        SlopeDbPerOctave slope;
        std::vector<IndependentBiquadCoeffs> hp_sections;
        std::vector<IndependentBiquadCoeffs> lp_sections;
    };

    const std::array<SlopeCase, 6> slopes{{
        {SlopeDbPerOctave::DB_6, {HP_6_SECTIONS, HP_6_SECTIONS + 1}, {LP_6_SECTIONS, LP_6_SECTIONS + 1}},
        {SlopeDbPerOctave::DB_12, {HP_12_SECTIONS, HP_12_SECTIONS + 1}, {LP_12_SECTIONS, LP_12_SECTIONS + 1}},
        {SlopeDbPerOctave::DB_18, {HP_18_SECTIONS, HP_18_SECTIONS + 2}, {LP_18_SECTIONS, LP_18_SECTIONS + 2}},
        {SlopeDbPerOctave::DB_24, {HP_24_SECTIONS, HP_24_SECTIONS + 2}, {LP_24_SECTIONS, LP_24_SECTIONS + 2}},
        {SlopeDbPerOctave::DB_36, {HP_36_SECTIONS, HP_36_SECTIONS + 3}, {LP_36_SECTIONS, LP_36_SECTIONS + 3}},
        {SlopeDbPerOctave::DB_48, {HP_48_SECTIONS, HP_48_SECTIONS + 4}, {LP_48_SECTIONS, LP_48_SECTIONS + 4}},
    }};

    for (const auto& sc : slopes) {
        for (const auto filter_type : {EqFilterType::HIGH_PASS, EqFilterType::LOW_PASS}) {
            const auto& sections = (filter_type == EqFilterType::HIGH_PASS) ? sc.hp_sections : sc.lp_sections;
            auto band = *EqBandParameters::create(uuid, true, filter_type, EqRouting::STEREO, PassPayload{1000.0, sc.slope}).value();
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

            const auto actual = *output.value()->view().channel(0).value();

            std::vector<double> expected(128, 0.0);
            IndependentCascadeTdf2State o2_cascade(sections);
            o2_cascade.process_block(impulse, expected);

            verify_rendered_against_o2(actual, expected);

            // O3 Cutoff Magnitude Verification: |H(fcut)| == 1 / sqrt(2)
            const auto h_cut = cascade_transfer_function(sections, 1000.0, fs);
            const double target_mag = 1.0 / std::numbers::sqrt2;
            QVERIFY2(std::abs(std::abs(h_cut) - target_mag) <= 1e-10, std::to_string(std::abs(std::abs(h_cut) - target_mag)).c_str());
        }
    }
}

void ParametricEqGoldenTest::independentOraclesVerification()
{
    const double fs = 48000.0;
    const auto& coeffs = BELL_1K_PLUS6_Q0707;

    std::vector<double> impulse(32, 0.0);
    impulse[0] = 1.0;

    std::vector<double> expected(32, 0.0);
    IndependentCascadeTdf2State o2_cascade({coeffs});
    o2_cascade.process_block(impulse, expected);

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
    verify_rendered_against_o2(actual, expected);
}

void ParametricEqGoldenTest::multitoneAndSweepVerification()
{
    const double fs = 48000.0;
    const std::size_t n = 1024;
    std::vector<double> signal(n, 0.0);

    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / fs;
        signal[i] = 0.3 * std::sin(2.0 * std::numbers::pi * 100.0 * t) +
                    0.3 * std::sin(2.0 * std::numbers::pi * 1000.0 * t) +
                    0.3 * std::sin(2.0 * std::numbers::pi * 10000.0 * t);
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
        frame_count(static_cast<std::int64_t>(n))};
    QVERIFY(module->prepare(spec));

    auto input = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, signal);
    auto output = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, signal);
    QVERIFY(module->process(
        input.value()->view(), output.value()->mutable_view(),
        DspProcessContext{frame_range(0, static_cast<std::int64_t>(n)), true, true}));

    const auto actual_multitone = *output.value()->view().channel(0).value();
    std::vector<double> expected_multitone(n, 0.0);
    IndependentCascadeTdf2State o2_multitone({BELL_1K_PLUS6_Q0707});
    o2_multitone.process_block(signal, expected_multitone);
    verify_rendered_against_o2(actual_multitone, expected_multitone);

    // Log sweep verification
    module->reset();
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

    const auto actual_sweep = *out_sweep.value()->view().channel(0).value();
    std::vector<double> expected_sweep(n, 0.0);
    IndependentCascadeTdf2State o2_sweep({BELL_1K_PLUS6_Q0707});
    o2_sweep.process_block(sweep, expected_sweep);
    verify_rendered_against_o2(actual_sweep, expected_sweep);
}

void ParametricEqGoldenTest::multiSampleRateQualification()
{
    struct SrCase {
        double fs;
        IndependentBiquadCoeffs coeffs;
    };

    const std::array<SrCase, 3> sample_rates{{
        {44100.0, BELL_1K_PLUS3_Q1_44100},
        {48000.0, BELL_1K_PLUS3_Q1_48000},
        {96000.0, BELL_1K_PLUS3_Q1_96000},
    }};

    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto uuid = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000001").value();
    auto band = *EqBandParameters::create(uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, 3.0, 1.0}).value();
    std::vector<EqBandParameters> bands{band};
    auto params = *ParametricEqParameters::create(bands).value();

    for (const auto& sr : sample_rates) {
        auto module = make_eq_module(*registry.value(), params);
        const DspProcessSpec spec{
            format(rgsml::audio::ChannelLayout::STEREO_LR, sr.fs),
            rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            frame_count(128)};
        QVERIFY2(module->prepare(spec), std::to_string(sr.fs).c_str());

        std::vector<double> in_l(128, 0.5);
        std::vector<double> in_r(128, -0.5);
        auto input = rgsml::audio::AudioBuffer::create(
            format(rgsml::audio::ChannelLayout::STEREO_LR, sr.fs),
            rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            rgsml::core::FrameIndex{0},
            frame_count(128));
        auto output = rgsml::audio::AudioBuffer::create(
            format(rgsml::audio::ChannelLayout::STEREO_LR, sr.fs),
            rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            rgsml::core::FrameIndex{0},
            frame_count(128));
        QVERIFY(input);
        QVERIFY(output);

        auto in_l_span = *input.value()->mutable_view().channel(0).value();
        auto in_r_span = *input.value()->mutable_view().channel(1).value();
        std::copy(in_l.begin(), in_l.end(), in_l_span.begin());
        std::copy(in_r.begin(), in_r.end(), in_r_span.begin());

        QVERIFY(module->process(
            input.value()->view(), output.value()->mutable_view(),
            DspProcessContext{frame_range(0, 128), true, true}));

        const auto out_l = *output.value()->view().channel(0).value();
        const auto out_r = *output.value()->view().channel(1).value();

        std::vector<double> expected_l(128, 0.0);
        std::vector<double> expected_r(128, 0.0);
        IndependentCascadeTdf2State o2_l({sr.coeffs});
        IndependentCascadeTdf2State o2_r({sr.coeffs});
        o2_l.process_block(in_l, expected_l);
        o2_r.process_block(in_r, expected_r);

        verify_rendered_against_o2(out_l, expected_l);
        verify_rendered_against_o2(out_r, expected_r);
    }
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::ParametricEqGoldenTest)

#include "test_parametric_eq_golden.moc"
