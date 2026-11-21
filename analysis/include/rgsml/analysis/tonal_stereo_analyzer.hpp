#pragma once
// L1-M20 Basic Tonal + Stereo: read-only Product-facing scope analysis.
// Reuses the existing FFT/Hann/PSD and M19's independently supplied
// retained-block/eligibility evidence. NO alternative spectrum engine,
// fabricated loudness gate, or canonical M21 GOLD comparison here.
#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace rgsml::analysis {

struct TonalStereoRetainedInterval final {
    std::uint64_t begin{0},end{0}; // absolute, half-open
};

struct TonalStereoTrustedSupport final {
    // If unavailable, explicitly include ALL complete STFT windows,
    // never infer a mask from LUFS alone.
    bool has_spectral_gate{false};
    std::span<const std::uint8_t> spectral_frame_mask{};
    // M19 retained-loudness-block union; if absent, mark whole-scope
    // width fallback. Presence of an empty array means no content.
    bool has_width_gate{false};
    std::span<const TonalStereoRetainedInterval> retained_width_blocks{};
    // 0=MISSING, 1=ELIGIBLE, 2=LOW_SUPPORT, from an *external* support
    // owner. Absent means no normalized band fractions are authorized.
    std::span<const std::uint8_t> authoritative_band_eligibility{};
};

enum class TonalStereoMetricState : std::uint8_t {
    AVAILABLE,
    NOT_APPLICABLE,
    INSUFFICIENT_CONTENT,
    UNDEFINED_DENOMINATOR,
    UNAVAILABLE_AUTHORITATIVE_SUPPORT
};

struct TonalStereoValue final {
    std::optional<double> value{};
    TonalStereoMetricState state{
        TonalStereoMetricState::UNAVAILABLE_AUTHORITATIVE_SUPPORT};
};

struct TonalBandMeasurement final {
    int band_index{0};
    double center_hz{0};
    double low_hz{0};
    double high_hz{0};
    bool fully_supported{false};
    double measured_linear_energy{0.0};
    // No zero, placeholder fraction, or automatic support inference.
    TonalStereoValue normalized_fraction{};
};

struct CorrelationMeasurement final {
    std::uint64_t absolute_window_start{0};
    TonalStereoValue correlation{};
};

struct TonalStereoAnalysisResult final {
    std::int64_t scope_begin_frame{0};
    std::int64_t scope_frame_count{0};
    std::int64_t sample_rate_hz{0};
    std::size_t channel_count{0};
    audio::FrameDomainId frame_domain{
        audio::FrameDomainId::SOURCE_PROCESSING_RATE};
    std::uint64_t complete_spectral_windows{0};
    std::uint64_t selected_spectral_windows{0};
    bool spectral_all_frames_fallback{false};
    bool width_all_scope_fallback{false};
    // One canonical mean-channel linear PSD, not a second FFT.
    std::vector<double> mean_linear_psd;
    std::vector<TonalBandMeasurement> primary_bands_1_12;
    TonalStereoMetricState spectrum_state{
        TonalStereoMetricState::INSUFFICIENT_CONTENT};
    TonalStereoValue width_ratio{};
    TonalStereoValue correlation_p10{};
    TonalStereoValue correlation_p50{};
    TonalStereoValue correlation_p90{};
    std::vector<CorrelationMeasurement> correlation_grid100{};
};

// Uses the existing AudioBufferView absolute-domain subview. Native,
// bounded-window FFT/PSD accumulation; no per-STFT-frame PSD history.
// Width and Grid100 correlation reuse the canonical M20 private math.
// Explicit M19/Phase-6 gate inputs are borrowed for this synchronous
// call, never computed, stored, or invented by M20.
[[nodiscard]] core::Result<TonalStereoAnalysisResult>
measure_tonal_stereo_scope(
    const audio::AudioBufferView& resource,
    core::FrameIndex absoluteScopeBegin,
    core::FrameCount scopeFrames,
    audio::FrameDomainId expectedDomain,
    const TonalStereoTrustedSupport& supplied = {});

} // namespace rgsml::analysis
