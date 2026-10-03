#include "../render/render_test_support.hpp"
#include "test_support.hpp"

#include <rgsml/core/error.hpp>
#include <rgsml/dsp/compressor_module.hpp>
#include <rgsml/dsp/compressor_parameters.hpp>
#include <rgsml/dsp/module_parameter_codec.hpp>
#include <rgsml/dsp/module_registry.hpp>

#include <QtTest/QTest>

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace rgsml::dsp;
using namespace render_support;
using dsp_support::error_category;

class CompressorTest final : public QObject {
    Q_OBJECT

private slots:
    void parametersValidationAndBounds();
    void codecJsonRoundtripAndStrictSchema();
    void registryDescriptorAndFactory();
    void peakAndRmsDetectorMath();
    void exactDigitalZero();
    void hardAndSoftKneeCurve();
    void ratioNeutrality();
    void attackAndReleaseBallistics();
    void channelLinkModesAndMonoInvariance();
    void lookaheadAndTiesToEvenSampleRates();
    void mixAndMakeupGain();
    void resetAndChunkInvariance();
    void checkpointAndRestoreContinuation();
    void finalizeEosShortSourceCases();
    void sonicFingerprintAndCheckpointRejection();
};

void CompressorTest::parametersValidationAndBounds()
{
    // Defaults
    auto def = CompressorParameters::create_default();
    QVERIFY(def);
    QCOMPARE(def.value()->detector_mode(), CompressorDetectorMode::RMS);
    QCOMPARE(def.value()->channel_link(), CompressorChannelLink::LINKED_MAX);
    QCOMPARE(def.value()->threshold_dbfs(), -24.0);
    QCOMPARE(def.value()->ratio(), 2.0);
    QCOMPARE(def.value()->knee_db(), 6.0);
    QCOMPARE(def.value()->attack_ms(), 30.0);
    QCOMPARE(def.value()->release_ms(), 200.0);
    QCOMPARE(def.value()->rms_time_constant_ms(), 50.0);
    QCOMPARE(def.value()->look_ahead_ms(), 5.0);
    QCOMPARE(def.value()->mix_percent(), 100.0);
    QCOMPARE(def.value()->makeup_gain_db(), 0.0);

    const double nan_v = std::numeric_limits<double>::quiet_NaN();
    const double pos_inf = std::numeric_limits<double>::infinity();
    const double neg_inf = -std::numeric_limits<double>::infinity();

    // Systematic numeric parameter boundary testing
    // thresholdDbfs [-120.0, 0.0]
    QVERIFY(CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -120.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, 0.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -120.0001, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, 0.0001, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, nan_v, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, pos_inf, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, neg_inf, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));

    // ratio [1.0, 20.0]
    QVERIFY(CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 1.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 20.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 0.9999, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 20.0001, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, nan_v, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, pos_inf, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, neg_inf, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));

    // kneeDb [0.0, 24.0]
    QVERIFY(CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 0.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 24.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, -0.0001, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 24.0001, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, nan_v, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, pos_inf, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, neg_inf, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));

    // attackMs [0.1, 500.0]
    QVERIFY(CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 0.1, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 500.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 0.0999, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 500.0001, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, nan_v, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, pos_inf, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, neg_inf, 200.0, 50.0, 5.0, 100.0, 0.0));

    // releaseMs [1.0, 5000.0]
    QVERIFY(CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 1.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 5000.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 0.9999, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 5000.0001, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, nan_v, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, pos_inf, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, neg_inf, 50.0, 5.0, 100.0, 0.0));

    // rmsTimeConstantMs [1.0, 500.0]
    QVERIFY(CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 1.0, 5.0, 100.0, 0.0));
    QVERIFY(CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 500.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 0.9999, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 500.0001, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, nan_v, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, pos_inf, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, neg_inf, 5.0, 100.0, 0.0));

    // lookAheadMs [0.0, 20.0]
    QVERIFY(CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, 0.0, 100.0, 0.0));
    QVERIFY(CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, 20.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, -0.0001, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, 20.0001, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, nan_v, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, pos_inf, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, neg_inf, 100.0, 0.0));

    // mixPercent [0.0, 100.0]
    QVERIFY(CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 0.0, 0.0));
    QVERIFY(CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, -0.0001, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0001, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, nan_v, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, pos_inf, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, neg_inf, 0.0));

    // makeupGainDb [-24.0, 24.0]
    QVERIFY(CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, -24.0));
    QVERIFY(CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 24.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, -24.0001));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 24.0001));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, nan_v));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, pos_inf));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, neg_inf));

    // Enum validation
    QVERIFY(!CompressorParameters::create(static_cast<CompressorDetectorMode>(99), CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, static_cast<CompressorChannelLink>(99), -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
}

void CompressorTest::codecJsonRoundtripAndStrictSchema()
{
    auto orig = *CompressorParameters::create(
        CompressorDetectorMode::PEAK, CompressorChannelLink::DUAL_MONO,
        -18.5, 4.0, 3.0, 15.0, 150.0, 25.0, 2.5, 75.0, 3.0).value();

    auto json_str = encode_compressor_parameters_json(orig);
    QVERIFY(json_str);

    auto decoded = decode_compressor_parameters_json(*json_str.value());
    QVERIFY(decoded);
    QCOMPARE(*decoded.value(), orig);

    // Decode via generic codec
    auto payload_res = decode_module_parameters_json("rgsml.dsp.compressor.parameters/1.0.0", *json_str.value());
    QVERIFY(payload_res);
    const auto* dec_comp = std::get_if<CompressorParameters>(payload_res.value());
    QVERIFY(dec_comp != nullptr);
    QCOMPARE(*dec_comp, orig);

    // Invalid schema
    auto bad_schema = decode_module_parameters_json("rgsml.dsp.wrong/1.0.0", *json_str.value());
    QVERIFY(!bad_schema);

    // Missing key in JSON (strict 11 keys required)
    auto bad_json = decode_compressor_parameters_json("{\"detectorMode\":\"RMS\"}");
    QVERIFY(!bad_json);
}

void CompressorTest::registryDescriptorAndFactory()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    auto desc = registry.value()->find_descriptor("rgsml.dsp.compressor");
    QVERIFY(desc);
    QCOMPARE(desc.value()->get().algorithm_version(), std::optional<std::string_view>{"1.0.0"});
    QCOMPARE(desc.value()->get().parameter_schema_id(), std::optional<std::string_view>{"rgsml.dsp.compressor.parameters/1.0.0"});
    QVERIFY(registry.value()->has_factory("rgsml.dsp.compressor"));

    auto mod = registry.value()->create_module("rgsml.dsp.compressor");
    QVERIFY(mod);
    const auto* comp = dynamic_cast<const CompressorModule*>(mod.value()->get());
    QVERIFY(comp != nullptr);
    QCOMPARE(comp->descriptor().type_id(), std::string_view{"rgsml.dsp.compressor"});
}

