#include "../render/render_test_support.hpp"
#include "../../oracles/parametric_eq/parametric_eq_oracle.hpp"
#include "../../../dsp/src/internal/parametric_eq_coefficients.hpp"

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

class ParametricEqTest final : public QObject {
    Q_OBJECT

private slots:
    void parameterValidationAndRanges();
    void routingAndMidSideMath();
    void canonicalChunkSizesInvariance();
    void deterministicRaggedPartitionInvariance();
    void multibandCanonicalOrderAndDisabledBands();
    void zeroGainFiltersStructuralSemantics();
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

    // Non-finite parameters
    QVERIFY(!EqBandParameters::create(valid_uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{std::numeric_limits<double>::quiet_NaN(), 0.0, 0.707}));
    QVERIFY(!EqBandParameters::create(valid_uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, std::numeric_limits<double>::quiet_NaN(), 0.707}));
    QVERIFY(!EqBandParameters::create(valid_uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, 0.0, std::numeric_limits<double>::quiet_NaN()}));
    QVERIFY(!EqBandParameters::create(valid_uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{std::numeric_limits<double>::infinity(), 0.0, 0.707}));

    // Mismatched payload
    QVERIFY(!EqBandParameters::create(valid_uuid, true, EqFilterType::BELL, EqRouting::STEREO, NotchPayload{1000.0, 1.0}));
    QVERIFY(!EqBandParameters::create(valid_uuid, true, EqFilterType::NOTCH, EqRouting::STEREO, BellPayload{1000.0, 0.0, 1.0}));
    QVERIFY(!EqBandParameters::create(valid_uuid, true, EqFilterType::LOW_SHELF, EqRouting::STEREO, PassPayload{1000.0, SlopeDbPerOctave::DB_12}));

    // Invalid casted enums
    const auto invalid_filter = static_cast<EqFilterType>(99);
    const auto invalid_routing = static_cast<EqRouting>(99);
    const auto invalid_slope = static_cast<SlopeDbPerOctave>(99);
    QVERIFY(!EqBandParameters::create(valid_uuid, true, invalid_filter, EqRouting::STEREO, BellPayload{1000.0, 0.0, 0.707}));
    QVERIFY(!EqBandParameters::create(valid_uuid, true, EqFilterType::BELL, invalid_routing, BellPayload{1000.0, 0.0, 0.707}));
    QVERIFY(!EqBandParameters::create(valid_uuid, true, EqFilterType::HIGH_PASS, EqRouting::STEREO, PassPayload{1000.0, invalid_slope}));

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

    // Duplicate band ID
    std::vector<EqBandParameters> dup_bands;
    dup_bands.push_back(*EqBandParameters::create(valid_uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, 0.0, 0.707}).value());
    dup_bands.push_back(*EqBandParameters::create(valid_uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{2000.0, 0.0, 0.707}).value());
    QVERIFY(!ParametricEqParameters::create(dup_bands));
}

