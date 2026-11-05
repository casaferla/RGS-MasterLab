// M18 isolated full Master-to-WAV preproduction integration, not app/FQ.
// Test: all six clocks, both formats and channel layouts, immutable source,
// distinct dither seeds, fail-closed cancellation/errors/no-overwrite,
// source-ownership/checkpoint and independently verifiable receipts.
#include "internal/m18_offline_export.hpp"
#include "internal/m18_wav_candidate.hpp"
#include <rgsml/core/sha256.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

using namespace rgsml::audio::internal;
namespace fs=std::filesystem;

struct Provider final : M18MasterFrameProvider {
    std::uint64_t origin{0};
    std::uint64_t calls{0};
    std::uint32_t seenChannels{0};
    bool fail{false};
    bool badSample{false};
    bool nonfinite{false};
    bool wrongPosition{false};
    const fs::path* competingPath{nullptr};
    [[nodiscard]] bool read_exact(std::uint64_t first,std::uint32_t count,
        std::uint32_t channels,std::span<double> output) noexcept override {
        if(output.size()!=static_cast<std::size_t>(count)*channels)return false;
        if(first!=origin+calls*4096U)wrongPosition=true;
        if(fail && calls>0)return false;
        ++calls;
        if(competingPath && calls==2U){
            try{std::ofstream other(*competingPath,std::ios::binary);
                other<<"other-writer-owned-file";
                if(!other)return false;
            }catch(...){return false;}
        }
        seenChannels=channels;
        for(std::uint32_t f=0;f<count;++f)for(std::uint32_t c=0;c<channels;++c){
            const int code=static_cast<int>((first+f)%7U)-3;
            double value=static_cast<double>(code)*0x1.0p-25;
            if(c!=0U)value=-value;
            if(badSample && f==count-1 && calls==1)value=2.0;
            if(nonfinite && f==0)value=std::numeric_limits<double>::infinity();
            output[static_cast<std::size_t>(f)*channels+c]=value;
        }
        return true;
    }
};
struct Stop final : M18ExportStopSignal {
    const Provider* provider{nullptr};
    std::uint64_t maxCalls{0};
    [[nodiscard]] bool cancelled() const noexcept override {
        return provider->calls>=maxCalls;
    }
};

struct Folder final {
    fs::path p;
    ~Folder(){std::error_code e;fs::remove_all(p,e);}
};

static std::string digest(std::span<const std::uint8_t> x){
    return rgsml::core::sha256_hex(std::as_bytes(x));
}
static std::vector<std::uint8_t> bytes(const fs::path& path){
    std::ifstream in(path,std::ios::binary);
    return {std::istreambuf_iterator<char>{in},std::istreambuf_iterator<char>{}};
}
static bool private_is_gone(const fs::path& parent){
    for(const auto& e:fs::directory_iterator(parent)){
        if(e.path().filename().string().find(".rgsml-m18-private-")!=
            std::string::npos)return false;
    }
    return true;
}