void CompressorTest::peakAndRmsDetectorMath()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    // 1. Peak detector math test: sample 0.5 = -6.020599913279624 dBFS
    // Threshold = -12 dBFS, ratio = 4.0, knee = 0 dB, attack = 0.1 ms (48 kHz)
    auto peak_params = *CompressorParameters::create(
        CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX,
        -12.0, 4.0, 0.0, 0.1, 1000.0, 50.0, 0.0, 100.0, 0.0).value();
    auto peak_mod = std::move(*CompressorModule::create(desc, peak_params).value());

    const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(512)};
    QVERIFY(peak_mod->prepare(spec));

    const std::array peak_samples{0.5};
    auto in_peak = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, peak_samples);
    auto out_peak = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, peak_samples);

    QVERIFY(peak_mod->process(in_peak.value()->view(), out_peak.value()->mutable_view(), DspProcessContext{frame_range(0, 1), true, false}));

    const double p0 = 0.5;
    const double x_db = 20.0 * std::log10(p0); // -6.020599913279624
    const double targ0 = (x_db - (-12.0)) * (1.0 - (1.0 / 4.0)); // 4.484550065040282 dB
    const double a_att = std::exp(-1.0 / (0.0001 * 48000.0)); // exp(-0.20833333333333334) = 0.8119223126131828
    const double expected_red0 = (1.0 - a_att) * targ0; // 0.8434455850989379 dB
    const double expected_gain = std::pow(10.0, -expected_red0 / 20.0); // 0.9074609653597405
    const double expected_out = 0.5 * expected_gain; // 0.45373048267987025

    const double actual_out = (*out_peak.value()->view().channel(0).value())[0];
    QCOMPARE(actual_out, expected_out);

    // 2. RMS detector math test: sample 0.5
    // m[0] = (1 - a_rms) * (0.5)^2
    auto rms_params = *CompressorParameters::create(
        CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX,
        -12.0, 4.0, 0.0, 0.1, 1000.0, 50.0, 0.0, 100.0, 0.0).value();
    auto rms_mod = std::move(*CompressorModule::create(desc, rms_params).value());
    QVERIFY(rms_mod->prepare(spec));

    auto in_rms = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, peak_samples);
    auto out_rms = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, peak_samples);
    QVERIFY(rms_mod->process(in_rms.value()->view(), out_rms.value()->mutable_view(), DspProcessContext{frame_range(0, 1), true, false}));

    const double a_rms = std::exp(-1.0 / (0.05 * 48000.0)); // exp(-1/2400) = 0.9995834201388658
    const double expected_m0 = (1.0 - a_rms) * (0.5 * 0.5); // 0.00010414496528354228
    const double expected_p0 = std::sqrt(expected_m0); // 0.01020514405991127 (-39.8236 dBFS)
    QVERIFY(expected_p0 > 0.01 && expected_p0 < 0.02);
    // Since -39.8236 dBFS < -12.0 dBFS threshold, target reduction = 0 dB!
    // Therefore smoothed reduction = 0 dB, output sample = 0.5.
    const double actual_rms_out = (*out_rms.value()->view().channel(0).value())[0];
    QCOMPARE(actual_rms_out, 0.5);
}

void CompressorTest::exactDigitalZero()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    auto params = *CompressorParameters::create_default().value();
    auto mod = std::move(*CompressorModule::create(desc, params).value());

    const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::STEREO_LR, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(512)};
    QVERIFY(mod->prepare(spec));

    const std::array zero_left{0.0, 0.0, 0.0, 0.0};
    const std::array zero_right{0.0, 0.0, 0.0, 0.0};
    auto in_buf = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, zero_left, zero_right);
    auto out_buf = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, zero_left, zero_right);

    const DspProcessContext context{frame_range(0, 4), true, true};
    QVERIFY(mod->process(in_buf.value()->view(), out_buf.value()->mutable_view(), context));

    // Output must be exact 0.0
    for (std::size_t ch = 0; ch < 2; ++ch) {
        const auto plane = *out_buf.value()->view().channel(ch).value();
        for (const double sample : plane) {
            QCOMPARE(sample, 0.0);
        }
    }
}

