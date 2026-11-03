// M18 low-level musical failure-screen against correlated quantization error.
// The coherent triangular template is exactly dyadic in binary64: no host
// transcendental sampler influences the expected source bits.
// NOT an FFT/THD+N/listening/fully-adopted mathematical Gate G certificate.
#include "internal/m18_terminal_sample_codec.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>

using namespace rgsml::audio::internal;

int main(){
    constexpr auto seed=UINT64_C(0x0123456789ABCDEF);
    constexpr std::uint64_t n=UINT64_C(65536);
    for(std::uint32_t ch=0;ch<2U;++ch){
        double sum=0.0, second=0.0, correlated=0.0, templateEnergy=0.0;
        std::uint64_t lastZeroCount=0U, lastNonzeroCount=0U;
        for(std::uint64_t f=0;f<n;++f){
            const auto phase=static_cast<int>(f%UINT64_C(256));
            const auto triangular=phase<=128?phase:256-phase;
            // exactly [-1/4,+1/4] PCM24 LSB for non-tail frames.
            const double t=f>=n-8192U?0.0:
                static_cast<double>(triangular-64)*0x1.0p-8;
            const auto out=m18_quantize_pcm24(t*0x1.0p-23,seed,f,ch);
            if(out.status!=M18CodecStatus::OK){
                std::fprintf(stderr,"M18_AUDIO_CODEC_FAIL ch=%u frame=%llu\n",
                    ch,static_cast<unsigned long long>(f));
                return 1;
            }
            const double error=static_cast<double>(out.code)-t;
            sum+=error;second+=error*error;
            correlated+=error*t;
            templateEnergy+=t*t;
            if(f>=n-8192U){
                ++lastZeroCount;
                if(out.code!=0)++lastNonzeroCount;
            }
        }
        const auto mean=sum/static_cast<double>(n);
        const auto rawSecond=second/static_cast<double>(n);
        const auto ratio=correlated/templateEnergy;
        const auto nonzero=static_cast<double>(lastNonzeroCount)/
            static_cast<double>(lastZeroCount);
        // Undithered RNE code is identically 0 for this entire input;
        // its error=-t and its coherent template projection is -1.
        // Proper TPDF destroys the strong coherent quantization birdie.
        // These are conservative preproduction diagnostic margins, NOT
        // the pre-audit formal spectral acceptance proposal.
        const bool pass=std::fabs(ratio)<0.12
            && std::fabs(mean)<0.025
            && std::fabs(rawSecond-0.25)<0.03
            && std::fabs(nonzero-0.25)<0.055;
        std::printf("M18_LOWLEVEL,ch=%u,coherent_ratio=%.9g,mean=%.9g,"
            "error_second=%.9g,zero_tail_nonzero_ratio=%.9g,%s\n",
            ch,ratio,mean,rawSecond,nonzero,pass?"PASS":"FAIL");
        if(!pass)return 2;
    }
    std::puts("M18_LOWLEVEL_DITHER_DECORRELATION_PASS_NOT_FORMAL_GATE_G");
    return 0;
}
