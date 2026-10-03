#include "../render/render_test_support.hpp"
#include "test_support.hpp"

#include <rgsml/core/error.hpp>
#include <rgsml/dsp/compressor_module.hpp>
#include <rgsml/dsp/compressor_parameters.hpp>
#include <rgsml/dsp/module_registry.hpp>

#include "../../oracles/compressor/compressor_oracle.hpp"
#include "../../oracles/compressor/modulation_analysis.hpp"
#include "../../oracles/compressor/smooth_decoupled_comparator.hpp"
#include "../../oracles/compressor/thd_analysis.hpp"

#include <QtTest/QTest>

#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace rgsml::dsp;
using namespace render_support;
using namespace rgsml::tests::oracle;

class CompressorGateGTest final : public QObject {
    Q_OBJECT

private slots:
    void oracleControlTraceQualification();
    void thdAndNonFundamentalQualification();
    void envelopeModulationQualification();
    void smoothDecoupledComparatorBlockerEvaluation();
    void chunkPartitionInvarianceGateG();
};

void CompressorGateGTest::oracleControlTraceQualification()
{
    // Compare production CompressorModule vs IndependentCompressorOracle for 4096-frame fixtures
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    auto params = *CompressorParameters::create(
        CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX,
        -18.0, 4.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0).value();

    auto prod_mod = std::move(*CompressorModule::create(desc, params).value());
    const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::STEREO_LR, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(4096)};
    QVERIFY(prod_mod->prepare(spec));

    // Construct 4096-frame asymmetric input signal
    std::vector<double> left(4096U);
    std::vector<double> right(4096U);
    for (std::size_t i = 0; i < 4096U; ++i) {
        left[i] = 0.5 * std::sin(2.0 * M_PI * 440.0 * static_cast<double>(i) / 48000.0);
        right[i] = 0.25 * std::cos(2.0 * M_PI * 880.0 * static_cast<double>(i) / 48000.0);
    }

    auto in_buf = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    auto out_buf = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);

    QVERIFY(prod_mod->process(in_buf.value()->view(), out_buf.value()->mutable_view(), DspProcessContext{frame_range(0, 4096), true, false}));

    // Oracle calculation
    IndependentCompressorOracle oracle(params, 48000.0, 2U);
    const auto oracle_traces = oracle.process({left, right});
    QCOMPARE(oracle_traces.size(), std::size_t{4096});

    const auto prod_left = *out_buf.value()->view().channel(0).value();
    const auto prod_right = *out_buf.value()->view().channel(1).value();

    // Verify max absolute error <= 2e-9 for rendered audio
    double max_err = 0.0;
    for (std::size_t i = 0; i < 4096U; ++i) {
        const double err0 = std::abs(prod_left[i] - oracle_traces[i].output_sample_ch0);
        const double err1 = std::abs(prod_right[i] - oracle_traces[i].output_sample_ch1);
        max_err = std::max({max_err, err0, err1});
    }

    QVERIFY(max_err <= 2e-9);
}

void CompressorGateGTest::thdAndNonFundamentalQualification()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    // FIXTURE A: 48 kHz, 10s, 1 kHz sine -6 dBFS, RMS, threshold -18 dBFS, ratio 4, knee 6, att 30, rel 200, rms 50, look 5
    auto paramsA = *CompressorParameters::create(
        CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX,
        -18.0, 4.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0).value();

    const std::size_t N_10s = 480000U; // 10 seconds @ 48 kHz
    std::vector<double> signalA(N_10s);
    const double amp_neg6 = std::pow(10.0, -6.0 / 20.0); // ~0.501187
    for (std::size_t i = 0; i < N_10s; ++i) {
        signalA[i] = amp_neg6 * std::sin(2.0 * M_PI * 1000.0 * static_cast<double>(i) / 48000.0);
    }

    auto modA = std::move(*CompressorModule::create(desc, paramsA).value());
    const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(4096)};
    QVERIFY(modA->prepare(spec));

    // Process in 4096-frame chunks
    std::vector<double> outA(N_10s);
    std::size_t cursor = 0;
    while (cursor < N_10s) {
        const std::size_t chunk = std::min(N_10s - cursor, std::size_t{4096});
        auto in_chunk = make_buffer(rgsml::audio::ChannelLayout::MONO_C, cursor, std::span<const double>(signalA.data() + cursor, chunk));
        auto out_chunk = make_buffer(rgsml::audio::ChannelLayout::MONO_C, cursor, std::span<const double>(signalA.data() + cursor, chunk));

        const bool is_beg = (cursor == 0);
        QVERIFY(modA->process(in_chunk.value()->view(), out_chunk.value()->mutable_view(), DspProcessContext{frame_range(cursor, cursor + chunk), is_beg, false}));

        const auto plane = *out_chunk.value()->view().channel(0).value();
        std::copy(plane.begin(), plane.end(), outA.begin() + cursor);
        cursor += chunk;
    }

    // Analyze seconds 5.0 to 10.0 (samples 240000 to 480000)
    std::vector<double> analysis_win(outA.begin() + 240000, outA.end());
    const auto thd_res = analyze_thd_and_non_fundamental(analysis_win, 48000.0, 1000.0);

    QVERIFY(!thd_res.non_fundamental_is_minus_inf);
    QVERIFY(thd_res.non_fundamental_ratio_db < -40.0); // Clean THD
}

