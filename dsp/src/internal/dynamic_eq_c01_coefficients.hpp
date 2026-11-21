#pragma once
// L3-M01 C01 accepted/frozen audio realization: immutable-shape, per-sample
// normalized RBJ coefficient source. This helper neither advances audio
// recursive state nor implements a complete Dynamic EQ IModule.
#include <rgsml/dsp/dynamic_eq_parameters.hpp>
#include "parametric_eq_coefficients.hpp"
#include <cstdint>

namespace rgsml::dsp::internal {

class C01CoefficientGenerator final {
public:
    [[nodiscard]] static core::Result<C01CoefficientGenerator> create(
        const DynamicEqBandParameters& band,
        std::uint32_t sample_rate_hz, std::uint32_t channels);
    // Audio coefficients are recomputed for each authoritative frame, with
    // the current P04/P05 effective gain; no gain clock/interpolation.
    [[nodiscard]] core::Result<BiquadCoefficients>
    audio_coefficients(double effective_gain_db) const;
    // Fixed dedicated detector does not observe audio gain or shelfSlope.
    [[nodiscard]] core::Result<BiquadCoefficients>
    detector_coefficients() const;
private:
    C01CoefficientGenerator(DynamicEqFilterType type,
                            double c, double s, double shape) noexcept;
    DynamicEqFilterType type_;
    double c_;
    double s_;
    double shape_; // Q for Bell, S for shelves
};

} // namespace rgsml::dsp::internal
