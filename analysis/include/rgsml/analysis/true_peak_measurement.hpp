#pragma once

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/core/result.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace rgsml::analysis {

// Product Gate G remains OPEN. A supported numeric reading from this API
// is not proof of official ITU/EBU / six-rate release qualification.
enum class TruePeakQualificationStatus : std::uint8_t {
    QUALIFIED, SUPPORTED_UNQUALIFIED, UNSUPPORTED
};

// Explicit tie/silence prevents fabricated left/right channel ownership.
enum class TruePeakPeakChannel : std::uint8_t {
    NONE, MONO_C, LEFT, RIGHT, TIE
};

class TruePeakMeasurement final {
public:
    [[nodiscard]] constexpr std::string_view algorithm_version() const noexcept
    { return "1.0.0"; }
    [[nodiscard]] constexpr std::string_view true_peak_policy_id() const noexcept
    { return "rgsml.analysis.true-peak.itu4x/1.0.0"; }
    [[nodiscard]] constexpr std::string_view result_schema_id() const noexcept
    { return "rgsml.analysis.true-peak.result/1.0.0"; }
    [[nodiscard]] constexpr std::string_view filter_asset_sha256() const noexcept
    {
        return "D1DF3373D3C5A82A3E4CFE3576BDDF97E153F3281FF9287554E024FA6CB0D316";
    }

    [[nodiscard]] std::int64_t sample_rate_hz() const noexcept { return sample_rate_hz_; }
    [[nodiscard]] std::size_t channel_count() const noexcept { return channel_count_; }
    [[nodiscard]] TruePeakQualificationStatus qualification_status() const noexcept
    { return qualification_status_; }
    [[nodiscard]] std::span<const double> per_channel_linear_peak() const noexcept
    { return {per_channel_linear_peak_.data(), channel_count_}; }
    [[nodiscard]] std::span<const std::optional<double>> per_channel_db_tp() const noexcept
    { return {per_channel_db_tp_.data(), channel_count_}; }
    [[nodiscard]] double global_linear_peak() const noexcept { return global_linear_peak_; }
    [[nodiscard]] const std::optional<double>& global_db_tp() const noexcept
    { return global_db_tp_; }
    [[nodiscard]] TruePeakPeakChannel global_peak_channel() const noexcept
    { return global_peak_channel_; }
    [[nodiscard]] bool is_exact_silence() const noexcept { return is_exact_silence_; }

private:
    friend core::Result<TruePeakMeasurement>
    measure_true_peak_full_resource(const audio::AudioBuffer& resource);
    TruePeakMeasurement() = default;

    std::int64_t sample_rate_hz_{0};
    std::size_t channel_count_{0};
    std::array<double, 2> per_channel_linear_peak_{};
    std::array<std::optional<double>, 2> per_channel_db_tp_{};
    double global_linear_peak_{0.0};
    std::optional<double> global_db_tp_{};
    TruePeakPeakChannel global_peak_channel_{TruePeakPeakChannel::NONE};
    bool is_exact_silence_{true};
    // Future QUALIFIED promotions MUST come from formal exact-build Gate G
    // authority, not a setter, rate whitelist, app flag or CI result.
    TruePeakQualificationStatus qualification_status_{
        TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED};
};

// Complete owning AudioBuffer only. Returns categorical errors, not a fake
// UNSUPPORTED result with numeric values, on invalid Fs/layout/PCM/empty.
[[nodiscard]] core::Result<TruePeakMeasurement>
measure_true_peak_full_resource(const audio::AudioBuffer& resource);

}  // namespace rgsml::analysis
