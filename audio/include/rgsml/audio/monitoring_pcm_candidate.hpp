#pragma once

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/full_resource_output_rate_candidate.hpp>
#include <rgsml/audio/master_monitoring_rate_plan.hpp>
#include <rgsml/core/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>

namespace rgsml::audio {

// EXPERIMENTAL and OFFLINE only. This owner is NOT an M17 final master,
// verified terminal output, direct Windows sink, or qualified M16 LIVE tap.
// The device-rate PCM has a distinct allocation and distinct provenance.
// It can only be installed by a future explicit monitoring-only transport.
struct MonitoringPcmCandidate final {
    AudioBuffer device_pcm;
    core::SampleRate immutable_master_rate;
    core::SampleRate device_rate;
    core::FrameCount immutable_master_frames;
    bool device_src_applied;
};

// Validate an already-materialized monitor owner before touching any hardware.
// This proves only structural ownership/rate/duration metadata; it does not
// qualify audio fidelity, playback continuity, M17 or live M16 measurements.
[[nodiscard]] inline core::Status validate_monitoring_pcm_candidate(
    const MonitoringPcmCandidate& candidate)
{
    auto rejected=[](const char* message)->core::Status {
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidArgument,message});
    };
    const auto sourceHz=candidate.immutable_master_rate.value();
    const auto deviceHz=candidate.device_rate.value();
    const auto frames=candidate.immutable_master_frames.value();
    auto plan=plan_master_monitoring_rate(
        candidate.immutable_master_rate,candidate.device_rate);
    if(!plan)return core::Status::failure(*plan.error());
    if(deviceHz!=44100 && deviceHz!=48000 && deviceHz!=88200
       && deviceHz!=96000 && deviceHz!=176400 && deviceHz!=192000)
        return rejected("MONITOR_CANDIDATE_DEVICE_RATE_UNSUPPORTED");
    const auto pcm=candidate.device_pcm.view();
    if(pcm.timebase().frame_domain_id()!=FrameDomainId::OUTPUT_RATE
       ||pcm.absolute_start_frame().value()!=0
       ||pcm.frame_count().value()<=0
       ||pcm.format().sample_rate()!=candidate.device_rate
       ||frames<=0
       ||candidate.device_src_applied!=plan.value()->monitoring_src_required
       ||(pcm.format().channel_layout()!=ChannelLayout::MONO_C
           &&pcm.format().channel_layout()!=ChannelLayout::STEREO_LR))
        return rejected("MONITOR_CANDIDATE_PROVENANCE_MISMATCH");
    const auto q=frames/sourceHz;
    const auto r=frames%sourceHz;
    const auto extra=(r*deviceHz+sourceHz-1)/sourceHz;
    if(q>(std::numeric_limits<std::int64_t>::max()-extra)/deviceHz)
        return rejected("MONITOR_CANDIDATE_FRAME_COUNT_OVERFLOW");
    if(pcm.frame_count().value()!=q*deviceHz+extra)
        return rejected("MONITOR_CANDIDATE_FRAME_COUNT_MISMATCH");
    return core::Status::success();
}

// Prototype of the independent M17/M18 -> hardware monitoring boundary.
// The immutable M17 PCM is borrowed read-only and never retimed or modified.
// Reuses the offline candidate SRC in a separate SOURCE-domain scratch owner:
// no reinterpretation of OUTPUT_RATE as SOURCE_RATE and no second M17 render.
// This deliberately allocates a full scratch PCM; performance, cancellation,
// device handoff, loop scheduling and LIVE provenance are NOT qualified.
[[nodiscard]] inline core::Result<MonitoringPcmCandidate>
materialize_monitoring_pcm_candidate(
    AudioBufferView verified_master_pcm,
    core::SampleRate observed_device_rate,
    core::FrameCount offline_block)
{
    auto refused=[](core::ErrorCode code,const char* message)
        -> core::Result<MonitoringPcmCandidate> {
        return core::Result<MonitoringPcmCandidate>::failure(
            core::Error{code,message});
    };
    if(verified_master_pcm.timebase().frame_domain_id()
           !=FrameDomainId::OUTPUT_RATE
       ||verified_master_pcm.absolute_start_frame().value()!=0
       ||verified_master_pcm.frame_count().value()<=0
       ||offline_block.value()<=0
       ||offline_block.value()>PlaybackSampleRateAdapter::kMaximumOutputBlockFrames)
        return refused(core::ErrorCode::InvalidArgument,
            "MONITOR_SRC_MASTER_OR_BLOCK_INVALID: require full zero-origin OUTPUT_RATE master.");
    const auto master_rate=verified_master_pcm.format().sample_rate();
    auto plan=plan_master_monitoring_rate(master_rate,observed_device_rate);
    if(!plan)
        return core::Result<MonitoringPcmCandidate>::failure(*plan.error());
    const auto device_hz=observed_device_rate.value();
    if(device_hz!=44100 && device_hz!=48000 && device_hz!=88200
       && device_hz!=96000 && device_hz!=176400 && device_hz!=192000)
        return refused(core::ErrorCode::UnsupportedOperation,
            "MONITOR_SRC_DEVICE_RATE_UNSUPPORTED: experimental converter accepts six rates.");

    // Make the domain conversion explicit with a separate scratch allocation.
    // A copied SOURCE_PROCESSING_RATE view is for the offline SRC API only,
    // never for publication as a real audio Source or a verified master.
    auto scratch=AudioBuffer::create(verified_master_pcm.format(),
        FrameDomainId::SOURCE_PROCESSING_RATE,core::FrameIndex{0},
        verified_master_pcm.frame_count());
    if(!scratch)
        return core::Result<MonitoringPcmCandidate>::failure(*scratch.error());
    for(std::size_t ch=0;ch<verified_master_pcm.format().channel_count();++ch) {
        auto from=verified_master_pcm.channel(ch);
        auto to=scratch.value()->mutable_view().channel(ch);
        if(!from||!to)
            return refused(core::ErrorCode::InvalidState,
                "MONITOR_SRC_CHANNEL_MISSING");
        std::copy(from.value()->begin(),from.value()->end(),to.value()->begin());
    }
    auto converted=materialize_full_resource_output_rate_candidate(
        scratch.value()->view(),observed_device_rate,offline_block);
    if(!converted)
        return core::Result<MonitoringPcmCandidate>::failure(*converted.error());
    if(converted.value()->pcm.view().format().sample_rate()
           !=observed_device_rate
       ||converted.value()->pcm.view().timebase().frame_domain_id()
           !=FrameDomainId::OUTPUT_RATE)
        return refused(core::ErrorCode::InvalidState,
            "MONITOR_SRC_OUTPUT_PROVENANCE_INVALID");
    MonitoringPcmCandidate candidate{
        std::move(converted.value()->pcm),master_rate,observed_device_rate,
        verified_master_pcm.frame_count(),plan.value()->monitoring_src_required
    };
    const auto checked=validate_monitoring_pcm_candidate(candidate);
    if(!checked)
        return core::Result<MonitoringPcmCandidate>::failure(*checked.error());
    return core::Result<MonitoringPcmCandidate>::success(std::move(candidate));
}
} // namespace rgsml::audio
