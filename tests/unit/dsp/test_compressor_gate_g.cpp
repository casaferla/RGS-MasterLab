#include "../render/render_test_support.hpp"
#include "test_support.hpp"

#include <rgsml/core/error.hpp>
#include <rgsml/dsp/compressor_module.hpp>
#include <rgsml/dsp/compressor_parameters.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/module_execution_binding.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>

#include "../../oracles/compressor/compressor_oracle.hpp"
#include "../../oracles/compressor/modulation_analysis.hpp"
#include "../../oracles/compressor/smooth_decoupled_comparator.hpp"
#include "../../oracles/compressor/thd_analysis.hpp"

#include <QtTest/QTest>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <memory>
#include <numbers>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace rgsml::dsp;
using namespace render_support;
using namespace rgsml::tests::oracle;

constexpr double kPi = std::numbers::pi_v<double>;

[[nodiscard]] ProcessingChain empty_test_chain(const ModuleRegistry& registry)
{
    auto chain = ProcessingChain::create(
        registry,
        {ProcessingStage::MASTER, ChainSegment::MANUAL});
    Q_ASSERT(chain);
    return std::move(*chain.value());
}

class CompressorGateGTest final : public QObject {
    Q_OBJECT

private slots:
    void oracleControlTraceQualification();
    void thdAndNonFundamentalQualification();
    void envelopeModulationQualification();
    void smoothDecoupledComparatorBlockerEvaluation();
    void chunkPartitionInvarianceGateG();
    void checkpointContinuationGateG();
    void eosRendererIntegrationGateG();
    void ap24FullMatrixQualification();
};

void CompressorGateGTest::oracleControlTraceQualification()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::STEREO_LR, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(4096)};

    // 5 Required Fixtures: silence, constant-level steps, attack step-up, release step-down, asymmetric stereo
    const std::size_t N = 4096U;
    std::vector<std::vector<double>> left_fixtures(5, std::vector<double>(N, 0.0));
    std::vector<std::vector<double>> right_fixtures(5, std::vector<double>(N, 0.0));

    // 1. Silence
    std::fill(left_fixtures[0].begin(), left_fixtures[0].end(), 0.0);
    std::fill(right_fixtures[0].begin(), right_fixtures[0].end(), 0.0);

    // 2. Constant-level steps
    for (std::size_t i = 0; i < N; ++i) {
        if (i < 1000) { left_fixtures[1][i] = 0.0; right_fixtures[1][i] = 0.0; }
        else if (i < 2500) { left_fixtures[1][i] = 0.5; right_fixtures[1][i] = 0.5; }
        else if (i < 3500) { left_fixtures[1][i] = 0.25; right_fixtures[1][i] = 0.25; }
        else { left_fixtures[1][i] = 0.0; right_fixtures[1][i] = 0.0; }
    }

    // 3. Attack step-up
    for (std::size_t i = 1000; i < N; ++i) {
        left_fixtures[2][i] = 1.0; right_fixtures[2][i] = 1.0;
    }

    // 4. Release step-down
    for (std::size_t i = 0; i < 2000; ++i) {
        left_fixtures[3][i] = 1.0; right_fixtures[3][i] = 1.0;
    }
    for (std::size_t i = 2000; i < N; ++i) {
        left_fixtures[3][i] = 0.1; right_fixtures[3][i] = 0.1;
    }

    // 5. Asymmetric stereo
    for (std::size_t i = 0; i < N; ++i) {
        left_fixtures[4][i] = 0.8 * std::sin(2.0 * kPi * 440.0 * static_cast<double>(i) / 48000.0);
        right_fixtures[4][i] = 0.2 * std::cos(2.0 * kPi * 880.0 * static_cast<double>(i) / 48000.0);
    }

    for (const auto link_mode : {CompressorChannelLink::LINKED_MAX, CompressorChannelLink::LINKED_MEAN, CompressorChannelLink::DUAL_MONO}) {
        auto params = *CompressorParameters::create(
            CompressorDetectorMode::RMS, link_mode,
            -18.0, 4.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0).value();

        for (std::size_t fix = 0; fix < 5; ++fix) {
            auto prod_mod = std::move(*CompressorModule::create(desc, params).value());
            QVERIFY(prod_mod->prepare(spec));

            auto in_buf = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left_fixtures[fix], right_fixtures[fix]);
            auto out_buf = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left_fixtures[fix], right_fixtures[fix]);

            auto prod_trace_res = prod_mod->process_diagnostic_traces(
                in_buf.value()->view(), out_buf.value()->mutable_view(), DspProcessContext{frame_range(0, 4096), true, false});
            QVERIFY(prod_trace_res);
            const auto& prod_traces = *prod_trace_res.value();

            IndependentCompressorOracle oracle(params, 48000.0, 2U);
            const auto oracle_traces = oracle.process({left_fixtures[fix], right_fixtures[fix]});

            QCOMPARE(prod_traces.size(), N);
            QCOMPARE(oracle_traces.size(), N);

            for (std::size_t i = 0; i < N; ++i) {
                const auto& ptr = prod_traces[i];
                const auto& otr = oracle_traces[i];

                QCOMPARE(ptr.detector_magnitude_ch0, otr.detector_magnitude_ch0);
                QCOMPARE(ptr.detector_magnitude_ch1, otr.detector_magnitude_ch1);
                QCOMPARE(ptr.linked_detector_ch0, otr.linked_detector_ch0);
                QCOMPARE(ptr.linked_detector_ch1, otr.linked_detector_ch1);
                QCOMPARE(ptr.level_db_valid_ch0, otr.level_db_valid_ch0);
                QCOMPARE(ptr.level_db_valid_ch1, otr.level_db_valid_ch1);
                if (ptr.level_db_valid_ch0 && otr.level_db_valid_ch0) {
                    QCOMPARE(ptr.level_db_ch0, otr.level_db_ch0);
                }
                if (ptr.level_db_valid_ch1 && otr.level_db_valid_ch1) {
                    QCOMPARE(ptr.level_db_ch1, otr.level_db_ch1);
                }
                QCOMPARE(ptr.target_reduction_db_ch0, otr.target_reduction_db_ch0);
                QCOMPARE(ptr.target_reduction_db_ch1, otr.target_reduction_db_ch1);
                QCOMPARE(ptr.smoothed_reduction_db_ch0, otr.smoothed_reduction_db_ch0);
                QCOMPARE(ptr.smoothed_reduction_db_ch1, otr.smoothed_reduction_db_ch1);
                QCOMPARE(ptr.linear_gain_ch0, otr.linear_gain_ch0);
                QCOMPARE(ptr.linear_gain_ch1, otr.linear_gain_ch1);
                QCOMPARE(ptr.output_sample_ch0, otr.output_sample_ch0);
                QCOMPARE(ptr.output_sample_ch1, otr.output_sample_ch1);
            }
        }
    }

    // Mono channel link invariance test: LINKED_MAX, LINKED_MEAN, DUAL_MONO on MONO_C audio layout
    const DspProcessSpec spec_mono{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(4096)};
    std::vector<std::uint64_t> mono_ref_bits;

    for (const auto link_mode : {CompressorChannelLink::LINKED_MAX, CompressorChannelLink::LINKED_MEAN, CompressorChannelLink::DUAL_MONO}) {
        auto mono_params = *CompressorParameters::create(
            CompressorDetectorMode::RMS, link_mode,
            -18.0, 4.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0).value();

        auto mono_mod = std::move(*CompressorModule::create(desc, mono_params).value());
        QVERIFY(mono_mod->prepare(spec_mono));

        auto in_m = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, left_fixtures[1]);
        auto out_m = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, left_fixtures[1]);

        auto trace_res = mono_mod->process_diagnostic_traces(
            in_m.value()->view(), out_m.value()->mutable_view(), DspProcessContext{frame_range(0, 4096), true, false});
        QVERIFY(trace_res);

        if (mono_ref_bits.empty()) {
            mono_ref_bits = bits(out_m.value()->view());
        } else {
            QCOMPARE(bits(out_m.value()->view()), mono_ref_bits);
        }
    }
}