void ParametricEqTest::routingAndMidSideMath()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto uuid = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000001").value();
    const double fs = 48000.0;
    const std::size_t n = 256;

    std::vector<double> in_l(n, 0.0);
    std::vector<double> in_r(n, 0.0);
    in_l[0] = 1.0;
    in_r[0] = 0.5;

    const auto sqrt2 = std::numbers::sqrt2;

    for (const auto routing : {EqRouting::STEREO, EqRouting::LEFT, EqRouting::RIGHT, EqRouting::MID, EqRouting::SIDE}) {
        auto band = *EqBandParameters::create(uuid, true, EqFilterType::BELL, routing, BellPayload{1000.0, 6.0, 0.707}).value();
        std::vector<EqBandParameters> bands{band};
        auto params = *ParametricEqParameters::create(bands).value();
        auto module = make_eq_module(*registry.value(), params);

        const DspProcessSpec spec{
            format(rgsml::audio::ChannelLayout::STEREO_LR, fs),
            rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            frame_count(static_cast<std::int64_t>(n))};
        QVERIFY(module->prepare(spec));

        auto input = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, in_l, in_r);
        auto output = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, in_l, in_r);

        QVERIFY(module->process(
            input.value()->view(), output.value()->mutable_view(),
            DspProcessContext{frame_range(0, static_cast<std::int64_t>(n)), true, true}));

        const auto actual_l = *output.value()->view().channel(0).value();
        const auto actual_r = *output.value()->view().channel(1).value();

        std::vector<double> expected_l(n, 0.0);
        std::vector<double> expected_r(n, 0.0);

        if (routing == EqRouting::STEREO) {
            IndependentCascadeTdf2State o2_l({BELL_1K_PLUS6_Q0707});
            IndependentCascadeTdf2State o2_r({BELL_1K_PLUS6_Q0707});
            o2_l.process_block(in_l, expected_l);
            o2_r.process_block(in_r, expected_r);
        } else if (routing == EqRouting::LEFT) {
            IndependentCascadeTdf2State o2_l({BELL_1K_PLUS6_Q0707});
            o2_l.process_block(in_l, expected_l);
            expected_r = in_r;
        } else if (routing == EqRouting::RIGHT) {
            expected_l = in_l;
            IndependentCascadeTdf2State o2_r({BELL_1K_PLUS6_Q0707});
            o2_r.process_block(in_r, expected_r);
        } else if (routing == EqRouting::MID) {
            std::vector<double> mid(n, 0.0);
            std::vector<double> side(n, 0.0);
            for (std::size_t i = 0; i < n; ++i) {
                mid[i] = (in_l[i] + in_r[i]) / sqrt2;
                side[i] = (in_l[i] - in_r[i]) / sqrt2;
            }
            std::vector<double> proc_mid(n, 0.0);
            IndependentCascadeTdf2State o2_mid({BELL_1K_PLUS6_Q0707});
            o2_mid.process_block(mid, proc_mid);
            for (std::size_t i = 0; i < n; ++i) {
                expected_l[i] = (proc_mid[i] + side[i]) / sqrt2;
                expected_r[i] = (proc_mid[i] - side[i]) / sqrt2;
            }
        } else if (routing == EqRouting::SIDE) {
            std::vector<double> mid(n, 0.0);
            std::vector<double> side(n, 0.0);
            for (std::size_t i = 0; i < n; ++i) {
                mid[i] = (in_l[i] + in_r[i]) / sqrt2;
                side[i] = (in_l[i] - in_r[i]) / sqrt2;
            }
            std::vector<double> proc_side(n, 0.0);
            IndependentCascadeTdf2State o2_side({BELL_1K_PLUS6_Q0707});
            o2_side.process_block(side, proc_side);
            for (std::size_t i = 0; i < n; ++i) {
                expected_l[i] = (mid[i] + proc_side[i]) / sqrt2;
                expected_r[i] = (mid[i] - proc_side[i]) / sqrt2;
            }
        }

        verify_rendered_against_o2(actual_l, expected_l);
        verify_rendered_against_o2(actual_r, expected_r);
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

    auto mod1 = make_eq_module(*registry.value(), params);
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

    const auto res1 = *out1.value()->view().channel(0).value();

    // Canonical chunk matrix test: same-build bit-identical output against one-block reference separately for EACH fixed chunk size
    const std::array<std::size_t, 13> canonical_chunks{1, 2, 3, 7, 31, 64, 127, 256, 511, 1024, 4096, 8191, 65536};
    for (const std::size_t chunk_size : canonical_chunks) {
        auto mod = make_eq_module(*registry.value(), params);
        QVERIFY(mod->prepare(spec1));

        std::vector<double> out_samples(total_samples, 0.0);
        std::size_t offset = 0;
        while (offset < total_samples) {
            const std::size_t count = std::min(chunk_size, total_samples - offset);
            std::vector<double> chunk_in(impulse.begin() + offset, impulse.begin() + offset + count);
            auto in_chunk = make_buffer(rgsml::audio::ChannelLayout::MONO_C, static_cast<std::int64_t>(offset), chunk_in);
            auto out_chunk = make_buffer(rgsml::audio::ChannelLayout::MONO_C, static_cast<std::int64_t>(offset), chunk_in);

            QVERIFY(mod->process(
                in_chunk.value()->view(), out_chunk.value()->mutable_view(),
                DspProcessContext{frame_range(static_cast<std::int64_t>(offset), static_cast<std::int64_t>(offset + count)), true, true}));

            const auto chunk_res = *out_chunk.value()->view().channel(0).value();
            std::copy(chunk_res.begin(), chunk_res.end(), out_samples.begin() + offset);
            offset += count;
        }

        for (std::size_t i = 0; i < total_samples; ++i) {
            QCOMPARE(res1[i], out_samples[i]);
        }
    }
}

