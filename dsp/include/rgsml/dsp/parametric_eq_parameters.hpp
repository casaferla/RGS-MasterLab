#pragma once

#include <rgsml/core/result.hpp>
#include <rgsml/core/uuid.hpp>

#include <cstdint>
#include <variant>
#include <vector>

namespace rgsml::dsp {

enum class EqFilterType : std::uint8_t {
    BELL,
    NOTCH,
    LOW_SHELF,
    HIGH_SHELF,
    HIGH_PASS,
    LOW_PASS,
};

enum class EqRouting : std::uint8_t {
    STEREO,
    MID,
    SIDE,
    LEFT,
    RIGHT,
};

enum class SlopeDbPerOctave : std::uint16_t {
    DB_6 = 6,
    DB_12 = 12,
    DB_18 = 18,
    DB_24 = 24,
    DB_36 = 36,
    DB_48 = 48,
};

struct BellPayload final {
    double frequency_hz{1000.0};
    double gain_db{0.0};
    double q{0.707};

    friend bool operator==(const BellPayload&, const BellPayload&) = default;
};

struct NotchPayload final {
    double frequency_hz{1000.0};
    double q{0.707};

    friend bool operator==(const NotchPayload&, const NotchPayload&) = default;
};

struct ShelfPayload final {
    double frequency_hz{1000.0};
    double gain_db{0.0};
    double shelf_slope{1.0};

    friend bool operator==(const ShelfPayload&, const ShelfPayload&) = default;
};

struct PassPayload final {
    double frequency_hz{1000.0};
    SlopeDbPerOctave slope_db_per_octave{SlopeDbPerOctave::DB_12};

    friend bool operator==(const PassPayload&, const PassPayload&) = default;
};

using EqBandPayload = std::variant<BellPayload, NotchPayload, ShelfPayload, PassPayload>;

class EqBandParameters final {
public:
    [[nodiscard]] static rgsml::core::Result<EqBandParameters> create(
        rgsml::core::Uuid band_id,
        bool enabled,
        EqFilterType filter_type,
        EqRouting routing,
        EqBandPayload payload);

    [[nodiscard]] const rgsml::core::Uuid& band_id() const noexcept;
    [[nodiscard]] bool enabled() const noexcept;
    [[nodiscard]] EqFilterType filter_type() const noexcept;
    [[nodiscard]] EqRouting routing() const noexcept;
    [[nodiscard]] const EqBandPayload& payload() const noexcept;

    friend bool operator==(const EqBandParameters&, const EqBandParameters&) = default;

private:
    EqBandParameters(
        rgsml::core::Uuid band_id,
        bool enabled,
        EqFilterType filter_type,
        EqRouting routing,
        EqBandPayload payload) noexcept;

    rgsml::core::Uuid band_id_;
    bool enabled_;
    EqFilterType filter_type_;
    EqRouting routing_;
    EqBandPayload payload_;
};

class ParametricEqParameters final {
public:
    [[nodiscard]] static rgsml::core::Result<ParametricEqParameters> create(
        std::vector<EqBandParameters> bands);

    [[nodiscard]] static rgsml::core::Result<ParametricEqParameters> create_legacy_default();

    [[nodiscard]] const std::vector<EqBandParameters>& bands() const noexcept;

    friend bool operator==(const ParametricEqParameters&, const ParametricEqParameters&) = default;

private:
    explicit ParametricEqParameters(std::vector<EqBandParameters> bands) noexcept;

    std::vector<EqBandParameters> bands_;
};

}  // namespace rgsml::dsp