static int positive_matrix(const fs::path& dir,bool outputFixtures){
    constexpr std::array<std::uint32_t,6> rates{
        44100U,48000U,88200U,96000U,176400U,192000U};
    for(const auto rate:rates)for(const auto encoding:
        {M18WavEncoding::PCM24,M18WavEncoding::IEEE_FLOAT32})
    for(const auto channel:{1U,2U}){
        const std::size_t n=outputFixtures?4161U:4223U;
        const auto seed=UINT64_C(0x0123456789ABCDEF);
        M18WavSpec spec{encoding,rate,channel,n,seed};
        const auto start=UINT64_C(4294967295)-3U;
        Provider source; source.origin=start;
        const auto path=dir/("m18-"+std::to_string(rate)+"-"+
            (encoding==M18WavEncoding::PCM24?"pcm24":"float32")+"-"+
            std::to_string(channel)+".wav");
        auto result=m18_export_offline_candidate(source,path,spec,start);
        if(result.status!=M18OfflineExportStatus::OK||!result.receipt)
            return 10;
        if(source.wrongPosition||source.seenChannels!=channel
           ||source.calls!=2U)return 11;
        auto contents=bytes(path);
        if(contents.size()!=result.receipt->plan.file_bytes
           ||digest(contents)!=result.receipt->full_file_sha256)return 12;
        const auto p=m18_plan_wav(spec);
        if(p.status!=M18WavStatus::OK)return 13;
        std::vector<double> reference(n*channel);
        for(std::size_t f=0;f<n;++f)for(unsigned c=0;c<channel;++c){
            const auto frame=start+f;
            const int code=static_cast<int>(frame%7U)-3;
            double value=static_cast<double>(code)*0x1.0p-25;
            if(c)value=-value;
            reference[f*channel+c]=value;
        }
        const auto oracle=m18_make_bounded_wav_candidate(spec,reference,start);
        if(oracle.status!=M18WavStatus::OK||contents!=oracle.bytes)return 14;
        if(digest(std::span<const std::uint8_t>{contents}.subspan(
            p.plan.header_bytes))!=result.receipt->payload_sha256)return 15;
        if(!private_is_gone(dir))return 16;
        if(outputFixtures){
            const auto fmt=encoding==M18WavEncoding::PCM24?"PCM24":"FLOAT32";
            std::cout<<"M18_FILE\t"<<path.filename().string()<<"\t"<<fmt
                <<"\t"<<rate<<"\t"<<channel<<"\t"<<n<<"\t"<<start
                <<"\t"<<seed<<"\t"<<result.receipt->full_file_sha256
                <<"\t"<<result.receipt->payload_sha256<<"\n";
        } else fs::remove(path);
    }
    return 0;
}

