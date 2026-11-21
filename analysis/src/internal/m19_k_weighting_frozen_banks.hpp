#pragma once
// M19 PREPRODUCTION-ONLY immutable K-weighting tables; NOT registered into
// production rgsml_analysis or the Phase-6 FeatureDefinitionRegistry.
//
// PRIMARY AUTHORITY: Product Owner ACCEPTED/FROZEN 2026-10-05:
// RGS_MasterLab_Phase6_Residual_Semantic_Closure_P01_P06_Product_Owner_Decision_
// ACCEPTED_FROZEN_2026-10-05_v1.0
// Google Drive document ID: 1rBiwPOz5lMGDMGuKyFNVzw9d9SIBhS3Znx3aWVr-eE4
// P01 policy ID: rgsml.kweight.bs1770-5.anchor48-blt.v1
//
// These values are transcribed verbatim as exact binary64 hexadecimal literals
// from the PO-frozen six rate members (12 doubles per member, two biquads).
// Non-48-kHz coefficients are RGS-derived fixed banks, NOT "official ITU
// coefficient banks"; 48 kHz preserves its frozen BS.1770-5 values.
// Derivation of coefficients during runtime is FORBIDDEN. A different sample
// rate is UNSUPPORTED: no fallback, resampling, or nearest-bank substitution.
// Mathematical freeze is NOT cross-platform or official Gate G qualification.

#include <array>
#include <cstdint>
#include <limits>
#include <string_view>

namespace rgsml::analysis::internal::m19 {

struct FrozenKWeightingBankCandidate final {
    std::uint32_t sample_rate_hz{};
    std::array<double,6> shelf_b0_b1_b2_a0_a1_a2{};
    std::array<double,6> rlb_b0_b1_b2_a0_a1_a2{};
    std::string_view member_id{};
    std::string_view canonical_le_sha256{};
};

static_assert(std::numeric_limits<double>::is_iec559,
              "Frozen P01 banks require IEEE-754 binary64");

inline constexpr std::array<FrozenKWeightingBankCandidate,6>
    kFrozenKWeightingBanksCandidate{{
    {44100U,
     {0x1.87e7c5de40831p+0, -0x1.535f7aaa89349p+1, 0x1.2b5a214edb1cdp+0, 0x1.0000000000000p+0, -0x1.a9f577e8e3201p+0, 0x1.6cf0d381d8adcp-1},
     {0x1.ffc65649072c9p-1, -0x1.ffc65649072c9p+0, 0x1.ffc65649072c9p-1, 0x1.0000000000000p+0, -0x1.fd3a39583a234p+0, 0x1.fa784beb457e4p-1},
     "rgsml.kweight.bs1770-5.anchor48-blt.v1.fs44100",
     "7fd4ee385758b5a93cd7f9cb90cf6cf53a427923ff5e3169afa6b088c1f0c3a3"},
    {48000U,
     {0x1.88fdf15b33df7p+0, -0x1.5889803022552p+1, 0x1.32c9df0a5fdf9p+0, 0x1.0000000000000p+0, -0x1.b0cf0c24e59d0p+0, 0x1.7707b85469635p-1},
     {0x1.0000000000000p+0, -0x1.0000000000000p+1, 0x1.0000000000000p+0, 0x1.0000000000000p+0, -0x1.fd73bffffffecp+0, 0x1.faeabfffffff8p-1},
     "rgsml.kweight.bs1770-5.anchor48-blt.v1.fs48000",
     "d770751cb4aec9759a6fd27f277672b3ddd147ce156002ad0f013d42372875bd"},
    {88200U,
     {0x1.8eb446dca7d1ap+0, -0x1.73d35974a2554p+1, 0x1.5c59b4c2c36dap+0, 0x1.0000000000000p+0, -0x1.d497b552d71d2p+0, 0x1.affdfc11fb63cp-1},
     {0x1.0094e52742676p+0, -0x1.0094e52742676p+1, 0x1.0094e52742676p+0, 0x1.0000000000000p+0, -0x1.fe9ca1ac33f9cp+0, 0x1.fd3a3a522ee84p-1},
     "rgsml.kweight.bs1770-5.anchor48-blt.v1.fs88200",
     "a7be030ffe9de49fa0851243d56d3a0e80e40caea972dee522a742483c64725d"},
    {96000U,
     {0x1.8f44799d2846ep+0, -0x1.7687a8bd7c3d0p+1, 0x1.60af615abe86ep+0, 0x1.0000000000000p+0, -0x1.d8197c120077dp+0, 0x1.b5fc0b1ddd96fp-1},
     {0x1.00a35dfe775a5p+0, -0x1.00a35dfe775a5p+1, 0x1.00a35dfe775a5p+0, 0x1.0000000000000p+0, -0x1.feb9782440633p+0, 0x1.fd73c0cd3d25ap-1},
     "rgsml.kweight.bs1770-5.anchor48-blt.v1.fs96000",
     "6679b495d85e09150f82a285521987a072fe4dc22fd0ba2d85bb8188f673c95d"},
    {176400U,
     {0x1.92313ef10a5dbp+0, -0x1.849d3b90d8f75p+1, 0x1.77ec7c47d40dap+0, 0x1.0000000000000p+0, -0x1.ea3e32ff119a1p+0, 0x1.d642ee2c7c2dap-1},
     {0x1.00ee0798494d6p+0, -0x1.00ee0798494d6p+1, 0x1.00ee0798494d6p+0, 0x1.0000000000000p+0, -0x1.ff4e320625d31p+0, 0x1.fe9ca1e0306dcp-1},
     "rgsml.kweight.bs1770-5.anchor48-blt.v1.fs176400",
     "e5d17a6f3fa27b991fcb438c03ee17e19b0ed07dfa56769c8e06761ef04cf56b"},
    {192000U,
     {0x1.927a58e047a3dp+0, -0x1.85fe39cc2e444p+1, 0x1.7a429a7d04f6ep+0, 0x1.0000000000000p+0, -0x1.ec0211c52ee99p+0, 0x1.d98523143df76p-1},
     {0x1.00f547a1dbef9p+0, -0x1.00f547a1dbef9p+1, 0x1.00f547a1dbef9p+0, 0x1.0000000000000p+0, -0x1.ff5ca20ecb4a1p+0, 0x1.feb9784f69211p-1},
     "rgsml.kweight.bs1770-5.anchor48-blt.v1.fs192000",
     "956e5c393292be0bfe46a87011c3353540ef5305a4bd6ead11a52da57e7e94dc"}
}};

[[nodiscard]] constexpr const FrozenKWeightingBankCandidate*
find_frozen_k_weighting_bank_candidate(std::uint32_t sample_rate_hz) noexcept
{
    for (const auto& bank : kFrozenKWeightingBanksCandidate) {
        if (bank.sample_rate_hz == sample_rate_hz) return &bank;
    }
    return nullptr;
}

} // namespace rgsml::analysis::internal::m19