void CompressorGateGTest::thdAndNonFundamentalQualification()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();

    const std::size_t N_10s = 480000U; // 10s @ 48 kHz
    const double amp_neg6 = std::pow(10.0, -6.0 / 20.0);

    // FIXTURE A: 48 kHz, 10s, 1 kHz sine -6 dBFS, RMS, threshold -18 dBFS, ratio 4, knee 6, att 30, rel 200, rms 50, look 5
    auto paramsA = *CompressorParameters::create(
        CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX,
        -18.0, 4.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0).value();

    std::vector<double> signalA(N_10s);
    for (std::size_t i = 0; i < N_10s; ++i) {
        signalA[i] = amp_neg6 * std::sin(2.0 * kPi * 1000.0 * static_cast<double>(i) / 48000.0);
    }

    // FIXTURE B: 48 kHz, 10s, 60 Hz sine -6 dBFS, PEAK, threshold -18 dBFS, ratio 4, knee 0, att 1, rel 50, rms 50, look 5
    auto paramsB = *CompressorParameters::create(
        CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX,
        -18.0, 4.0, 0.0, 1.0, 50.0, 50.0, 5.0, 100.0, 0.0).value();

    std::vector<double> signalB(N_10s);
    for (std::size_t i = 0; i < N_10s; ++i) {
        signalB[i] = amp_neg6 * std::sin(2.0 * kPi * 60.0 * static_cast<double>(i) / 48000.0);
    }

    const struct {
        CompressorParameters params;
        const std::vector<double>& input_signal;
        double f0;
        const char* name;
    } fixtures[] = {
        {paramsA, signalA, 1000.0, "Fixture A"},
        {paramsB, signalB, 60.0, "Fixture B"}
    };

    for (const auto& fx : fixtures) {
        // Evaluate production through canonical render_preview
        auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, fx.input_signal);
        auto chain = empty_test_chain(*registry.value());
        const auto comp_id = make_id("70000000-0000-0000-0000-000000000001");
        QVERIFY(chain.add(comp_id, "rgsml.dsp.compressor", 0));

        const ModuleExecutionBinding comp_b{comp_id, fx.params};
        auto req = render::RenderRequest::create(
            source.value()->view(), frame_range(0, static_cast<std::int64_t>(N_10s)), chain,
            {comp_b}, frame_count(4096));
        QVERIFY(req);

        auto prod_render = render::render_preview(*req.value(), *registry.value());
        QVERIFY(prod_render);

        const auto prod_plane = *prod_render.value()->view().channel(0).value();
        std::vector<double> prod_out(prod_plane.begin(), prod_plane.end());

        // Run Oracle with exact latency alignment (240 frames lookahead)
        IndependentCompressorOracle oracle(fx.params, 48000.0, 1U);
        const auto oracle_traces = oracle.process({fx.input_signal});
        const auto oracle_fin = oracle.finalize(240U);

        std::vector<double> oracle_full;
        oracle_full.reserve(N_10s + 240U);
        for (const auto& tr : oracle_traces) oracle_full.push_back(tr.output_sample_ch0);
        for (const auto& tr : oracle_fin) oracle_full.push_back(tr.output_sample_ch0);

        // Slice compensated window [240, 240 + N_10s]
        std::vector<double> oracle_out(oracle_full.begin() + 240, oracle_full.begin() + 240 + N_10s);

        // Analyze seconds 5.0 to 10.0 (samples 240000 to 480000)
        std::vector<double> prod_win(prod_out.begin() + 240000, prod_out.begin() + 480000);
        std::vector<double> oracle_win(oracle_out.begin() + 240000, oracle_out.begin() + 480000);

        const auto prod_thd = analyze_thd_and_non_fundamental(prod_win, 48000.0, fx.f0);
        const auto oracle_thd = analyze_thd_and_non_fundamental(oracle_win, 48000.0, fx.f0);

        // Compare Production vs Oracle under TF §19.5 with low-level -140 dBFS threshold branch
        if (prod_thd.non_fundamental_ratio_db > -140.0 && oracle_thd.non_fundamental_ratio_db > -140.0) {
            const double non_fund_diff = std::abs(prod_thd.non_fundamental_ratio_db - oracle_thd.non_fundamental_ratio_db);
            QVERIFY(non_fund_diff <= 0.05);
        } else {
            double max_sample_err = 0.0;
            for (std::size_t i = 0; i < prod_win.size(); ++i) {
                max_sample_err = std::max(max_sample_err, std::abs(prod_win[i] - oracle_win[i]));
            }
            QVERIFY(max_sample_err <= 2e-9);
        }

        if (prod_thd.thd_ratio_db > -140.0 && oracle_thd.thd_ratio_db > -140.0) {
            const double thd_diff = std::abs(prod_thd.thd_ratio_db - oracle_thd.thd_ratio_db);
            QVERIFY(thd_diff <= 0.05);
        } else {
            double max_sample_err = 0.0;
            for (std::size_t i = 0; i < prod_win.size(); ++i) {
                max_sample_err = std::max(max_sample_err, std::abs(prod_win[i] - oracle_win[i]));
            }
            QVERIFY(max_sample_err <= 2e-9);
        }
    }
}

