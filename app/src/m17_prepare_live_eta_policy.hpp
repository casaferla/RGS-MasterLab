#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

namespace rgsml::app {

// First-run live Prepare ETA: observational and always provisional.
// NO previous completed full Prepare, automatic calibration, PCM, device
// approval or user profile is needed to produce an estimate. Missing
// evidence returns N/A rather than inventing a total or a fake M16
// progress denominator. All returned bounds describe remaining WHOLE
// Prepare time, not current-stage progress.
enum class M17PrepareLivePhase {
    FINAL_SRC, M17_CORE, M16_PRIMARY, VERIFICATION_TRIM,
    M16_SECONDARY, FINISHING
};
struct M17PrepareLiveEvidence {
    M17PrepareLivePhase phase{M17PrepareLivePhase::FINAL_SRC};
    std::int64_t phase_elapsed_ms{0};
    double phase_fraction{0.0};
    bool phase_fraction_known{false};
    std::int64_t observed_m17_ms{0};
    std::int64_t observed_primary_m16_ms{0};
    // Optional bounded on-host DSP CPU probe, never a qualified full ETA.
    std::optional<std::pair<std::int64_t,std::int64_t>>
        provisional_dsp_us;
};
struct M17PrepareLiveRemaining {
    std::int64_t low_ms{0},high_ms{0};
};

[[nodiscard]] inline std::optional<M17PrepareLiveRemaining>
m17_provisional_live_remaining(const M17PrepareLiveEvidence& x) noexcept
{
    if(x.phase_elapsed_ms<0)return std::nullopt;
    constexpr long double maxMs=86'400'000.0L;
    const auto bounded=[](long double ms)->std::int64_t {
        if(!std::isfinite(ms)||ms<=0)return 0;
        return static_cast<std::int64_t>(
            std::min(ms,maxMs));
    };
    const auto band=[&](long double low,long double high)
        ->std::optional<M17PrepareLiveRemaining> {
        if(!std::isfinite(low)||!std::isfinite(high)
           ||high<0||low>maxMs)return std::nullopt;
        const auto l=bounded(low),h=std::max(l,bounded(high));
        return M17PrepareLiveRemaining{l,h};
    };
    const auto spent=static_cast<long double>(x.phase_elapsed_ms);
    const bool fractionValid=x.phase_fraction_known
        &&std::isfinite(x.phase_fraction)
        &&x.phase_fraction>=0.0 &&x.phase_fraction<=1.0;
    const auto f=fractionValid
        ?static_cast<long double>(x.phase_fraction):0.0L;

    switch(x.phase) {
    case M17PrepareLivePhase::FINAL_SRC: {
        if(!fractionValid||f<0.025L||spent<2000)return std::nullopt;
        const auto left=spent*(1.0L-f)/f;
        // Remaining future M17+M16 is unmeasured on a first pass.
        // No fake precision: wide, explicit provisional bounds.
        const bool calibrated=x.provisional_dsp_us
            &&x.provisional_dsp_us->first>0
            &&x.provisional_dsp_us->second
                >=x.provisional_dsp_us->first;
        const auto low=calibrated
            ?x.provisional_dsp_us->first/1000.0L*0.7L
            :spent*0.3L;
        const auto high=calibrated
            ?x.provisional_dsp_us->second/1000.0L*2.2L
            :spent*3.0L+50'000.0L;
        return band(left*0.85L+low,left*1.2L+high);
    }
    case M17PrepareLivePhase::M17_CORE: {
        if(!fractionValid||f<0.025L||spent<2000)return std::nullopt;
        const auto projected=spent/f;
        const auto left=std::max(0.0L,projected-spent);
        // Primary M16 does not yet have a measured progress counter:
        // the 0.45x CORE term is only an explicitly provisional prior.
        const auto verifier=projected*0.45L;
        const auto uncertainty=0.65L*(1.0L-f)+0.20L;
        const auto mid=left+verifier+4'000.0L;
        const auto spread=left*uncertainty
            +verifier*(0.60L+0.35L*(1.0L-f))+5'000.0L;
        return band(std::max(0.0L,mid-spread),mid+spread);
    }
    case M17PrepareLivePhase::M16_PRIMARY:
    case M17PrepareLivePhase::M16_SECONDARY: {
        // M16 supplies no per-frame measurement here. Anchor a provisional
        // estimate only in the preceding actual M17 or primary M16 timing,
        // not a made-up 30 s default or a false M16 percentage.
        const auto nominal=x.phase==M17PrepareLivePhase::M16_SECONDARY
            ?static_cast<long double>(x.observed_primary_m16_ms)
            :x.observed_m17_ms>0
                ?std::max(15'000.0L,
                    static_cast<long double>(x.observed_m17_ms)*0.48L)
                :0.0L;
        if(nominal<=0)return std::nullopt;
        const auto projected=std::max(nominal,spent*1.22L);
        const auto left=std::max(0.0L,projected-spent);
        const auto mid=left+3'000.0L;
        const auto spread=std::max(2'000.0L,
            left*(0.36L*(1.0L-std::min(1.0L,spent/projected))
                +0.12L)+2'000.0L);
        return band(std::max(0.0L,mid-spread),mid+spread);
    }
    case M17PrepareLivePhase::VERIFICATION_TRIM: {
        // A trim is EXTRA work discovered after the primary M16 pass.
        // No invented trim progress. Anchor a broad second-pass envelope
        // in the actual primary measurement; allow the high end to grow
        // when an unexpectedly long trim is still running.
        if(x.observed_primary_m16_ms<=0)return std::nullopt;
        const auto primary=static_cast<long double>(
            x.observed_primary_m16_ms);
        return band(0.0L,
            2.0L*primary+std::max(30'000.0L,spent*1.5L));
    }
    case M17PrepareLivePhase::FINISHING:
        return band(0.0L,8'000.0L);
    }
    return std::nullopt;
}

} // namespace rgsml::app
