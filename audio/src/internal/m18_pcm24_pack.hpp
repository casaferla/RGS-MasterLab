#pragma once
// L1-M18 preproduction only: exact lossless byte representation of a
// PRE-VALIDATED signed PCM24 integer code. NOT a quantizer or dither engine.
// Outside [-2^23,2^23-1] is rejected; never wrap, saturate or clamp.
// Does not touch M17, PCM source samples or any output writer.

#include <array>
#include <cstdint>
#include <optional>

namespace rgsml::audio::internal {

using M18Pcm24Bytes = std::array<std::uint8_t, 3>;

[[nodiscard]] constexpr std::optional<M18Pcm24Bytes>
m18_pack_pcm24_checked(std::int64_t code) noexcept
{
    if (code < -INT64_C(8388608) || code > INT64_C(8388607))
        return std::nullopt;
    // Conversion from signed to unsigned is defined modulo 2^32.
    // The three low bytes are the exact two's-complement PCM24 payload.
    const auto bits = static_cast<std::uint32_t>(code);
    return M18Pcm24Bytes{{
        static_cast<std::uint8_t>(bits),
        static_cast<std::uint8_t>(bits >> 8),
        static_cast<std::uint8_t>(bits >> 16)
    }};
}

[[nodiscard]] constexpr std::int32_t
m18_decode_pcm24_for_test(M18Pcm24Bytes packed) noexcept
{
    // Explicit sign extension; no implementation-defined signed shifts.
    const std::uint32_t bits=static_cast<std::uint32_t>(packed[0])
        | (static_cast<std::uint32_t>(packed[1]) << 8)
        | (static_cast<std::uint32_t>(packed[2]) << 16);
    const std::int64_t signedCode=(bits & 0x00800000U)
        ? static_cast<std::int64_t>(bits) - INT64_C(16777216)
        : static_cast<std::int64_t>(bits);
    return static_cast<std::int32_t>(signedCode);
}

static_assert(m18_pack_pcm24_checked(0).value()
    == M18Pcm24Bytes{{0U,0U,0U}});
static_assert(m18_pack_pcm24_checked(-1).value()
    == M18Pcm24Bytes{{0xFFU,0xFFU,0xFFU}});
static_assert(m18_pack_pcm24_checked(-8388608).value()
    == M18Pcm24Bytes{{0U,0U,0x80U}});
static_assert(m18_pack_pcm24_checked(8388607).value()
    == M18Pcm24Bytes{{0xFFU,0xFFU,0x7FU}});
static_assert(!m18_pack_pcm24_checked(INT64_C(8388608)));
static_assert(!m18_pack_pcm24_checked(-INT64_C(8388609)));

} // namespace rgsml::audio::internal