void ParametricEqTest::deterministicRaggedPartitionInvariance()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto uuid = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000001").value();
    auto band = *EqBandParameters::create(uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, 6.0, 1.0}).value();
    std::vector<EqBandParameters> bands{band};
    auto params = *ParametricEqParameters::create(bands).value();

    const std::size_t total_samples = 65536;
    std::vector<double> impulse(total_samples, 0.0);
    impulse[0] = 1.0;

    auto mod1 = make_eq_module(*registry.value(), params);
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

    const auto res1 = *out1.value()->view().channel(0).value();

    // Deterministic ragged partition test using fixed nonrepeating sequence and final remainder
    const std::array<std::size_t, 11> ragged_chunks{1, 13, 2, 97, 3, 512, 19, 7, 2048, 11, 4000};
    auto mod2 = make_eq_module(*registry.value(), params);
    QVERIFY(mod2->prepare(spec1));

    std::vector<double> out2_samples(total_samples, 0.0);
    std::size_t offset = 0;
    std::size_t chunk_idx = 0;
    while (offset < total_samples) {
        const std::size_t chunk = ragged_chunks[chunk_idx % ragged_chunks.size()];
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

    for (std::size_t i = 0; i < total_samples; ++i) {
        QCOMPARE(res1[i], out2_samples[i]);
    }
}

void ParametricEqTest::multibandCanonicalOrderAndDisabledBands()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto uuid1 = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000001").value();
    const auto uuid2 = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000002").value();

    auto band1 = *EqBandParameters::create(uuid1, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, 6.0, 0.707}).value();
    auto band2 = *EqBandParameters::create(uuid2, false, EqFilterType::BELL, EqRouting::STEREO, BellPayload{280.0, -6.0, 0.707}).value();

    std::vector<EqBandParameters> bands{band1, band2};
    auto params_with_disabled = *ParametricEqParameters::create(bands).value();

    std::vector<EqBandParameters> single_band{band1};
    auto params_single = *ParametricEqParameters::create(single_band).value();

    auto mod_with_disabled = make_eq_module(*registry.value(), params_with_disabled);
    auto mod_single = make_eq_module(*registry.value(), params_single);

    const DspProcessSpec spec{
        format(rgsml::audio::ChannelLayout::MONO_C, 48000.0),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(100)};
    QVERIFY(mod_with_disabled->prepare(spec));
    QVERIFY(mod_single->prepare(spec));

    // Compare runtime requirements between enabled-only and enabled + disabled band
    auto reqs_disabled = mod_with_disabled->runtime_requirements(spec);
    auto reqs_single = mod_single->runtime_requirements(spec);
    QVERIFY(reqs_disabled);
    QVERIFY(reqs_single);
    QCOMPARE(reqs_disabled.value()->execution_model, reqs_single.value()->execution_model);
    QCOMPARE(reqs_disabled.value()->algorithmic_latency_frames.value(), reqs_single.value()->algorithmic_latency_frames.value());
    QCOMPARE(reqs_disabled.value()->look_ahead_frames.value(), reqs_single.value()->look_ahead_frames.value());
    QCOMPARE(reqs_disabled.value()->pre_context_frames.value(), reqs_single.value()->pre_context_frames.value());
    QCOMPARE(reqs_disabled.value()->post_context_frames.value(), reqs_single.value()->post_context_frames.value());
    QCOMPARE(reqs_disabled.value()->effective_tail_frames.value(), reqs_single.value()->effective_tail_frames.value());
    QCOMPARE(reqs_disabled.value()->requires_prepass, reqs_single.value()->requires_prepass);

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
        QCOMPARE(res1[i], res2[i]);
    }
}

