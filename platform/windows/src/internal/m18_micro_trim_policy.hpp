#pragma once
// M18 optional user-authorized micro trim: strictly delivery-only.
// The original M17 verified binary64 Master and its frozen ceiling remain
// unchanged. No automatic, invisible or multi-pass reprocessing is allowed.
#include <rgsml/audio/wav_format.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace rgsml::platform::windows::internal {
inline constexpr double kM18MicroTrimDb=0.01;
// Pin the precisely rounded binary64 gain (10^(-0.01/20)).
inline constexpr double kM18MicroTrimLinearGain=0.9988493699365052;

// Verified M16 BS.1770 4x/12 FIR phases have absolute coefficient sum <3.
// By the triangle inequality, for ANY sequence of bounded sample errors
// and zero-padded edges, TP measurement delta <= 3*max_sample_error.
// No waveform-dependent guessing or arbitrary 0.01-dB acceptance window.
inline constexpr double kM18FrozenM16FirL1SafeBound=3.0;
[[nodiscard]] inline bool m18_micro_trim_eligible(
    double decodedTruePeakLinear,double ceilingDbTp,double maxSampleError,
    audio::WavSampleFormat format) noexcept
{
    if(format!=audio::WavSampleFormat::PCM_S24
       &&format!=audio::WavSampleFormat::IEEE_F32)return false;
    if(!std::isfinite(decodedTruePeakLinear)
       ||!std::isfinite(ceilingDbTp)
       ||!std::isfinite(maxSampleError)
       ||decodedTruePeakLinear<0.0 ||maxSampleError<=0.0
       ||ceilingDbTp< -12.0 ||ceilingDbTp>0.0)return false;
    const double nominal=std::pow(10.0,ceilingDbTp/20.0);
    const double tolerance=kM18FrozenM16FirL1SafeBound*maxSampleError;
    // One ULP allowance for finite double reductions/log conversion; the
    // audio discrepancy must remain bounded by actual per-sample encoding.
    return decodedTruePeakLinear>nominal
        && decodedTruePeakLinear<=nominal+tolerance
            +4.0*std::numeric_limits<double>::epsilon();
}
static_assert(kM18MicroTrimLinearGain>0.0
              &&kM18MicroTrimLinearGain<1.0);
} // namespace rgsml::platform::windows::internal
