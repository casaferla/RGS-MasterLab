#pragma once

// Frame-domain validation is independent of which FIR COEFFICIENTS were
// used to render SOURCE -> MASTER. The immutable M17 result is already
// checked by TerminalCandidateGate. At installation, the playback adapter
// supplies only the shared half-open-ceil coordinate policy, NOT the Q0
// coefficients or an alternate audio render.
#include <rgsml/audio/internal/phase11_frozen_rgsmsr1_pins.hpp>
#include <rgsml/audio/playback_sample_rate_adapter.hpp>

#include <cstdint>
#include <numeric>
#include <string_view>

namespace rgsml::app::internal {

[[nodiscard]] inline bool accepted_terminal_src_frame_map_provenance(
    core::SampleRate source_rate,core::SampleRate master_rate,
    core::FrameCount source_frames,core::FrameCount master_frames,
    std::string_view accepted_fir_sha,
    const audio::PlaybackSampleRateAdapter& coordinate_adapter) noexcept
{
    const auto fin=source_rate.value();
    const auto fout=master_rate.value();
    if(fin<=0||fout<=0||fin==fout||accepted_fir_sha.empty()
       ||coordinate_adapter.input_rate()!=source_rate
       ||coordinate_adapter.output_rate()!=master_rate
       ||coordinate_adapter.input_frame_count()!=source_frames
       ||coordinate_adapter.output_frame_count()!=master_frames)
        return false;

    // Legacy/provisional Playback SRC and its own frame mapper: unchanged.
    if(accepted_fir_sha==coordinate_adapter.kernel_sha256())return true;

    // Original authenticated Phase-11 Final SRC uses a DIFFERENT SHA,
    // but the SAME exact rational ceil-frame timeline. Only a source-pinned
    // original SHA for the precise L/M ratio may share that mapping.
    // The loader has already authenticated the actual original bytes.
    const auto common=std::gcd(fin,fout);
    const auto* pin=audio::internal::phase11_frozen_original_fir_pin(
        static_cast<std::uint32_t>(fout/common),
        static_cast<std::uint32_t>(fin/common));
    return pin&&accepted_fir_sha==pin->sha256;
}

} // namespace rgsml::app::internal
