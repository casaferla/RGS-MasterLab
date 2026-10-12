#pragma once

#include <rgsml/core/result.hpp>

#include <cstdint>

namespace rgsml::dsp {

// L1-M15 accepted/frozen persistence values. Width is derived from broadband
// Mid/Side gain and sideMuted; it is not an independently serialized field.
enum class MonoBassMode : std::uint8_t {
    OFF,
    LR12,
    LR24,
};

class StereoMsParameters final {
public:
    [[nodiscard]] static rgsml::core::Result<StereoMsParameters> create(
        double mid_gain_db = 0.0,
        double side_gain_db = 0.0,
        bool side_muted = false,
        MonoBassMode mono_bass_mode = MonoBassMode::OFF,
        double mono_bass_cutoff_hz = 120.0,
        double low_band_width_percent = 100.0);

    [[nodiscard]] static rgsml::core::Result<StereoMsParameters>
    create_default() noexcept;

    [[nodiscard]] double mid_gain_db() const noexcept;
    [[nodiscard]] double side_gain_db() const noexcept;
    [[nodiscard]] bool side_muted() const noexcept;
    [[nodiscard]] MonoBassMode mono_bass_mode() const noexcept;
    [[nodiscard]] double mono_bass_cutoff_hz() const noexcept;
    [[nodiscard]] double low_band_width_percent() const noexcept;

    friend bool operator==(const StereoMsParameters&,
                           const StereoMsParameters&) = default;

private:
    StereoMsParameters(
        double mid_gain_db,
        double side_gain_db,
        bool side_muted,
        MonoBassMode mono_bass_mode,
        double mono_bass_cutoff_hz,
        double low_band_width_percent) noexcept;

    double mid_gain_db_;
    double side_gain_db_;
    bool side_muted_;
    MonoBassMode mono_bass_mode_;
    double mono_bass_cutoff_hz_;
    double low_band_width_percent_;
};

}  // namespace rgsml::dsp
