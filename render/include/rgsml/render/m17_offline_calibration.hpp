#pragma once
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/true_peak_limiter_parameters.hpp>
#include <rgsml/render/true_peak_limiter_terminal_candidate.hpp>
#include <cstdint>

namespace rgsml::render {

// EXPERIMENTAL: an offline, non-audible calibration probe for the EXACT
// accelerated M17 + full-resource M16 verifier, never a substitute DSP.
// Run only in a cancellable, non-audio worker. No device, file or user track.
enum class M17CalibrationStimulus : std::uint8_t {
    QUIET_PROGRAMME=0, TRANSIENT_STRESS=1
};
struct M17OfflineCalibrationMeasurement final {
    core::SampleRate output_rate;
    core::FrameCount probe_frames;
    audio::ChannelLayout layout;
    double ceiling_db_tp{0.0};
    M17CalibrationStimulus stimulus{M17CalibrationStimulus::QUIET_PROGRAMME};
    std::int64_t m17_and_m16_us{0};
    std::int64_t m17_core_us{-1};
    std::int64_t m16_primary_us{-1};
    std::int64_t verification_trim_us{-1};
    std::int64_t m16_secondary_us{-1};
};

// Generates a deterministic, bounded test signal in the requested OUTPUT_RATE,
// measures the genuine M17 accelerated candidate + authoritative full M16
// (including trim/recheck when needed), then discards ALL PCM. Never writes
// to project, transport or final WAV. No Final Output SRC is benchmarked.
[[nodiscard]] core::Result<M17OfflineCalibrationMeasurement>
calibrate_m17_offline_once(
    core::SampleRate output_rate,
    audio::ChannelLayout layout,
    dsp::TruePeakLimiterParameters params,
    M17CalibrationStimulus stimulus,
    core::FrameCount probe_frames,
    const dsp::ModuleRegistry& registry,
    core::FrameCount maximum_block_frames,
    const TerminalRenderCancel& cancelled = {});

// Estimated compute time only (M17 + M16), NOT total Prepare/Render:
// excludes Final Output SRC, disk I/O, scheduling and WAV encoding.
// Caller must retain rates/channels/Ceiling/stimulus provenance and never
// project a rate using a measurement captured at another rate.
[[nodiscard]] core::Result<std::int64_t>
estimate_m17_m16_microseconds(
    const M17OfflineCalibrationMeasurement& calibration,
    core::SampleRate requested_output_rate,
    audio::ChannelLayout requested_layout,
    double requested_ceiling_db_tp,
    M17CalibrationStimulus requested_stimulus,
    core::FrameCount full_output_frames);

} // namespace rgsml::render