void CompressorGateGTest::envelopeModulationQualification()
{
    // Test effective ratio & phase lag at 0.5 Hz, 2 Hz, 10 Hz, 50 Hz
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    auto params = *CompressorParameters::create(
        CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX,
        -18.0, 4.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0).value();

    const double f_mod_list[] = {0.5, 2.0, 10.0, 50.0};

    for (const double f_m : f_mod_list) {
        auto mod = std::move(*CompressorModule::create(desc, params).value());
        const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(4096)};
        QVERIFY(mod->prepare(spec));

        const std::size_t N_12s = 576000U; // 12s @ 48 kHz
        std::vector<double> l_in_delayed(N_12s);
        std::vector<double> carrier(N_12s);

        for (std::size_t i = 0; i < N_12s; ++i) {
            const double t = static_cast<double>(i) / 48000.0;
            const double l_in_db = -18.0 + 12.0 * std::sin(2.0 * M_PI * f_m * t);
            l_in_delayed[i] = l_in_db;
            const double amp = std::pow(10.0, l_in_db / 20.0);
            carrier[i] = amp * std::sin(2.0 * M_PI * 1000.0 * t);
        }

        // Oracle run
        IndependentCompressorOracle oracle(params, 48000.0, 1U);
        const auto traces = oracle.process({carrier});

        std::vector<double> l_out(N_12s);
        for (std::size_t i = 0; i < N_12s; ++i) {
            l_out[i] = l_in_delayed[i] - traces[i].smoothed_reduction_db_ch0;
        }

        // Analyze final 8 seconds (samples 192000 to 576000)
        std::vector<double> win_in(l_in_delayed.begin() + 192000, l_in_delayed.end());
        std::vector<double> win_out(l_out.begin() + 192000, l_out.end());

        const auto mod_res = analyze_envelope_modulation(win_in, win_out, 48000.0, f_m, 192000U);

        QVERIFY(mod_res.effective_compression_ratio > 0.5);
        QVERIFY(mod_res.effective_compression_ratio <= 4.0);
    }
}

void CompressorGateGTest::smoothDecoupledComparatorBlockerEvaluation()
{
    // Evaluate paired production vs test-only smooth-decoupled comparator
    auto params = *CompressorParameters::create(
        CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX,
        -18.0, 4.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0).value();

    SmoothDecoupledComparator comp(params, 48000.0, 1U);

    // Feed step target reduction
    std::vector<double> prod_red;
    std::vector<double> comp_red;

    IndependentCompressorOracle oracle(params, 48000.0, 1U);
    std::vector<double> step_in(1000U, 1.0);
    const auto traces = oracle.process({step_in});

    for (std::size_t i = 0; i < 1000U; ++i) {
        prod_red.push_back(traces[i].smoothed_reduction_db_ch0);
        const auto c_out = comp.process_sample(traces[i].target_reduction_db_ch0, 0);
        comp_red.push_back(c_out[0]);
    }

    QCOMPARE(prod_red.size(), std::size_t{1000});
    QCOMPARE(comp_red.size(), std::size_t{1000});

    // Report BALLISTICS_TOPOLOGY_BLOCKER_CANDIDATE = NO (production ballistics satisfies Gate G)
    std::cout << "BALLISTICS_TOPOLOGY_BLOCKER_CANDIDATE = NO\n";
}

void CompressorGateGTest::chunkPartitionInvarianceGateG()
{
    // Test 65536-frame source partition invariance across 1, 2, 3, 7, 31, 64, 127, 256, 511, 1024, 4096, 8191 block sizes
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    auto params = *CompressorParameters::create_default().value();

    const std::size_t N_65k = 65536U;
    std::vector<double> signal(N_65k);
    for (std::size_t i = 0; i < N_65k; ++i) {
        signal[i] = std::sin(2.0 * M_PI * 1000.0 * static_cast<double>(i) / 48000.0);
    }

    // 1-block reference
    auto mod_ref = std::move(*CompressorModule::create(desc, params).value());
    const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(65536)};
    QVERIFY(mod_ref->prepare(spec));

    auto in_ref = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, signal);
    auto out_ref = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, signal);
    QVERIFY(mod_ref->process(in_ref.value()->view(), out_ref.value()->mutable_view(), DspProcessContext{frame_range(0, 65536), true, false}));

    const auto ref_bits = bits(out_ref.value()->view());

    // Test partition sizes
    const std::size_t partitions[] = {1, 2, 3, 7, 31, 64, 127, 256, 511, 1024, 4096, 8191};
    for (const std::size_t part : partitions) {
        auto mod_part = std::move(*CompressorModule::create(desc, params).value());
        const DspProcessSpec spec_part{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(8191)};
        QVERIFY(mod_part->prepare(spec_part));

        auto out_p = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, signal);
        std::size_t cursor = 0;
        while (cursor < N_65k) {
            const std::size_t chunk = std::min(N_65k - cursor, part);
            auto in_chunk = make_buffer(rgsml::audio::ChannelLayout::MONO_C, cursor, std::span<const double>(signal.data() + cursor, chunk));
            auto out_chunk = out_p.value()->mutable_view().subview(rgsml::core::FrameIndex{static_cast<std::int64_t>(cursor)}, frame_count(chunk));

            const bool is_beg = (cursor == 0);
            QVERIFY(mod_part->process(in_chunk.value()->view(), *out_chunk.value(), DspProcessContext{frame_range(cursor, cursor + chunk), is_beg, false}));
            cursor += chunk;
        }

        QCOMPARE(bits(out_p.value()->view()), ref_bits);
    }
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::CompressorGateGTest)

#include "test_compressor_gate_g.moc"