void CompressorTest::hardAndSoftKneeCurve()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(512)};

    // 1. Hard knee W = 0, threshold = -12.0 dBFS (0.251188643150958), ratio = 4.0, attack = 0.1 ms
    // Below threshold: x_db = -20 dBFS (sample = 0.1) -> target reduction = 0 dB -> output = 0.1
    // At threshold: x_db = -12 dBFS (sample = 0.251188643150958) -> target reduction = 0 dB -> output = 0.251188643150958
    // Above threshold: x_db = -6.0205999 dBFS (sample = 0.5) -> target reduction = (-6.0205999 - (-12)) * 0.75 = 4.48455 dB -> compressed
    auto hard_params = *CompressorParameters::create(
        CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX,
        -12.0, 4.0, 0.0, 0.1, 1000.0, 50.0, 0.0, 100.0, 0.0).value();
    auto hard_mod = std::move(*CompressorModule::create(desc, hard_params).value());
    QVERIFY(hard_mod->prepare(spec));

    const std::array hard_test_samples{
        0.1,                 // -20 dBFS (below threshold)
        0.251188643150958,   // -12 dBFS (at threshold)
        0.5                  // -6.02 dBFS (above threshold)
    };
    auto in_hard = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, hard_test_samples);
    auto out_hard = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, hard_test_samples);
    QVERIFY(hard_mod->process(in_hard.value()->view(), out_hard.value()->mutable_view(), DspProcessContext{frame_range(0, 3), true, false}));

    const auto hard_out = *out_hard.value()->view().channel(0).value();
    QCOMPARE(hard_out[0], 0.1);                // Below threshold: exact dry
    QCOMPARE(hard_out[1], 0.251188643150958);  // At threshold: exact dry
    QVERIFY(hard_out[2] < 0.5);                // Above threshold: compressed

    // 2. Soft knee W = 10, threshold = -12 dB, ratio = 4.0
    // Knee region: [-17.0, -7.0] dBFS.
    // Lower boundary: -17 dBFS (sample = 0.14125375446227544) -> target reduction = 0 dB
    // Inside knee: -12 dBFS (sample = 0.251188643150958) -> diff = 0, term = 5, y_db = -12 + ((-0.75)*25)/20 = -12.9375 -> targ = 0.9375 dB
    // Upper boundary: -7 dBFS (sample = 0.44668359215096315) -> diff = 5, targ = 5 * 0.75 = 3.75 dB
    // Above knee: 0 dBFS (sample = 1.0) -> diff = 12, targ = 12 * 0.75 = 9.0 dB
    auto soft_params = *CompressorParameters::create(
        CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX,
        -12.0, 4.0, 10.0, 0.1, 1000.0, 50.0, 0.0, 100.0, 0.0).value();
    auto soft_mod = std::move(*CompressorModule::create(desc, soft_params).value());
    QVERIFY(soft_mod->prepare(spec));

    const std::array soft_test_samples{
        0.14125375446227544, // -17 dBFS (lower boundary)
        0.251188643150958,   // -12 dBFS (inside knee center)
        0.44668359215096315, // -7 dBFS (upper boundary)
        1.0                  // 0 dBFS (above knee)
    };
    auto in_soft = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, soft_test_samples);
    auto out_soft = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, soft_test_samples);
    QVERIFY(soft_mod->process(in_soft.value()->view(), out_soft.value()->mutable_view(), DspProcessContext{frame_range(0, 4), true, false}));

    const auto soft_out = *out_soft.value()->view().channel(0).value();
    QVERIFY(soft_out[0] > 0.13); // Lower boundary: uncompressed
    QVERIFY(soft_out[1] < 0.25118864); // Inside knee: soft compression applied
    QVERIFY(soft_out[2] < soft_out[1] * 2.0); // Upper boundary: compressed
    QVERIFY(soft_out[3] < 1.0); // Above knee: gain reduction applied (< input 1.0)
}

void CompressorTest::ratioNeutrality()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    // ratio = 1.0 (neutral compression)
    auto params = *CompressorParameters::create(
        CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX,
        -20.0, 1.0, 0.0, 0.1, 10.0, 50.0, 0.0, 100.0, 0.0).value();
    auto mod = std::move(*CompressorModule::create(desc, params).value());

    const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(512)};
    QVERIFY(mod->prepare(spec));

    const std::array samples{0.5, 1.0, -0.8, 0.2};
    auto in_buf = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
    auto out_buf = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);

    QVERIFY(mod->process(in_buf.value()->view(), out_buf.value()->mutable_view(), DspProcessContext{frame_range(0, 4), true, true}));

    // Output matches input exactly when ratio = 1.0 and makeup = 0, lookahead = 0
    QCOMPARE(bits(out_buf.value()->view()), bits(in_buf.value()->view()));
}