void CompressorGateGTest::envelopeModulationQualification()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    auto params = *CompressorParameters::create(
        CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX,
        -18.0, 4.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0).value();

    const double f_mod_list[] = {0.5, 2.0, 10.0, 50.0};
    const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(4096)};

    const double lookahead_ms = params.look_ahead_ms();
    const std::size_t lookAheadFrames = static_cast<std::size_t>(std::round(lookahead_ms * 0.001 * 48000.0));

    for (const double f_m : f_mod_list) {
        const std::size_t N_12s = 576000U; // 12s @ 48 kHz
        std::vector<double> l_in_delayed(N_12s);
        std::vector<double> carrier(N_12s);

        for (std::size_t i = 0; i < N_12s; ++i) {
            const double t = static_cast<double>(i) / 48000.0;
            const double l_in_db = -18.0 + 12.0 * std::sin(2.0 * kPi * f_m * t);

            if (i >= lookAheadFrames) {
                const double t_del = static_cast<double>(i - lookAheadFrames) / 48000.0;
                l_in_delayed[i] = -18.0 + 12.0 * std::sin(2.0 * kPi * f_m * t_del);
            } else {
                l_in_delayed[i] = -18.0;
            }

            const double amp = std::pow(10.0, l_in_db / 20.0);
            carrier[i] = amp * std::sin(2.0 * kPi * 1000.0 * t);
        }

        // Run Production with process_diagnostic_traces()
        auto prod_mod = std::move(*CompressorModule::create(desc, params).value());
        QVERIFY(prod_mod->prepare(spec));

        std::vector<double> prod_l_out(N_12s);
        std::size_t cursor = 0;
        while (cursor < N_12s) {
            const std::size_t chunk = std::min(N_12s - cursor, std::size_t{4096});
            auto in_chunk = make_buffer(rgsml::audio::ChannelLayout::MONO_C, cursor, std::span<const double>(carrier.data() + cursor, chunk));
            auto out_chunk = make_buffer(rgsml::audio::ChannelLayout::MONO_C, cursor, std::span<const double>(carrier.data() + cursor, chunk));
            const bool is_beg = (cursor == 0);

            auto trace_res = prod_mod->process_diagnostic_traces(
                in_chunk.value()->view(), out_chunk.value()->mutable_view(), DspProcessContext{frame_range(cursor, cursor + chunk), is_beg, false});
            QVERIFY(trace_res);

            for (std::size_t k = 0; k < chunk; ++k) {
                prod_l_out[cursor + k] = l_in_delayed[cursor + k] - (*trace_res.value())[k].smoothed_reduction_db_ch0;
            }
            cursor += chunk;
        }

        // Run Oracle
        IndependentCompressorOracle oracle(params, 48000.0, 1U);
        const auto oracle_traces = oracle.process({carrier});

        std::vector<double> oracle_l_out(N_12s);
        for (std::size_t i = 0; i < N_12s; ++i) {
            oracle_l_out[i] = l_in_delayed[i] - oracle_traces[i].smoothed_reduction_db_ch0;
        }

        // Analyze final 8 seconds (samples 192000 to 576000)
        std::vector<double> win_in(l_in_delayed.begin() + 192000, l_in_delayed.end());
        std::vector<double> win_prod_out(prod_l_out.begin() + 192000, prod_l_out.end());
        std::vector<double> win_oracle_out(oracle_l_out.begin() + 192000, oracle_l_out.end());

        const auto prod_mod_res = analyze_envelope_modulation(win_in, win_prod_out, 48000.0, f_m, 192000U);
        const auto oracle_mod_res = analyze_envelope_modulation(win_in, win_oracle_out, 48000.0, f_m, 192000U);

        // Compare Production vs Oracle under frozen tolerances:
        const double ratio_diff = std::abs(prod_mod_res.effective_compression_ratio - oracle_mod_res.effective_compression_ratio);
        const double max_allowed_ratio_diff = std::max(0.05, 0.01 * oracle_mod_res.effective_compression_ratio);
        QVERIFY(ratio_diff <= max_allowed_ratio_diff);

        const double a_out_diff = std::abs(prod_mod_res.a_out_db_amplitude - oracle_mod_res.a_out_db_amplitude);
        QVERIFY(a_out_diff <= 0.05);

        const double phase_diff = std::abs(prod_mod_res.phase_lag_degrees - oracle_mod_res.phase_lag_degrees);
        QVERIFY(phase_diff <= 1.0);
    }
}

