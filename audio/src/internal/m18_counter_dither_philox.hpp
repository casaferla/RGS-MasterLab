#pragma once
// M18 PREPRODUCTION: deterministic, state-free counter-based dither source.
// Accepted/frozen 2026-09-12 semantics; corrected independent 2026-10-04
// Philox vectors. This helper is NOT a quantizer, file writer, exporter,
// M16 decoded-file qualifier, or M18 product/release authorization.
// Kept in audio/src/internal: no public API or M17 change.

#include <array>
#include <cstdint>

namespace rgsml::audio::internal {

using M18PhiloxWords = std::array<std::uint32_t, 4>;
using M18PhiloxKey = std::array<std::uint32_t, 2>;

[[nodiscard]] constexpr M18PhiloxWords m18_philox4x32_10_core(
    M18PhiloxWords counter, M18PhiloxKey key) noexcept
{
    constexpr std::uint32_t m0 = 0xD2511F53U;
    constexpr std::uint32_t m1 = 0xCD9E8D57U;
    constexpr std::uint32_t w0 = 0x9E3779B9U;
    constexpr std::uint32_t w1 = 0xBB67AE85U;
    for (int round = 0; round < 10; ++round) {
        const std::uint64_t p0 =
            static_cast<std::uint64_t>(counter[0]) * m0;
        const std::uint64_t p1 =
            static_cast<std::uint64_t>(counter[2]) * m1;
        counter = {{
            static_cast<std::uint32_t>((p1 >> 32) ^ counter[1] ^ key[0]),
            static_cast<std::uint32_t>(p1),
            static_cast<std::uint32_t>((p0 >> 32) ^ counter[3] ^ key[1]),
            static_cast<std::uint32_t>(p0)
        }};
        // Round 0 uses the initial key; no increment after round 9.
        if (round != 9) {
            key[0] += w0;
            key[1] += w1;
        }
    }
    return counter;
}

[[nodiscard]] constexpr M18PhiloxWords m18_philox4x32_10(
    std::uint64_t ditherSeed, std::uint64_t absoluteOutputFrame,
    std::uint32_t channelIndex) noexcept
{
    return m18_philox4x32_10_core(
        {{
            static_cast<std::uint32_t>(absoluteOutputFrame),
            static_cast<std::uint32_t>(absoluteOutputFrame >> 32),
            channelIndex, 0x52475344U
        }},
        {{
            static_cast<std::uint32_t>(ditherSeed),
            static_cast<std::uint32_t>(ditherSeed >> 32)
        }});
}

[[nodiscard]] constexpr std::uint64_t m18_uniform53(
    std::uint32_t high, std::uint32_t low) noexcept
{
    return (static_cast<std::uint64_t>(high) << 21) | (low >> 11);
}

struct M18TpdfPair {
    double u0;
    double u1;
    [[nodiscard]] constexpr double dither_lsb() const noexcept
    {
        return u0 - u1;
    }
};

[[nodiscard]] constexpr M18TpdfPair m18_tpdf_from_words(
    M18PhiloxWords words) noexcept
{
    constexpr double inverse2pow53 = 0x1.0p-53;
    return {
        static_cast<double>(m18_uniform53(words[0], words[1]))
            * inverse2pow53,
        static_cast<double>(m18_uniform53(words[2], words[3]))
            * inverse2pow53
    };
}

[[nodiscard]] constexpr M18TpdfPair m18_tpdf(
    std::uint64_t ditherSeed, std::uint64_t absoluteOutputFrame,
    std::uint32_t channelIndex) noexcept
{
    return m18_tpdf_from_words(m18_philox4x32_10(
        ditherSeed, absoluteOutputFrame, channelIndex));
}

static_assert(m18_philox4x32_10_core({{0U, 0U, 0U, 0U}},
    {{0U, 0U}}) ==
    M18PhiloxWords{{0x6627E8D5U, 0xE169C58DU, 0xBC57AC4CU, 0x9B00DBD8U}},
    "Independent generic Philox zero-key KAT mismatch");

} // namespace rgsml::audio::internal
