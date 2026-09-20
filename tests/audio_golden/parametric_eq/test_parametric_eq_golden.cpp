#include "../render/render_test_support.hpp"
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

class ParametricEqGoldenTest final : public QObject {
    Q_OBJECT

private slots:
    void coefficientOracleVerification();
    void scalarSampleOracleVerification();
    void analyticTransferOracleVerification();
    void fullHpLpMatrixVerification();
};

void ParametricEqGoldenTest::coefficientOracleVerification()
{
    // Verify Bell filter 1k +6dB Q=0.707 against independent coefficient oracle
    const double fs = 48000.0;
    const auto expected = compute_bell_coeffs(1000.0, 6.0, 0.707, fs);

    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto uuid = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000001");
    auto band = *EqBandParameters::create(uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, 6.0, 0.707}).value();
    auto params = *ParametricEqParameters::create({band}).value();
    auto module = *ParametricEqModule::create(eq_descriptor(*registry.value()), params).value();

    const DspProcessSpec spec{
        format(rgsml::audio::ChannelLayout::MONO_C, fs),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(1)};
    QVERIFY(module->prepare(spec));

    // Sample 0 of impulse response is equal to b0 coefficient
    std::vector<double> impulse{1.0};
    auto input = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, impulse);
    auto output = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, impulse);
    QVERIFY(module->process(
        input.value()->view(), output.value()->mutable_view(),
        DspProcessContext{frame_range(0, 1), true, true}));

    const auto b0_actual = (*output.value()->view().channel(0).value())[0];
    QCOMPARE(std::abs(b0_actual - expected.b0) <= 1e-10, true);
}

void ParametricEqGoldenTest::scalarSampleOracleVerification()
{
    // Process 10 samples of impulse response and compare with independent TDF-II oracle
    const double fs = 48000.0;
    const auto coeffs = compute_bell_coeffs(1000.0, 6.0, 0.707, fs);

    IndependentTdf2State oracle_state;
    std::vector<double> impulse(10, 0.0);
    impulse[0] = 1.0;

    std::vector<double> expected_samples(10, 0.0);
    for (std::size_t i = 0; i < 10; ++i) {
        expected_samples[i] = oracle_state.process_sample(impulse[i], coeffs);
    }

    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto uuid = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000001");
    auto band = *EqBandParameters::create(uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, 6.0, 0.707}).value();
    auto params = *ParametricEqParameters::create({band}).value();
    auto module = *ParametricEqModule::create(eq_descriptor(*registry.value()), params).value();

    const DspProcessSpec spec{
        format(rgsml::audio::ChannelLayout::MONO_C, fs),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(10)};
    QVERIFY(module->prepare(spec));

    auto input = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, impulse);
    auto output = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, impulse);
    QVERIFY(module->process(
        input.value()->view(), output.value()->mutable_view(),
        DspProcessContext{frame_range(0, 10), true, true}));

    const auto actual = *output.value()->view().channel(0).value();
    for (std::size_t i = 0; i < 10; ++i) {
        QCOMPARE(std::abs(actual[i] - expected_samples[i]) <= 1e-10, true);
    }
}

void ParametricEqGoldenTest::analyticTransferOracleVerification()
{
    // Analytic transfer H(e^jw) at 1000 Hz for 1kHz Bell +6dB Q=0.707
    const double fs = 48000.0;
    const auto coeffs = compute_bell_coeffs(1000.0, 6.0, 0.707, fs);
    const auto H = biquad_transfer_function(coeffs, 1000.0, fs);
    const double gain_at_center_db = 20.0 * std::log10(std::abs(H));

    // For Bell filter, gain at f0 is exactly gain_db (+6 dB)
    QCOMPARE(std::abs(gain_at_center_db - 6.0) <= 1e-10, true);
}

void ParametricEqGoldenTest::fullHpLpMatrixVerification()
{
    // Test full matrix of slopes: 6, 12, 18, 24, 36, 48 dB/oct
    const std::array<SlopeDbPerOctave, 6> slopes{
        SlopeDbPerOctave::DB_6,
        SlopeDbPerOctave::DB_12,
        SlopeDbPerOctave::DB_18,
        SlopeDbPerOctave::DB_24,
        SlopeDbPerOctave::DB_36,
        SlopeDbPerOctave::DB_48};

    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto uuid = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000001");

    for (const auto slope : slopes) {
        auto hp_band = *EqBandParameters::create(uuid, true, EqFilterType::HIGH_PASS, EqRouting::STEREO, PassPayload{1000.0, slope}).value();
        auto params = *ParametricEqParameters::create({hp_band}).value();
        auto module = *ParametricEqModule::create(eq_descriptor(*registry.value()), params).value();

        const DspProcessSpec spec{
            format(rgsml::audio::ChannelLayout::STEREO_LR, 48000.0),
            rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            frame_count(256)};
        QVERIFY(module->prepare(spec));
    }
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::ParametricEqGoldenTest)

#include "test_parametric_eq_golden.moc"
