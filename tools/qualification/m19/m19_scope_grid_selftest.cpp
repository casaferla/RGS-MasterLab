// M19 private numerical preproduction: independently frozen temporal-grid
// fixtures from the Oct-04 mathematical preaudit, without any M19 DSP.
#include "internal/m19_scope_grid_candidate.hpp"
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace m19 = rgsml::analysis::internal::m19;

void require(bool b)
{
    if (!b) throw std::runtime_error("M19_SCOPE_GRID_REFERENCE_MISMATCH");
}

int main()
{
    constexpr std::array<std::uint32_t,6> clocks{
        44100U, 48000U, 88200U, 96000U, 176400U, 192000U};
    constexpr std::array<std::uint64_t,6> window{
        17640U, 19200U, 35280U, 38400U, 70560U, 76800U};
    constexpr std::array<std::uint64_t,6> hop{
        4410U,4800U,8820U,9600U,17640U,19200U};
    for (std::size_t i = 0; i < clocks.size(); ++i) {
        auto duration = m19::block_400ms_frames(clocks[i]);
        require(duration && *duration==window[i]);
        for (std::uint64_t j=0;j<100U;++j) {
            const auto step=m19::relative_100ms_grid_start(j,clocks[i]);
            require(step && *step==j*hop[i]);
            const auto absolute=m19::absolute_block_start(j,clocks[i],12345U);
            require(absolute && *absolute==12345U+*step);
        }
        require(!m19::complete_block_fits(0U, clocks[i], window[i]-1U));
        require(m19::complete_block_fits(0U, clocks[i], window[i]));
        require(!m19::complete_block_fits(1U, clocks[i],
                                          window[i]+hop[i]-1U));
        require(m19::complete_block_fits(1U, clocks[i], window[i]+hop[i]));
        require(!m19::complete_block_fits(2U, clocks[i],
                                          window[i]+hop[i]*2U-1U));
        require(m19::complete_block_fits(2U, clocks[i],
                                         window[i]+hop[i]*2U));
    }

    // 44,105 Hz is a DIAGNOSTIC (not an authorized K-weighting bank).
    // Repeated rounded 4410-frame additions drift from frozen absolute-grid law.
    constexpr std::array<std::uint64_t,11> irregular{
        0U,4410U,8821U,13232U,17642U,22052U,
        26463U,30874U,35284U,39694U,44105U};
    auto b=m19::block_400ms_frames(44105U);
    require(b && *b==17642U);
    for (std::uint64_t j=0;j<irregular.size();++j) {
        auto offset=m19::relative_100ms_grid_start(j,44105U);
        require(offset && *offset==irregular[j]);
        auto absolute=m19::absolute_block_start(j,44105U,12345U);
        require(absolute && *absolute==12345U+irregular[j]);
    }
    require(!m19::complete_block_fits(6U,44105U,26462U+17642U));
    require(m19::complete_block_fits(6U,44105U,26463U+17642U));

    // Exact integer ties to even; never rely on ambient FP rounding mode.
    require(m19::checked_round_ties_even_ratio(1U,5U,10U)==0U);
    require(m19::checked_round_ties_even_ratio(3U,5U,10U)==2U);
    require(m19::checked_round_ties_even_ratio(5U,5U,10U)==2U);
    require(m19::checked_round_ties_even_ratio(7U,5U,10U)==4U);
    require(m19::checked_round_ties_even_ratio(9U,5U,10U)==4U);

    constexpr auto max=(std::numeric_limits<std::uint64_t>::max)();
    require(!m19::relative_100ms_grid_start(max,192000U));
    require(!m19::absolute_block_start(1U,48000U,max));
    require(!m19::absolute_block_start(1U,0U,0U));
    require(!m19::checked_round_ties_even_ratio(3U,4U,0U));
    return 0;
}
