#pragma once
// Private M18 preproduction render identity. NOT a source/file checksum,
// provenance certificate, persisted project format or product export token.
// Versions are semantic constants, not generated from a compiled output.
// The frozen result includes seed ONLY for PCM24: Float32 is dither-free.
#include "m18_wav_candidate.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace rgsml::audio::internal {
inline constexpr std::string_view kM18TpdfVersion =
    "rgsml.m18.tpdf.philox4x32-10/1";
inline constexpr std::string_view kM18Pcm24QuantizerVersion =
    "rgsml.m18.pcm24-rn64-rne/1";
inline constexpr std::string_view kM18Float32ConverterVersion =
    "rgsml.m18.float32-rne/1";
inline constexpr std::string_view kM18ContainerVersion =
    "rgsml.m18.wav-candidate/1";

struct M18DeliveryIdentity {
    M18WavEncoding encoding;
    std::uint32_t master_rate;
    std::uint32_t channels;
    std::uint64_t first_absolute_output_frame;
    std::uint64_t frame_count;
    std::optional<std::uint64_t> dither_seed;
    std::string_view dither_version;
    std::string_view converter_version;
    std::string_view container_version;

    [[nodiscard]] friend constexpr bool operator==(
        const M18DeliveryIdentity&,
        const M18DeliveryIdentity&) noexcept = default;
};

[[nodiscard]] constexpr std::optional<M18DeliveryIdentity>
m18_make_delivery_identity(M18WavSpec spec,
                           std::uint64_t first_absolute_output_frame=0U) noexcept
{
    const auto p=m18_plan_wav(spec);
    if(p.status!=M18WavStatus::OK ||
        (spec.frame_count!=0U && spec.frame_count-1U>
            UINT64_MAX-first_absolute_output_frame))
        return std::nullopt;
    const bool pcm=spec.encoding==M18WavEncoding::PCM24;
    return M18DeliveryIdentity{
        spec.encoding,spec.sample_rate,spec.channels,
        first_absolute_output_frame,spec.frame_count,
        pcm ? std::optional<std::uint64_t>{spec.dither_seed}
            : std::nullopt,
        pcm?kM18TpdfVersion:std::string_view{},
        pcm?kM18Pcm24QuantizerVersion:kM18Float32ConverterVersion,
        kM18ContainerVersion};
}

static_assert(m18_make_delivery_identity(
    {M18WavEncoding::PCM24,48000U,2U,1U,123U})
    ->dither_seed.value()==123U);
static_assert(!m18_make_delivery_identity(
    {M18WavEncoding::IEEE_FLOAT32,48000U,2U,1U,123U})
    ->dither_seed.has_value());

} // namespace rgsml::audio::internal