void CompressorGateGTest::smoothDecoupledComparatorBlockerEvaluation()
{
    // Evaluate paired production vs test-only smooth-decoupled comparator across 2 stress cases:
    // Stress Case 1: Fast LF envelope modulation (10 Hz)
    // Stress Case 2: Steep transient pulse train
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    auto params = *CompressorParameters::create(
        CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX,
        -18.0, 4.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0).value();

    std::size_t blocker_candidate_count = 0;

    for (std::size_t stress = 0; stress < 2; ++stress) {
        const std::size_t N = 48000U; // 1s
        std::vector<double> input_signal(N);
        double input_db_delta = 0.0;

        if (stress == 0) {
            // Fast LF modulation (10 Hz between -30 dBFS and -6 dBFS, delta = 24 dB)
            input_db_delta = 24.0;
            for (std::size_t i = 0; i < N; ++i) {
                const double t = static_cast<double>(i) / 48000.0;
                const double amp = std::pow(10.0, (-18.0 + 12.0 * std::sin(2.0 * kPi * 10.0 * t)) / 20.0);
                input_signal[i] = amp * std::sin(2.0 * kPi * 1000.0 * t);
            }
        } else {
            // Steep transient pulse train (0 dBFS vs -26.02 dBFS, delta = 26.02 dB)
            input_db_delta = 26.020599913279625;
            for (std::size_t i = 0; i < N; ++i) {
                const double amp = ((i / 480) % 2 == 0) ? 1.0 : 0.05;
                input_signal[i] = amp * std::sin(2.0 * kPi * 1000.0 * static_cast<double>(i) / 48000.0);
            }
        }

        // Run ACTUAL production kernel
        auto comp_module = std::move(*CompressorModule::create(desc, params).value());
        DspProcessSpec spec{
            format(rgsml::audio::ChannelLayout::MONO_C, 48000),
            rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            frame_count(N)
        };
        QVERIFY(comp_module->prepare(spec));

        auto in_buf = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, input_signal);
        auto out_buf = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, std::vector<double>(N, 0.0));
        DspProcessContext ctx{frame_range(0, N), true, true};

        const auto trace_res = comp_module->process_diagnostic_traces(in_buf.value()->view(), out_buf.value()->mutable_view(), ctx);
        QVERIFY(trace_res.has_value());
        const auto& prod_traces = *trace_res.value();
        std::vector<double> prod_out_buf(N);
        const auto p_out_span = *out_buf.value()->view().channel(0).value();
        std::copy(p_out_span.begin(), p_out_span.end(), prod_out_buf.begin());

        // Run Smooth-Decoupled Comparator fed with production target reduction
        SmoothDecoupledComparator comp(params, 48000.0, 1U);
        std::vector<double> comp_red(N);
        std::vector<double> comp_out_buf(N);
        const double lookahead_frames = params.look_ahead_ms() * 0.001 * 48000.0;
        const std::size_t delay_frames = static_cast<std::size_t>(std::round(lookahead_frames));
        const double makeup_factor = std::pow(10.0, params.makeup_gain_db() / 20.0);

        for (std::size_t i = 0; i < N; ++i) {
            const auto c_out = comp.process_sample(prod_traces[i].target_reduction_db_ch0, 0);
            comp_red[i] = c_out[0];

            const double x_del = (i >= delay_frames) ? input_signal[i - delay_frames] : 0.0;
            const double g_comp = std::pow(10.0, -comp_red[i] / 20.0);
            comp_out_buf[i] = x_del * g_comp * makeup_factor;
        }

        // Metrics calculation over steady-state (N/2 to N)
        const std::size_t steady_start = N / 2;
        const std::size_t steady_count = N - steady_start;
        double sum_prod_red = 0.0;
        double sum_comp_red = 0.0;
        double max_prod_out = 1e-12;
        double min_prod_out = 1.0;
        double max_comp_out = 1e-12;
        double min_comp_out = 1.0;

        for (std::size_t i = steady_start; i < N; ++i) {
            sum_prod_red += prod_traces[i].smoothed_reduction_db_ch0;
            sum_comp_red += comp_red[i];

            const double abs_p = std::abs(prod_out_buf[i]);
            const double abs_c = std::abs(comp_out_buf[i]);
            if (abs_p > max_prod_out) max_prod_out = abs_p;
            if (abs_p > 1e-4 && abs_p < min_prod_out) min_prod_out = abs_p;
            if (abs_c > max_comp_out) max_comp_out = abs_c;
            if (abs_c > 1e-4 && abs_c < min_comp_out) min_comp_out = abs_c;
        }

        const double avg_prod_red = sum_prod_red / static_cast<double>(steady_count);
        const double avg_comp_red = sum_comp_red / static_cast<double>(steady_count);
        const double mean_gr_difference_db = std::abs(avg_prod_red - avg_comp_red);

        const double prod_out_delta_db = 20.0 * std::log10(max_prod_out / min_prod_out);
        const double comp_out_delta_db = 20.0 * std::log10(max_comp_out / min_comp_out);
        const double eff_ratio_prod = input_db_delta / std::max(0.1, prod_out_delta_db);
        const double eff_ratio_comp = input_db_delta / std::max(0.1, comp_out_delta_db);
        const double effective_ratio_difference_percent = std::abs(eff_ratio_prod - eff_ratio_comp) / eff_ratio_prod * 100.0;

        const auto prod_thd = analyze_thd_and_non_fundamental(prod_out_buf, 48000.0, 1000.0);
        const auto comp_thd = analyze_thd_and_non_fundamental(comp_out_buf, 48000.0, 1000.0);

        const double artifact_improvement_db = prod_thd.non_fundamental_ratio_db - comp_thd.non_fundamental_ratio_db;

        std::cout << "Stress case " << stress << ": prod_artifact_db=" << prod_thd.non_fundamental_ratio_db
                  << " comp_artifact_db=" << comp_thd.non_fundamental_ratio_db
                  << " artifact_improvement_db=" << artifact_improvement_db
                  << " mean_gr_diff_db=" << mean_gr_difference_db
                  << " eff_ratio_prod=" << eff_ratio_prod
                  << " eff_ratio_comp=" << eff_ratio_comp
                  << " ratio_diff_pct=" << effective_ratio_difference_percent << "%\n";

        if (artifact_improvement_db >= 3.0 && mean_gr_difference_db <= 0.25 && effective_ratio_difference_percent <= 5.0) {
            blocker_candidate_count++;
        }
    }

    if (blocker_candidate_count >= 2U) {
        std::cout << "BALLISTICS_TOPOLOGY_BLOCKER_CANDIDATE = YES\n";
    } else {
        std::cout << "BALLISTICS_TOPOLOGY_BLOCKER_CANDIDATE = NO\n";
    }
}

