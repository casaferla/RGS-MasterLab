// RGS MasterLab M18 - independent statistical qualification CANDIDATE.
// This is NOT formally adopted Gate G and is not a release certificate.
// Frozen math: TPDF raw mean 0 and second moment 1/6 LSB^2;
// PCM24 total-error raw mean 0 and second moment 1/4 LSB^2.
// Full proposed acceptance intervals, sample count, lag addressing and
// 87 predeclared primary tests: 2026-10-04 Independent M18 Pre-Audit section G.
// --smoke: cheap broad-margin structure screen for M18-only PR CI.
// --full-proposal: EXACT N=2^20 and pre-audit PROPOSED bounds; manual-only CI.
// This invokes the actual private C++ Philox and RN64 quantizer, not a
// reproduced RNG or a substituted floating-point rounding shortcut.
#include "internal/m18_counter_dither_philox.hpp"
#include "internal/m18_terminal_sample_codec.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

namespace {
using namespace rgsml::audio::internal;

constexpr std::uint64_t kNFull=UINT64_C(1048576);
constexpr std::uint64_t kNSmoke=UINT64_C(8192);
constexpr std::array<std::uint64_t,3> kSeeds{{
    UINT64_C(0),UINT64_C(0x0123456789ABCDEF),UINT64_MAX}};
constexpr std::array<std::uint32_t,2> kChannels{{0U,1U}};
constexpr std::array<std::uint64_t,4> kLags{{
    UINT64_C(1),UINT64_C(7),UINT64_C(31),UINT64_C(1024)}};
constexpr double kMeanD=0.0;
constexpr double kSecondD=1.0/6.0;
constexpr double kMeanError=0.0;
constexpr double kSecondError=0.25;

// Proposal bounds are the published pre-audit rendered numbers and are
// frozen in this test before any observed result, NOT PO-adopted limits.
// Smoke bounds are deliberately coarse; never interpret as full Gate G.
struct Bounds {
    double meanD, secondD, correlation, meanE, secondE;
};
constexpr Bounds kProposed{
    0.00247713140466, 0.00120285030056,
    0.00611085463606, 0.00303719895126, 0.00263880970848};
constexpr Bounds kSmoke{
    0.050, 0.035, 0.150, 0.080, 0.080};

enum class Family : std::size_t {
    D_MEAN=0, D_SECOND=1, LAG=2, CROSS=3,
    ERROR_MEAN=4, ERROR_SECOND=5, COUNT=6
};
constexpr std::array<std::size_t,6> kExpectedCounts{{
    6U,6U,24U,3U,24U,24U}};
constexpr std::array<const char*,6> kNames{{
    "dither_mean","dither_second","lag_normalized",
    "stereo_normalized","error_mean","error_second"}};

struct Evidence {
    bool ok=true;
    std::array<std::size_t,6> counts{};
    double worst_ratio=0.0;
    void measure(Family family, std::uint64_t seed,
                 std::uint32_t channel, unsigned caseIndex,
                 double observed, double expected, double halfWidth)
    {
        const auto index=static_cast<std::size_t>(family);
        ++counts[index];
        const double delta=std::fabs(observed-expected);
        const double ratio=delta/halfWidth;
        if(!std::isfinite(observed)||!std::isfinite(ratio)||ratio>1.0)ok=false;
        if(std::isfinite(ratio)&&ratio>worst_ratio)worst_ratio=ratio;
        // Each metric is independently auditable in the bounded workflow log.
        std::printf("M18_STAT,%s,%016llX,%u,%u,%.15g,%.15g,%.15g,%s\n",
            kNames[index],static_cast<unsigned long long>(seed),
            static_cast<unsigned>(channel),caseIndex,
            observed,expected,halfWidth,
            (std::isfinite(ratio)&&ratio<=1.0)?"PASS":"FAIL");
    }
    [[nodiscard]] bool finalize() const
    {
        std::size_t count=0;
        bool shape=true;
        for(std::size_t i=0;i<counts.size();++i){
            count+=counts[i];
            if(counts[i]!=kExpectedCounts[i])shape=false;
            std::printf("M18_STAT_COUNT,%s,%zu,%zu\n",
                kNames[i],counts[i],kExpectedCounts[i]);
        }
        std::printf("M18_STAT_SUMMARY,total=%zu,expected=87,worst_ratio=%.12g\n",
            count,worst_ratio);
        return ok&&shape&&count==87U;
    }
};

[[nodiscard]] double dither(std::uint64_t seed,
                            std::uint64_t frame,
                            std::uint32_t channel) noexcept
{
    return m18_tpdf(seed,frame,channel).dither_lsb();
}

void check_dither_moments(Evidence& evidence, std::uint64_t n,
                          const Bounds& limits)
{
    for(const auto seed:kSeeds){
        for(const auto channel:kChannels){
            double sum=0.0, second=0.0;
            for(std::uint64_t frame=0;frame<n;++frame){
                const double value=dither(seed,frame,channel);
                if(!(value>-1.0&&value<1.0))evidence.ok=false;
                sum+=value;
                second+=value*value;
            }
            evidence.measure(Family::D_MEAN,seed,channel,0U,
                sum/static_cast<double>(n),kMeanD,limits.meanD);
            evidence.measure(Family::D_SECOND,seed,channel,0U,
                second/static_cast<double>(n),kSecondD,limits.secondD);
        }
    }
}

void check_disjoint_lags_and_stereo(Evidence& evidence,
                                   std::uint64_t n,
                                   const Bounds& limits)
{
    for(const auto seed:kSeeds){
        for(const auto channel:kChannels){
            for(std::size_t caseIndex=0;caseIndex<kLags.size();++caseIndex){
                const auto lag=kLags[caseIndex];
                double sum=0.0;
                for(std::uint64_t j=0;j<n;++j){
                    // Disjoint pairs EXACTLY as independent pre-audit:
                    // (2*lag*j, 2*lag*j+lag). No overlap within this stat.
                    const std::uint64_t base=2U*lag*j;
                    sum+=dither(seed,base,channel)
                        *dither(seed,base+lag,channel);
                }
                const double normalized=(sum/static_cast<double>(n))/kSecondD;
                evidence.measure(Family::LAG,seed,channel,
                    static_cast<unsigned>(lag),normalized,0.0,
                    limits.correlation);
            }
        }
        double cross=0.0;
        for(std::uint64_t frame=0;frame<n;++frame)
            cross+=dither(seed,frame,0U)*dither(seed,frame,1U);
        evidence.measure(Family::CROSS,seed,0U,0U,
            (cross/static_cast<double>(n))/kSecondD,0.0,
            limits.correlation);
    }
}

[[nodiscard]] constexpr double fixture_t(
    std::uint64_t frame,unsigned fixture) noexcept
{
    switch(fixture){
        case 0U:return 0.0;
        case 1U:return 0.25;
        case 2U:return 0.5;
        default:
            return static_cast<double>(frame%UINT64_C(257))/256.0-0.5;
    }
}

void check_total_error(Evidence& evidence,std::uint64_t n,
                       const Bounds& limits)
{
    for(const auto seed:kSeeds){
        for(const auto channel:kChannels){
            for(unsigned fixture=0U;fixture<4U;++fixture){
                double sum=0.0, second=0.0;
                for(std::uint64_t frame=0;frame<n;++frame){
                    const double t=fixture_t(frame,fixture);
                    const auto quantized=m18_quantize_pcm24(
                        t*0x1.0p-23,seed,frame,channel);
                    if(quantized.status!=M18CodecStatus::OK){
                        std::fprintf(stderr,
                            "M18_ERR_CODEC_REJECT seed=%016llX ch=%u fixture=%u frame=%llu\n",
                            static_cast<unsigned long long>(seed),channel,
                            fixture,static_cast<unsigned long long>(frame));
                        evidence.ok=false;
                        // The shape of the statistical run is now invalid.
                        // Return rather than manufacture metrics for a
                        // truncated sample. The 87-count gate will fail.
                        return;
                    }
                    const double error=static_cast<double>(
                        quantized.code)-t;
                    sum+=error;
                    second+=error*error;
                }
                evidence.measure(Family::ERROR_MEAN,seed,channel,fixture,
                    sum/static_cast<double>(n),kMeanError,limits.meanE);
                evidence.measure(Family::ERROR_SECOND,seed,channel,fixture,
                    second/static_cast<double>(n),kSecondError,
                    limits.secondE);
            }
        }
    }
}
} // namespace

int main(int argc,char** argv)
{
    if(argc!=2 || (std::strcmp(argv[1],"--smoke")!=0 &&
                   std::strcmp(argv[1],"--full-proposal")!=0)){
        std::fputs("usage: m18_gate_g_candidate --smoke|--full-proposal\n",stderr);
        return 2;
    }
    const bool full=std::strcmp(argv[1],"--full-proposal")==0;
    const std::uint64_t n=full?kNFull:kNSmoke;
    const Bounds limits=full?kProposed:kSmoke;
    std::printf("M18_GATE_G_%s_BEGIN N=%llu seeds=3 channels=2 primary=87\n",
        full?"PROPOSAL":"SMOKE",static_cast<unsigned long long>(n));
    Evidence result;
    check_dither_moments(result,n,limits);
    check_disjoint_lags_and_stereo(result,n,limits);
    check_total_error(result,n,limits);
    if(!result.finalize()){
        std::fputs("M18_GATE_G_CANDIDATE_FAIL\n",stderr);
        return 1;
    }
    std::printf("M18_GATE_G_%s_PASS_NOT_FORMAL_GATE_G\n",
        full?"PROPOSAL":"SMOKE");
    return 0;
}
