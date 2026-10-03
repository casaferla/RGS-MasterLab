#pragma once

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/dsp/compressor_parameters.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace rgsml::tests::oracle {

struct CompressorControlTraceFrame final {
    double detector_magnitude_ch0{0.0};
    double detector_magnitude_ch1{0.0};
    double linked_detector_ch0{0.0};
    double linked_detector_ch1{0.0};
    double level_db_ch0{-140.0};
    double level_db_ch1{-140.0};
    bool level_db_valid_ch0{false};
    bool level_db_valid_ch1{false};
    double target_reduction_db_ch0{0.0};
    double target_reduction_db_ch1{0.0};
    double smoothed_reduction_db_ch0{0.0};
    double smoothed_reduction_db_ch1{0.0};
    double linear_gain_ch0{1.0};
    double linear_gain_ch1{1.0};
    double output_sample_ch0{0.0};
    double output_sample_ch1{0.0};
};

class IndependentCompressorOracle final {
public:
    explicit IndependentCompressorOracle(
        rgsml::dsp::CompressorParameters params,
        double sample_rate,
        std::size_t channel_count)
        : params_(params)
        , sample_rate_(sample_rate)
        , channels_(channel_count)
    {
        lookahead_frames_ = round_ties_to_even(params_.look_ahead_ms() * sample_rate_ / 1000.0);
        const double rms_sec = params_.rms_time_constant_ms() / 1000.0;
        a_rms_ = std::exp(-1.0 / (rms_sec * sample_rate_));

        const double att_sec = params_.attack_ms() / 1000.0;
        a_attack_ = std::exp(-1.0 / (att_sec * sample_rate_));

        const double rel_sec = params_.release_ms() / 1000.0;
        a_release_ = std::exp(-1.0 / (rel_sec * sample_rate_));

        const std::size_t delay_cap = (lookahead_frames_ > 0) ? static_cast<std::size_t>(lookahead_frames_) : 1U;
        rms_states_.assign(channels_, 0.0);
        smoothed_reduction_db_.assign(channels_, 0.0);
        delay_cursors_.assign(channels_, 0U);
        delay_buffers_.assign(channels_, std::vector<double>(delay_cap, 0.0));
    }

    std::vector<CompressorControlTraceFrame> process(
        const std::vector<std::vector<double>>& input)
    {
        const std::size_t frames = input.empty() ? 0 : input[0].size();
        std::vector<CompressorControlTraceFrame> traces;
        traces.reserve(frames);

        const bool is_mono = (channels_ == 1U);
        const double threshold = params_.threshold_dbfs();
        const double ratio = params_.ratio();
        const double knee = params_.knee_db();
        const double makeup_factor = std::pow(10.0, params_.makeup_gain_db() / 20.0);
        const double mix_m = params_.mix_percent() / 100.0;
        const double inv_ratio_sub_one = (1.0 / ratio) - 1.0;
        const double one_sub_inv_ratio = 1.0 - (1.0 / ratio);

        const auto compute_target = [&](double p) -> double {
            if (p == 0.0 || ratio == 1.0) {
                return 0.0;
            }
            const double x_db = 20.0 * std::log10(p);
            if (knee == 0.0) {
                if (x_db <= threshold) return 0.0;
                return (x_db - threshold) * one_sub_inv_ratio;
            }
            const double diff = x_db - threshold;
            if (2.0 * diff < -knee) return 0.0;
            if (2.0 * std::abs(diff) <= knee) {
                const double term = diff + (knee * 0.5);
                const double y_db = x_db + (inv_ratio_sub_one * term * term) / (2.0 * knee);
                return x_db - y_db;
            }
            return diff * one_sub_inv_ratio;
        };

        for (std::size_t i = 0; i < frames; ++i) {
            CompressorControlTraceFrame tr;

            double p0 = 0.0;
            double p1 = 0.0;

            if (params_.detector_mode() == rgsml::dsp::CompressorDetectorMode::PEAK) {
                p0 = std::abs(input[0][i]);
                if (!is_mono) p1 = std::abs(input[1][i]);
            } else {
                const double x0 = input[0][i];
                rms_states_[0] = a_rms_ * rms_states_[0] + (1.0 - a_rms_) * (x0 * x0);
                p0 = std::sqrt(std::max(rms_states_[0], 1e-30));

                if (!is_mono) {
                    const double x1 = input[1][i];
                    rms_states_[1] = a_rms_ * rms_states_[1] + (1.0 - a_rms_) * (x1 * x1);
                    p1 = std::sqrt(std::max(rms_states_[1], 1e-30));
                }
            }

            tr.detector_magnitude_ch0 = p0;
            tr.detector_magnitude_ch1 = p1;

            double pl0 = p0;
            double pl1 = p1;
            if (!is_mono) {
                switch (params_.channel_link()) {
                case rgsml::dsp::CompressorChannelLink::LINKED_MAX: {
                    const double mx = std::max(p0, p1);
                    pl0 = mx; pl1 = mx;
                    break;
                }
                case rgsml::dsp::CompressorChannelLink::LINKED_MEAN: {
                    const double mn = std::sqrt((p0 * p0 + p1 * p1) * 0.5);
                    pl0 = mn; pl1 = mn;
                    break;
                }
                case rgsml::dsp::CompressorChannelLink::DUAL_MONO:
                    pl0 = p0; pl1 = p1;
                    break;
                }
            }

            tr.linked_detector_ch0 = pl0;
            tr.linked_detector_ch1 = pl1;

            if (pl0 > 0.0) {
                tr.level_db_ch0 = 20.0 * std::log10(pl0);
                tr.level_db_valid_ch0 = true;
            } else {
                tr.level_db_ch0 = -std::numeric_limits<double>::infinity();
                tr.level_db_valid_ch0 = false;
            }

            if (!is_mono && pl1 > 0.0) {
                tr.level_db_ch1 = 20.0 * std::log10(pl1);
                tr.level_db_valid_ch1 = true;
            } else {
                tr.level_db_ch1 = -std::numeric_limits<double>::infinity();
                tr.level_db_valid_ch1 = false;
            }

            const double targ0 = compute_target(pl0);
            const double targ1 = is_mono ? 0.0 : compute_target(pl1);

            tr.target_reduction_db_ch0 = targ0;
            tr.target_reduction_db_ch1 = targ1;

            const auto smooth_red = [&](double targ, std::size_t ch) -> double {
                const double prev = smoothed_reduction_db_[ch];
                const double coeff = (targ > prev) ? a_attack_ : a_release_;
                const double curr = coeff * prev + (1.0 - coeff) * targ;
                smoothed_reduction_db_[ch] = curr;
                return curr;
            };

            const double red0 = smooth_red(targ0, 0);
            const double red1 = is_mono ? 0.0 : smooth_red(targ1, 1);

            tr.smoothed_reduction_db_ch0 = red0;
            tr.smoothed_reduction_db_ch1 = red1;

            tr.linear_gain_ch0 = std::pow(10.0, -red0 / 20.0);
            tr.linear_gain_ch1 = std::pow(10.0, -red1 / 20.0);

            for (std::size_t ch = 0; ch < channels_; ++ch) {
                const double in_smp = input[ch][i];
                const double red = (ch == 0) ? red0 : red1;

                double x_del = in_smp;
                if (lookahead_frames_ > 0) {
                    auto& buf = delay_buffers_[ch];
                    auto& cur = delay_cursors_[ch];
                    x_del = buf[cur];
                    buf[cur] = in_smp;
                    cur = (cur + 1U) % static_cast<std::size_t>(lookahead_frames_);
                }

                const double g_lin = std::pow(10.0, -red / 20.0);
                const double wet = x_del * g_lin * makeup_factor;
                const double out_smp = (1.0 - mix_m) * x_del + mix_m * wet;

                if (ch == 0) tr.output_sample_ch0 = out_smp;
                else tr.output_sample_ch1 = out_smp;
            }

            traces.push_back(tr);
        }

        return traces;
    }

    std::vector<CompressorControlTraceFrame> finalize(std::size_t drain_frames)
    {
        std::vector<CompressorControlTraceFrame> traces;
        traces.reserve(drain_frames);

        const bool is_mono = (channels_ == 1U);
        const double threshold = params_.threshold_dbfs();
        const double ratio = params_.ratio();
        const double knee = params_.knee_db();
        const double makeup_factor = std::pow(10.0, params_.makeup_gain_db() / 20.0);
        const double mix_m = params_.mix_percent() / 100.0;
        const double inv_ratio_sub_one = (1.0 / ratio) - 1.0;
        const double one_sub_inv_ratio = 1.0 - (1.0 / ratio);

        const auto compute_target = [&](double p) -> double {
            if (p == 0.0 || ratio == 1.0) {
                return 0.0;
            }
            const double x_db = 20.0 * std::log10(p);
            if (knee == 0.0) {
                if (x_db <= threshold) return 0.0;
                return (x_db - threshold) * one_sub_inv_ratio;
            }
            const double diff = x_db - threshold;
            if (2.0 * diff < -knee) return 0.0;
            if (2.0 * std::abs(diff) <= knee) {
                const double term = diff + (knee * 0.5);
                const double y_db = x_db + (inv_ratio_sub_one * term * term) / (2.0 * knee);
                return x_db - y_db;
            }
            return diff * one_sub_inv_ratio;
        };

        for (std::size_t i = 0; i < drain_frames; ++i) {
            CompressorControlTraceFrame tr;

            double p0 = 0.0;
            double p1 = 0.0;

            if (params_.detector_mode() == rgsml::dsp::CompressorDetectorMode::PEAK) {
                p0 = 0.0; p1 = 0.0;
            } else {
                rms_states_[0] = a_rms_ * rms_states_[0] + (1.0 - a_rms_) * 0.0;
                p0 = std::sqrt(std::max(rms_states_[0], 1e-30));

                if (!is_mono) {
                    rms_states_[1] = a_rms_ * rms_states_[1] + (1.0 - a_rms_) * 0.0;
                    p1 = std::sqrt(std::max(rms_states_[1], 1e-30));
                }
            }

            tr.detector_magnitude_ch0 = p0;
            tr.detector_magnitude_ch1 = p1;

            double pl0 = p0;
            double pl1 = p1;
            if (!is_mono) {
                switch (params_.channel_link()) {
                case rgsml::dsp::CompressorChannelLink::LINKED_MAX: {
                    const double mx = std::max(p0, p1);
                    pl0 = mx; pl1 = mx;
                    break;
                }
                case rgsml::dsp::CompressorChannelLink::LINKED_MEAN: {
                    const double mn = std::sqrt((p0 * p0 + p1 * p1) * 0.5);
                    pl0 = mn; pl1 = mn;
                    break;
                }
                case rgsml::dsp::CompressorChannelLink::DUAL_MONO:
                    pl0 = p0; pl1 = p1;
                    break;
                }
            }

            tr.linked_detector_ch0 = pl0;
            tr.linked_detector_ch1 = pl1;

            const double targ0 = compute_target(pl0);
            const double targ1 = is_mono ? 0.0 : compute_target(pl1);

            tr.target_reduction_db_ch0 = targ0;
            tr.target_reduction_db_ch1 = targ1;

            const auto smooth_red = [&](double targ, std::size_t ch) -> double {
                const double prev = smoothed_reduction_db_[ch];
                const double coeff = (targ > prev) ? a_attack_ : a_release_;
                const double curr = coeff * prev + (1.0 - coeff) * targ;
                smoothed_reduction_db_[ch] = curr;
                return curr;
            };

            const double red0 = smooth_red(targ0, 0);
            const double red1 = is_mono ? 0.0 : smooth_red(targ1, 1);

            tr.smoothed_reduction_db_ch0 = red0;
            tr.smoothed_reduction_db_ch1 = red1;

            for (std::size_t ch = 0; ch < channels_; ++ch) {
                const double red = (ch == 0) ? red0 : red1;

                double x_del = 0.0;
                if (lookahead_frames_ > 0) {
                    auto& buf = delay_buffers_[ch];
                    auto& cur = delay_cursors_[ch];
                    x_del = buf[cur];
                    buf[cur] = 0.0;
                    cur = (cur + 1U) % static_cast<std::size_t>(lookahead_frames_);
                }

                const double g_lin = std::pow(10.0, -red / 20.0);
                const double wet = x_del * g_lin * makeup_factor;
                const double out_smp = (1.0 - mix_m) * x_del + mix_m * wet;

                if (ch == 0) tr.output_sample_ch0 = out_smp;
                else tr.output_sample_ch1 = out_smp;
            }

            traces.push_back(tr);
        }

        return traces;
    }

    [[nodiscard]] std::int64_t lookahead_frames() const noexcept
    {
        return lookahead_frames_;
    }

private:
    [[nodiscard]] static std::int64_t round_ties_to_even(double x) noexcept
    {
        const double floor_val = std::floor(x);
        const double diff = x - floor_val;
        if (diff < 0.5) return static_cast<std::int64_t>(floor_val);
        if (diff > 0.5) return static_cast<std::int64_t>(floor_val + 1.0);
        const std::int64_t int_floor = static_cast<std::int64_t>(floor_val);
        return (int_floor % 2 == 0) ? int_floor : (int_floor + 1);
    }

    rgsml::dsp::CompressorParameters params_;
    double sample_rate_;
    std::size_t channels_;
    std::int64_t lookahead_frames_{0};
    double a_rms_{0.0};
    double a_attack_{0.0};
    double a_release_{0.0};

    std::vector<double> rms_states_;
    std::vector<double> smoothed_reduction_db_;
    std::vector<std::vector<double>> delay_buffers_;
    std::vector<std::size_t> delay_cursors_;
};

}  // namespace rgsml::tests::oracle