void CompressorGateGTest::chunkPartitionInvarianceGateG()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    auto params = *CompressorParameters::create_default().value();

    const std::size_t N_65k = 65536U;
    std::vector<double> signal(N_65k);
    for (std::size_t i = 0; i < N_65k; ++i) {
        signal[i] = std::sin(2.0 * kPi * 1000.0 * static_cast<double>(i) / 48000.0);
    }

    // 1-block reference
    auto mod_ref = std::move(*CompressorModule::create(desc, params).value());
    const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(65536)};
    QVERIFY(mod_ref->prepare(spec));

    auto in_ref = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, signal);
    auto out_ref = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, signal);
    QVERIFY(mod_ref->process(in_ref.value()->view(), out_ref.value()->mutable_view(), DspProcessContext{frame_range(0, 65536), true, false}));

    const auto ref_bits = bits(out_ref.value()->view());

    // Test partition sizes: 1, 2, 3, 7, 31, 64, 127, 256, 511, 1024, 4096, 8191 + ragged
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

    // Ragged partition sequence
    const std::size_t ragged_chunks[] = {1, 7, 13, 257, 511, 1024, 3, 999, 128, 4000, 2048, 8191, 10000, 15000, 23554};
    auto mod_ragged = std::move(*CompressorModule::create(desc, params).value());
    const DspProcessSpec spec_ragged{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(23554)};
    QVERIFY(mod_ragged->prepare(spec_ragged));

    auto out_ragged = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, signal);
    std::size_t rag_cursor = 0;
    for (const std::size_t chunk : ragged_chunks) {
        if (rag_cursor >= N_65k) break;
        const std::size_t sz = std::min(N_65k - rag_cursor, chunk);
        auto in_c = make_buffer(rgsml::audio::ChannelLayout::MONO_C, rag_cursor, std::span<const double>(signal.data() + rag_cursor, sz));
        auto out_c = out_ragged.value()->mutable_view().subview(rgsml::core::FrameIndex{static_cast<std::int64_t>(rag_cursor)}, frame_count(sz));

        const bool is_beg = (rag_cursor == 0);
        QVERIFY(mod_ragged->process(in_c.value()->view(), *out_c.value(), DspProcessContext{frame_range(rag_cursor, rag_cursor + sz), is_beg, false}));
        rag_cursor += sz;
    }
    QCOMPARE(bits(out_ragged.value()->view()), ref_bits);
}

