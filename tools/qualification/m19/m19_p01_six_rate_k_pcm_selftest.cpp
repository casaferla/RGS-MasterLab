// M19 preproduction P01 six-rate frozen K-weighting *actual PCM* oracle.
// Independent numerical values were generated from exact-rational PO P01
// coefficient derivation + separate 48k-to-target two-section reference.
// These checks are NOT certification of compiler QTP or official EBU Gate G.
#include "internal/m19_basic_level_candidate.hpp"
#include "internal/m19_k_weighting_frozen_banks.hpp"
#include "internal/m19_scope_grid_candidate.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace m19=rgsml::analysis::internal::m19;

void require(bool condition)
{
    if(!condition)throw std::runtime_error("M19_P01_SIX_RATE_K_PCM_FAILURE");
}
void near(double actual,double target,double tolerance=0.00005)
{
    require(std::isfinite(actual)&&std::isfinite(target)
            &&std::abs(actual-target)<tolerance);
}
using Snapshot=m19::BasicLevelPreproductionSnapshot;

Snapshot analyze(std::uint32_t fs,
                 const std::vector<double>& left,
                 const std::vector<double>& right,
                 std::uint64_t s0, bool fragmented)
{
    require(!left.empty() && (right.empty()||right.size()==left.size()));
    m19::BasicLevelPreproduction calc(
        fs,right.empty()?1U:2U,s0,left.size());
    if(fragmented) {
        constexpr std::array<std::size_t,8> cuts{
            1U,3U,7U,127U,4096U,2U,511U,64U};
        for(std::size_t pos=0,index=0;pos<left.size();++index) {
            const std::size_t count=std::min(cuts[index%cuts.size()],
                                              left.size()-pos);
            auto l=std::span<const double>(left).subspan(pos,count);
            auto r=right.empty()?std::span<const double>{}
                :std::span<const double>(right).subspan(pos,count);
            require(calc.push(l,r));
            pos+=count;
        }
    } else {
        require(calc.push(left,right));
    }
    const auto result=calc.finalize();
    require(result.has_value());
    require(!calc.finalize().has_value());
    return *result;
}

int main()
{
    constexpr std::array<std::uint32_t,6> clocks{
        44100U,48000U,88200U,96000U,176400U,192000U};
    // Independent fixed first-400-ms 997-Hz, amplitude-0.125 sine oracles,
    // zero K state at logical scope start. Each input uses its REAL target Fs.
    constexpr std::array<double,6> toneLufs{
        -21.070566265290324,-21.071135026298090,
        -21.073279899827530,-21.073418250980165,
        -21.073936631402304,-21.073969594131775};
    // Independent exact-rational P01, 48k printed-decimal anchor, first
    // actual impulse at scope offset zero; these numbers must vary with Fs.
    constexpr std::array<double,6> impulseLufs{
        -39.34595955072482,-39.69495697662176,
        -42.239182752896575,-42.59771987346224,
        -45.19122952625317,-45.55454105285134};
    constexpr double pi=3.141592653589793238462643383279502884;
    for(std::size_t i=0;i<clocks.size();++i) {
        const std::uint32_t fs=clocks[i];
        const auto* frozen=m19::find_frozen_k_weighting_bank_candidate(fs);
        require(frozen!=nullptr);
        const auto block=m19::block_400ms_frames(fs);
        const auto hop=m19::relative_100ms_grid_start(1U,fs);
        require(block.has_value()&&hop.has_value());
        const std::size_t n=static_cast<std::size_t>(*block);
        std::vector<double> dc(n,1.0);
        const auto a=analyze(fs,dc,{},12345U,false);
        const auto b=analyze(fs,dc,{},12345U,true);
        require(a.k_weighting_bank_member_id==frozen->member_id);
        require(a.k_weighting_bank_sha256==frozen->canonical_le_sha256);
        require(a.integrated_lufs.has_value());
        near(*a.integrated_lufs,-26.2768703195911,0.00002);
        require(a.integrated_lufs==b.integrated_lufs);
        require(a.sample_peak_dbfs==b.sample_peak_dbfs);
        require(a.full_scope_rms_dbfs==b.full_scope_rms_dbfs);
        require(a.sample_peak_crest_db==b.sample_peak_crest_db);
        require(!a.true_peak_db_tp);
        near(*a.sample_peak_dbfs,0.0,1e-13);
        near(*a.full_scope_rms_dbfs,0.0,1e-13);

        // This is the P01 400-ms support law with NO padding.
        dc.pop_back();
        const auto shorter=analyze(fs,dc,{},0U,true);
        require(!shorter.integrated_lufs);
        require(shorter.loudness_state==
                m19::LoudnessPreproductionState::insufficient_duration);

        std::vector<double> pulse(n,0.0);
        pulse[0]=1.0;
        auto impulse=analyze(fs,pulse,{},96000U,true);
        require(impulse.integrated_lufs.has_value());
        near(*impulse.integrated_lufs,impulseLufs[i],0.00005);
        near(*impulse.sample_peak_dbfs,0.0,1e-13);
        near(*impulse.sample_peak_crest_db,
             10.0*std::log10(static_cast<double>(n)),1e-10);

        std::vector<double> sine(n,0.0);
        for(std::size_t sample=0;sample<n;++sample) {
            sine[sample]=0.125*std::sin(2*pi*997.0*
                    static_cast<double>(sample)/static_cast<double>(fs));
        }
        const auto tone=analyze(fs,sine,{},12345U,true);
        require(tone.integrated_lufs.has_value());
        near(*tone.integrated_lufs,toneLufs[i],0.00005);

        const auto duplicated=analyze(fs,dc,dc,12345U,false);
        // n-1 sample layout is still too short for loudness.
        require(!duplicated.integrated_lufs);
        std::vector<double> full(n,1.0),inverse(n,-1.0);
        const auto stereo=analyze(fs,full,inverse,96000U,true);
        require(stereo.integrated_lufs.has_value());
        near(*stereo.integrated_lufs-*a.integrated_lufs,
             10.0*std::log10(2.0),0.00005);
        near(*stereo.full_scope_rms_dbfs,*a.full_scope_rms_dbfs,1e-12);
        near(*stereo.sample_peak_dbfs,*a.sample_peak_dbfs,1e-12);
        near(*stereo.sample_peak_crest_db,0.0,1e-12);
        require(!stereo.true_peak_db_tp);
    }
    // No invention at non-P01 physical sample clocks.
    constexpr std::array<std::uint32_t,4> forbidden{
        0U,32000U,44105U,384000U};
    const std::vector<double> sample(2048U,0.25);
    for(const auto fs:forbidden) {
        m19::BasicLevelPreproduction invalid(fs,1U,0U,sample.size());
        require(!invalid.push(sample));
        require(!invalid.finalize().has_value());
    }
    return 0;
}
