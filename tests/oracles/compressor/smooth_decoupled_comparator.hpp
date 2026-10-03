#pragma once

#include <rgsml/dsp/compressor_parameters.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace rgsml::tests::oracle {

class SmoothDecoupledComparator final {
public:
    explicit SmoothDecoupledComparator(
        rgsml::dsp::CompressorParameters params,
        double sample_rate,
        std::size_t channel_count)
        : params_(params)
        , sample_rate_(sample_rate)
        , channels_(channel_count)
    {
        const double att_sec = params_.attack_ms() / 1000.0;
        a_attack_ = std::exp(-1.0 / (att_sec * sample_rate_));

        const double rel_sec = params_.release_ms() / 1000.0;
        a_release_ = std::exp(-1.0 / (rel_sec * sample_rate_));

        release_envelopes_.assign(channels_, 0.0);
        smoothed_reductions_.assign(channels_, 0.0);
    }

    std::vector<double> process_sample(double target_reduction_db, std::size_t channel)
    {
        const double x = std::max(0.0, target_reduction_db);

        const double rel_env = std::max(
            x,
            a_release_ * release_envelopes_[channel] + (1.0 - a_release_) * x
        );
        release_envelopes_[channel] = rel_env;

        const double y = a_attack_ * smoothed_reductions_[channel] + (1.0 - a_attack_) * rel_env;
        smoothed_reductions_[channel] = y;

        return {y};
    }

private:
    rgsml::dsp::CompressorParameters params_;
    double sample_rate_;
    std::size_t channels_;
    double a_attack_{0.0};
    double a_release_{0.0};

    std::vector<double> release_envelopes_;
    std::vector<double> smoothed_reductions_;
};

}  // namespace rgsml::tests::oracle
