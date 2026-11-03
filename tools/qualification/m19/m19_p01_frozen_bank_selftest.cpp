// M19 P01 frozen six-rate coefficient bank mechanical smoke.
// NOT a new K filter design, no runtime coefficient generation and no Gate G.
#include "internal/m19_k_weighting_frozen_banks.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace m19=rgsml::analysis::internal::m19;

static_assert(sizeof(double)==8U);
static_assert(std::numeric_limits<double>::is_iec559);
static_assert(std::endian::native == std::endian::little);
static_assert(m19::kFrozenKWeightingBanksCandidate.size()==6U);
static_assert(m19::find_frozen_k_weighting_bank_candidate(44100U)->sample_rate_hz == 44100U);
static_assert(m19::find_frozen_k_weighting_bank_candidate(48000U)->sample_rate_hz == 48000U);
static_assert(m19::find_frozen_k_weighting_bank_candidate(192000U)->sample_rate_hz == 192000U);
static_assert(m19::find_frozen_k_weighting_bank_candidate(0U) == nullptr);
static_assert(m19::find_frozen_k_weighting_bank_candidate(32000U) == nullptr);
static_assert(m19::find_frozen_k_weighting_bank_candidate(44105U) == nullptr);
static_assert(m19::find_frozen_k_weighting_bank_candidate(384000U) == nullptr);

// The 48-kHz frozen physical bank MUST reproduce the original literal
// coefficient bytes already used in the source-scope candidate. No redesign.
constexpr auto* anchor=m19::find_frozen_k_weighting_bank_candidate(48000U);
static_assert(anchor->shelf_b0_b1_b2_a0_a1_a2[0]==1.53512485958697);
static_assert(anchor->shelf_b0_b1_b2_a0_a1_a2[1]==-2.69169618940638);
static_assert(anchor->shelf_b0_b1_b2_a0_a1_a2[2]==1.19839281085285);
static_assert(anchor->shelf_b0_b1_b2_a0_a1_a2[4]==-1.69065929318241);
static_assert(anchor->shelf_b0_b1_b2_a0_a1_a2[5]==0.73248077421585);
static_assert(anchor->rlb_b0_b1_b2_a0_a1_a2[0]==1.0);
static_assert(anchor->rlb_b0_b1_b2_a0_a1_a2[1]==-2.0);
static_assert(anchor->rlb_b0_b1_b2_a0_a1_a2[2]==1.0);
static_assert(anchor->rlb_b0_b1_b2_a0_a1_a2[4]==-1.99004745483398);
static_assert(anchor->rlb_b0_b1_b2_a0_a1_a2[5]==0.99007225036621);
static_assert(anchor->canonical_le_sha256 ==
    std::string_view{"d770751cb4aec9759a6fd27f277672b3ddd147ce156002ad0f013d42372875bd"});

void require(bool value) {
    if (!value) throw std::runtime_error("M19_P01_FROZEN_K_BANK_FAILURE");
}

bool stable_section(const std::array<double,6>& section)
{
    for (double x : section) {
        if (!std::isfinite(x)) return false;
    }
    const double a0=section[3],a1=section[4],a2=section[5];
    // Schur stability of normalized real-coefficient second-order pole pair.
    return a0==1.0 && a2 < 1.0 && a2 > -1.0
        && (1.0 + a1 + a2) > 0.0 && (1.0 - a1 + a2) > 0.0;
}

int main() {
    constexpr std::array<std::uint32_t,6> rates{
        44100U,48000U,88200U,96000U,176400U,192000U};
    constexpr std::array<std::string_view,6> hashes{
        "7fd4ee385758b5a93cd7f9cb90cf6cf53a427923ff5e3169afa6b088c1f0c3a3",
        "d770751cb4aec9759a6fd27f277672b3ddd147ce156002ad0f013d42372875bd",
        "a7be030ffe9de49fa0851243d56d3a0e80e40caea972dee522a742483c64725d",
        "6679b495d85e09150f82a285521987a072fe4dc22fd0ba2d85bb8188f673c95d",
        "e5d17a6f3fa27b991fcb438c03ee17e19b0ed07dfa56769c8e06761ef04cf56b",
        "956e5c393292be0bfe46a87011c3353540ef5305a4bd6ead11a52da57e7e94dc"};
    for (std::size_t i=0;i<rates.size();++i) {
        const auto* bank=m19::find_frozen_k_weighting_bank_candidate(rates[i]);
        require(bank!=nullptr);
        require(bank->sample_rate_hz==rates[i]);
        require(bank->canonical_le_sha256==hashes[i]);
        require(bank->member_id.starts_with(
            "rgsml.kweight.bs1770-5.anchor48-blt.v1.fs"));
        require(stable_section(bank->shelf_b0_b1_b2_a0_a1_a2));
        require(stable_section(bank->rlb_b0_b1_b2_a0_a1_a2));
        const auto& high=bank->rlb_b0_b1_b2_a0_a1_a2;
        // RLB numerator is exactly constant*(1-z^-1)^2 by the PO
        // fraction-preserving mapping, for all six rates.
        require(high[0]==high[2] && high[1]==-2.0*high[0]);
        if (rates[i]!=48000U) {
            require(bank->shelf_b0_b1_b2_a0_a1_a2[0]
                   != anchor->shelf_b0_b1_b2_a0_a1_a2[0]);
        }
    }
    return 0;
}