void CompressorTest::attackAndReleaseBallistics()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    // Attack = 10 ms, Release = 100 ms @ 48 kHz
    // a_attack = exp(-1/(0.010 * 48000)) = exp(-1/480) = 0.9979188373204907
    // a_release = exp(-1/(0.100 * 48000)) = exp(-1/4800) = 0.9997916880088812
    auto params = *CompressorParameters::create(
        CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX,
        -20.0, 4.0, 0.0, 10.0, 100.0, 50.0, 0.0, 100.0, 0.0).value();
    auto mod = std::move(*CompressorModule::create(desc, params).value());

    const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(512)};
    QVERIFY(mod->prepare(spec));

    // Attack phase: high amplitude sample 1.0 (0 dBFS, target reduction = 15.0 dB)
    std::vector<double> high_samples(10U, 1.0);
    auto in_att = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, high_samples);
    auto out_att = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, high_samples);
    QVERIFY(mod->process(in_att.value()->view(), out_att.value()->mutable_view(), DspProcessContext{frame_range(0, 10), true, false}));

    const auto att_out = *out_att.value()->view().channel(0).value();

    // Verify first attack recurrence value against exact formula:
    // red_att_0 = (1 - a_attack) * 15.0 = 0.0312174401926392 dB
    // expected out 0 = 1.0 * 10^(-0.0312174401926392 / 20) = 0.9964115160868846
    const double a_att = std::exp(-1.0 / (0.01 * 48000.0));
    const double exp_red0 = (1.0 - a_att) * 15.0;
    const double exp_out0 = std::pow(10.0, -exp_red0 / 20.0);
    QCOMPARE(att_out[0], exp_out0);

    // Verify second attack recurrence value:
    const double exp_red1 = a_att * exp_red0 + (1.0 - a_att) * 15.0;
    const double exp_out1 = std::pow(10.0, -exp_red1 / 20.0);
    QCOMPARE(att_out[1], exp_out1);

    // Release phase: feed non-zero samples below threshold (sample = 0.1 = -20 dBFS)
    // Target reduction = 0 dB < prev_reduction, so release ballistics a_release kicks in!
    std::vector<double> thresh_samples(10U, 0.1);
    auto in_rel = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 10, thresh_samples);
    auto out_rel = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 10, thresh_samples);
    QVERIFY(mod->process(in_rel.value()->view(), out_rel.value()->mutable_view(), DspProcessContext{frame_range(10, 20), false, false}));

    const auto rel_out = *out_rel.value()->view().channel(0).value();

    // Calculate last reduction after 10 attack steps:
    double curr_red = 0.0;
    for (std::size_t s = 0; s < 10; ++s) {
        curr_red = a_att * curr_red + (1.0 - a_att) * 15.0;
    }

    const double a_rel = std::exp(-1.0 / (0.100 * 48000.0));

    // First release step:
    const double exp_rel_red0 = a_rel * curr_red + (1.0 - a_rel) * 0.0;
    const double exp_rel_out0 = 0.1 * std::pow(10.0, -exp_rel_red0 / 20.0);
    QCOMPARE(rel_out[0], exp_rel_out0);

    // Second release step:
    const double exp_rel_red1 = a_rel * exp_rel_red0 + (1.0 - a_rel) * 0.0;
    const double exp_rel_out1 = 0.1 * std::pow(10.0, -exp_rel_red1 / 20.0);
    QCOMPARE(rel_out[1], exp_rel_out1);

    // Release phase gain increases monotonically (out_rel[1] > out_rel[0])
    QVERIFY(rel_out[1] > rel_out[0]);
}

void CompressorTest::channelLinkModesAndMonoInvariance()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    // 1. Stereo asymmetric input: Left = 1.0 (0 dBFS), Right = 0.25 (-12.04 dBFS)
    const std::array st_left{1.0, 1.0, 1.0, 1.0, 1.0};
    const std::array st_right{0.25, 0.25, 0.25, 0.25, 0.25};
    auto in_st = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, st_left, st_right);

    const DspProcessSpec spec_st{format(rgsml::audio::ChannelLayout::STEREO_LR, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(512)};

    // LINKED_MAX
    auto max_params = *CompressorParameters::create(
        CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX,
        -12.0, 4.0, 0.0, 0.1, 50.0, 50.0, 0.0, 100.0, 0.0).value();
    auto mod_max = std::move(*CompressorModule::create(desc, max_params).value());
    QVERIFY(mod_max->prepare(spec_st));
    auto out_max = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, st_left, st_right);
    QVERIFY(mod_max->process(in_st.value()->view(), out_max.value()->mutable_view(), DspProcessContext{frame_range(0, 5), true, false}));

    // LINKED_MEAN
    auto mean_params = *CompressorParameters::create(
        CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MEAN,
        -12.0, 4.0, 0.0, 0.1, 50.0, 50.0, 0.0, 100.0, 0.0).value();
    auto mod_mean = std::move(*CompressorModule::create(desc, mean_params).value());
    QVERIFY(mod_mean->prepare(spec_st));
    auto out_mean = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, st_left, st_right);
    QVERIFY(mod_mean->process(in_st.value()->view(), out_mean.value()->mutable_view(), DspProcessContext{frame_range(0, 5), true, false}));

    // DUAL_MONO
    auto dual_params = *CompressorParameters::create(
        CompressorDetectorMode::PEAK, CompressorChannelLink::DUAL_MONO,
        -12.0, 4.0, 0.0, 0.1, 50.0, 50.0, 0.0, 100.0, 0.0).value();
    auto mod_dual = std::move(*CompressorModule::create(desc, dual_params).value());
    QVERIFY(mod_dual->prepare(spec_st));
    auto out_dual = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, st_left, st_right);
    QVERIFY(mod_dual->process(in_st.value()->view(), out_dual.value()->mutable_view(), DspProcessContext{frame_range(0, 5), true, false}));

    // Verify LINKED_MAX, LINKED_MEAN, and DUAL_MONO produce distinct outputs for asymmetric stereo!
    QVERIFY(bits(out_max.value()->view()) != bits(out_mean.value()->view()));
    QVERIFY(bits(out_max.value()->view()) != bits(out_dual.value()->view()));

    // 2. Mono input: LINKED_MAX, LINKED_MEAN, DUAL_MONO produce 100% bit-identical output
    const std::array mono_samples{0.5, 0.8, -0.9, 0.1, 0.3};
    auto in_mono = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, mono_samples);

    std::vector<std::uint64_t> ref_bits;

    for (const auto link_mode : {CompressorChannelLink::LINKED_MAX, CompressorChannelLink::LINKED_MEAN, CompressorChannelLink::DUAL_MONO}) {
        auto params = *CompressorParameters::create(
            CompressorDetectorMode::PEAK, link_mode,
            -12.0, 4.0, 0.0, 5.0, 50.0, 50.0, 0.0, 100.0, 0.0).value();
        auto mod = std::move(*CompressorModule::create(desc, params).value());

        const DspProcessSpec spec_mono{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(512)};
        QVERIFY(mod->prepare(spec_mono));

        auto out_buf = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, mono_samples);
        QVERIFY(mod->process(in_mono.value()->view(), out_buf.value()->mutable_view(), DspProcessContext{frame_range(0, 5), true, true}));

        if (ref_bits.empty()) {
            ref_bits = bits(out_buf.value()->view());
        } else {
            QCOMPARE(bits(out_buf.value()->view()), ref_bits);
        }
    }
}

