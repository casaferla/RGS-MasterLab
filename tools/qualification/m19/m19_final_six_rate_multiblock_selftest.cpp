// M19 last isolated pre-merge regression: ALL six PO-frozen K banks,
// multi-block whole-scope processing, feeder cuts, false-region guards.
//
// No DSP, no QML, no independent M19 true peak, no production registry.
// These are deterministic candidate invariants, NOT Gate U/G or official
// ITU/EBU fixture qualification.
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
#include <vector>

namespace m19 = rgsml::analysis::internal::m19;
using Snapshot = m19::BasicLevelPreproductionSnapshot;

void require(bool p) {
    if (!p) throw std::runtime_error("M19_FINAL_PREMERGE_SIX_RATE_FAILURE");
}
void close(double a, double b, double tol=5e-5) {
    require(std::isfinite(a) && std::isfinite(b)
            && std::abs(a-b) <= tol);
}
void same_payload(const Snapshot& a, const Snapshot& b) {
    require(a.integrated_lufs==b.integrated_lufs);
    require(a.full_scope_rms_dbfs==b.full_scope_rms_dbfs);
    require(a.sample_peak_dbfs==b.sample_peak_dbfs);
    require(a.sample_peak_crest_db==b.sample_peak_crest_db);
    require(a.true_peak_db_tp==b.true_peak_db_tp);
    require(a.loudness_state==b.loudness_state);
    require(a.k_weighting_bank_member_id==b.k_weighting_bank_member_id);
    require(a.k_weighting_bank_sha256==b.k_weighting_bank_sha256);
    require(a.scope_frames==b.scope_frames);
    require(a.sample_rate_hz==b.sample_rate_hz);
    require(a.channel_count==b.channel_count);
}

Snapshot run(std::uint32_t fs, const std::vector<double>& l,
             const std::vector<double>& r, std::uint64_t origin,
             const std::vector<std::size_t>& pattern) {
    require(!l.empty() && (r.empty() || l.size()==r.size()));
    require(!pattern.empty());
    m19::BasicLevelPreproduction candidate(fs,r.empty()?1U:2U,
                                           origin,l.size());
    std::size_t consumed=0,index=0;
    while(consumed<l.size()) {
        const std::size_t chunk=std::min(pattern[index%pattern.size()],
                                         l.size()-consumed);
        require(chunk>0);
        const auto lc=std::span<const double>(l).subspan(consumed,chunk);
        const auto rc=r.empty()?std::span<const double>{}
            :std::span<const double>(r).subspan(consumed,chunk);
        require(candidate.push(lc,rc));
        consumed+=chunk;
        ++index;
    }
    const auto finalized=candidate.finalize();
    require(finalized.has_value());
    require(!candidate.finalize().has_value());
    require(!candidate.push(l));
    return *finalized;
}

int main() {
    constexpr std::array<std::uint32_t,6> clocks{
        44100U,48000U,88200U,96000U,176400U,192000U};
    constexpr std::array<std::uint64_t,2> origins{12345U,96000U};
    // Feeder partition does not alter 100ms energy buckets or K state.
    const std::array<std::vector<std::size_t>,5> plans{{
        {400000U},
        {4096U},
        {127U,1U,3U,511U,2U},
        {1001U,2048U,33U,7U,96U},
        {8191U,64U,4096U,251U}
    }};
    for (const auto fs:clocks) {
        const auto* bank=m19::find_frozen_k_weighting_bank_candidate(fs);
        require(bank!=nullptr);
        const auto b=m19::block_400ms_frames(fs);
        const auto step=m19::relative_100ms_grid_start(1U,fs);
        require(b.has_value() && step.has_value());
        // 8 full 100ms cells + incomplete final tail: exactly 5
        // completely contained 400ms blocks at indices j=0...4.
        const std::size_t frames=static_cast<std::size_t>(*step*8U+17U);
        for (std::uint64_t j=0;j<5U;++j)
            require(m19::complete_block_fits(j,fs,frames));
        require(!m19::complete_block_fits(5U,fs,frames));

        std::vector<double> left(frames,0.2);
        for(std::size_t i=0;i<frames;++i) {
            if(i>=static_cast<std::size_t>(*step)*4U)
                left[i]=0.1;
        }
        auto reference=run(fs,left,{},origins[0],plans[0]);
        require(reference.integrated_lufs.has_value());
        require(reference.loudness_state==
                m19::LoudnessPreproductionState::available);
        require(reference.k_weighting_bank_member_id==bank->member_id);
        require(reference.k_weighting_bank_sha256==bank->canonical_le_sha256);
        require(!reference.true_peak_db_tp);
        for (const auto& cuts:plans) {
            auto fragmented=run(fs,left,{},origins[0],cuts);
            same_payload(fragmented,reference);
        }
        auto relocated=run(fs,left,{},origins[1],plans[2]);
        same_payload(relocated,reference);
        require(relocated.scope_begin_frame==origins[1]);

        // True PCM amplitude x2, both level and Integrated loudness
        // increase by +20log10(2). Gate membership remains active here.
        auto louder=left;
        for(double& value:louder)value*=2.0;
        auto twice=run(fs,louder,{},origins[1],plans[3]);
        require(twice.integrated_lufs.has_value());
        close(*twice.integrated_lufs-*reference.integrated_lufs,
              20.0*std::log10(2.0));
        close(*twice.full_scope_rms_dbfs-*reference.full_scope_rms_dbfs,
              20.0*std::log10(2.0),5e-12);
        close(*twice.sample_peak_dbfs-*reference.sample_peak_dbfs,
              20.0*std::log10(2.0),5e-12);
        close(*twice.sample_peak_crest_db,
              *reference.sample_peak_crest_db,5e-12);

        auto right=left;
        for(double& value:right)value=-value;
        auto stereo=run(fs,left,right,origins[0],plans[4]);
        require(stereo.integrated_lufs.has_value());
        close(*stereo.integrated_lufs-*reference.integrated_lufs,
              10.0*std::log10(2.0));
        close(*stereo.full_scope_rms_dbfs,*reference.full_scope_rms_dbfs,5e-12);
        close(*stereo.sample_peak_dbfs,*reference.sample_peak_dbfs,5e-12);
        close(*stereo.sample_peak_crest_db,*reference.sample_peak_crest_db,5e-12);
        require(!stereo.true_peak_db_tp);

        // 17-frame trailing PCM is part of RMS/peak, but NEVER creates
        // a 6th loudness block. Invalid data here must invalidate scope.
        auto contaminated=left;
        contaminated.back()=(std::numeric_limits<double>::quiet_NaN)();
        m19::BasicLevelPreproduction poison(fs,1U,0U,frames);
        require(!poison.push(contaminated));
        require(!poison.finalize().has_value());
        // A declined malformed chunk cannot partially advance state.
        require(poison.push(left));
        auto clean=poison.finalize();
        require(clean.has_value());
        same_payload(*clean,reference);
    }
    return 0;
}
