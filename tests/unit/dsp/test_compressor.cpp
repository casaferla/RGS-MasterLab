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

[[nodiscard]] std::int64_t round_ties_to_even(double x) noexcept
{
    const double floor_val = std::floor(x);
    const double diff = x - floor_val;
    if (diff < 0.5) {
        return static_cast<std::int64_t>(floor_val);
    }
    if (diff > 0.5) {
        return static_cast<std::int64_t>(floor_val + 1.0);
    }
    const std::int64_t int_floor = static_cast<std::int64_t>(floor_val);
    return (int_floor % 2 == 0) ? int_floor : (int_floor + 1);
}

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

    // Inclusive bounds pass
    QVERIFY(CompressorParameters::create(CompressorDetectorMode::PEAK, CompressorChannelLink::DUAL_MONO, -120.0, 1.0, 0.0, 0.1, 1.0, 1.0, 0.0, 0.0, -24.0));
    QVERIFY(CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MEAN, 0.0, 20.0, 24.0, 500.0, 5000.0, 500.0, 20.0, 100.0, 24.0));

    // Outside bounds reject
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -120.0001, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, 0.0001, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 0.999, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 20.001, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, -0.001, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 24.001, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 0.099, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 500.001, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 0.999, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, 2.0, 6.0, 30.0, 5000.001, 50.0, 5.0, 100.0, 0.0));

    // NaN / Inf reject
    const double nan_v = std::numeric_limits<double>::quiet_NaN();
    const double inf_v = std::numeric_limits<double>::infinity();
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, nan_v, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
    QVERIFY(!CompressorParameters::create(CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX, -24.0, inf_v, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0));
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

    // Peak detector: responds immediately
    auto peak_params = *CompressorParameters::create(
        CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX,
        -12.0, 4.0, 0.0, 0.1, 1000.0, 50.0, 0.0, 100.0, 0.0).value();
    auto peak_mod = std::move(*CompressorModule::create(desc, peak_params).value());

    const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(512)};
    QVERIFY(peak_mod->prepare(spec));

    // RMS detector
    auto rms_params = *CompressorParameters::create(
        CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX,
        -12.0, 4.0, 0.0, 0.1, 1000.0, 50.0, 0.0, 100.0, 0.0).value();
    auto rms_mod = std::move(*CompressorModule::create(desc, rms_params).value());
    QVERIFY(rms_mod->prepare(spec));
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

    // Hard knee W = 0
    auto hard_params = *CompressorParameters::create(
        CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX,
        -12.0, 4.0, 0.0, 0.1, 1000.0, 50.0, 0.0, 100.0, 0.0).value();
    auto hard_mod = std::move(*CompressorModule::create(desc, hard_params).value());

    // Soft knee W = 10
    auto soft_params = *CompressorParameters::create(
        CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX,
        -12.0, 4.0, 10.0, 0.1, 1000.0, 50.0, 0.0, 100.0, 0.0).value();
    auto soft_mod = std::move(*CompressorModule::create(desc, soft_params).value());

    const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(512)};
    QVERIFY(hard_mod->prepare(spec));
    QVERIFY(soft_mod->prepare(spec));
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

    auto params = *CompressorParameters::create(
        CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX,
        -20.0, 4.0, 0.0, 10.0, 100.0, 50.0, 0.0, 100.0, 0.0).value();
    auto mod = std::move(*CompressorModule::create(desc, params).value());

    const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(512)};
    QVERIFY(mod->prepare(spec));
}