void CompressorTest::lookaheadAndTiesToEvenSampleRates()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    // 1. Qualify production lookAheadMs -> lookAheadFrames ties-to-even rounding through runtime_requirements at Fs = 1000 Hz
    const DspProcessSpec spec1000{format(rgsml::audio::ChannelLayout::MONO_C, 1000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(512)};

    const struct {
        double look_ms;
        std::int64_t expected_frames;
    } tie_cases[] = {
        {0.5, 0},
        {1.5, 2},
        {2.5, 2},
        {3.5, 4}
    };

    for (const auto& tc : tie_cases) {
        auto params = *CompressorParameters::create(
            CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX,
            -12.0, 2.0, 6.0, 30.0, 200.0, 50.0, tc.look_ms, 100.0, 0.0).value();
        auto mod = std::move(*CompressorModule::create(desc, params).value());
        auto reqs = mod->runtime_requirements(spec1000);
        QVERIFY(reqs);
        QCOMPARE(reqs.value()->algorithmic_latency_frames.value(), tc.expected_frames);
        QCOMPARE(reqs.value()->look_ahead_frames.value(), tc.expected_frames);
    }

    // 2. Sample rate conversion cases: 44.1 / 48 / 96 / 192 kHz with look_ahead_ms = 5.0
    const struct {
        std::int64_t sr;
        std::int64_t expected_frames;
    } sr_cases[] = {
        {44100, 220}, // 220.5 -> 220
        {48000, 240}, // 240.0 -> 240
        {96000, 480}, // 480.0 -> 480
        {192000, 960} // 960.0 -> 960
    };

    for (const auto& sc : sr_cases) {
        auto params = *CompressorParameters::create(
            CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX,
            -12.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0).value();
        auto mod = std::move(*CompressorModule::create(desc, params).value());

        const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::STEREO_LR, sc.sr), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(512)};
        auto reqs = mod->runtime_requirements(spec);
        QVERIFY(reqs);

        QCOMPARE(reqs.value()->algorithmic_latency_frames.value(), sc.expected_frames);
        QCOMPARE(reqs.value()->look_ahead_frames.value(), sc.expected_frames);
    }
}

void CompressorTest::mixAndMakeupGain()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(512)};

    // Matrix testing mixPercent (0, 50, 100) and makeupGainDb (-24, 0, +24)
    // Threshold = 0.0 dBFS (0 dB gain reduction on 0.5 input sample)
    const double dry = 0.5;
    const std::array test_samples{dry};

    const double mixes[] = {0.0, 50.0, 100.0};
    const double makeups[] = {-24.0, 0.0, 24.0};

    for (const double mix : mixes) {
        for (const double makeup : makeups) {
            auto params = *CompressorParameters::create(
                CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX,
                0.0, 1.0, 0.0, 0.1, 10.0, 50.0, 0.0, mix, makeup).value();
            auto mod = std::move(*CompressorModule::create(desc, params).value());
            QVERIFY(mod->prepare(spec));

            auto in_b = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, test_samples);
            auto out_b = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, test_samples);
            QVERIFY(mod->process(in_b.value()->view(), out_b.value()->mutable_view(), DspProcessContext{frame_range(0, 1), true, false}));

            const double mk_factor = std::pow(10.0, makeup / 20.0);
            const double wet = dry * 1.0 * mk_factor; // uncompressed wet * makeup
            const double exp_out = (1.0 - mix / 100.0) * dry + (mix / 100.0) * wet;

            const double actual_out = (*out_b.value()->view().channel(0).value())[0];
            QCOMPARE(actual_out, exp_out);
        }
    }

    // Prove mix = 0% is active delayed dry (not un-delayed bypass identity) when lookahead > 0
    // lookahead = 5.0 ms @ 48 kHz = 240 frames
    auto delay_mix_zero = *CompressorParameters::create(
        CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX,
        -12.0, 4.0, 0.0, 0.1, 10.0, 50.0, 5.0, 0.0, 12.0).value();
    auto mod_del = std::move(*CompressorModule::create(desc, delay_mix_zero).value());
    QVERIFY(mod_del->prepare(spec));

    std::vector<double> in_del_smp(241U, 0.0);
    in_del_smp[0] = 0.5; // Impulse at frame 0
    auto in_del = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, in_del_smp);
    auto out_del = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, in_del_smp);
    QVERIFY(mod_del->process(in_del.value()->view(), out_del.value()->mutable_view(), DspProcessContext{frame_range(0, 241), true, false}));

    const auto del_out = *out_del.value()->view().channel(0).value();
    QCOMPARE(del_out[0], 0.0);   // Frame 0 is zero due to lookahead delay
    QCOMPARE(del_out[240], 0.5); // Frame 240 outputs the exact delayed dry impulse!
}

