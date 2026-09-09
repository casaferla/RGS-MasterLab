#pragma once

#include <array>
#include <cstdint>

namespace rgsml::tests::audio_golden {

// Derived independently from q * 2^-15, not from the production decoder.
inline constexpr std::array<std::uint64_t, 3> kRiffPcm16MonoExpectedBits{
    0xbff0000000000000ULL,
    0x0000000000000000ULL,
    0x3fefffc000000000ULL,
};

inline constexpr std::array<std::uint64_t, 1> kRf64F64ExpectedBits{
    0x8000000000000000ULL,
};

}  // namespace rgsml::tests::audio_golden