void CompressorTest::channelLinkModesAndMonoInvariance()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    // Mono input: LINKED_MAX, LINKED_MEAN, DUAL_MONO produce bit-identical output
    const std::array mono_samples{0.5, 0.8, -0.9, 0.1, 0.3};
    auto in_buf = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, mono_samples);

    std::vector<std::uint64_t> ref_bits;

    for (const auto link_mode : {CompressorChannelLink::LINKED_MAX, CompressorChannelLink::LINKED_MEAN, CompressorChannelLink::DUAL_MONO}) {
        auto params = *CompressorParameters::create(
            CompressorDetectorMode::PEAK, link_mode,
            -12.0, 4.0, 0.0, 5.0, 50.0, 50.0, 0.0, 100.0, 0.0).value();
        auto mod = std::move(*CompressorModule::create(desc, params).value());

        const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(512)};
        QVERIFY(mod->prepare(spec));

        auto out_buf = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, mono_samples);
        QVERIFY(mod->process(in_buf.value()->view(), out_buf.value()->mutable_view(), DspProcessContext{frame_range(0, 5), true, true}));

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

    for (const auto sr : {44100, 48000, 96000, 192000}) {
        auto params = *CompressorParameters::create(
            CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX,
            -12.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0).value();
        auto mod = std::move(*CompressorModule::create(desc, params).value());

        const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::STEREO_LR, sr), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(512)};
        auto reqs = mod->runtime_requirements(spec);
        QVERIFY(reqs);

        const std::int64_t expected_look_frames = round_ties_to_even(5.0 * static_cast<double>(sr) / 1000.0);
        QCOMPARE(reqs.value()->algorithmic_latency_frames.value(), expected_look_frames);
        QCOMPARE(reqs.value()->look_ahead_frames.value(), expected_look_frames);
    }
}

void CompressorTest::mixAndMakeupGain()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    // mix = 0% is active delayed dry
    auto zero_mix = *CompressorParameters::create(
        CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX,
        -12.0, 4.0, 0.0, 5.0, 50.0, 50.0, 0.0, 0.0, 12.0).value();
    auto mod = std::move(*CompressorModule::create(desc, zero_mix).value());

    const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(512)};
    QVERIFY(mod->prepare(spec));

    const std::array samples{0.2, 0.5, -0.7};
    auto in_buf = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
    auto out_buf = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);

    QVERIFY(mod->process(in_buf.value()->view(), out_buf.value()->mutable_view(), DspProcessContext{frame_range(0, 3), true, true}));

    // Output is delayed dry (0.0 lookahead, so exact input samples)
    QCOMPARE(bits(out_buf.value()->view()), bits(in_buf.value()->view()));
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

    // Restore into mod2
    QVERIFY(mod2->restore_runtime_checkpoint(*cp_res.value()));

    // Process second 50 samples in both
    auto out1 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 50, std::span<const double>(samples.data() + 50, 50));
    auto out2 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 50, std::span<const double>(samples.data() + 50, 50));
    auto chunk2_in = in_buf.value()->view().subview(rgsml::core::FrameIndex{50}, frame_count(50));

    QVERIFY(mod1->process(*chunk2_in.value(), out1.value()->mutable_view(), DspProcessContext{frame_range(50, 100), false, true}));
    QVERIFY(mod2->process(*chunk2_in.value(), out2.value()->mutable_view(), DspProcessContext{frame_range(50, 100), false, true}));

    // Bit-identical continuation
    QCOMPARE(bits(out1.value()->view()), bits(out2.value()->view()));
}

void CompressorTest::finalizeEosShortSourceCases()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    auto params = *CompressorParameters::create_default().value();
    auto mod = std::move(*CompressorModule::create(desc, params).value());

    const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(512)};
    QVERIFY(mod->prepare(spec));

    // N = 0 empty stream finalize
    auto fin_out = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, std::vector<double>(240U, 0.0));
    QVERIFY(mod->finalize(fin_out.value()->mutable_view(), DspProcessContext{frame_range(0, 240), true, true}));

    // Checkpoint after finalize rejected
    auto cp_res = mod->runtime_checkpoint();
    QVERIFY(!cp_res);
    QCOMPARE(cp_res.error()->code(), rgsml::core::ErrorCode::UnsupportedOperation);
    QCOMPARE(dsp_support::error_category(*cp_res.error()), std::string_view{"RUNTIME_CHECKPOINT_DURING_FINALIZE_UNSUPPORTED"});
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::CompressorTest)

#include "test_compressor.moc"
