// M19 private F07 K-filter range smoke, not production QTP/ITU Gate G.
// Exercises ZERO_STATE_AT_LOGICAL_ANALYSIS_SCOPE_START using actual PCM
// and the fixed 48k BS.1770 coefficient bank only.
#include "internal/m19_basic_level_candidate.hpp"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace m19=rgsml::analysis::internal::m19;
void require(bool x) {
    if(!x)throw std::runtime_error("M19_48K_GATE_RANGE_PCM_FAILURE");
}
void near(double a,double b,double eps=2e-5) {
    require(std::isfinite(a) && std::isfinite(b) &&
            std::abs(a-b)<=eps);
}
m19::BasicLevelPreproductionSnapshot measure(
    double amplitude,bool stereo=false,bool opposite=false,
    std::uint64_t origin=12345U,bool fragmented=false)
{
    constexpr std::size_t frames=19200U;
    std::vector<double> left(frames,amplitude);
    std::vector<double> right(frames,opposite?-amplitude:amplitude);
    m19::BasicLevelPreproduction analyzer(48000U,stereo?2U:1U,
                                          origin,frames);
    if(!fragmented) {
        require(analyzer.push(left,stereo?std::span<const double>{right}
                                    :std::span<const double>{}));
    } else {
        constexpr std::size_t cuts[]={1U,7U,2048U,3U,511U,2U,127U};
        std::size_t offset=0,index=0;
        while(offset<frames) {
            const std::size_t count=std::min(cuts[index%7U],frames-offset);
            const auto a=std::span<const double>(left).subspan(offset,count);
            const auto b=stereo?std::span<const double>(right).subspan(offset,count)
                                :std::span<const double>{};
            require(analyzer.push(a,b));
            offset+=count;
            ++index;
        }
    }
    auto snapshot=analyzer.finalize();
    require(snapshot.has_value());
    return *snapshot;
}
int main() {
    // The accepted 48 kHz DC first-scope 400-ms K energy is
    // 0.00276320412174399344 for unit gain; amplitudes 0.006 and 0.007
    // straddle the -70 LKFS absolute gate by a *safe* margin. They do NOT
    // claim one-ULP near-irrational-threshold QTP.
    const auto below=measure(0.006,false,false,12345U,true);
    const auto above=measure(0.007,false,false,12345U,true);
    require(!below.integrated_lufs);
    require(below.loudness_state==
            m19::LoudnessPreproductionState::no_absolute_gated_blocks);
    require(below.full_scope_rms_dbfs.has_value());
    near(*below.full_scope_rms_dbfs,20.0*std::log10(0.006),1e-12);
    require(above.integrated_lufs.has_value());
    near(*above.integrated_lufs,-26.2768703195911+
                               20.0*std::log10(0.007),2e-5);
    require(above.loudness_state==
            m19::LoudnessPreproductionState::available);
    require(!above.true_peak_db_tp);

    // Two times the signal magnitude must shift both Integrated and RMS
    // by 20log10(2) without inventing another metric or moving the gate.
    const auto twice=measure(0.014,false,false,96000U,false);
    require(twice.integrated_lufs.has_value());
    near(*twice.integrated_lufs-*above.integrated_lufs,
         20.0*std::log10(2.0),2e-5);
    near(*twice.full_scope_rms_dbfs-*above.full_scope_rms_dbfs,
         20.0*std::log10(2.0),2e-12);
    near(*twice.sample_peak_crest_db,0.0,1e-12);

    // Opposite signs in LEFT and RIGHT cannot cancel K-weighted power.
    // Global RMS and sample peak stay the same; Integrated adds 3.0103 LU.
    const auto opposite=measure(0.007,true,true,12345U,false);
    require(opposite.integrated_lufs.has_value());
    near(*opposite.integrated_lufs-*above.integrated_lufs,
         10.0*std::log10(2.0),2e-5);
    near(*opposite.full_scope_rms_dbfs,*above.full_scope_rms_dbfs,1e-12);
    near(*opposite.sample_peak_dbfs,*above.sample_peak_dbfs,1e-12);

    // Extremely small finite PCM must retain defined sample-peak/RMS,
    // while the under-threshold K result is ABSENT, not -Inf or fake LUFS.
    const double microscopic=std::ldexp(1.0,-600);
    const auto tiny=measure(microscopic);
    require(!tiny.integrated_lufs);
    require(tiny.loudness_state==
            m19::LoudnessPreproductionState::no_absolute_gated_blocks);
    near(*tiny.sample_peak_dbfs,20.0*std::log10(microscopic),2e-12);
    near(*tiny.full_scope_rms_dbfs,20.0*std::log10(microscopic),2e-12);

    // Grossly oversized finite PCM can overflow the current physical
    // K-filter binary64 recurrence. The diagnostic must reject the result
    // rather than fabricate a valid LUFS, -Inf, or clamped meter reading.
    const double enormous=std::ldexp(1.0,512);
    std::vector<double> large(19200U,enormous);
    m19::BasicLevelPreproduction invalid(48000U,1U,0U,large.size());
    require(!invalid.push(large));
    require(!invalid.finalize().has_value());
    return 0;
}