void ParametricEqTest::zeroGainFiltersStructuralSemantics()
{
    const auto uuid = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000001").value();
    const double fs = 48000.0;

    struct ZeroCase {
        EqFilterType type;
        EqBandPayload payload;
    };

    const std::array<ZeroCase, 3> cases{{
        {EqFilterType::BELL, BellPayload{1000.0, 0.0, 0.707}},
        {EqFilterType::LOW_SHELF, ShelfPayload{100.0, 0.0, 0.5}},
        {EqFilterType::HIGH_SHELF, ShelfPayload{10000.0, 0.0, 0.5}},
    }};

    for (const auto& zc : cases) {
        auto band = *EqBandParameters::create(uuid, true, zc.type, EqRouting::STEREO, zc.payload).value();
        std::vector<EqBandParameters> bands{band};
        auto params = *ParametricEqParameters::create(bands).value();

        auto prod_coeffs = internal::compute_parametric_eq_coefficients(params, fs);
        QVERIFY(prod_coeffs);
        QCOMPARE(prod_coeffs.value()->bands.size(), std::size_t{1});
        QCOMPARE(prod_coeffs.value()->bands[0].sections.size(), std::size_t{1});

        const auto& section = prod_coeffs.value()->bands[0].sections[0];
        QVERIFY(section.settling_frames > 0);
        QVERIFY(section.rmax > 0.0);

        auto registry = ModuleRegistry::create_dsp_package_v1();
        auto module = make_eq_module(*registry.value(), params);

        const DspProcessSpec spec{
            format(rgsml::audio::ChannelLayout::MONO_C, fs),
            rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            frame_count(100)};
        QVERIFY(module->prepare(spec));

        std::vector<double> impulse(100, 0.0);
        impulse[0] = 1.0;

        auto in = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, impulse);
        auto out = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, impulse);

        QVERIFY(module->process(in.value()->view(), out.value()->mutable_view(), DspProcessContext{frame_range(0, 100), true, true}));

        const auto res = *out.value()->view().channel(0).value();
        for (std::size_t i = 0; i < 100; ++i) {
            QCOMPARE(res[i], impulse[i]);
        }
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
    auto module = make_eq_module(*registry.value(), params);

    const DspProcessSpec spec{
        format(rgsml::audio::ChannelLayout::STEREO_LR, 48000.0),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(256)};

    auto reqs = module->runtime_requirements(spec);
    QVERIFY(reqs);
    QCOMPARE(reqs.value()->execution_model, DspExecutionModel::STREAMING_CAUSAL);
    QCOMPARE(reqs.value()->algorithmic_latency_frames.value(), std::int64_t{0});
    QCOMPARE(reqs.value()->look_ahead_frames.value(), std::int64_t{0});
    QVERIFY(reqs.value()->effective_tail_frames.value() > 0);
    QCOMPARE(reqs.value()->pre_context_frames.value(), reqs.value()->effective_tail_frames.value());
    QCOMPARE(reqs.value()->post_context_frames.value(), reqs.value()->effective_tail_frames.value());
    QVERIFY(!reqs.value()->requires_prepass);
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
    auto module = make_eq_module(*registry.value(), params);

    // Fs = 44100 -> 0.45 * Fs = 19845 Hz < 20000 Hz
    const DspProcessSpec spec{
        format(rgsml::audio::ChannelLayout::STEREO_LR, 44100.0),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(256)};
    auto prep_status = module->prepare(spec);
    QVERIFY(!prep_status);
    QCOMPARE(prep_status.error()->code(), rgsml::core::ErrorCode::OutOfRange);

    // Mono layout with non-STEREO routing (MID, SIDE, LEFT, RIGHT)
    for (const auto non_stereo : {EqRouting::MID, EqRouting::SIDE, EqRouting::LEFT, EqRouting::RIGHT}) {
        auto non_stereo_band = *EqBandParameters::create(uuid, true, EqFilterType::BELL, non_stereo, BellPayload{1000.0, 0.0, 0.707}).value();
        std::vector<EqBandParameters> mono_bands{non_stereo_band};
        auto mono_params = *ParametricEqParameters::create(mono_bands).value();
        auto mono_module = make_eq_module(*registry.value(), mono_params);
        const DspProcessSpec mono_spec{
            format(rgsml::audio::ChannelLayout::MONO_C, 48000.0),
            rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            frame_count(256)};
        auto mono_prep = mono_module->prepare(mono_spec);
        QVERIFY(!mono_prep);
        QCOMPARE(mono_prep.error()->code(), rgsml::core::ErrorCode::InvalidArgument);
    }
}

