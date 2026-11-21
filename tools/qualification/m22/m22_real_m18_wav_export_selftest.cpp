// M22 private REAL M18 writer seam: Full and region files are created with
// the existing M18 transactional publisher. Input is deterministic synthetic
// double MASTER-style PCM, NOT real M17 output or an authenticated realization.
#include "m22_m18_offline_export_port_candidate.hpp"
#include "internal/m18_wav_candidate.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs=std::filesystem;
using namespace rgsml::m22_private;
using namespace rgsml::audio::internal;
#define REQUIRE(x) do{if(!(x)) throw std::runtime_error(std::string("M22_M18_WAV_FAIL_LINE_")+std::to_string(__LINE__));}while(0)

static constexpr std::uint64_t N=4223;
static constexpr Window crop{41,4215};

static Intent make_intent(std::uint32_t rate,std::uint8_t ch,Format fmt,Mode mode){
    Intent in;
    in.source_identity="immutable-source";in.source_revision="read-only-rev-1";
    in.prepared_identity="prepared-realization";in.prepared_revision="rev-2";
    in.chain_identity="m22-typed-chain";in.chain_revision=44;
    in.source_frames=N; in.window=mode==Mode::FULL?Window{0,N}:crop;in.mode=mode;
    in.processing_rate_hz=rate;in.target_rate_hz=rate;in.channels=ch;
    in.terminal_limiter_signature="M17-verified-artifact-placeholder";
    in.terminal_limiter_parameters="frozen-configuration-placeholder";
    in.execution_policy_signature="binary64-preview-full-identity";
    in.format=fmt;
    if(fmt==Format::PCM24)in.dither_seed=UINT64_C(0xDEADBEEFBADDCAFE);
    return in;
}
static std::vector<double> sound(std::uint8_t ch){
    std::vector<double> samples(N*ch);
    for(std::uint64_t f=0;f<N;++f)for(std::uint8_t c=0;c<ch;++c) {
        const auto code=static_cast<std::int64_t>((f*59U+c*113U)%1501U)-750;
        samples[static_cast<std::size_t>(f)*ch+c]=static_cast<double>(code)/2048.0;
    }
    return samples;
}
struct ExactProvider final:M18MasterFrameProvider {
    const std::vector<double>& x;
    std::uint32_t source_channels;
    bool fail_read{false};
    bool corrupt_pcm24{false};
    std::uint32_t calls{0};
    explicit ExactProvider(const std::vector<double>& samples,std::uint32_t channels)
        :x(samples),source_channels(channels){}
    bool read_exact(std::uint64_t first,std::uint32_t count,
                    std::uint32_t channels,std::span<double> output) noexcept override{
        ++calls;
        if(fail_read && calls>=2)return false;
        const auto full_frames=x.size()/source_channels;
        if(channels!=source_channels || first>full_frames || count>full_frames-first
            || output.size()!=static_cast<std::size_t>(count)*channels)
            return false;
        const auto index=static_cast<std::size_t>(first)*channels;
        std::copy_n(x.begin()+static_cast<std::ptrdiff_t>(index),output.size(),output.begin());
        if(corrupt_pcm24 && !output.empty())output[0]=2.0;
        return true;
    }
};
struct TempDir {
    fs::path root;
    explicit TempDir(fs::path p):root(std::move(p)){}
    ~TempDir(){std::error_code ec;fs::remove_all(root,ec);}
};
static std::vector<std::uint8_t> read_file(const fs::path& p){
    std::ifstream in(p,std::ios::binary);
    REQUIRE(in.good());
    return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
static bool clean_private_staging(const fs::path& p) {
    for(const auto& item:fs::directory_iterator(p))
        if(item.path().filename().string().find(".rgsml-m18-private-")!=std::string::npos)
            return false;
    return true;
}
int main() {
try {
    std::error_code ec;
    auto dir=fs::temp_directory_path(ec)/("rgsml-m22-private-"+std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    REQUIRE(!ec && fs::create_directory(dir,ec) && !ec);
    TempDir cleanup(dir);
    constexpr std::array<std::uint32_t,6> rates{44100,48000,88200,96000,176400,192000};
    for(auto rate:rates)for(auto channels:{std::uint8_t{1},std::uint8_t{2}})
    for(auto fmt:{Format::PCM24,Format::FLOAT32}) {
        auto pcm=sound(channels);
        const auto immutable=pcm;
        Failure err{};
        auto wholeRequest=ResolvedRenderRequest::freeze(
            make_intent(rate,channels,fmt,Mode::FULL),err);
        REQUIRE(wholeRequest && err==Failure::NONE);
        auto wholePlan=RenderExecutionPlan::preflight(*wholeRequest,err);
        REQUIRE(wholePlan && err==Failure::NONE);
        auto previewRequest=ResolvedRenderRequest::freeze(
            make_intent(rate,channels,fmt,Mode::PREVIEW),err);
        REQUIRE(previewRequest && err==Failure::NONE);
        auto previewPlan=RenderExecutionPlan::preflight(*previewRequest,err);
        REQUIRE(previewPlan && err==Failure::NONE);
        auto fullSpec=m22_m18_wav_spec(*wholePlan);
        auto previewSpec=m22_m18_wav_spec(*previewPlan);
        REQUIRE(fullSpec && previewSpec);
        REQUIRE(fullSpec->frame_count==N);
        REQUIRE(previewSpec->frame_count==crop.end-crop.first);
        REQUIRE(previewPlan->absolute_dither_first_frame()==crop.first);
        REQUIRE(fullSpec->sample_rate==previewSpec->sample_rate);
        REQUIRE(fullSpec->channels==previewSpec->channels);
        REQUIRE(fullSpec->encoding==previewSpec->encoding);
        if(fmt==Format::FLOAT32)REQUIRE(previewSpec->dither_seed==0);
        const auto tag=std::to_string(rate)+"-"+std::to_string(channels)+"-"+
            (fmt==Format::PCM24?"p24":"f32");
        const auto fullFile=dir/("full-"+tag+".wav");
        const auto regionFile=dir/("region-"+tag+".wav");
        ExactProvider fullProvider(pcm,channels);
        const auto first=m22_export_existing_m18_candidate(
            *wholePlan,fullProvider,fullFile);
        REQUIRE(first.status==M18OfflineExportStatus::OK && first.receipt);
        REQUIRE(first.receipt->frames==N);
        ExactProvider regionProvider(pcm,channels);
        const auto second=m22_export_existing_m18_candidate(
            *previewPlan,regionProvider,regionFile);
        REQUIRE(second.status==M18OfflineExportStatus::OK && second.receipt);
        REQUIRE(second.receipt->frames==crop.end-crop.first);
        REQUIRE(fullProvider.calls==2 && regionProvider.calls==2);
        const auto bytesFull=read_file(fullFile);
        const auto bytesRegion=read_file(regionFile);
        const auto fullWav=m18_plan_wav(*fullSpec);
        const auto regionWav=m18_plan_wav(*previewSpec);
        REQUIRE(fullWav.status==M18WavStatus::OK && regionWav.status==M18WavStatus::OK);
        REQUIRE(bytesFull.size()==fullWav.plan.file_bytes);
        REQUIRE(bytesRegion.size()==regionWav.plan.file_bytes);
        const std::size_t sample_bytes=fmt==Format::PCM24?3U:4U;
        const auto payloadOffset=static_cast<std::size_t>(crop.first)*
            channels*sample_bytes;
        const auto regionLength=static_cast<std::size_t>(crop.end-crop.first)*
            channels*sample_bytes;
        REQUIRE(std::equal(
            bytesRegion.begin()+regionWav.plan.header_bytes,
            bytesRegion.end(),
            bytesFull.begin()+fullWav.plan.header_bytes+
               static_cast<std::ptrdiff_t>(payloadOffset)));
        REQUIRE(regionLength==bytesRegion.size()-regionWav.plan.header_bytes);
        const auto segment=std::span<const double>(pcm).subspan(
            static_cast<std::size_t>(crop.first)*channels,
            static_cast<std::size_t>(crop.end-crop.first)*channels);
        const auto oracle=m18_make_bounded_wav_candidate(*previewSpec,segment,crop.first);
        REQUIRE(oracle.status==M18WavStatus::OK && oracle.bytes==bytesRegion);
        REQUIRE(pcm==immutable);
        REQUIRE(clean_private_staging(dir));
        // Existing targets may not be overwritten, even by a byte-identical replay.
        ExactProvider collisionProvider(pcm,channels);
        const auto collision=m22_export_existing_m18_candidate(
            *previewPlan,collisionProvider,regionFile);
        REQUIRE(collision.status==M18OfflineExportStatus::TARGET_EXISTS);
        REQUIRE(collisionProvider.calls==0 && read_file(regionFile)==bytesRegion);
        // An invalid upstream provider must not publish partial output.
        const auto failedFile=dir/("failed-"+tag+".wav");
        ExactProvider badProvider(pcm,channels);
        if(fmt==Format::PCM24)badProvider.corrupt_pcm24=true;
        else badProvider.fail_read=true;
        const auto failed=m22_export_existing_m18_candidate(
            *wholePlan,badProvider,failedFile);
        REQUIRE(failed.status==M18OfflineExportStatus::STREAM_FAILED
             ||failed.status==M18OfflineExportStatus::SOURCE_FAILED);
        REQUIRE(!fs::exists(failedFile) && clean_private_staging(dir));
    }
    std::cout<<"M22_REAL_M18_DISK_FULL_PREVIEW_24_COMBINATIONS_PASS_NOT_REAL_M17_OR_GATE_I_G_L\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
