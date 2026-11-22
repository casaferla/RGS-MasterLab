#include "transient_activity_playback_projection.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>

using namespace rgsml;
using namespace rgsml::research::transient_ui;
using rgsml::audio::FrameDomainId;
using rgsml::core::RealizationId;

static core::PlaybackSnapshot playback(std::int64_t position,
                                       core::PlaybackState state) {
    core::PlaybackSnapshot p;
    p.state=state;
    p.position=core::FrameIndex(position);
    p.audibleRealization.phase=core::AudibleHandoffPhase::NEW;
    p.audibleRealization.realizationId=RealizationId{42};
    return p;
}

int main() {
    TransientActivityCollector collector(48000,0,5);
    for (std::uint64_t i=0;i<960U*2U;++i)
        assert(collector.push(i,1.5,-0.5));
    assert(collector.finish());
    TransientActivityRenderIdentity sidecar{
        RealizationId{42},19,
        FrameDomainId::SOURCE_PROCESSING_RATE,48000,true};
    auto snap=playback(960,core::PlaybackState::PLAYING);
    const auto source=FrameDomainId::SOURCE_PROCESSING_RATE;
    auto project=[&](bool audition=true,bool bypass=false,
                     std::uint64_t revision=19,
                     FrameDomainId playback_domain=
                         FrameDomainId::SOURCE_PROCESSING_RATE,
                     std::uint32_t sample_rate=48000) {
        return project_transient_activity(
            collector,sidecar,snap,playback_domain,sample_rate,
            revision,audition,bypass);
    };
    {
        const auto x=project();
        assert(x.status==PlaybackActivityStatus::ACTIVE);
        assert(x.available() && x.history.size()==1U);
        assert(x.history[0].frame_end_exclusive==960);
        assert(x.audible_frame==960 && x.realization_id
               && x.realization_id->value==42);
    }
    {
        snap=playback(1920,core::PlaybackState::PAUSED);
        const auto x=project();
        assert(x.status==PlaybackActivityStatus::PAUSED);
        assert(x.available() && x.history.size()==2U);
        assert(x.history.back().frame_end_exclusive==1920U);
    }
    {
        // Seek back: the same immutable accepted history remains valid.
        snap=playback(960,core::PlaybackState::PLAYING);
        assert(project().history.size()==1U);
        assert(project(false).status
               ==PlaybackActivityStatus::NOT_AUDITIONING_PROCESSED);
        assert(project(true,true).status==PlaybackActivityStatus::BYPASS);
        assert(project(true,false,18).status
               ==PlaybackActivityStatus::STALE_REVISION);
        assert(project(true,false,19,FrameDomainId::OUTPUT_RATE).status
               ==PlaybackActivityStatus::FRAME_DOMAIN_MISMATCH);
        assert(project(true,false,19,source,44100).status
               ==PlaybackActivityStatus::SAMPLE_RATE_MISMATCH);
    }
    {
        snap.audibleRealization.realizationId=RealizationId{41};
        assert(project().status==PlaybackActivityStatus::STALE_REALIZATION);
        snap.audibleRealization.realizationId=RealizationId{42};
        snap.audibleRealization.phase=core::AudibleHandoffPhase::TRANSITION;
        assert(project().status==PlaybackActivityStatus::TRANSITION);
        snap.audibleRealization.phase=core::AudibleHandoffPhase::NEW;
        snap.state=core::PlaybackState::STOPPED;
        assert(project().status==PlaybackActivityStatus::STOPPED);
        snap=playback(0,core::PlaybackState::PLAYING);
        assert(project().status==PlaybackActivityStatus::INCOMPLETE_TELEMETRY);
        snap=playback(-5,core::PlaybackState::PLAYING);
        assert(project().status==PlaybackActivityStatus::INCOMPLETE_TELEMETRY);
    }
    {
        sidecar.sidecar_valid=false;
        assert(project().status==PlaybackActivityStatus::UNAVAILABLE);
        sidecar.sidecar_valid=true;
        sidecar.realization_id=RealizationId{0};
        assert(project().status==PlaybackActivityStatus::UNAVAILABLE);
    }
    std::cout<<"transient playback realization/domain/revision binding: PASS\n";
}
