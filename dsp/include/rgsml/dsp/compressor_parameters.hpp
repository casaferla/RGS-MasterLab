#pragma once

#include <rgsml/core/result.hpp>

#include <cstdint>

namespace rgsml::dsp {

enum class CompressorDetectorMode : std::uint8_t {
    PEAK,
    RMS,
};

enum class CompressorChannelLink : std::uint8_t {
    LINKED_MAX,
    LINKED_MEAN,
    DUAL_MONO,
};

class CompressorParameters final {
public:
    [[nodiscard]] static rgsml::core::Result<CompressorParameters> create(
        CompressorDetectorMode detector_mode = CompressorDetectorMode::RMS,
        CompressorChannelLink channel_link = CompressorChannelLink::LINKED_MAX,
        double threshold_dbfs = -24.0,
        double ratio = 2.0,
        double knee_db = 6.0,
        double attack_ms = 30.0,
        double release_ms = 200.0,
        double rms_time_constant_ms = 50.0,
        double look_ahead_ms = 5.0,
        double mix_percent = 100.0,
        double makeup_gain_db = 0.0);

    [[nodiscard]] static rgsml::core::Result<CompressorParameters> create_default() noexcept;

    [[nodiscard]] CompressorDetectorMode detector_mode() const noexcept;
    [[nodiscard]] CompressorChannelLink channel_link() const noexcept;
    [[nodiscard]] double threshold_dbfs() const noexcept;
    [[nodiscard]] double ratio() const noexcept;
    [[nodiscard]] double knee_db() const noexcept;
    [[nodiscard]] double attack_ms() const noexcept;
    [[nodiscard]] double release_ms() const noexcept;
    [[nodiscard]] double rms_time_constant_ms() const noexcept;
    [[nodiscard]] double look_ahead_ms() const noexcept;
    [[nodiscard]] double mix_percent() const noexcept;
    [[nodiscard]] double makeup_gain_db() const noexcept;

    friend bool operator==(
        const CompressorParameters&,
        const CompressorParameters&) = default;

private:
    CompressorParameters(
        CompressorDetectorMode detector_mode,
        CompressorChannelLink channel_link,
        double threshold_dbfs,
        double ratio,
        double knee_db,
        double attack_ms,
        double release_ms,
        double rms_time_constant_ms,
        double look_ahead_ms,
        double mix_percent,
        double makeup_gain_db) noexcept;

    CompressorDetectorMode detector_mode_;
    CompressorChannelLink channel_link_;
    double threshold_dbfs_;
    double ratio_;
    double knee_db_;
    double attack_ms_;
    double release_ms_;
    double rms_time_constant_ms_;
    double look_ahead_ms_;
    double mix_percent_;
    double makeup_gain_db_;
};

}  // namespace rgsml::dsp