static int failure_matrix(const fs::path& dir){
    constexpr auto seed=UINT64_C(0x0123456789ABCDEF);
    M18WavSpec spec{M18WavEncoding::PCM24,48000U,2U,8257U,seed};
    const auto target=dir/"failure.wav";
    Provider source;
    source.fail=true;
    auto result=m18_export_offline_candidate(source,target,spec,0);
    if(result.status!=M18OfflineExportStatus::SOURCE_FAILED
       ||fs::exists(target)||!private_is_gone(dir))return 50;
    source=Provider{};
    Stop cancel; cancel.provider=&source;cancel.maxCalls=1U;
    result=m18_export_offline_candidate(source,target,spec,0,&cancel);
    if(result.status!=M18OfflineExportStatus::CANCELLED
       ||fs::exists(target)||!private_is_gone(dir))return 51;
    source=Provider{};
    source.badSample=true;
    result=m18_export_offline_candidate(source,target,spec,0);
    if(result.status!=M18OfflineExportStatus::STREAM_FAILED
       ||result.codec_status!=M18CodecStatus::PCM24_OVERLOAD
       ||fs::exists(target)||!private_is_gone(dir))return 52;
    source=Provider{};
    source.nonfinite=true;
    result=m18_export_offline_candidate(source,target,spec,0);
    if(result.status!=M18OfflineExportStatus::STREAM_FAILED
       ||result.codec_status!=M18CodecStatus::NONFINITE_INPUT
       ||fs::exists(target)||!private_is_gone(dir))return 53;
    source=Provider{};
    result=m18_export_offline_candidate(source,target,spec,UINT64_MAX-2U);
    if(result.status!=M18OfflineExportStatus::ADDRESS_OVERFLOW
       ||source.calls!=0U||!private_is_gone(dir))return 54;
    spec.channels=3U;
    result=m18_export_offline_candidate(source,target,spec,0);
    if(result.status!=M18OfflineExportStatus::INVALID_PLAN
       ||source.calls!=0U||!private_is_gone(dir))return 55;
    spec.channels=2U;
    spec.frame_count=0U;
    result=m18_export_offline_candidate(source,target,spec,UINT64_MAX);
    if(result.status!=M18OfflineExportStatus::OK||!result.receipt
       ||source.calls!=0U||!fs::exists(target))return 56;
    const auto existing=bytes(target);
    result=m18_export_offline_candidate(source,target,spec,0);
    if(result.status!=M18OfflineExportStatus::TARGET_EXISTS
       ||bytes(target)!=existing||!private_is_gone(dir))return 57;
    fs::remove(target);
    // Float32 rejects infinity and does not apply a dither seed.
    spec={M18WavEncoding::IEEE_FLOAT32,48000U,1U,17U,0U};
    source=Provider{};source.nonfinite=true;
    result=m18_export_offline_candidate(source,target,spec,0);
    if(result.status!=M18OfflineExportStatus::STREAM_FAILED
       ||result.codec_status!=M18CodecStatus::NONFINITE_INPUT
       ||fs::exists(target)||!private_is_gone(dir))return 58;
    // A second writer wins the path after private staging opened.
    // No TOCTOU path replacement: no-overwrite publish MUST fail closed.
    spec={M18WavEncoding::PCM24,48000U,2U,8257U,seed};
    source=Provider{};source.competingPath=&target;
    result=m18_export_offline_candidate(source,target,spec,0);
    if(result.status!=M18OfflineExportStatus::PUBLICATION_FAILED
       ||result.file_status!=M18FileStatus::TARGET_EXISTS
       ||!fs::exists(target)||!private_is_gone(dir))return 59;
    const auto raceBytes=bytes(target);
    const std::string witness(raceBytes.begin(),raceBytes.end());
    if(witness!="other-writer-owned-file")return 60;
    fs::remove(target);
    // Seed is an immutable byte identity for PCM24 but NOT Float32.
    for(auto fmt:{M18WavEncoding::PCM24,M18WavEncoding::IEEE_FLOAT32}){
        spec={fmt,48000U,2U,4097U,seed};
        source=Provider{};
        const auto one=dir/"seed-one.wav";
        const auto two=dir/"seed-two.wav";
        result=m18_export_offline_candidate(source,one,spec,0);
        if(result.status!=M18OfflineExportStatus::OK||!result.receipt)
            return 61;
        const auto identityOne=result.receipt->identity;
        spec.dither_seed=seed+1U;
        source=Provider{};
        result=m18_export_offline_candidate(source,two,spec,0);
        if(result.status!=M18OfflineExportStatus::OK||!result.receipt)
            return 62;
        const bool same=bytes(one)==bytes(two);
        if((fmt==M18WavEncoding::PCM24 && (same ||
            identityOne.dither_seed==result.receipt->identity.dither_seed))
           ||(fmt==M18WavEncoding::IEEE_FLOAT32 && (!same ||
            identityOne.dither_seed || result.receipt->identity.dither_seed)))
            return 63;
        fs::remove(one);fs::remove(two);
        if(!private_is_gone(dir))return 64;
    }
    return 0;
}

int main(int argc,char** argv){
    bool emit=false;
    if(argc!=1){
        if(argc!=3||std::string(argv[1])!="--emit-fixtures")return 2;
        emit=true;
    }
    std::error_code ec;
    fs::path path;
    if(emit){path=argv[2];
        if(!fs::is_directory(path,ec)||ec)return 3;
    }else{
        path=fs::temp_directory_path(ec)/(
            "m18-offline-"+std::to_string(std::chrono::steady_clock::now()
                .time_since_epoch().count()));
        if(ec||!fs::create_directory(path,ec)||ec)return 4;
    }
    Folder cleanup{emit?fs::path{}:path};
    if(const auto x=positive_matrix(path,emit);x){
        std::cerr<<"M18_OFFLINE_POSITIVE_FAIL "<<x<<"\n";return x;}
    if(const auto x=failure_matrix(path);x){
        std::cerr<<"M18_OFFLINE_FAILURE_FAIL "<<x<<"\n";return x;}
    std::cout<<"M18_OFFLINE_EXPORT_PREPRODUCTION_PASS\n";
    return 0;
}
