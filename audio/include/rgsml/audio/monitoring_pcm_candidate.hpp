#pragma once

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/full_resource_output_rate_candidate.hpp>
#include <rgsml/audio/master_monitoring_rate_plan.hpp>
#include <rgsml/core/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
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

// Read-only preflight for FULL-DURATION monitoring PCM. Counts only the
// already-owned immutable MASTER and separately materialized DEVICE buffer:
// not peak process RSS, Qt buffers, OS audio memory or allocation latency.
// The work estimate is only for the *experimental rational FIR*, not the
// frozen 44.1<->48 kernels, and is not a measured duration.
struct MonitoringPcmResourceProjection final {
    core::FrameCount device_frames;
    std::uint64_t master_pcm_bytes;
    std::uint64_t device_pcm_bytes;
    std::uint64_t combined_pcm_bytes;
    std::optional<std::uint64_t> experimental_fir_mac_operations;
};

[[nodiscard]] inline core::Result<MonitoringPcmResourceProjection>
project_monitoring_pcm_resource_cost(
    core::SampleRate masterRate,core::FrameCount masterFrames,
    core::SampleRate deviceRate,ChannelLayout layout)
{
    using Projection=MonitoringPcmResourceProjection;
    const auto invalid=[](core::ErrorCode code,const char* message)
        ->core::Result<Projection> {
        return core::Result<Projection>::failure(core::Error{code,message});
    };
    const auto sourceHz=masterRate.value();
    const auto deviceHz=deviceRate.value();
    const auto frames=masterFrames.value();
    if(frames<=0)
        return invalid(core::ErrorCode::InvalidArgument,
            "MONITOR_RESOURCE_MASTER_FRAMES_INVALID");
    const auto plan=plan_master_monitoring_rate(masterRate,deviceRate);
    if(!plan)return core::Result<Projection>::failure(*plan.error());
    if(deviceHz!=44100 && deviceHz!=48000 && deviceHz!=88200
       && deviceHz!=96000 && deviceHz!=176400 && deviceHz!=192000)
        return invalid(core::ErrorCode::UnsupportedOperation,
            "MONITOR_RESOURCE_DEVICE_RATE_UNSUPPORTED");
    auto format=AudioFormat::create(masterRate,layout);
    if(!format)return core::Result<Projection>::failure(*format.error());
    const auto q=frames/sourceHz;
    const auto r=frames%sourceHz;
    const auto tail=(r*deviceHz+sourceHz-1)/sourceHz;
    const auto maxFrames=std::numeric_limits<std::int64_t>::max();
    if(q>(maxFrames-tail)/deviceHz)
        return invalid(core::ErrorCode::IntegerOverflow,
            "MONITOR_RESOURCE_DEVICE_FRAMES_OVERFLOW");
    const auto outFrames=q*deviceHz+tail;
    auto count=core::FrameCount::create(outFrames);
    if(!count)return core::Result<Projection>::failure(*count.error());
    const auto bytesPerFrame=static_cast<std::uint64_t>(
        format.value()->channel_count()*sizeof(double));
    const auto maxBytes=static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max());
    if(static_cast<std::uint64_t>(frames)>maxBytes/bytesPerFrame
       ||static_cast<std::uint64_t>(outFrames)>maxBytes/bytesPerFrame)
        return invalid(core::ErrorCode::IntegerOverflow,
            "MONITOR_RESOURCE_PCM_BYTES_OVERFLOW");
    const auto masterBytes=static_cast<std::uint64_t>(frames)*bytesPerFrame;
    const auto deviceBytes=static_cast<std::uint64_t>(outFrames)*bytesPerFrame;
    // Each allocation respects AudioBuffer's checked signed-int64 byte
    // ceiling; the combined two-buffer minimum fits in uint64.
    const bool frozenPair=(sourceHz==44100 && deviceHz==48000)
        ||(sourceHz==48000 && deviceHz==44100);
    std::optional<std::uint64_t> macs;
    if(sourceHz!=deviceHz && !frozenPair) {
        // Mirrors the rational FIR's tap geometry, without allocating PCM.
        auto taps=(128*sourceHz+deviceHz-1)/deviceHz;
        if(taps<128)taps=128;
        taps+=taps%2;
        const auto channels=static_cast<std::uint64_t>(
            format.value()->channel_count());
        const auto perOutput=static_cast<std::uint64_t>(taps)*channels;
        if(static_cast<std::uint64_t>(outFrames)
            <=std::numeric_limits<std::uint64_t>::max()/perOutput)
            macs=static_cast<std::uint64_t>(outFrames)*perOutput;
    }
    return core::Result<Projection>::success({
        *count.value(),masterBytes,deviceBytes,masterBytes+deviceBytes,macs});
}

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
// Uses a separate, explicitly OUTPUT_RATE-domain offline SRC entrypoint.
// The immutable M17 master is only read, with no SOURCE-domain alias or
// full-resource SOURCE scratch clone. Playback/output, cancellation,
// performance and M16 LIVE provenance remain unqualified.
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

    // Fail closed on impossible PCM byte counts before any device buffer
    // allocation. This projection does not impose an arbitrary memory cap.
    auto resource=project_monitoring_pcm_resource_cost(
        master_rate,verified_master_pcm.frame_count(),observed_device_rate,
        verified_master_pcm.format().channel_layout());
    if(!resource)
        return core::Result<MonitoringPcmCandidate>::failure(*resource.error());

    // No extra O(track duration) SRC scratch allocation. The borrowed view
    // remains OUTPUT_RATE, and only this explicit monitor entrypoint may
    // consume it as the immutable input to a separate device-rate PCM.
    auto converted=materialize_output_master_monitoring_rate_candidate(
        verified_master_pcm,observed_device_rate,offline_block);
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