void CompressorTest::resetAndChunkInvariance()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    auto params = *CompressorParameters::create_default().value();
    auto mod1 = std::move(*CompressorModule::create(desc, params).value());
    auto mod2 = std::move(*CompressorModule::create(desc, params).value());

    const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(512)};
    QVERIFY(mod1->prepare(spec));
    QVERIFY(mod2->prepare(spec));

    std::vector<double> samples(200U);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = std::sin(2.0 * M_PI * 1000.0 * static_cast<double>(i) / 48000.0);
    }
    auto in_buf = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);

    // Process all 200 in single block
    auto out1 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
    QVERIFY(mod1->process(in_buf.value()->view(), out1.value()->mutable_view(), DspProcessContext{frame_range(0, 200), true, true}));

    // Process in two 100-sample chunks
    auto out2 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
    auto chunk1_in = in_buf.value()->view().subview(rgsml::core::FrameIndex{0}, frame_count(100));
    auto chunk1_out = out2.value()->mutable_view().subview(rgsml::core::FrameIndex{0}, frame_count(100));
    QVERIFY(mod2->process(*chunk1_in.value(), *chunk1_out.value(), DspProcessContext{frame_range(0, 100), true, false}));

    auto chunk2_in = in_buf.value()->view().subview(rgsml::core::FrameIndex{100}, frame_count(100));
    auto chunk2_out = out2.value()->mutable_view().subview(rgsml::core::FrameIndex{100}, frame_count(100));
    QVERIFY(mod2->process(*chunk2_in.value(), *chunk2_out.value(), DspProcessContext{frame_range(100, 200), false, true}));

    QCOMPARE(bits(out1.value()->view()), bits(out2.value()->view()));

    // Reset fresh equivalence test
    mod2->reset();
    auto out_reset = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
    QVERIFY(mod2->process(in_buf.value()->view(), out_reset.value()->mutable_view(), DspProcessContext{frame_range(0, 200), true, true}));
    QCOMPARE(bits(out1.value()->view()), bits(out_reset.value()->view()));
}

void CompressorTest::checkpointAndRestoreContinuation()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    auto params = *CompressorParameters::create_default().value();
    auto mod1 = std::move(*CompressorModule::create(desc, params).value());
    auto mod2 = std::move(*CompressorModule::create(desc, params).value());

    const DspProcessSpec spec1{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(512)};
    const DspProcessSpec spec2{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(1024)};

    // Checkpoint before prepare fails -> InvalidState / DSP_MODULE_NOT_PREPARED
    auto cp_unprep = mod1->runtime_checkpoint();
    QVERIFY(!cp_unprep);
    QCOMPARE(cp_unprep.error()->code(), rgsml::core::ErrorCode::InvalidState);

    QVERIFY(mod1->prepare(spec1));
    QVERIFY(mod2->prepare(spec2)); // Different max block frames!

    std::vector<double> samples(100U);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = std::sin(2.0 * M_PI * 440.0 * static_cast<double>(i) / 48000.0);
    }
    auto in_buf = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
    auto out_buf = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);

    auto chunk1_in = in_buf.value()->view().subview(rgsml::core::FrameIndex{0}, frame_count(50));
    auto chunk1_out = out_buf.value()->mutable_view().subview(rgsml::core::FrameIndex{0}, frame_count(50));
    QVERIFY(mod1->process(*chunk1_in.value(), *chunk1_out.value(), DspProcessContext{frame_range(0, 50), true, false}));

    // Checkpoint mod1
    auto cp_res = mod1->runtime_checkpoint();
    QVERIFY(cp_res);
    const auto cp = *cp_res.value();

    // Rejection tests:
    auto bad_type = cp; bad_type.module_type_id = "rgsml.dsp.wrong";
    QVERIFY(!mod2->restore_runtime_checkpoint(bad_type));

    auto bad_algo = cp; bad_algo.algorithm_version = "9.9.9";
    QVERIFY(!mod2->restore_runtime_checkpoint(bad_algo));

    auto bad_schema = cp; bad_schema.parameter_schema_id = "wrong/1.0.0";
    QVERIFY(!mod2->restore_runtime_checkpoint(bad_schema));

    auto bad_fp = cp; bad_fp.sonic_fingerprint = "wrong_fp";
    QVERIFY(!mod2->restore_runtime_checkpoint(bad_fp));

    auto bad_domain = cp; bad_domain.frame_domain_id = rgsml::audio::FrameDomainId::OUTPUT_RATE;
    QVERIFY(!mod2->restore_runtime_checkpoint(bad_domain));

    auto bad_cp_ver = cp; bad_cp_ver.checkpoint_schema_version = "wrong/1.0.0";
    QVERIFY(!mod2->restore_runtime_checkpoint(bad_cp_ver));

    auto bad_backend = cp; bad_backend.backend_identity = "wrong_backend";
    QVERIFY(!mod2->restore_runtime_checkpoint(bad_backend));

    auto corrupt_p = cp; corrupt_p.payload.resize(2); // Truncated
    QVERIFY(!mod2->restore_runtime_checkpoint(corrupt_p));

    auto trailing_p = cp; trailing_p.payload.push_back(0xFF); // Trailing byte
    QVERIFY(!mod2->restore_runtime_checkpoint(trailing_p));

    // Restore valid cp into mod2 (unbound)
    QVERIFY(mod2->restore_runtime_checkpoint(cp));

    // Process second 50 samples in both
    auto out1 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 50, std::span<const double>(samples.data() + 50, 50));
    auto out2 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 50, std::span<const double>(samples.data() + 50, 50));
    auto chunk2_in = in_buf.value()->view().subview(rgsml::core::FrameIndex{50}, frame_count(50));

    QVERIFY(mod1->process(*chunk2_in.value(), out1.value()->mutable_view(), DspProcessContext{frame_range(50, 100), false, true}));
    QVERIFY(mod2->process(*chunk2_in.value(), out2.value()->mutable_view(), DspProcessContext{frame_range(50, 100), false, true}));

    // Bit-identical continuation
    QCOMPARE(bits(out1.value()->view()), bits(out2.value()->view()));

    // Bound restore next_input_frame mismatch rejection test:
    // Process 10 frames on mod2 -> mod2 next_input_frame is 100
    auto extra_in = in_buf.value()->view().subview(rgsml::core::FrameIndex{0}, frame_count(10));
    auto extra_out = out_buf.value()->mutable_view().subview(rgsml::core::FrameIndex{0}, frame_count(10));
    QVERIFY(mod2->process(*extra_in.value(), *extra_out.value(), DspProcessContext{frame_range(100, 110), false, false}));

    // mod2 is bound at next_input_frame = 110.
    // cp has next_input_frame = 50 -> restoring cp (50) into mod2 (bound at 110) must be REJECTED!
    auto mismatch_next = mod2->restore_runtime_checkpoint(cp);
    QVERIFY(!mismatch_next);

    // Unbound module (fresh prepare) accepts cp (50)
    QVERIFY(mod2->prepare(spec2));
    QVERIFY(mod2->restore_runtime_checkpoint(cp));
}

