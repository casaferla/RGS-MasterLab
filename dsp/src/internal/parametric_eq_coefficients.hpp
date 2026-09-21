#pragma once

#include <rgsml/core/result.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rgsml::dsp::internal {

struct BiquadCoefficients final {
    double b0{1.0};
    double b1{0.0};
    double b2{0.0};
    double a1{0.0};
    double a2{0.0};

    friend bool operator==(const BiquadCoefficients&, const BiquadCoefficients&) = default;
};

struct SectionCoefficients final {
    BiquadCoefficients coeffs;
    double rmax{0.0};
    std::int64_t settling_frames{0};

    friend bool operator==(const SectionCoefficients&, const SectionCoefficients&) = default;
};

struct BandCoefficients final {
    rgsml::core::Uuid band_id;
    bool enabled{true};
    EqFilterType filter_type{EqFilterType::BELL};
    EqRouting routing{EqRouting::STEREO};
    std::vector<SectionCoefficients> sections;

    friend bool operator==(const BandCoefficients&, const BandCoefficients&) = default;
};

struct ParametricEqCoefficients final {
    std::vector<BandCoefficients> bands;
    std::int64_t total_settling_frames{0};

    friend bool operator==(const ParametricEqCoefficients&, const ParametricEqCoefficients&) = default;
};

[[nodiscard]] rgsml::core::Result<ParametricEqCoefficients> compute_parametric_eq_coefficients(
    const ParametricEqParameters& params,
    double sample_rate_hz);

}  // namespace rgsml::dsp::internal
