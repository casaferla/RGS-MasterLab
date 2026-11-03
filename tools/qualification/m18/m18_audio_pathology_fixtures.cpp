// M18 preproduction audio pathology WAV suite, not production listening/FQ.
// Uses the same real isolated exporter and explicit output-frame addressing.
// Generates 7 tiny-amplitude 65536-frame PCM24 WAV test programs (not music).
#include "internal/m18_offline_export.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

using namespace rgsml::audio::internal;
namespace fs=std::filesystem;

struct Probe final : M18MasterFrameProvider {
    enum class Kind { LowSine, TwoLsbSine, Fade, Sparse, LowFrequency,
                      IdenticalStereo } kind{Kind::LowSine};
    static constexpr std::uint64_t n=65536U;
    static constexpr double pi=3.141592653589793238462643383279502884;
    [[nodiscard]] bool read_exact(std::uint64_t start,
        std::uint32_t frames,std::uint32_t channels,
        std::span<double> destination) noexcept override {
        if(start>n||frames>n-start ||
           destination.size()!=static_cast<std::size_t>(frames)*channels)
            return false;
        for(std::uint32_t f=0;f<frames;++f){
            const auto index=start+f;
            const double angle=2.0*pi*static_cast<double>(index)/
                static_cast<double>(n);
            double t=0.0;
            switch(kind){
            case Kind::LowSine: t=0.25*std::sin(997.0*angle);break;
            case Kind::TwoLsbSine: t=2.0*std::sin(997.0*angle);break;
            case Kind::Fade:
                t=16.0*(static_cast<double>(n-1U-index)/
                    static_cast<double>(n-1U))
                    *(index%8U<4U?1.0:-1.0);
                break;
            case Kind::Sparse:
                t=(index%2048U==0U)
                    ?(8.0/(1.0+static_cast<double>(index)/2048.0))
                        *(index%4096U==0U?1.0:-1.0):0.0;
                break;
            case Kind::LowFrequency:
                t=8.0*std::exp(-static_cast<double>(index)/22000.0)
                    *std::sin(17.0*angle);
                break;
            case Kind::IdenticalStereo:
                t=0.2*std::sin(997.0*angle)
                    +0.16*std::sin(113.0*angle);
                break;
            }
            // Identical stereo binary64 pre-dither program: all stereo
            // differences must come only from per-channel addressed TPDF.
            for(std::uint32_t ch=0;ch<channels;++ch)
                destination[static_cast<std::size_t>(f)*channels+ch]
                    =t*0x1.0p-23;
        }
        return true;
    }
};

struct Folder {fs::path path;~Folder(){if(!path.empty()){
    std::error_code ec;fs::remove_all(path,ec);}}};

int main(int argc,char** argv){
    bool emit=argc==3 && std::string(argv[1])=="--emit-fixtures";
    if(argc!=1&&!emit)return 2;
    std::error_code ec;
    fs::path dir;
    if(emit){
        dir=argv[2];
        if(!fs::is_directory(dir,ec)||ec)return 3;
    }else{
        dir=fs::temp_directory_path(ec)/(
            "m18-audio-suite-"+std::to_string(std::chrono::steady_clock::now()
                .time_since_epoch().count()));
        if(ec||!fs::create_directory(dir,ec)||ec)return 4;
    }
    Folder cleanup{emit?fs::path{}:dir};
    constexpr std::array<const char*,6> names{{
        "sine_025lsb","sine_2lsb","fade_16lsb","sparse_tail",
        "low_frequency_decay","identical_stereo_ambience"}};
    constexpr auto seed=UINT64_C(0x0123456789ABCDEF);
    std::size_t count=0;
    for(unsigned i=0;i<names.size();++i){
        for(const auto rate:(i==0U?std::array<std::uint32_t,2>{{48000U,192000U}}
                                    :std::array<std::uint32_t,2>{{48000U,0U}})){
            if(rate==0U)continue;
            Probe probe;probe.kind=static_cast<Probe::Kind>(i);
            const std::uint32_t channels=i==5U?2U:1U;
            const M18WavSpec spec{
                M18WavEncoding::PCM24,rate,channels,Probe::n,seed};
            const std::string filename=std::string(names[i])+"_"+
                std::to_string(rate)+"_"+std::to_string(channels)+".wav";
            const fs::path output=dir/filename;
            const auto result=m18_export_offline_candidate(
                probe,output,spec,0U);
            if(result.status!=M18OfflineExportStatus::OK||!result.receipt){
                std::cerr<<"M18_PATHOLOGY_EXPORT_FAIL="<<filename<<"\n";
                return 10;
            }
            if(emit)
                std::cout<<"M18_AUDIO\t"<<filename<<"\t"<<names[i]
                    <<"\t"<<rate<<"\t"<<channels<<"\t"<<Probe::n
                    <<"\t"<<result.receipt->full_file_sha256
                    <<"\t"<<result.receipt->payload_sha256<<"\n";
            ++count;
        }
    }
    if(count!=7U)return 11;
    std::cout<<"M18_SEVEN_LOWLEVEL_WAV_PROGRAMS_PASS_NOT_LISTENING_FQ\n";
    return 0;
}