void CompressorTest::finalizeEosShortSourceCases()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    // Lookahead L = 5.0 ms @ 48 kHz = 240 frames
    auto params = *CompressorParameters::create(
        CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX,
        0.0, 1.0, 0.0, 0.1, 10.0, 50.0, 5.0, 100.0, 0.0).value();
    auto mod = std::move(*CompressorModule::create(desc, params).value());

    const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(512)};
    QVERIFY(mod->prepare(spec));

    // 1. Non-finite sample pre-scan validation in process
    const double nan_val = std::numeric_limits<double>::quiet_NaN();
    const std::array nan_samples{0.1, nan_val, 0.2};
    auto nan_in = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, nan_samples);
    auto nan_out = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, nan_samples);
    auto nan_res = mod->process(nan_in.value()->view(), nan_out.value()->mutable_view(), DspProcessContext{frame_range(0, 3), true, false});
    QVERIFY(!nan_res);
    QCOMPARE(nan_res.error()->code(), rgsml::core::ErrorCode::InvalidAudioSample);

    // Re-prepare clean module
    QVERIFY(mod->prepare(spec));

    // 2. N = 0 empty stream direct finalize (240 frames)
    auto fin_empty = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, std::vector<double>(240U, 0.0));
    QVERIFY(mod->finalize(fin_empty.value()->mutable_view(), DspProcessContext{frame_range(0, 240), true, true}));

    // Checkpoint after completed finalize rejected
    auto cp_res = mod->runtime_checkpoint();
    QVERIFY(!cp_res);
    QCOMPARE(cp_res.error()->code(), rgsml::core::ErrorCode::UnsupportedOperation);
    QCOMPARE(dsp_support::error_category(*cp_res.error()), std::string_view{"RUNTIME_CHECKPOINT_DURING_FINALIZE_UNSUPPORTED"});

    // 3. 0 < N < L (N = 100 < L = 240)
    QVERIFY(mod->prepare(spec));
    std::vector<double> smp100(100U, 0.5);
    auto in100 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, smp100);
    auto out100 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, smp100);
    QVERIFY(mod->process(in100.value()->view(), out100.value()->mutable_view(), DspProcessContext{frame_range(0, 100), true, false}));
    auto fin100 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 100, std::vector<double>(240U, 0.0));
    QVERIFY(mod->finalize(fin100.value()->mutable_view(), DspProcessContext{frame_range(100, 340), false, true}));

    // 4. N = L (N = 240 = L = 240)
    QVERIFY(mod->prepare(spec));
    std::vector<double> smp240(240U, 0.5);
    auto in240 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, smp240);
    auto out240 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, smp240);
    QVERIFY(mod->process(in240.value()->view(), out240.value()->mutable_view(), DspProcessContext{frame_range(0, 240), true, false}));
    auto fin240 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 240, std::vector<double>(240U, 0.0));
    QVERIFY(mod->finalize(fin240.value()->mutable_view(), DspProcessContext{frame_range(240, 480), false, true}));

    // 5. N > L (N = 500 > L = 240) + Last-sample impulse test
    QVERIFY(mod->prepare(spec));
    std::vector<double> smp500(500U, 0.0);
    smp500[499] = 0.8; // Impulse on last sample before EOS
    auto in500 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, smp500);
    auto out500 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, smp500);
    QVERIFY(mod->process(in500.value()->view(), out500.value()->mutable_view(), DspProcessContext{frame_range(0, 500), true, false}));

    auto fin500 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 500, std::vector<double>(240U, 0.0));
    QVERIFY(mod->finalize(fin500.value()->mutable_view(), DspProcessContext{frame_range(500, 740), false, true}));

    const auto fin_plane = *fin500.value()->view().channel(0).value();
    // Frame 239 of finalize (the 240th frame after frame 499) outputs the impulse 0.8!
    QCOMPARE(fin_plane[239], 0.8);

    // 6. Requesting 241 frames (exceeding remaining drain of 240) rejected
    QVERIFY(mod->prepare(spec));
    QVERIFY(mod->process(in100.value()->view(), out100.value()->mutable_view(), DspProcessContext{frame_range(0, 100), true, false}));
    auto over_drain = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 100, std::vector<double>(241U, 0.0));
    auto over_res = mod->finalize(over_drain.value()->mutable_view(), DspProcessContext{frame_range(100, 341), false, true});
    QVERIFY(!over_res);
    QCOMPARE(over_res.error()->code(), rgsml::core::ErrorCode::InvalidArgument);

    // 7. Ragged/chunked finalize: 100 + 100 + 40 = 240
    auto f1 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 100, std::vector<double>(100U, 0.0));
    auto f2 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 200, std::vector<double>(100U, 0.0));
    auto f3 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 300, std::vector<double>(40U, 0.0));

    QVERIFY(mod->finalize(f1.value()->mutable_view(), DspProcessContext{frame_range(100, 200), false, false}));
    QVERIFY(mod->finalize(f2.value()->mutable_view(), DspProcessContext{frame_range(200, 300), false, false}));
    QVERIFY(mod->finalize(f3.value()->mutable_view(), DspProcessContext{frame_range(300, 340), false, true}));
}

