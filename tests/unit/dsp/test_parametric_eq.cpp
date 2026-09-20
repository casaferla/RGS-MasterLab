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

[[nodiscard]] const ModuleDescriptor& eq_descriptor(const ModuleRegistry& registry)
{
    return registry.find_descriptor("rgsml.dsp.parametric-eq").value()->get();
}

class ParametricEqTest final : public QObject {
    Q_OBJECT

private slots:
    void parameterValidationAndRanges();
    void routingAndMidSideMath();
    void canonicalChunkSizesInvariance();
    void multibandCanonicalOrderAndDisabledBands();
    void settlingAndRuntimeRequirements();
    void rejectsInvalidAndNonFinite();
    void rejectionAtomicityAndPreparedState();
};

void ParametricEqTest::parameterValidationAndRanges()
{
    const auto valid_uuid = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000001").value();

    // Frequency ranges [20, 20000]
    QVERIFY(EqBandParameters::create(valid_uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{20.0, 0.0, 0.707}));
    QVERIFY(EqBandParameters::create(valid_uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{20000.0, 0.0, 0.707}));
    QVERIFY(!EqBandParameters::create(valid_uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{19.9, 0.0, 0.707}));
    QVERIFY(!EqBandParameters::create(valid_uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{20000.1, 0.0, 0.707}));

    // Gain ranges [-18, +18]
    QVERIFY(EqBandParameters::create(valid_uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, -18.0, 0.707}));
    QVERIFY(EqBandParameters::create(valid_uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, 18.0, 0.707}));
    QVERIFY(!EqBandParameters::create(valid_uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, -18.1, 0.707}));
    QVERIFY(!EqBandParameters::create(valid_uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, 18.1, 0.707}));

    // Q ranges [0.10, 12]
    QVERIFY(EqBandParameters::create(valid_uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, 0.0, 0.10}));
    QVERIFY(EqBandParameters::create(valid_uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, 0.0, 12.0}));
    QVERIFY(!EqBandParameters::create(valid_uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, 0.0, 0.099}));
    QVERIFY(!EqBandParameters::create(valid_uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, 0.0, 12.01}));

    // Band count [1, 6]
    std::vector<EqBandParameters> bands;
    QVERIFY(!ParametricEqParameters::create(bands)); // 0 bands
    for (int i = 0; i < 6; ++i) {
        std::string uuid_str = "10000000-0000-0000-0000-00000000000" + std::to_string(i + 1);
        auto uuid = *rgsml::core::Uuid::parse(uuid_str).value();
        bands.push_back(*EqBandParameters::create(uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, 0.0, 0.707}).value());
    }
    QVERIFY(ParametricEqParameters::create(bands)); // 6 bands
    auto extra_uuid = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000007").value();
    bands.push_back(*EqBandParameters::create(extra_uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, 0.0, 0.707}).value());
    QVERIFY(!ParametricEqParameters::create(bands)); // 7 bands
}

void ParametricEqTest::routingAndMidSideMath()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    const auto uuid = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000001").value();
    // MID routing +6dB Bell at 1kHz
    auto mid_band = *EqBandParameters::create(uuid, true, EqFilterType::BELL, EqRouting::MID, BellPayload{1000.0, 6.0, 0.707}).value();
    std::vector<EqBandParameters> bands;
    bands.push_back(mid_band);
    auto params = *ParametricEqParameters::create(bands).value();
    auto module = *ParametricEqModule::create(eq_descriptor(*registry.value()), params).value();

    const DspProcessSpec spec{
        format(rgsml::audio::ChannelLayout::STEREO_LR, 48000.0),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(100)};
    QVERIFY(module->prepare(spec));

    // Pure side signal: L = 1.0, R = -1.0 -> M = 0.0, S = sqrt(2).
    // Mid filter should leave M=0 unchanged, resulting in identical output L = 1.0, R = -1.0.
    std::vector<double> left(100, 1.0);
    std::vector<double> right(100, -1.0);
    auto input = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    auto output = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);

    QVERIFY(module->process(
        input.value()->view(), output.value()->mutable_view(),
        DspProcessContext{frame_range(0, 100), true, true}));

    const auto out_l = *output.value()->view().channel(0).value();
    const auto out_r = *output.value()->view().channel(1).value();
    for (std::size_t i = 0; i < 100; ++i) {
        QCOMPARE(out_l[i], 1.0);
        QCOMPARE(out_r[i], -1.0);
    }
}

