#pragma once
// L3-M01 Dynamic EQ (ACCEPTED/FROZEN P01–P08, 2026-10-06).
// Public typed parameter/identity slice only: no module factory, no DSP
// execution, and no accidental claim of L3-M01/M02 qualification.
// Reuse the existing common EqRouting and Uuid authority, not a parallel
// parameter codec or a second Qt/DSP model.
#include <rgsml/core/result.hpp>
#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>
#include <cstdint>
#include <variant>
#include <vector>

namespace rgsml::dsp {

enum class DynamicEqFilterType : std::uint8_t {
    BELL, LOW_SHELF, HIGH_SHELF
};

struct DynamicEqBellShape final {
    double frequency_hz{1000.0};
    double q{0.707};
    friend bool operator==(const DynamicEqBellShape&,const DynamicEqBellShape&)=default;
};
struct DynamicEqShelfShape final {
    double frequency_hz{1000.0};
    double shelf_slope{1.0};
    friend bool operator==(const DynamicEqShelfShape&,const DynamicEqShelfShape&)=default;
};
// q and shelfSlope are *discriminated*, not concurrent sonic controls.
using DynamicEqShape=std::variant<DynamicEqBellShape,DynamicEqShelfShape>;

struct DynamicEqControl final {
    double base_gain_db{0.0};
    double range_db{0.0};
    double threshold_dbfs{-24.0};
    double ratio{2.0};
    double rms_time_constant_ms{10.0};
    double attack_ms{30.0};
    double release_ms{200.0};
    friend bool operator==(const DynamicEqControl&,const DynamicEqControl&)=default;
};

struct DynamicEqBandSpec final {
    core::Uuid band_id;
    bool enabled{true};
    DynamicEqFilterType filter_type{DynamicEqFilterType::BELL};
    EqRouting routing{EqRouting::STEREO};
    DynamicEqShape shape{DynamicEqBellShape{}};
    DynamicEqControl control{};
};

class DynamicEqBandParameters final {
public:
    [[nodiscard]] static core::Result<DynamicEqBandParameters> create(
        DynamicEqBandSpec spec);
    [[nodiscard]] const DynamicEqBandSpec& spec() const noexcept;
    [[nodiscard]] core::Status validate_for_audio(
        std::uint32_t sample_rate_hz,std::uint32_t channels) const;
    friend bool operator==(const DynamicEqBandParameters&,
                           const DynamicEqBandParameters&)=default;
private:
    explicit DynamicEqBandParameters(DynamicEqBandSpec spec);
    DynamicEqBandSpec spec_;
};

class DynamicEqParameters final {
public:
    [[nodiscard]] static core::Result<DynamicEqParameters> create(
        std::vector<DynamicEqBandParameters> bands);
    [[nodiscard]] static core::Result<DynamicEqParameters> create_default(
        core::Uuid band_id);
    [[nodiscard]] const std::vector<DynamicEqBandParameters>& bands() const noexcept;
    [[nodiscard]] core::Status validate_for_audio(
        std::uint32_t sample_rate_hz,std::uint32_t channels) const;
    friend bool operator==(const DynamicEqParameters&,
                           const DynamicEqParameters&)=default;
private:
    explicit DynamicEqParameters(std::vector<DynamicEqBandParameters> bands);
    std::vector<DynamicEqBandParameters> bands_;
};

} // namespace rgsml::dsp