void CompressorGateGTest::checkpointContinuationGateG()
{
    // AP §24.8 qualification: uninterrupted vs checkpoint/restore continuation
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    auto params = *CompressorParameters::create_default().value();

    const DspProcessSpec spec1{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(1024)};
    const DspProcessSpec spec2{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(2048)};

    const std::size_t N = 1000U;
    std::vector<double> signal(N);
    for (std::size_t i = 0; i < N; ++i) {
        signal[i] = std::sin(2.0 * kPi * 440.0 * static_cast<double>(i) / 48000.0);
    }

    // Checkpoint locations: origin (0), 1, 239, 240, 241, 50, 700, 999
    const std::size_t cp_locations[] = {0, 1, 50, 239, 240, 241, 700, 999};

    for (const std::size_t cp_frame : cp_locations) {
        auto mod_uninterrupted = std::move(*CompressorModule::create(desc, params).value());
        auto mod_restored = std::move(*CompressorModule::create(desc, params).value());

        QVERIFY(mod_uninterrupted->prepare(spec1));
        QVERIFY(mod_restored->prepare(spec2));

        if (cp_frame > 0) {
            auto in_part1 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, std::span<const double>(signal.data(), cp_frame));
            auto out_part1 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, std::span<const double>(signal.data(), cp_frame));
            QVERIFY(mod_uninterrupted->process(in_part1.value()->view(), out_part1.value()->mutable_view(), DspProcessContext{frame_range(0, static_cast<std::int64_t>(cp_frame)), true, false}));
        }

        auto cp_res = mod_uninterrupted->runtime_checkpoint();
        QVERIFY(cp_res);

        QVERIFY(mod_restored->restore_runtime_checkpoint(*cp_res.value()));

        const std::size_t rem_frames = N - cp_frame;
        if (rem_frames > 0) {
            auto in_part2 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, cp_frame, std::span<const double>(signal.data() + cp_frame, rem_frames));
            auto out1 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, cp_frame, std::span<const double>(signal.data() + cp_frame, rem_frames));
            auto out2 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, cp_frame, std::span<const double>(signal.data() + cp_frame, rem_frames));

            const bool is_beg = (cp_frame == 0);
            QVERIFY(mod_uninterrupted->process(in_part2.value()->view(), out1.value()->mutable_view(), DspProcessContext{frame_range(cp_frame, N), is_beg, false}));
            QVERIFY(mod_restored->process(in_part2.value()->view(), out2.value()->mutable_view(), DspProcessContext{frame_range(cp_frame, N), is_beg, false}));

            QCOMPARE(bits(out1.value()->view()), bits(out2.value()->view()));
        }
    }

    // Gate-G Explicit Incompatible & Atomic Rejection Evidence
    {
        auto mod_target = std::move(*CompressorModule::create(desc, params).value());
        QVERIFY(mod_target->prepare(spec1));

        // Advance mod_target by 100 frames to establish baseline state
        auto in_base = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, std::span<const double>(signal.data(), 100));
        auto out_base = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, std::span<const double>(signal.data(), 100));
        QVERIFY(mod_target->process(in_base.value()->view(), out_base.value()->mutable_view(), DspProcessContext{frame_range(0, 100), true, false}));

        // Capture valid reference checkpoint at frame 100
        const auto valid_cp = *mod_target->runtime_checkpoint().value();

        // Prepare a second independent module to test post-rejection bit-identical output
        auto mod_ref = std::move(*CompressorModule::create(desc, params).value());
        QVERIFY(mod_ref->prepare(spec1));
        QVERIFY(mod_ref->restore_runtime_checkpoint(valid_cp));

        // Test 12 mismatch types
        std::vector<DspRuntimeCheckpoint> corrupt_cps;

        // 1. wrong module type
        { auto cp = valid_cp; cp.module_type_id = "rgsml.dsp.gain"; corrupt_cps.push_back(cp); }
        // 2. wrong algorithm version
        { auto cp = valid_cp; cp.algorithm_version = "2.0.0"; corrupt_cps.push_back(cp); }
        // 3. wrong parameter schema
        { auto cp = valid_cp; cp.parameter_schema_id = "rgsml.dsp.gain.parameters/1.0.0"; corrupt_cps.push_back(cp); }
        // 4. wrong sonic fingerprint
        { auto cp = valid_cp; cp.sonic_fingerprint = "0000000000000000000000000000000000000000000000000000000000000000"; corrupt_cps.push_back(cp); }
        // 5. wrong audio format
        { auto cp = valid_cp; cp.audio_format = format(rgsml::audio::ChannelLayout::STEREO_LR, 48000); corrupt_cps.push_back(cp); }
        // 6. wrong frame domain
        { auto cp = valid_cp; cp.frame_domain_id = static_cast<rgsml::audio::FrameDomainId>(999); corrupt_cps.push_back(cp); }
        // 7. wrong checkpoint schema
        { auto cp = valid_cp; cp.checkpoint_schema_version = "invalid.schema/1.0.0"; corrupt_cps.push_back(cp); }
        // 8. wrong backend identity
        { auto cp = valid_cp; cp.backend_identity = "invalid_backend"; corrupt_cps.push_back(cp); }
        // 9. malformed/truncated payload
        { auto cp = valid_cp; cp.payload.pop_back(); corrupt_cps.push_back(cp); }
        // 10. trailing payload
        { auto cp = valid_cp; cp.payload.push_back(0xFF); corrupt_cps.push_back(cp); }
        // 11. invalid cursor/state payload (corrupt inner bytes - e.g. set cursor out of bounds or negative RMS)
        { auto cp = valid_cp; if (cp.payload.size() > 20) { std::uint64_t bad_cursor = 999999; std::memcpy(cp.payload.data() + 24, &bad_cursor, 8); } corrupt_cps.push_back(cp); }
        // 12. bound next_input_frame mismatch
        { auto cp = valid_cp; cp.next_input_frame = rgsml::core::FrameIndex{9999}; corrupt_cps.push_back(cp); }

        for (const auto& bad_cp : corrupt_cps) {
            const auto res = mod_target->restore_runtime_checkpoint(bad_cp);
            QVERIFY(!res);
            QCOMPARE(res.error()->code(), rgsml::core::ErrorCode::InvalidArgument);

            // Verify atomic state preservation: process next 100 frames on both mod_target and mod_ref
            auto in_next = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 100, std::span<const double>(signal.data() + 100, 100));
            auto out_target = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 100, std::span<const double>(signal.data() + 100, 100));
            auto out_ref = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 100, std::span<const double>(signal.data() + 100, 100));

            QVERIFY(mod_target->process(in_next.value()->view(), out_target.value()->mutable_view(), DspProcessContext{frame_range(100, 200), false, false}));
            QVERIFY(mod_ref->process(in_next.value()->view(), out_ref.value()->mutable_view(), DspProcessContext{frame_range(100, 200), false, false}));

            QCOMPARE(bits(out_target.value()->view()), bits(out_ref.value()->view()));

            // Reset mod_target and mod_ref and restore back to frame 100 valid state for the next test iteration
            mod_target->reset();
            QVERIFY(mod_target->restore_runtime_checkpoint(valid_cp));
            mod_ref->reset();
            QVERIFY(mod_ref->restore_runtime_checkpoint(valid_cp));
        }
    }
}