void ParametricEqTest::canonicalChunkSizesInvariance()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto uuid = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000001").value();
    auto band = *EqBandParameters::create(uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, 6.0, 1.0}).value();
    std::vector<EqBandParameters> bands;
    bands.push_back(band);
    auto params = *ParametricEqParameters::create(bands).value();

    const std::size_t total_samples = 65536;
    std::vector<double> impulse(total_samples, 0.0);
    impulse[0] = 1.0;

    // Process all samples in one large block
    auto mod1 = *ParametricEqModule::create(eq_descriptor(*registry.value()), params).value();
    const DspProcessSpec spec1{
        format(rgsml::audio::ChannelLayout::MONO_C, 48000.0),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(static_cast<std::int64_t>(total_samples))};
    QVERIFY(mod1->prepare(spec1));

    auto in1 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, impulse);
    auto out1 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, impulse);
    QVERIFY(mod1->process(
        in1.value()->view(), out1.value()->mutable_view(),
        DspProcessContext{frame_range(0, static_cast<std::int64_t>(total_samples)), true, true}));

    // Process using required canonical chunk sizes: 1, 2, 3, 7, 31, 64, 127, 256, 511, 1024, 4096, 8191
    const std::array<std::size_t, 12> chunk_sizes{1, 2, 3, 7, 31, 64, 127, 256, 511, 1024, 4096, 8191};
    auto mod2 = *ParametricEqModule::create(eq_descriptor(*registry.value()), params).value();
    QVERIFY(mod2->prepare(spec1));

    std::vector<double> out2_samples(total_samples, 0.0);
    std::size_t offset = 0;
    std::size_t chunk_idx = 0;
    while (offset < total_samples) {
        const std::size_t chunk = chunk_sizes[chunk_idx % chunk_sizes.size()];
        const std::size_t count = std::min(chunk, total_samples - offset);
        std::vector<double> chunk_in(impulse.begin() + offset, impulse.begin() + offset + count);
        auto in_chunk = make_buffer(rgsml::audio::ChannelLayout::MONO_C, static_cast<std::int64_t>(offset), chunk_in);
        auto out_chunk = make_buffer(rgsml::audio::ChannelLayout::MONO_C, static_cast<std::int64_t>(offset), chunk_in);

        QVERIFY(mod2->process(
            in_chunk.value()->view(), out_chunk.value()->mutable_view(),
            DspProcessContext{frame_range(static_cast<std::int64_t>(offset), static_cast<std::int64_t>(offset + count)), true, true}));

        const auto chunk_res = *out_chunk.value()->view().channel(0).value();
        std::copy(chunk_res.begin(), chunk_res.end(), out2_samples.begin() + offset);
        offset += count;
        chunk_idx++;
    }

    const auto res1 = *out1.value()->view().channel(0).value();
    for (std::size_t i = 0; i < total_samples; ++i) {
        QCOMPARE(res1[i], out2_samples[i]); // Bit-identical state/chunk invariance
    }
}

void ParametricEqTest::multibandCanonicalOrderAndDisabledBands()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto uuid1 = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000001").value();
    const auto uuid2 = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000002").value();

    // Band 1: Enabled +6dB Bell at 1kHz
    auto band1 = *EqBandParameters::create(uuid1, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, 6.0, 0.707}).value();
    // Band 2: Disabled -6dB Bell at 280Hz
    auto band2 = *EqBandParameters::create(uuid2, false, EqFilterType::BELL, EqRouting::STEREO, BellPayload{280.0, -6.0, 0.707}).value();

    std::vector<EqBandParameters> bands{band1, band2};
    auto params_with_disabled = *ParametricEqParameters::create(bands).value();

    std::vector<EqBandParameters> single_band{band1};
    auto params_single = *ParametricEqParameters::create(single_band).value();

    auto mod_with_disabled = *ParametricEqModule::create(eq_descriptor(*registry.value()), params_with_disabled).value();
    auto mod_single = *ParametricEqModule::create(eq_descriptor(*registry.value()), params_single).value();

    const DspProcessSpec spec{
        format(rgsml::audio::ChannelLayout::MONO_C, 48000.0),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(100)};
    QVERIFY(mod_with_disabled->prepare(spec));
    QVERIFY(mod_single->prepare(spec));

    std::vector<double> impulse(100, 0.0);
    impulse[0] = 1.0;

    auto in1 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, impulse);
    auto out1 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, impulse);
    auto in2 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, impulse);
    auto out2 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, impulse);

    QVERIFY(mod_with_disabled->process(in1.value()->view(), out1.value()->mutable_view(), DspProcessContext{frame_range(0, 100), true, true}));
    QVERIFY(mod_single->process(in2.value()->view(), out2.value()->mutable_view(), DspProcessContext{frame_range(0, 100), true, true}));

    const auto res1 = *out1.value()->view().channel(0).value();
    const auto res2 = *out2.value()->view().channel(0).value();
    for (std::size_t i = 0; i < 100; ++i) {
        QCOMPARE(res1[i], res2[i]); // Disabled band must contribute zero processing
    }
}