void CompressorTest::sonicFingerprintAndCheckpointRejection()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    // 1. Two parameter sets differing only by a small double value (-24.0000001 vs -24.0000002)
    auto paramsA = *CompressorParameters::create(
        CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX,
        -24.0000001, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0).value();
    auto paramsB = *CompressorParameters::create(
        CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX,
        -24.0000002, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0).value();

    auto modA = std::move(*CompressorModule::create(desc, paramsA).value());
    auto modB = std::move(*CompressorModule::create(desc, paramsB).value());

    const DspProcessSpec spec_st{format(rgsml::audio::ChannelLayout::STEREO_LR, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(512)};
    QVERIFY(modA->prepare(spec_st));
    QVERIFY(modB->prepare(spec_st));

    auto cpA_res = modA->runtime_checkpoint();
    auto cpB_res = modB->runtime_checkpoint();
    QVERIFY(cpA_res);
    QVERIFY(cpB_res);

    // Fingerprints MUST be different!
    QVERIFY(cpA_res.value()->sonic_fingerprint != cpB_res.value()->sonic_fingerprint);

    // 2. Restoring checkpoint from modA into modB MUST be rejected!
    auto restore_res = modB->restore_runtime_checkpoint(*cpA_res.value());
    QVERIFY(!restore_res);
    QCOMPARE(restore_res.error()->code(), rgsml::core::ErrorCode::InvalidArgument);

    // 3. MONO layout: LINKED_MAX, LINKED_MEAN, DUAL_MONO produce IDENTICAL fingerprints
    const DspProcessSpec spec_mono{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(512)};

    auto mono_max = *CompressorParameters::create(
        CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX,
        -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0).value();
    auto mono_mean = *CompressorParameters::create(
        CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MEAN,
        -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0).value();
    auto mono_dual = *CompressorParameters::create(
        CompressorDetectorMode::RMS, CompressorChannelLink::DUAL_MONO,
        -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0).value();

    auto mod_mono_max = std::move(*CompressorModule::create(desc, mono_max).value());
    auto mod_mono_mean = std::move(*CompressorModule::create(desc, mono_mean).value());
    auto mod_mono_dual = std::move(*CompressorModule::create(desc, mono_dual).value());

    QVERIFY(mod_mono_max->prepare(spec_mono));
    QVERIFY(mod_mono_mean->prepare(spec_mono));
    QVERIFY(mod_mono_dual->prepare(spec_mono));

    auto cp_mono_max = mod_mono_max->runtime_checkpoint();
    auto cp_mono_mean = mod_mono_mean->runtime_checkpoint();
    auto cp_mono_dual = mod_mono_dual->runtime_checkpoint();
    QVERIFY(cp_mono_max); QVERIFY(cp_mono_mean); QVERIFY(cp_mono_dual);

    QCOMPARE(cp_mono_max.value()->sonic_fingerprint, cp_mono_mean.value()->sonic_fingerprint);
    QCOMPARE(cp_mono_max.value()->sonic_fingerprint, cp_mono_dual.value()->sonic_fingerprint);

    // 4. STEREO layout: LINKED_MAX vs LINKED_MEAN produce DIFFERENT fingerprints
    auto mod_st_max = std::move(*CompressorModule::create(desc, mono_max).value());
    auto mod_st_mean = std::move(*CompressorModule::create(desc, mono_mean).value());

    QVERIFY(mod_st_max->prepare(spec_st));
    QVERIFY(mod_st_mean->prepare(spec_st));

    auto cp_st_max = mod_st_max->runtime_checkpoint();
    auto cp_st_mean = mod_st_mean->runtime_checkpoint();
    QVERIFY(cp_st_max); QVERIFY(cp_st_mean);

    QVERIFY(cp_st_max.value()->sonic_fingerprint != cp_st_mean.value()->sonic_fingerprint);
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::CompressorTest)

#include "test_compressor.moc"
