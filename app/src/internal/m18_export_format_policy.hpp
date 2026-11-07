#pragma once
// M18 private Product preflight. The verified M17 MASTER is always binary64.
// PCM24 requires a deterministic TPDF seed at final integer conversion;
// IEEE-F32 delivery never receives dither. Do not infer this from a GUI
// selection, and reject invalid output choices before reading Qt owners.
#include <rgsml/audio/wav_format.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <cstdint>
#include <optional>
#include <cmath>

namespace rgsml::app::internal {
[[nodiscard]] inline core::Status validate_m18_export_format(
    audio::WavSampleFormat format,
    std::optional<std::uint64_t> seed,
    double ceilingDbTp)
{
    if(!std::isfinite(ceilingDbTp) || ceilingDbTp < -12.0 || ceilingDbTp > 0.0)
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidArgument,"M18_EXPORT_CEILING_OUT_OF_RANGE"});
    if(format==audio::WavSampleFormat::PCM_S24 && seed)
        return core::Status::success();
    if(format==audio::WavSampleFormat::IEEE_F32 && !seed)
        return core::Status::success();
    return core::Status::failure(core::Error{
        core::ErrorCode::InvalidArgument,
        "M18_EXPORT_FORMAT_REQUIRES_EXACT_PCM24_TPDF_OR_UNDITHERED_FLOAT"});
}
} // namespace rgsml::app::internal
