#pragma once

#include <array>
#include <cstdint>

namespace rgsml::tests::gain_golden {

// Generated independently with gmpy2 2.3.1 / MPFR 4.2.2 at 256-bit
// precision by tests/oracles/gain/generate_gain_oracle.py. Values are the
// correctly rounded IEEE-754 binary64 results of x * 10^(gainDb/20).
inline constexpr std::array<std::uint64_t, 6> kInputBits{
    UINT64_C(0x3fe0000000000000),
    UINT64_C(0xbfd0000000000000),
    UINT64_C(0x3ff4000000000000),
    UINT64_C(0xc000000000000000),
    UINT64_C(0x0000000000000010),
    UINT64_C(0x8000000000000010),
};

struct GoldenVector final {
    double gain_db;
    std::uint64_t factor_bits;
    std::array<std::uint64_t, 6> output_bits;
};

inline constexpr std::array<GoldenVector, 4> kGoldenVectors{{
    {6.0, UINT64_C(0x3fffec982d5bb8af), {
        UINT64_C(0x3fefec982d5bb8af), UINT64_C(0xbfdfec982d5bb8af),
        UINT64_C(0x4003f3df1c59536e), UINT64_C(0xc00fec982d5bb8af),
        UINT64_C(0x0000000000000020), UINT64_C(0x8000000000000020)}},
    {-12.0, UINT64_C(0x3fd0137987dd704c), {
        UINT64_C(0x3fc0137987dd704c), UINT64_C(0xbfb0137987dd704c),
        UINT64_C(0x3fd41857e9d4cc5f), UINT64_C(0xbfe0137987dd704c),
        UINT64_C(0x0000000000000004), UINT64_C(0x8000000000000004)}},
    {-24.0, UINT64_C(0x3fb0270ac3f8a9fa), {
        UINT64_C(0x3fa0270ac3f8a9fa), UINT64_C(0xbf90270ac3f8a9fa),
        UINT64_C(0x3fb430cd74f6d478), UINT64_C(0xbfc0270ac3f8a9fa),
        UINT64_C(0x0000000000000001), UINT64_C(0x8000000000000001)}},
    {24.0, UINT64_C(0x402fb2a734897867), {
        UINT64_C(0x401fb2a734897867), UINT64_C(0xc00fb2a734897867),
        UINT64_C(0x4033cfa880d5eb40), UINT64_C(0xc03fb2a734897867),
        UINT64_C(0x00000000000000fe), UINT64_C(0x80000000000000fe)}},
}};

}  // namespace rgsml::tests::gain_golden
