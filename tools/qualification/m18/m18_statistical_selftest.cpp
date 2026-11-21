// M18 preproduction statistical early-warning only; NOT approved Gate G.
// See October independent pre-audit for PROPOSED 2^20 samples and
// Bernstein criteria. This fast lane deliberately checks broad bounds
// without claiming the formal mathematical acceptance gate.
#include "internal/m18_counter_dither_philox.hpp"
#include "internal/m18_terminal_sample_codec.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using rgsml::audio::internal::M18CodecStatus;
using rgsml::audio::internal::m18_quantize_pcm24;
using rgsml::audio::internal::m18_tpdf;

namespace {
constexpr std::size_t kDitherSamples = 131072U;
constexpr std::size_t kErrorSamples = 8192U;
constexpr double kVarDither = 1.0 / 6.0;

bool dither_screen(std::uint64_t seed)
{
    std::array<std::vector<double>, 2> d{
        std::vector<double>(kDitherSamples),
        std::vector<double>(kDitherSamples)
    };
    for (std::size_t i=0; i<kDitherSamples; ++i) {
        for (std::uint32_t c=0; c<2U; ++c) {
            const double value=m18_tpdf(
                seed,static_cast<std::uint64_t>(i),c).dither_lsb();
            if(!(value>-1.0 && value<1.0))return false;
            d[c][i]=value;
        }
    }
    for(const auto& values:d) {
        double sum=0.0, second=0.0;
        for(double value:values) {
            sum+=value;
            second+=value*value;
        }
        if(std::fabs(sum/kDitherSamples)>0.012
           ||std::fabs(second/kDitherSamples-kVarDither)>0.012)
            return false;
        for(const std::size_t lag:{1U,7U,31U,1024U}) {
            double product=0.0;
            for(std::size_t i=0;i+lag<kDitherSamples;++i)
                product+=values[i]*values[i+lag];
            const double normalized=(product/
                static_cast<double>(kDitherSamples-lag))/kVarDither;
            if(std::fabs(normalized)>0.05)return false;
        }
    }
    double cross=0.0;
    for(std::size_t i=0;i<kDitherSamples;++i)
        cross+=d[0][i]*d[1][i];
    return std::fabs(cross/kDitherSamples/kVarDither)<0.05;
}

bool quantizer_screen()
{
    constexpr std::uint64_t seed=UINT64_C(0x0123456789ABCDEF);
    constexpr double scale=0x1.0p-23;
    for(std::uint32_t channel=0;channel<2U;++channel) {
        for(int fixture=0;fixture<4;++fixture) {
            std::array<std::size_t,3> zeroHistogram{};
            double sum=0.0,second=0.0;
            for(std::size_t i=0;i<kErrorSamples;++i) {
                const double t=fixture==0?0.0
                    :fixture==1?0.25
                    :fixture==2?0.5
                    :static_cast<double>(i%257U)/256.0-0.5;
                const auto output=m18_quantize_pcm24(
                    t*scale,seed,static_cast<std::uint64_t>(i),channel);
                if(output.status!=M18CodecStatus::OK)return false;
                if(fixture==0) {
                    if(output.code< -1 || output.code>1)return false;
                    ++zeroHistogram[static_cast<std::size_t>(output.code+1)];
                }
                const double error=static_cast<double>(output.code)-t;
                sum+=error;
                second+=error*error;
            }
            if(std::fabs(sum/kErrorSamples)>0.045
               ||std::fabs(second/kErrorSamples-0.25)>0.045)
                return false;
            if(fixture==0){
                for(std::size_t k=0;k<3U;++k){
                    const double target=k==1U?0.75:0.125;
                    if(std::fabs(static_cast<double>(zeroHistogram[k])/
                        kErrorSamples-target)>0.035)return false;
                }
            }
        }
    }
    return true;
}
} // namespace

int main()
{
    for(const auto seed:{UINT64_C(0),UINT64_C(0x0123456789ABCDEF),
                         UINT64_MAX}){
        if(!dither_screen(seed)){
            std::fprintf(stderr,"M18_TPDF_SCREEN_FAIL seed=%016llx\n",
                         static_cast<unsigned long long>(seed));
            return 1;
        }
    }
    if(!quantizer_screen()){
        std::fputs("M18_TOTAL_ERROR_SCREEN_FAIL\n",stderr);
        return 2;
    }
    std::puts("M18_STATISTICAL_SCREEN_PASS_NOT_GATE_G");
    return 0;
}
