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
} // namespace rgsml::app::l3_automatch::m05m06_real
