#pragma once

#include <rgsml/dsp/stereo_ms_crossover.hpp>

#include <array>

namespace rgsml::dsp {

// Exact frozen LR topology: separate Low and High states for each Mid/Side
// lane, two serial sections per branch, TDF-II normalized feedback convention.
// This A4b-1 kernel does not independently authorize ModuleRegistry activation.
struct StereoMsCrossoverFrame final {
    double mid;
    double side;
};

class StereoMsCrossoverRuntime final {
public:
    explicit StereoMsCrossoverRuntime(StereoMsCrossoverDesign design) noexcept;

    void reset() noexcept;

    // Called only on prepared, finite, gain-adjusted M/S samples with
    // beta=lowBandWidthPercent/100. The caller owns stream-level transaction
    // validation, sample finiteness and checkpoint serialization.
    [[nodiscard]] StereoMsCrossoverFrame process(
        double mid, double side, double beta) noexcept;

    [[nodiscard]] bool finite() const noexcept;

private:
    struct SectionDelay final {
        double z1{0.0};
        double z2{0.0};
    };

    using Cascade = std::array<SectionDelay, 2>;

    [[nodiscard]] static double section(
        double input, StereoMsFilterSection coeff, SectionDelay& state) noexcept;

    [[nodiscard]] static double cascade(
        double input, StereoMsFilterSection coeff, Cascade& state) noexcept;

    StereoMsCrossoverDesign design_;
    Cascade mid_low_{};
    Cascade mid_high_{};
    Cascade side_low_{};
    Cascade side_high_{};
};

}  // namespace rgsml::dsp
