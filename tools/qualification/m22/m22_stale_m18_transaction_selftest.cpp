// M22 private stale epoch against ACTUAL M18 offline transaction.
// Deliberately inactivate after first full-length export chunk; no WAV
// publication is permitted on obsolete M22 job. NOT product scheduler/FQ.
#include "m22_publication_epoch_candidate.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs=std::filesystem;
using namespace rgsml::m22_private;
using namespace rgsml::audio::internal;
#define REQUIRE(x) do{if(!(x))throw std::runtime_error(std::string("M22_STALE_FAIL_LINE_")+std::to_string(__LINE__));}while(0)

static Intent baseline(Format fmt=Format::PCM24) {
    Intent in;
    in.source_identity="SOURCE";in.source_revision="revision-A";
    in.prepared_identity="PREPARED";in.prepared_revision="revision-B";
    in.chain_identity="chain-owner";in.chain_revision=4;
    in.source_frames=4249;in.window={0,4249};in.mode=Mode::FULL;
    in.processing_rate_hz=48000;in.target_rate_hz=48000;in.channels=2;
    in.terminal_limiter_signature="M17-exact-full-owner";
    in.terminal_limiter_parameters="ceiling-revision-4";
    in.execution_policy_signature="rgsml-binary64-exact";
    in.format=fmt;
    if(fmt==Format::PCM24)in.dither_seed=91;
    in.stored_order={{"gain",InlineKind::GAIN,"algorithm-1","params-1",false,0}};
    return in;
}
struct DisposableDir {
    fs::path p;
    ~DisposableDir(){std::error_code ec;fs::remove_all(p,ec);}
};
static bool clean_stages(const fs::path& dir){
    for(const auto& item:fs::directory_iterator(dir))
        if(item.path().filename().string().find(".rgsml-m18-private-")!=
            std::string::npos)return false;
    return true;
}
struct PublishCandidateFrames final:M18MasterFrameProvider {
    M22PrivatePublicationEpoch& guard;
    bool invalidate_on_second_read{false};
    std::uint32_t calls{0};
    explicit PublishCandidateFrames(M22PrivatePublicationEpoch& g):guard(g){}
    bool read_exact(std::uint64_t absolute,std::uint32_t count,
                    std::uint32_t channels,std::span<double> out) noexcept override{
        ++calls;
        if(channels!=2 || out.size()!=static_cast<std::size_t>(count)*channels)
            return false;
        for(std::size_t n=0;n<out.size();++n){
            const auto f=absolute+n/channels;
            out[n]=(static_cast<double>((f*11U+n%channels)%257U)-128.0)/1024.0;
        }
        if(invalidate_on_second_read && calls==2)
            guard.invalidate();
        return true;
    }
};
int main(){
try {
    const auto path=fs::temp_directory_path()/("rgsml-m22-stale-"+std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code ec;
    REQUIRE(fs::create_directory(path,ec) && !ec);
    DisposableDir cleanup{path};
    for(auto fmt:{Format::PCM24,Format::FLOAT32}){
        Failure failure{};
        const auto initial=baseline(fmt);
        auto req=ResolvedRenderRequest::freeze(initial,failure);
        REQUIRE(req && failure==Failure::NONE);
        auto plan=RenderExecutionPlan::preflight(*req,failure);
        REQUIRE(plan);
        M22PrivatePublicationEpoch epoch;
        auto token=epoch.begin(*plan,"REAL-M17-OWNER-IDENTITY-PLACEHOLDER");
        REQUIRE(token);
        REQUIRE(epoch.still_current(*token,initial,
              "REAL-M17-OWNER-IDENTITY-PLACEHOLDER"));
        REQUIRE(!epoch.still_current(*token,initial,"OTHER-OWNER"));
        auto changed=initial;changed.source_revision="changed-source";
        REQUIRE(!epoch.still_current(*token,changed,
              "REAL-M17-OWNER-IDENTITY-PLACEHOLDER"));
        changed=initial;changed.prepared_revision="new-prepared";
        REQUIRE(!epoch.still_current(*token,changed,
              "REAL-M17-OWNER-IDENTITY-PLACEHOLDER"));
        changed=initial;changed.chain_revision++;
        REQUIRE(!epoch.still_current(*token,changed,
              "REAL-M17-OWNER-IDENTITY-PLACEHOLDER"));
        changed=initial;changed.stored_order[0].parameter_signature="new-EQ";
        REQUIRE(!epoch.still_current(*token,changed,
              "REAL-M17-OWNER-IDENTITY-PLACEHOLDER"));
        changed=initial;changed.terminal_limiter_parameters="new-ceiling";
        REQUIRE(!epoch.still_current(*token,changed,
              "REAL-M17-OWNER-IDENTITY-PLACEHOLDER"));
        changed=initial;changed.format=fmt==Format::PCM24?Format::FLOAT32:Format::PCM24;
        REQUIRE(!epoch.still_current(*token,changed,
              "REAL-M17-OWNER-IDENTITY-PLACEHOLDER"));
        changed=initial;changed.window={73,4199};changed.mode=Mode::PREVIEW;
        REQUIRE(!epoch.still_current(*token,changed,
              "REAL-M17-OWNER-IDENTITY-PLACEHOLDER"));
        // Audition-only loop, region audition gain and UI selection are
        // NOT sound-identity fields; no false invalidation occurs here.
        REQUIRE(epoch.still_current(*token,initial,
              "REAL-M17-OWNER-IDENTITY-PLACEHOLDER"));
        M22PrivateM18StaleStop stop(epoch,*token);
        REQUIRE(!stop.cancelled());
        PublishCandidateFrames provider(epoch);
        provider.invalidate_on_second_read=true;
        auto target=path/(fmt==Format::PCM24?"stale-p24.wav":"stale-f32.wav");
        const auto result=m22_export_existing_m18_candidate(
            *plan,provider,target,&stop);
        REQUIRE(provider.calls==2);
        REQUIRE(result.status==M18OfflineExportStatus::CANCELLED);
        REQUIRE(!fs::exists(target) && clean_stages(path));
        REQUIRE(stop.cancelled());
        // Old ticket remains stale after a newer job takes its epoch.
        auto replacement=epoch.begin(*plan,"REAL-M17-OWNER-IDENTITY-PLACEHOLDER");
        REQUIRE(replacement && replacement->writer_epoch>token->writer_epoch);
        REQUIRE(!epoch.still_current(*token,initial,
              "REAL-M17-OWNER-IDENTITY-PLACEHOLDER"));
        REQUIRE(epoch.still_current(*replacement,initial,
              "REAL-M17-OWNER-IDENTITY-PLACEHOLDER"));
        M22PrivateM18StaleStop freshStop(epoch,*replacement);
        PublishCandidateFrames good(epoch);
        const auto finished=m22_export_existing_m18_candidate(
            *plan,good,target,&freshStop);
        REQUIRE(finished.status==M18OfflineExportStatus::OK && finished.receipt);
        REQUIRE(fs::exists(target));
        REQUIRE(clean_stages(path));
        epoch.invalidate();
        REQUIRE(freshStop.cancelled());
        fs::remove(target);
    }
    std::cout<<"M22_PRIVATE_REAL_M18_STALE_CANCELLATION_ATOMIC_PASS_NOT_PRODUCT_SCHEDULER\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
