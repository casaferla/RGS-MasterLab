#include "dynamic_eq_activity_playback_projection.hpp"
#include <rgsml/dsp/dynamic_eq_parameters.hpp>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <span>
#include <vector>

using namespace rgsml;
using namespace rgsml::dsp;
using namespace rgsml::dsp::internal;
using namespace rgsml::research::dynamic_ui;
using audio::FrameDomainId;
static DynamicEqParameters parameters() {
    DynamicEqBandSpec s;
    const auto uid=core::Uuid::parse(
        "11111111-1111-4111-8111-111111111111");
    assert(uid);s.band_id=*uid.value();
    s.control.threshold_dbfs=-54.0;
    s.control.range_db=-6.0;
    const auto band=DynamicEqBandParameters::create(s);assert(band);
    const auto p=DynamicEqParameters::create({*band.value()});assert(p);
    return *p.value();
}
int main() {
    constexpr auto fs=48000U;
    auto engine=C01FrameEngine::create(parameters(),fs,1U);
    assert(engine);
    C01ActivityHistoryCollector history(fs,0,120);
    std::vector<C01FrameActivitySample> scratch(601);
    std::vector<double> in(601),out(601);
    constexpr std::size_t frames=fs*2U;
    for(std::size_t at=0;at<frames;) {
        const auto n=std::min(in.size(),frames-at);
        for(std::size_t i=0;i<n;++i) {
            const auto f=static_cast<double>(at+i);
            in[i]=0.3*std::sin(2.0*std::numbers::pi*441.0*f/fs);
        }
        assert(engine.value()->process_block(
            std::span<const double>(in.data(),n),{},
            std::span<double>(out.data(),n),{},
            &history,at,scratch));
        at+=n;
    }
    assert(history.finish());
    C01ActivityRenderProvenance id{
        core::RealizationId{4242},31U,
        FrameDomainId::SOURCE_PROCESSING_RATE,fs,true};
    core::PlaybackSnapshot snap;
    snap.state=core::PlaybackState::PLAYING;
    snap.position=core::FrameIndex(fs);
    snap.audibleRealization.phase=core::AudibleHandoffPhase::NEW;
    snap.audibleRealization.realizationId=core::RealizationId{4242};
    auto projection=[&](std::uint64_t revision=31U,
                        bool processed=true,bool bypass=false,
                        FrameDomainId domain=
                            FrameDomainId::SOURCE_PROCESSING_RATE,
                        std::uint32_t actual_fs=48000U) {
        return project_c01_activity(
            history,id,snap,domain,actual_fs,
            revision,processed,bypass);
    };
    {
        const auto result=projection();
        assert(result.status==C01ProjectionStatus::ACTIVE);
        assert(result.available() && result.history.size()==50U);
        assert(result.audible_frame==fs);
        assert(result.history.back().frame_end_exclusive==fs);
        assert(result.history[0].bands[0].band_id==
               *core::Uuid::parse("11111111-1111-4111-8111-111111111111").value());
    }
    snap.state=core::PlaybackState::PAUSED;
    assert(projection().status==C01ProjectionStatus::PAUSED);
    snap.position=core::FrameIndex(fs/2U);
    assert(projection().history.size()==25U); // seek backwards
    assert(projection(30U).status==C01ProjectionStatus::STALE_REVISION);
    assert(projection(31U,false).status==C01ProjectionStatus::NOT_PROCESSED);
    assert(projection(31U,true,true).status==C01ProjectionStatus::BYPASS);
    assert(projection(31U,true,false,FrameDomainId::OUTPUT_RATE).status
           ==C01ProjectionStatus::FRAME_DOMAIN_MISMATCH);
    assert(projection(31U,true,false,
                      FrameDomainId::SOURCE_PROCESSING_RATE,44100U).status
           ==C01ProjectionStatus::SAMPLE_RATE_MISMATCH);
    snap.state=core::PlaybackState::PLAYING;
    snap.audibleRealization.realizationId=core::RealizationId{4444};
    assert(projection().status==C01ProjectionStatus::STALE_REALIZATION);
    snap.audibleRealization.phase=core::AudibleHandoffPhase::TRANSITION;
    assert(projection().status==C01ProjectionStatus::TRANSITION);
    snap.state=core::PlaybackState::STOPPED;
    assert(projection().status==C01ProjectionStatus::STOPPED);
    snap.state=core::PlaybackState::PLAYING;
    snap.audibleRealization.phase=core::AudibleHandoffPhase::NEW;
    snap.audibleRealization.realizationId=core::RealizationId{4242};
    snap.position=core::FrameIndex(0);
    assert(projection().status==C01ProjectionStatus::HISTORY_INCOMPLETE);
    id.sidecar_accepted=false;
    assert(projection().status==C01ProjectionStatus::UNAVAILABLE);
    std::cout<<"C01 history accepted PCM projection: PASS\n";
}
