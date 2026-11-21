// Isolated M18 qualification preproduction: verify FINAL-MASTER -> file bytes
// under ambient FE rounding and (x86 only) FTZ/DAZ perturbations.
// Never links or modifies M17/Qt/application/production publisher.
// Exact source input is constructed from binary64 *bits*, not host arithmetic.
#include "internal/m18_offline_export.hpp"
#include "internal/m18_wav_candidate.hpp"
#include <rgsml/core/sha256.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cfenv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
#define M18_ENV_HAS_SSE_CSR 1
#include <xmmintrin.h>
#else
#define M18_ENV_HAS_SSE_CSR 0
#endif

using namespace rgsml::audio::internal;
namespace fs=std::filesystem;
namespace {
constexpr std::uint64_t kSeed = UINT64_C(0x0123456789ABCDEF);
constexpr std::uint64_t kOrigin = UINT64_C(4294967295)-7U;
constexpr std::array<std::uint32_t,6> kRates{{44100U,48000U,88200U,96000U,176400U,192000U}};
constexpr std::array<std::uint64_t,14> kPcmBits{{
    UINT64_C(0x0000000000000000), UINT64_C(0x8000000000000000),
    UINT64_C(0x3E60000000000000), UINT64_C(0xBE60000000000000),
    UINT64_C(0x3E70000000000000), UINT64_C(0xBE70000000000000),
    UINT64_C(0x3FE8000000000000), UINT64_C(0xBFE8000000000000),
    UINT64_C(0x0000000000000001), UINT64_C(0x8000000000000001),
    UINT64_C(0x3FE0000000000000), UINT64_C(0xBFE0000000000000),
    UINT64_C(0x3FD0000000000000), UINT64_C(0xBFD0000000000000)
}};
constexpr std::array<std::uint64_t,20> kFloatBits{{
    UINT64_C(0x0000000000000000), UINT64_C(0x8000000000000000),
    UINT64_C(0x3FF0000000000000), UINT64_C(0xBFF0000000000000),
    UINT64_C(0x3FF000000FFFFFFF), UINT64_C(0x3FF0000010000000),
    UINT64_C(0x3FF0000010000001), UINT64_C(0xBFF0000010000000),
    UINT64_C(0x36A0000000000000), UINT64_C(0xB6A0000000000000),
    UINT64_C(0x3690000000000000), UINT64_C(0xB690000000000000),
    UINT64_C(0x3690000000000004), UINT64_C(0xB690000000000004),
    UINT64_C(0x3810000000000000), UINT64_C(0xB810000000000000),
    UINT64_C(0x47EFFFFFE0000000), UINT64_C(0xC7EFFFFFE0000000),
    UINT64_C(0x0000000000000001), UINT64_C(0x8000000000000001)
}};

struct FrameSource final : M18MasterFrameProvider {
    M18WavEncoding encoding{};
    std::uint64_t origin{};
    std::uint64_t total{};
    std::uint64_t reads{};
    bool wrong_address{false};
    [[nodiscard]] static double sample(M18WavEncoding fmt,
        std::uint64_t ordinal,std::uint32_t channel) noexcept {
        const auto n=fmt==M18WavEncoding::PCM24?kPcmBits.size():kFloatBits.size();
        const auto i=static_cast<std::size_t>((ordinal+7U*channel)%n);
        const auto bits=fmt==M18WavEncoding::PCM24?kPcmBits[i]:kFloatBits[i];
        return std::bit_cast<double>(bits);
    }
    [[nodiscard]] bool read_exact(std::uint64_t address,
        std::uint32_t frames,std::uint32_t channels,
        std::span<double> output) noexcept override {
        if(address<origin||address-origin>total||
           frames>total-(address-origin)||
           output.size()!=static_cast<std::size_t>(frames)*channels)
            return false;
        if(address!=origin+reads*4096U)wrong_address=true;
        ++reads;
        for(std::uint32_t f=0;f<frames;++f)
            for(std::uint32_t ch=0;ch<channels;++ch)
                output[static_cast<std::size_t>(f)*channels+ch]=
                    sample(encoding,address-origin+f,ch);
        return true;
    }
};

struct RoundingRestore final {
    int originalMode{std::fegetround()};
#if M18_ENV_HAS_SSE_CSR
    unsigned csr{_mm_getcsr()};
#endif
    ~RoundingRestore() noexcept {
#if M18_ENV_HAS_SSE_CSR
        _mm_setcsr(csr);
#endif
        if(originalMode!=-1)(void)std::fesetround(originalMode);
    }
    RoundingRestore()=default;
    RoundingRestore(const RoundingRestore&)=delete;
    RoundingRestore& operator=(const RoundingRestore&)=delete;
};

[[nodiscard]] std::vector<std::uint8_t> read_file(const fs::path& p) {
    std::ifstream f(p,std::ios::binary);
    if(!f)return {};
    return {std::istreambuf_iterator<char>{f},std::istreambuf_iterator<char>{}};
}
[[nodiscard]] bool no_private_staging(const fs::path& root){
    for(const auto& x:fs::directory_iterator(root))
        if(x.path().filename().string().find(".rgsml-m18-private-")!=std::string::npos)
            return false;
    return true;
}
[[nodiscard]] std::vector<double> source_bits(M18WavSpec spec,std::uint64_t n){
    std::vector<double> v(static_cast<std::size_t>(n)*spec.channels);
    for(std::uint64_t i=0;i<n;++i)
        for(std::uint32_t ch=0;ch<spec.channels;++ch)
            v[static_cast<std::size_t>(i)*spec.channels+ch]=
                FrameSource::sample(spec.encoding,i,ch);
    return v;
}
[[nodiscard]] bool same_input_bits(std::span<const double> values,
    M18WavSpec spec) noexcept {
    for(std::size_t i=0;i<values.size();++i){
        const auto expected=FrameSource::sample(spec.encoding,
            i/spec.channels,static_cast<std::uint32_t>(i%spec.channels));
        if(std::bit_cast<std::uint64_t>(values[i])!=
           std::bit_cast<std::uint64_t>(expected))return false;
    }
    return true;
}
[[nodiscard]] int exercise(const fs::path& root, bool emitFiles) {
    RoundingRestore restore;
    if(std::fesetround(FE_TONEAREST)!=0 ||
       std::fegetround()!=FE_TONEAREST)return 10;
#if M18_ENV_HAS_SSE_CSR
    // FTZ+DAZ are x86 MXCSR bits 15 and 6. Only change within a guarded
    // scope, and never silently claim control on unsupported architectures.
    const auto baseMxcsr=_mm_getcsr();
    _mm_setcsr(baseMxcsr & ~0x8040U);
    if((_mm_getcsr()&0x8040U)!=0U)return 11;
#endif
    const std::array<int,4> modes{{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}};
    std::uint32_t acceptedModes=0;
    std::uint32_t actualFtzdaz=0;
    std::uint32_t comparedResources=0;
    for(auto rate:kRates)for(const auto format:
        {M18WavEncoding::PCM24,M18WavEncoding::IEEE_FLOAT32})
    for(const std::uint32_t channels:{1U,2U}){
        // Force one full 4096-boundary + 2^32-wrap fixture per format.
        const std::uint64_t n=(rate==48000U&&channels==2U)
            ? UINT64_C(4223):UINT64_C(257);
        const M18WavSpec spec{format,rate,channels,n,kSeed};
        const auto input=source_bits(spec,n);
        const auto baseline=m18_make_bounded_wav_candidate(spec,input,kOrigin);
        if(baseline.status!=M18WavStatus::OK||
            baseline.bytes.size()!=baseline.plan->file_bytes)return 12;
        const auto expectedSha=rgsml::core::sha256_hex(
            std::as_bytes(std::span{baseline.bytes}));
        const auto expectedPayload=rgsml::core::sha256_hex(
            std::as_bytes(std::span{baseline.bytes}.subspan(
                baseline.plan->header_bytes)));
        const std::string baseName="m18-env-"+std::to_string(rate)+"-"+
            (format==M18WavEncoding::PCM24?"pcm24":"float32")+"-"+
            std::to_string(channels);
        for(std::size_t m=0;m<modes.size();++m){
            const auto mode=modes[m];
            if(std::fesetround(mode)!=0){
                if(mode==FE_TONEAREST)return 13;
                std::cout<<"M18_FE_UNSUPPORTED mode="<<m<<"\n";
                continue;
            }
            if(std::fegetround()!=mode)return 14;
            if(rate==kRates.front()&&format==M18WavEncoding::PCM24&&
               channels==1U)++acceptedModes;
#if M18_ENV_HAS_SSE_CSR
            const std::array<unsigned,4> csrFlags{{0U,0x8000U,0x0040U,0x8040U}};
#else
            const std::array<unsigned,1> csrFlags{{0U}};
#endif
            for(std::size_t variant=0;variant<csrFlags.size();++variant){
#if M18_ENV_HAS_SSE_CSR
                const unsigned base=_mm_getcsr();
                _mm_setcsr((base&~0x8040U)|csrFlags[variant]);
                if((_mm_getcsr()&0x8040U)!=csrFlags[variant])return 15;
                if(rate==kRates.front()&&format==M18WavEncoding::PCM24&&
                   channels==1U&&m==0)++actualFtzdaz;
#endif
                if(std::fegetround()!=mode)return 16;
                const auto now=m18_make_bounded_wav_candidate(spec,input,kOrigin);
                if(now.status!=M18WavStatus::OK||now.bytes!=baseline.bytes||
                   !same_input_bits(input,spec))return 17;
                FrameSource source;
                source.encoding=format;
                source.origin=kOrigin;
                source.total=n;
                // Keep only the reference files emitted in baseline mode.
                const bool keep=emitFiles&&m==0&&variant==0;
                const auto target=root/(baseName+"-m"+std::to_string(m)+
                    "-v"+std::to_string(variant)+".wav");
                const auto result=m18_export_offline_candidate(
                    source,target,spec,kOrigin);
                if(result.status!=M18OfflineExportStatus::OK||
                   !result.receipt||source.wrong_address||
                   source.reads!=((n+4095U)/4096U))return 18;
                const auto file=read_file(target);
                if(file!=baseline.bytes||
                   result.receipt->full_file_sha256!=expectedSha||
                   result.receipt->payload_sha256!=expectedPayload||
                   result.receipt->identity.dither_seed.has_value()!=
                       (format==M18WavEncoding::PCM24))return 19;
                if(!no_private_staging(root))return 20;
                ++comparedResources;
                if(keep){
                    std::cout<<"M18_ENV_FILE\t"<<target.filename().string()
                        <<"\t"<<(format==M18WavEncoding::PCM24?"PCM24":"FLOAT32")
                        <<"\t"<<rate<<"\t"<<channels<<"\t"<<n
                        <<"\t"<<kOrigin<<"\t"<<kSeed
                        <<"\t"<<expectedSha<<"\t"<<expectedPayload<<"\n";
                } else {std::error_code ec;fs::remove(target,ec);if(ec)return 21;}
#if M18_ENV_HAS_SSE_CSR
                _mm_setcsr(base);
#endif
            }
        }
    }
    if(acceptedModes<1U||comparedResources<24U)return 22;
    std::cout<<"M18_ENV_MATRIX modes="<<acceptedModes
        <<" ftz_daz_states="<<actualFtzdaz
        <<" resources="<<comparedResources
        <<" x86csr="<<M18_ENV_HAS_SSE_CSR<<"\n";
    std::cout<<"M18_DELIVERED_FP_ENV_INVARIANCE_PASS_NOT_PRODUCT_GATE\n";
    return 0;
}
} // namespace

int main(int argc,char** argv){
    if(argc!=1&&!(argc==3&&std::string_view(argv[1])=="--emit-fixtures"))
        return 2;
    const bool emit=argc==3;
    std::error_code ec;
    fs::path root;
    if(emit){root=argv[2];
        if(!fs::is_directory(root,ec)||ec)return 3;
    }else{
        root=fs::temp_directory_path(ec)/(
            "m18-env-"+std::to_string(std::chrono::steady_clock::now()
                .time_since_epoch().count()));
        if(ec||!fs::create_directory(root,ec)||ec)return 4;
    }
    const int result=exercise(root,emit);
    if(!emit){std::error_code ignore;fs::remove_all(root,ignore);}
    return result;
}