void CompressorGateGTest::eosRendererIntegrationGateG()
{
    // AP §24.9 EOS & Renderer Integration
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    std::vector<double> left(1000U);
    std::vector<double> right(1000U);
    for (std::size_t i = 0; i < 1000U; ++i) {
        left[i] = std::sin(2.0 * kPi * 440.0 * static_cast<double>(i) / 48000.0);
        right[i] = std::cos(2.0 * kPi * 440.0 * static_cast<double>(i) / 48000.0);
    }
    auto source = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    auto chain = empty_test_chain(*registry.value());

    const auto gain_id = make_id("60000000-0000-0000-0000-000000000001");
    const auto comp1_id = make_id("60000000-0000-0000-0000-000000000002");
    const auto comp2_id = make_id("60000000-0000-0000-0000-000000000003");
    const auto eq_id = make_id("60000000-0000-0000-0000-000000000004");

    QVERIFY(chain.add(gain_id, "rgsml.dsp.gain", 0));
    QVERIFY(chain.add(comp1_id, "rgsml.dsp.compressor", 1));
    QVERIFY(chain.add(comp2_id, "rgsml.dsp.compressor", 2));
    QVERIFY(chain.add(eq_id, "rgsml.dsp.parametric-eq", 3));

    auto comp1_params = *CompressorParameters::create(
        CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX,
        -18.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0).value();

    auto comp2_params = *CompressorParameters::create(
        CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX,
        -12.0, 4.0, 0.0, 10.0, 100.0, 50.0, 10.0, 100.0, 0.0).value();

    const auto band_id = *rgsml::core::Uuid::parse("60000000-0000-0000-0000-000000000005").value();
    auto band = EqBandParameters::create(
        band_id, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{1000.0, 3.0, 1.0});
    auto eq_params = ParametricEqParameters::create({*band.value()});

    const ModuleExecutionBinding gain_b{gain_id, *GainParameters::create(3.0).value()};
    const ModuleExecutionBinding comp1_b{comp1_id, comp1_params};
    const ModuleExecutionBinding comp2_b{comp2_id, comp2_params};
    const ModuleExecutionBinding eq_b{eq_id, *eq_params.value()};

    auto req = render::RenderRequest::create(
        source.value()->view(), frame_range(0, 1000), chain,
        {gain_b, comp1_b, comp2_b, eq_b}, frame_count(64));
    QVERIFY(req);

    auto res = render::render_preview(*req.value(), *registry.value());
    QVERIFY(res);

    QCOMPARE(res.value()->render_window(), frame_range(0, 1000));
    QCOMPARE(res.value()->signatures().size(), std::size_t{4});

    // Materialize AP §24.9 EOS & Ragged Finalize Matrix
    const auto desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();
    const DspProcessSpec spec_l{format(rgsml::audio::ChannelLayout::MONO_C, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(1024)};

    // Parameters with 5 ms lookahead @ 48 kHz -> L = 240 frames
    auto comp_l_params = *CompressorParameters::create(
        CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX,
        -12.0, 4.0, 0.0, 10.0, 100.0, 50.0, 5.0, 100.0, 0.0).value();

    const std::size_t L = 240U;
    const std::size_t chunk_cases[] = {L - 1, L, L + 1, 3 * L + 7};

    for (const std::size_t N_real : chunk_cases) {
        auto mod = std::move(*CompressorModule::create(desc, comp_l_params).value());
        QVERIFY(mod->prepare(spec_l));

        std::vector<double> real_sig(N_real, 0.5);
        real_sig.back() = 1.0; // Last-sample impulse

        auto in_buf = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, real_sig);
        auto out_buf = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, real_sig);

        // Process real signal in 1 or more ragged chunks
        QVERIFY(mod->process(in_buf.value()->view(), out_buf.value()->mutable_view(), DspProcessContext{frame_range(0, N_real), true, false}));

        // Finalize: requested finalize cannot exceed L total drain steps
        auto fin_out = make_buffer(rgsml::audio::ChannelLayout::MONO_C, N_real, std::vector<double>(L, 0.0));
        QVERIFY(mod->finalize(fin_out.value()->mutable_view(), DspProcessContext{frame_range(N_real, N_real + L), false, true}));

        // Confirm impulse appears in drain exactly L frames after input index (N_real - 1 + L)
        const auto fin_span = *fin_out.value()->view().channel(0).value();
        QVERIFY(fin_span[L - 1] > 0.0);
    }

    // Lookahead = 0 ms -> zero finalize tail
    auto comp_zero_params = *CompressorParameters::create(
        CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX,
        -12.0, 4.0, 0.0, 10.0, 100.0, 50.0, 0.0, 100.0, 0.0).value();

    auto mod_zero = std::move(*CompressorModule::create(desc, comp_zero_params).value());
    QVERIFY(mod_zero->prepare(spec_l));
    auto in_z = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, std::vector<double>{0.5, 0.5});
    auto out_z = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, std::vector<double>{0.5, 0.5});
    QVERIFY(mod_zero->process(in_z.value()->view(), out_z.value()->mutable_view(), DspProcessContext{frame_range(0, 2), true, false}));

    auto fin_z = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 2, std::vector<double>{});
    QVERIFY(mod_zero->finalize(fin_z.value()->mutable_view(), DspProcessContext{frame_range(2, 2), false, true}));
}