void ParametricEqTest::settlingAndRuntimeRequirements()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto uuid = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000001").value();
    auto band = *EqBandParameters::create(uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{100.0, 12.0, 10.0}).value();
    std::vector<EqBandParameters> bands;
    bands.push_back(band);
    auto params = *ParametricEqParameters::create(bands).value();
    auto module = *ParametricEqModule::create(eq_descriptor(*registry.value()), params).value();

    const DspProcessSpec spec{
        format(rgsml::audio::ChannelLayout::STEREO_LR, 48000.0),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(256)};

    auto reqs = module->runtime_requirements(spec);
    QVERIFY(reqs);
    QCOMPARE(reqs.value()->execution_model, DspExecutionModel::STREAMING_CAUSAL);
    QVERIFY(reqs.value()->effective_tail_frames.value() > 0);
    QCOMPARE(reqs.value()->pre_context_frames.value(), reqs.value()->effective_tail_frames.value());
    QCOMPARE(reqs.value()->post_context_frames.value(), reqs.value()->effective_tail_frames.value());
}

void ParametricEqTest::rejectsInvalidAndNonFinite()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto uuid = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000001").value();

    // Frequency > 0.45 * Fs
    auto high_freq_band = *EqBandParameters::create(uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{20000.0, 0.0, 0.707}).value();
    std::vector<EqBandParameters> bands;
    bands.push_back(high_freq_band);
    auto params = *ParametricEqParameters::create(bands).value();
    auto module = *ParametricEqModule::create(eq_descriptor(*registry.value()), params).value();

    // Fs = 44100 -> 0.45 * Fs = 19845 Hz < 20000 Hz
    const DspProcessSpec spec{
        format(rgsml::audio::ChannelLayout::STEREO_LR, 44100.0),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(256)};
    auto prep_status = module->prepare(spec);
    QVERIFY(!prep_status);
    QCOMPARE(prep_status.error()->code(), rgsml::core::ErrorCode::OutOfRange);

    // Mono layout with non-STEREO routing
    auto side_band = *EqBandParameters::create(uuid, true, EqFilterType::BELL, EqRouting::SIDE, BellPayload{1000.0, 0.0, 0.707}).value();
    std::vector<EqBandParameters> mono_bands;
    mono_bands.push_back(side_band);
    auto mono_params = *ParametricEqParameters::create(mono_bands).value();
    auto mono_module = *ParametricEqModule::create(eq_descriptor(*registry.value()), mono_params).value();
    const DspProcessSpec mono_spec{
        format(rgsml::audio::ChannelLayout::MONO_C, 48000.0),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(256)};
    auto mono_prep = mono_module->prepare(mono_spec);
    QVERIFY(!mono_prep);
    QCOMPARE(mono_prep.error()->code(), rgsml::core::ErrorCode::InvalidArgument);
}

void ParametricEqTest::rejectionAtomicityAndPreparedState()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto uuid = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000001").value();
    auto band = *EqBandParameters::create(uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, 6.0, 0.707}).value();
    std::vector<EqBandParameters> bands{band};
    auto params = *ParametricEqParameters::create(bands).value();
    auto module = *ParametricEqModule::create(eq_descriptor(*registry.value()), params).value();

    std::vector<double> sample{1.0};
    auto in = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, sample);
    auto out = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, sample);

    // Process before prepare must fail
    auto unprep_status = module->process(in.value()->view(), out.value()->mutable_view(), DspProcessContext{frame_range(0, 1), true, true});
    QVERIFY(!unprep_status);
    QCOMPARE(unprep_status.error()->code(), rgsml::core::ErrorCode::InvalidState);

    // Prepare module
    const DspProcessSpec spec{
        format(rgsml::audio::ChannelLayout::MONO_C, 48000.0),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(1)};
    QVERIFY(module->prepare(spec));

    // Process with non-finite sample
    sample[0] = std::numeric_limits<double>::quiet_NaN();
    auto in_nan = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, sample);
    auto out_nan = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, sample);
    auto nan_status = module->process(in_nan.value()->view(), out_nan.value()->mutable_view(), DspProcessContext{frame_range(0, 1), true, true});
    QVERIFY(!nan_status);
    QCOMPARE(nan_status.error()->code(), rgsml::core::ErrorCode::InvalidAudioSample);
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::ParametricEqTest)

#include "test_parametric_eq.moc"
