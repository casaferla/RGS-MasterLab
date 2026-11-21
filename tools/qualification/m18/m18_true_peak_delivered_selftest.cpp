// M18 post-quantization True Peak seam: actual unmodified M16 internal FIR.
// This is numerical preproduction only, NOT M16 Gate G, M17 ceiling policy,
// product .rgsmaster rendering or a physical playback/Windows FQ.
#include "internal/m18_offline_export.hpp"
#include "internal/m18_candidate_decoded_measurement.hpp"
#include "internal/true_peak_itu4x_channel_stream.hpp"
#include <rgsml/core/sha256.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

using namespace rgsml::audio::internal;
namespace fs=std::filesystem;

class ImmutableMaster final : public M18MasterFrameProvider {
public:
    std::vector<double> input;
    std::uint64_t origin{};
    std::uint32_t channels{};
    [[nodiscard]] bool read_exact(std::uint64_t absolute,
        std::uint32_t frames,std::uint32_t ch,
        std::span<double> dest) noexcept override {
        if(ch!=channels||absolute<origin)return false;
        const std::uint64_t offset=absolute-origin;
        if(offset>input.size()/ch||frames>input.size()/ch-offset
           ||dest.size()!=static_cast<std::size_t>(frames)*ch)return false;
        std::copy_n(input.begin()+static_cast<std::ptrdiff_t>(offset*ch),
                    dest.size(),dest.begin());
        return true;
    }
};

[[nodiscard]] static std::vector<std::uint8_t> read_bytes(
    const fs::path& path){
    std::ifstream file(path,std::ios::binary);
    return {std::istreambuf_iterator<char>{file},std::istreambuf_iterator<char>{}};
}

[[nodiscard]] static bool measure_true_peak(
    std::span<const double> interleaved,std::uint32_t channels,
    std::array<double,2>& peaks){
    using rgsml::analysis::internal::TruePeakItu4xChannelStream;
    std::array<TruePeakItu4xChannelStream,2> bank{};
    if(channels!=1U && channels!=2U)return false;
    if(interleaved.empty()||interleaved.size()%channels!=0U)return false;
    for(std::size_t frame=0;frame<interleaved.size()/channels;++frame){
        for(std::uint32_t ch=0;ch<channels;++ch){
            auto r=bank[ch].push(interleaved[frame*channels+ch]);
            if(!r)return false;
        }
    }
    for(std::uint32_t ch=0;ch<channels;++ch){
        auto r=bank[ch].finalize(); // M16: all 11 EOS zero frames.
        if(!r)return false;
        peaks[ch]=*r.value();
    }
    return true;
}

static int check_one(const fs::path& dir, M18WavEncoding encoding,
                     std::uint32_t rate,std::uint32_t channels){
    constexpr std::uint64_t first=(UINT64_C(1)<<32)-2U;
    constexpr std::size_t n=4099U;
    constexpr double stages[8]{
        0.0,0.875,-0.625,0.5,-0.375,0.25,-0.125,0.0};
    constexpr auto seed=UINT64_C(0x0123456789ABCDEF);
    M18WavSpec spec{encoding,rate,channels,n,seed};
    ImmutableMaster original;
    original.origin=first;
    original.channels=channels;
    original.input.resize(n*channels);
    for(std::size_t frame=0;frame<n;++frame){
        const auto base=stages[frame%8U]
            +((frame%3U)==0U?0x1.0p-27:-0x1.0p-27);
        for(std::uint32_t ch=0;ch<channels;++ch)
            original.input[frame*channels+ch]=ch==0U?base:base*0.75;
    }
    const auto frozen=original.input;
    const auto file=dir/(std::to_string(rate)+"-"+
        (encoding==M18WavEncoding::PCM24?"p24":"f32")+"-"+
        std::to_string(channels)+".wav");
    const auto result=m18_export_offline_candidate(
        original,file,spec,first);
    if(result.status!=M18OfflineExportStatus::OK||!result.receipt)
        return 10;
    if(original.input!=frozen)return 11; // immutable real Master.
    const auto plan=m18_plan_wav(spec);
    if(plan.status!=M18WavStatus::OK)return 12;
    const auto bytes=read_bytes(file);
    if(bytes.size()!=plan.plan.file_bytes)return 13;
    if(rgsml::core::sha256_hex(std::as_bytes(std::span{bytes}))
        !=result.receipt->full_file_sha256)return 14;
    M18WavCandidate readback{};
    readback.status=M18WavStatus::OK;
    readback.codec_status=M18CodecStatus::OK;
    readback.plan=plan.plan;
    readback.bytes=bytes;
    const auto decoded=m18_decode_own_bounded_candidate(readback);
    if(decoded.status!=M18DecodeStatus::OK||decoded.sample_rate!=rate
       ||decoded.channels!=channels||decoded.interleaved.size()!=frozen.size())
        return 15;
    double maxSampleDelta=0.0;
    for(std::size_t i=0;i<frozen.size();++i){
        const auto difference=std::fabs(frozen[i]-decoded.interleaved[i]);
        maxSampleDelta=std::max(maxSampleDelta,difference);
    }
    std::array<double,2> before{},after{};
    if(!measure_true_peak(frozen,channels,before)
        ||!measure_true_peak(decoded.interleaved,channels,after))
        return 16;
    // The actual M16 4-phase 12-tap bank has maximal absolute phase
    // coefficient sum 2.0228271484375. The FIR L-infinity perturbation
    // bound is absolute, deterministic and does NOT assert decoded ceiling.
    constexpr double m16OperatorNorm=2.0228271484375;
    const double maxPeakMovement=m16OperatorNorm*maxSampleDelta+0x1.0p-40;
    for(std::uint32_t ch=0;ch<channels;++ch){
        if(!std::isfinite(after[ch])||after[ch]<0.0
           ||std::fabs(before[ch]-after[ch])>maxPeakMovement)
            return 17;
    }
    if(encoding==M18WavEncoding::PCM24
       && maxSampleDelta>(1.5+0x1.0p-27)*0x1.0p-23)
        return 18;
    if(encoding==M18WavEncoding::IEEE_FLOAT32
       && maxSampleDelta>0x1.0p-24)
        return 19;
    if(read_bytes(file)!=bytes)return 20; // M16 must be read only.
    fs::remove(file);
    std::cout<<"M18_POST_DECODE_M16,rate="<<rate
             <<",channels="<<channels
             <<",encoding="<<(encoding==M18WavEncoding::PCM24?"PCM24":"FLOAT32")
             <<",maxSampleDelta="<<maxSampleDelta
             <<",maxPeakBound="<<maxPeakMovement<<",PASS\n";
    return 0;
}

int main(){
    std::error_code ec;
    const auto folder=fs::temp_directory_path(ec)/"m18-m16-readback-test";
    if(ec)return 2;
    if(!fs::create_directory(folder,ec)||ec)return 3;
    struct Cleanup {fs::path p;~Cleanup(){std::error_code e;fs::remove_all(p,e);}} cleanup{folder};
    for(const auto fsHz:{44100U,48000U,88200U,96000U,176400U,192000U})
    for(const auto enc:{M18WavEncoding::PCM24,M18WavEncoding::IEEE_FLOAT32})
    for(const auto ch:{1U,2U}){
        if(const int rc=check_one(folder,enc,fsHz,ch);rc){
            std::cerr<<"M18_M16_DELIVERED_SEAM_FAIL "<<rc<<"\n";
            return rc;
        }
    }
    std::cout<<"M18_POST_QUANTIZATION_AUTHENTIC_M16_NUMERIC_SEAM_PASS_NOT_PRODUCT_GATE\n";
    return 0;
}