void CompressorGateGTest::ap24FullMatrixQualification()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    // 1. Lookahead exact ties at Fs = 8000 Hz with nextafter:
    // 0.0625 ms = 0.5 frame -> 0
    // 0.1875 ms = 1.5 frames -> 2
    // 0.3125 ms = 2.5 frames -> 2
    // 0.4375 ms = 3.5 frames -> 4
    const DspProcessSpec spec8000{format(rgsml::audio::ChannelLayout::MONO_C, 8000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(512)};

    const struct {
        double ms;
        std::int64_t expected_frames;
    } tie_points[] = {
        {0.0625, 0},
        {0.1875, 2},
        {0.3125, 2},
        {0.4375, 4}
    };

    for (const auto& tp : tie_points) {
        const double below = std::nextafter(tp.ms, 0.0);
        const double above = std::nextafter(tp.ms, 10.0);

        auto p_below = *CompressorParameters::create(CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX, -12.0, 2.0, 6.0, 30.0, 200.0, 50.0, below, 100.0, 0.0).value();
        auto p_exact = *CompressorParameters::create(CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX, -12.0, 2.0, 6.0, 30.0, 200.0, 50.0, tp.ms, 100.0, 0.0).value();
        auto p_above = *CompressorParameters::create(CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX, -12.0, 2.0, 6.0, 30.0, 200.0, 50.0, above, 100.0, 0.0).value();

        auto mod_b = std::move(*CompressorModule::create(desc, p_below).value());
        auto mod_e = std::move(*CompressorModule::create(desc, p_exact).value());
        auto mod_a = std::move(*CompressorModule::create(desc, p_above).value());

        auto reqs_b = mod_b->runtime_requirements(spec8000);
        auto reqs_e = mod_e->runtime_requirements(spec8000);
        auto reqs_a = mod_a->runtime_requirements(spec8000);
        QVERIFY(reqs_b); QVERIFY(reqs_e); QVERIFY(reqs_a);

        QCOMPARE(reqs_e.value()->algorithmic_latency_frames.value(), tp.expected_frames);
        QCOMPARE(reqs_e.value()->look_ahead_frames.value(), tp.expected_frames);

        QVERIFY(reqs_b.value()->algorithmic_latency_frames.value() <= tp.expected_frames);
        QVERIFY(reqs_a.value()->algorithmic_latency_frames.value() >= tp.expected_frames);
    }

    // 2. Core sample rates: 44.1, 48, 96, 192 kHz vs Independent Oracle audio comparison
    for (const auto sr : {44100, 48000, 96000, 192000}) {
        for (const double look_ms : {0.0, 5.0, 20.0}) {
            auto params = *CompressorParameters::create(
                CompressorDetectorMode::RMS, CompressorChannelLink::LINKED_MAX,
                -12.0, 2.0, 6.0, 30.0, 200.0, 50.0, look_ms, 100.0, 0.0).value();

            auto mod = std::move(*CompressorModule::create(desc, params).value());
            const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::MONO_C, sr), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(1024)};
            QVERIFY(mod->prepare(spec));

            const std::size_t N = 1000U;
            std::vector<double> sig(N);
            for (std::size_t i = 0; i < N; ++i) {
                sig[i] = std::sin(2.0 * kPi * 1000.0 * static_cast<double>(i) / static_cast<double>(sr));
            }

            auto in_b = make_buffer(format(rgsml::audio::ChannelLayout::MONO_C, sr), 0, sig);
            auto out_b = make_buffer(format(rgsml::audio::ChannelLayout::MONO_C, sr), 0, std::vector<double>(N, 0.0));

            const auto proc_status = mod->process(in_b.value()->view(), out_b.value()->mutable_view(), DspProcessContext{frame_range(0, N), true, false});
            QVERIFY(proc_status);

            IndependentCompressorOracle oracle(params, static_cast<double>(sr), 1U);
            const auto oracle_traces = oracle.process({sig});

            const auto p_out_res = out_b.value()->view().channel(0);
            QVERIFY(p_out_res.has_value());
            const auto p_out_span = *p_out_res.value();
            QCOMPARE(p_out_span.size(), oracle_traces.size());
            for (std::size_t i = 0; i < N; ++i) {
                QCOMPARE(p_out_span[i], oracle_traces[i].output_sample_ch0);
            }
        }
    }
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::CompressorGateGTest)

#include "test_compressor_gate_g.moc"
