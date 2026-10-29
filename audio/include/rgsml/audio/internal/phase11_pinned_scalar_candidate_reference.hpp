#pragma once

// Phase-11 unqualified REFERENCE bridge: 6 x 6 SOURCE->MASTER strict scalar
// with externally pinned RGSMSR1 bytes. It NEVER registers a product SRC,
// loads a runtime kernel, or grants a release/certification/manifest gate.
// A candidate checksum must ultimately come from independently approved
// proof evidence; synthetic test hashes are not product admission.
#include <rgsml/audio/internal/phase11_rgsmsr1_candidate_asset.hpp>
#include <rgsml/audio/internal/phase11_strict_scalar_reference.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <limits>
#include <span>
#include <string_view>

namespace rgsml::audio::internal {
[[nodiscard]] inline core::Result<void>
render_phase11_pinned_scalar_reference(
    std::span<const double> fullSource,
    std::span<double> output,
    std::int64_t absoluteOutputStart,
    std::int64_t sourceRateHz,
    std::int64_t targetMasterRateHz,
    std::uint32_t expectedFrozenT,
    std::span<const std::uint8_t> candidateBytes,
    std::string_view independentlyPinnedSha256)
{
    using Result=core::Result<void>;
    constexpr std::array<std::int64_t,6> approved{
        44100,48000,88200,96000,176400,192000};
    const auto supported=[&](std::int64_t hz) {
        for(const auto rate:approved)
            if(hz==rate)return true;
        return false;
    };
    if(!supported(sourceRateHz)||!supported(targetMasterRateHz))
        return Result::failure(phase11_candidate_detail::invalid(
            "Phase11 scalar reference refuses unsupported source/MASTER rate"));
    const auto common=std::gcd(sourceRateHz,targetMasterRateHz);
    const auto L=targetMasterRateHz/common;
    const auto M=sourceRateHz/common;
    if(sourceRateHz==targetMasterRateHz) {
        if(!candidateBytes.empty()||!independentlyPinnedSha256.empty())
            return Result::failure(phase11_candidate_detail::invalid(
                "Phase11 identity cannot load a filter or hash"));
        return render_phase11_strict_scalar_reference(
            fullSource,output,absoluteOutputStart,1,1,{});
    }
    if(L<=0||M<=0||L>std::numeric_limits<std::uint32_t>::max()
       ||M>std::numeric_limits<std::uint32_t>::max())
        return Result::failure(phase11_candidate_detail::invalid(
            "Phase11 conversion ratio cannot be represented"));
    auto parsed=parse_phase11_rgsmsr1_pinned_candidate_bits(
        candidateBytes,static_cast<std::uint32_t>(L),
        static_cast<std::uint32_t>(M),expectedFrozenT,
        independentlyPinnedSha256);
    if(!parsed)return Result::failure(*parsed.error());
    return render_phase11_strict_scalar_reference(
        fullSource,output,absoluteOutputStart,L,M,*parsed.value());
}
} // namespace rgsml::audio::internal
