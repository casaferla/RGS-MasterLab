#pragma once
// M05 frozen T3/T4/T5 tonal statistics directly from the EXISTING M20
// canonical mean-channel time-averaged PSD. No extra FFT, resampling,
// independent analyzer, narrower source interval or implicit M07 common
// comparison bandwidth. Caller supplies identical common interval for
// candidate+REFERENCE, and later authenticates M07/M08 eligibility.
// Output is only observed z=log2(frequency/1Hz), NOT M10 readiness.
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace rgsml::app::l3_automatch::m05m06_real {
struct CommonTonalBandwidth final {double low_hz{},high_hz{};};
struct TonalT3T5 final {
    CommonTonalBandwidth common;
    double energy{};
    double centroid_hz{};
    double rolloff85_hz{};
    double rolloff95_hz{};
    double centroid_log2_hz{};
    double rolloff85_log2_hz{};
    double rolloff95_log2_hz{};
};
// M05 authoritative E>=1e-12 floor. Grid is midpoint one-sided
// PSD cells; first/last cells are clipped to [0, fs/2] before the
// supplied common interval. Exactly the SAME cells serve all metrics.
[[nodiscard]] inline std::optional<TonalT3T5> observe_m05_t3t5(
    const analysis::TonalStereoAnalysisResult& producer,
    CommonTonalBandwidth common)
{
    using State=analysis::TonalStereoMetricState;
    const double fs=static_cast<double>(producer.sample_rate_hz);
    if(producer.spectrum_state!=State::AVAILABLE
       ||producer.complete_spectral_windows==0
       ||producer.sample_rate_hz<=0
       ||!std::isfinite(common.low_hz)
       ||!std::isfinite(common.high_hz)
       ||common.low_hz<20.0
       ||!(common.high_hz>common.low_hz)
       ||common.high_hz>std::min(20000.0,0.45*fs))
        return std::nullopt;
    const auto& psd=producer.mean_linear_psd;
    if(psd.size()<3||psd.size()>
        (std::numeric_limits<std::size_t>::max()/2))
        return std::nullopt;
    const std::size_t nfft=2*(psd.size()-1);
    if(!std::has_single_bit(nfft))return std::nullopt;
    const double df=fs/static_cast<double>(nfft);
    double E=0.0,moment=0.0;
    for(std::size_t k=0;k<psd.size();++k) {
        const double power=psd[k];
        if(!std::isfinite(power)||power<0.0)
            return std::nullopt;
        const double fk=static_cast<double>(k)*df;
        const double a=std::max({0.0,common.low_hz,fk-df/2.0});
        const double b=std::min({fs/2.0,common.high_hz,fk+df/2.0});
        if(b<=a)continue;
        E+=power*(b-a);
        moment+=power*((b*b-a*a)/2.0);
        if(!std::isfinite(E)||!std::isfinite(moment))
            return std::nullopt;
    }
    if(E<1e-12||!std::isfinite(E))return std::nullopt;
    const double centroid=moment/E;
    if(!std::isfinite(centroid)||centroid<=0.0)
        return std::nullopt;
    const auto percentile=[&](double q)->std::optional<double> {
        const double wanted=q*E;
        double sum=0.0;
        for(std::size_t k=0;k<psd.size();++k) {
            const double fk=static_cast<double>(k)*df;
            const double a=std::max({
                0.0,common.low_hz,fk-df/2.0});
            const double b=std::min({
                fs/2.0,common.high_hz,fk+df/2.0});
            if(b<=a||psd[k]<=0.0)continue;
            const double cell=psd[k]*(b-a);
            if(sum+cell>=wanted) {
                // This is interpolation INSIDE the first positive cell
                // crossing qE, not a nearest FFT-bin approximation.
                const double result=a+(wanted-sum)/psd[k];
                if(!std::isfinite(result)||result<a||result>b)
                    return std::nullopt;
                return result;
            }
            sum+=cell;
        }
        return std::nullopt;
    };
    const auto p85=percentile(0.85),p95=percentile(0.95);
    if(!p85||!p95||*p85>*p95)return std::nullopt;
    const double z1=std::log2(centroid);
    const double z2=std::log2(*p85);
    const double z3=std::log2(*p95);
    if(!std::isfinite(z1)||!std::isfinite(z2)
       ||!std::isfinite(z3))return std::nullopt;
    return TonalT3T5{common,E,centroid,*p85,*p95,z1,z2,z3};
}
// M05 frozen T2/T6 named-band spectral density OLS. The ACTUAL
// authoritative density floor is a REQUIRED M07/configuration input,
// not a magic DSP constant supplied by this source port.
struct TonalSlope final {
    double slope_db_per_octave{};
    std::size_t configured_bands{};
    std::size_t above_floor_bands{};
    double supplied_density_floor{};
};
[[nodiscard]] inline std::optional<TonalSlope> observe_m05_t2_or_t6(
    const analysis::TonalStereoAnalysisResult& producer,
    CommonTonalBandwidth common,double authoritative_density_floor,
    bool high_frequency_slope)
{
    using State=analysis::TonalStereoMetricState;
    const double lo=high_frequency_slope?8000.0:100.0;
    const double hi=high_frequency_slope?16000.0:10000.0;
    const double fs=static_cast<double>(producer.sample_rate_hz);
    if(producer.spectrum_state!=State::AVAILABLE
       ||producer.complete_spectral_windows==0
       ||producer.sample_rate_hz<=0
       ||!std::isfinite(common.low_hz)
       ||!std::isfinite(common.high_hz)
       ||common.low_hz>lo||common.high_hz<hi
       ||common.low_hz<20.0
       ||common.high_hz>std::min(20000.0,0.45*fs)
       ||!std::isfinite(authoritative_density_floor)
       ||authoritative_density_floor<=0.0)
        return std::nullopt;
    std::size_t total=0,used=0;
    double sx=0.0,sy=0.0,sxx=0.0,sxy=0.0;
    for(const auto& band:producer.primary_bands_1_12) {
        if(!std::isfinite(band.center_hz)
           ||!std::isfinite(band.low_hz)
           ||!std::isfinite(band.high_hz))
            return std::nullopt;
        // The M05 configured OLS interval is never shrunk to a
        // better-looking subset; excluded density cells retain mass
        // in the >=80% support test.
        if(band.center_hz<lo||band.center_hz>hi)continue;
        ++total;
        if(!band.fully_supported
           ||band.low_hz<common.low_hz
           ||band.high_hz>common.high_hz
           ||!(band.high_hz>band.low_hz)
           ||!std::isfinite(band.measured_linear_energy)
           ||band.measured_linear_energy<0.0)
            return std::nullopt;
        const double density=band.measured_linear_energy/
            (band.high_hz-band.low_hz);
        if(!std::isfinite(density)||density<0.0)
            return std::nullopt;
        if(density<=authoritative_density_floor)continue;
        const double x=std::log2(band.center_hz/1000.0);
        const double y=10.0*std::log10(density);
        if(!std::isfinite(x)||!std::isfinite(y))
            return std::nullopt;
        ++used;
        sx+=x;sy+=y;sxx+=x*x;sxy+=x*y;
        if(!std::isfinite(sx)||!std::isfinite(sy)
           ||!std::isfinite(sxx)||!std::isfinite(sxy))
            return std::nullopt;
    }
    if(total==0||used<3||5*used<4*total)
        return std::nullopt;
    const double n=static_cast<double>(used);
    const double denominator=n*sxx-sx*sx;
    if(!(denominator>0.0)||!std::isfinite(denominator))
        return std::nullopt;
    const double slope=(n*sxy-sx*sy)/denominator;
    if(!std::isfinite(slope))return std::nullopt;
    return TonalSlope{slope,total,used,authoritative_density_floor};
}
// Convenience observation bundle. Absent T2/T6 support or missing
// external floor stays explicit. A single call reuses the SAME canonical
// M20 PSD and band energies and cannot itself grant M07/M08 readiness.
struct TonalObservationBundle final {
    std::optional<TonalT3T5> t3_centroid_t4t5_rolloffs;
    std::optional<TonalSlope> t2_spectral_tilt;
    std::optional<TonalSlope> t6_high_frequency_slope;
    [[nodiscard]] bool authorizes_solver_objective()
        const noexcept {return false;}
};
[[nodiscard]] inline TonalObservationBundle
observe_m05_tonal_bundle(
    const analysis::TonalStereoAnalysisResult& actual_m20,
    CommonTonalBandwidth exact_common,double authoritative_density_floor)
{
    return {
        observe_m05_t3t5(actual_m20,exact_common),
        observe_m05_t2_or_t6(
            actual_m20,exact_common,authoritative_density_floor,false),
        observe_m05_t2_or_t6(
            actual_m20,exact_common,authoritative_density_floor,true)};
}
} // namespace rgsml::app::l3_automatch::m05m06_real