void ParametricEqTest::rejectionAtomicityAndPreparedState()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto uuid = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000001").value();
    auto band = *EqBandParameters::create(uuid, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, 6.0, 0.707}).value();
    std::vector<EqBandParameters> bands{band};
    auto params = *ParametricEqParameters::create(bands).value();

    auto module_a = make_eq_module(*registry.value(), params);
    auto module_b = make_eq_module(*registry.value(), params);

    const DspProcessSpec spec{
        format(rgsml::audio::ChannelLayout::MONO_C, 48000.0),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(10)};

    QVERIFY(module_a->prepare(spec));
    QVERIFY(module_b->prepare(spec));

    // Advance both modules identically with valid audio so IIR states are non-zero and identical
    std::vector<double> valid_block(10, 0.5);
    auto in_a1 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, valid_block);
    auto out_a1 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, valid_block);
    auto in_b1 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, valid_block);
    auto out_b1 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, valid_block);

    QVERIFY(module_a->process(in_a1.value()->view(), out_a1.value()->mutable_view(), DspProcessContext{frame_range(0, 10), true, true}));
    QVERIFY(module_b->process(in_b1.value()->view(), out_b1.value()->mutable_view(), DspProcessContext{frame_range(0, 10), true, true}));

    const auto res_a1 = *out_a1.value()->view().channel(0).value();
    const auto res_b1 = *out_b1.value()->view().channel(0).value();
    for (std::size_t i = 0; i < 10; ++i) {
        QCOMPARE(res_a1[i], res_b1[i]);
    }

    // On module A only, submit a finite sample sequence that causes processing failure (e.g. std::numeric_limits<double>::max())
    std::vector<double> finite_invalid_block(10, std::numeric_limits<double>::max());
    auto in_fail = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 10, finite_invalid_block);
    auto out_fail = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 10, finite_invalid_block);

    auto status_a = module_a->process(in_fail.value()->view(), out_fail.value()->mutable_view(), DspProcessContext{frame_range(10, 20), true, true});
    QVERIFY(!status_a);
    QCOMPARE(status_a.error()->code(), rgsml::core::ErrorCode::InvalidAudioSample);

    // Feed the next identical valid block to both A and B
    std::vector<double> next_valid_block(10, 0.25);
    auto in_a2 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 10, next_valid_block);
    auto out_a2 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 10, next_valid_block);
    auto in_b2 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 10, next_valid_block);
    auto out_b2 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 10, next_valid_block);

    QVERIFY(module_a->process(in_a2.value()->view(), out_a2.value()->mutable_view(), DspProcessContext{frame_range(10, 20), true, true}));
    QVERIFY(module_b->process(in_b2.value()->view(), out_b2.value()->mutable_view(), DspProcessContext{frame_range(10, 20), true, true}));

    const auto res_a2 = *out_a2.value()->view().channel(0).value();
    const auto res_b2 = *out_b2.value()->view().channel(0).value();
    for (std::size_t i = 0; i < 10; ++i) {
        QCOMPARE(res_a2[i], res_b2[i]); // State atomicity proof
    }
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::ParametricEqTest)

#include "test_parametric_eq.moc"
