#pragma once

#include <rgsml/audio/monitoring_pcm_candidate.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/audio_playback_service.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>

#include <cstdint>
#include <limits>

namespace rgsml::audio {

// A display/control-plane snapshot, never an audible DAC clock, M16 LIVE
// meter reading or proof of the original terminal master realization.
struct MonitoringMasterDisplaySnapshot final {
    core::PlaybackState state;
    core::FrameIndex master_display_position;
    core::FrameCount master_duration;
    std::optional<core::FrameRange> requested_master_loop;
    core::FrameIndex device_position;
    core::FrameCount device_duration;
    core::SampleRate master_rate;
    core::SampleRate device_rate;
};

// Pure master ↔ device listening coordinate map. Only the separately
// SRC-derived MONITOR buffer uses these coordinates; neither M17 master PCM
// nor SOURCE waveform/playhead is rewritten. No real sink or LIVE proof.
class MonitoringFrameMap final {
public:
    [[nodiscard]] static core::Result<MonitoringFrameMap> create(
        const MonitoringPcmCandidate& candidate)
    {
        auto validated=validate_monitoring_pcm_candidate(candidate);
        if(!validated)
            return core::Result<MonitoringFrameMap>::failure(*validated.error());
        return core::Result<MonitoringFrameMap>::success(MonitoringFrameMap{
            candidate.immutable_master_rate,candidate.device_rate,
            candidate.immutable_master_frames,
            candidate.device_pcm.view().frame_count()});
    }

    [[nodiscard]] core::SampleRate master_rate() const noexcept
    { return masterRate_; }
    [[nodiscard]] core::SampleRate device_rate() const noexcept
    { return deviceRate_; }
    [[nodiscard]] core::FrameCount master_frames() const noexcept
    { return masterFrames_; }
    [[nodiscard]] core::FrameCount device_frames() const noexcept
    { return deviceFrames_; }

    // Exact half-open ceil mapping, not real-time sample consumption.
    [[nodiscard]] core::Result<core::FrameIndex>
    master_to_device(core::FrameIndex frame) const
    {
        if(frame.value()<0||frame.value()>masterFrames_.value())
            return invalid<core::FrameIndex>(
                "MONITOR_FRAME_MAP_MASTER_OUT_OF_RANGE");
        auto mapped=ceil_ratio(frame.value(),masterRate_.value(),
            deviceRate_.value());
        if(!mapped)return core::Result<core::FrameIndex>::failure(*mapped.error());
        if(*mapped.value()>deviceFrames_.value())
            return invalid<core::FrameIndex>(
                "MONITOR_FRAME_MAP_DEVICE_OUT_OF_RANGE");
        return core::Result<core::FrameIndex>::success(
            core::FrameIndex{*mapped.value()});
    }

    // Display / seek cursor only. NOT an estimate of samples heard by a DAC.
    [[nodiscard]] core::Result<core::FrameIndex>
    device_to_master_cursor(core::FrameIndex frame) const
    {
        if(frame.value()<0||frame.value()>deviceFrames_.value())
            return invalid<core::FrameIndex>(
                "MONITOR_FRAME_MAP_DEVICE_OUT_OF_RANGE");
        const auto hz=deviceRate_.value();
        const auto q=frame.value()/hz;
        const auto r=frame.value()%hz;
        const auto m=masterRate_.value();
        if(q>std::numeric_limits<std::int64_t>::max()/m)
            return invalid<core::FrameIndex>(
                "MONITOR_FRAME_MAP_CURSOR_OVERFLOW");
        const auto mapped=q*m+(r*m)/hz;
        return core::Result<core::FrameIndex>::success(core::FrameIndex{
            mapped>masterFrames_.value()?masterFrames_.value():mapped});
    }

    [[nodiscard]] core::Result<core::FrameRange>
    master_loop_to_device(core::FrameRange region) const
    {
        if(region.begin().value()<0
           ||region.end().value()>masterFrames_.value()
           ||region.begin()>=region.end())
            return invalid<core::FrameRange>(
                "MONITOR_FRAME_MAP_INVALID_MASTER_REGION");
        auto first=master_to_device(region.begin());
        if(!first)return core::Result<core::FrameRange>::failure(*first.error());
        auto last=master_to_device(region.end());
        if(!last)return core::Result<core::FrameRange>::failure(*last.error());
        if(*first.value()>=*last.value())
            return invalid<core::FrameRange>(
                "MONITOR_FRAME_MAP_COLLAPSED_DEVICE_REGION");
        return core::FrameRange::create(*first.value(),*last.value());
    }

    [[nodiscard]] core::Result<MonitoringMasterDisplaySnapshot>
    project_display_snapshot(
        const core::PlaybackSnapshot& deviceSnapshot,
        std::optional<core::FrameRange> requestedMasterLoop) const
    {
        using Snapshot=MonitoringMasterDisplaySnapshot;
        if(!deviceSnapshot.duration
           ||*deviceSnapshot.duration!=deviceFrames_
           ||deviceSnapshot.state==core::PlaybackState::NO_SOURCE
           ||deviceSnapshot.position.value()<0
           ||deviceSnapshot.position.value()>deviceFrames_.value()
           ||deviceSnapshot.audibleRealization.realizationId)
            return invalid<Snapshot>("MONITOR_SNAPSHOT_DEVICE_PROVENANCE_INVALID");
        if(requestedMasterLoop) {
            auto mapped=master_loop_to_device(*requestedMasterLoop);
            if(!mapped)return core::Result<Snapshot>::failure(*mapped.error());
            if(!deviceSnapshot.loop||*deviceSnapshot.loop!=*mapped.value())
                return invalid<Snapshot>("MONITOR_SNAPSHOT_LOOP_EPOCH_MISMATCH");
        }else if(deviceSnapshot.loop) {
            return invalid<Snapshot>("MONITOR_SNAPSHOT_UNOWNED_DEVICE_LOOP");
        }
        auto display=device_to_master_cursor(deviceSnapshot.position);
        if(!display)return core::Result<Snapshot>::failure(*display.error());
        return core::Result<Snapshot>::success({
            deviceSnapshot.state,*display.value(),masterFrames_,
            requestedMasterLoop,deviceSnapshot.position,deviceFrames_,
            masterRate_,deviceRate_});
    }

private:
    MonitoringFrameMap(core::SampleRate m,core::SampleRate d,
        core::FrameCount mf,core::FrameCount df) noexcept
      :masterRate_(m),deviceRate_(d),masterFrames_(mf),deviceFrames_(df) {}

    template<typename T>
    [[nodiscard]] static core::Result<T> invalid(const char* msg)
    {
        return core::Result<T>::failure(core::Error{
            core::ErrorCode::InvalidArgument,msg});
    }

    [[nodiscard]] static core::Result<std::int64_t>
    ceil_ratio(std::int64_t frames,std::int64_t hz,std::int64_t target)
    {
        const auto q=frames/hz;
        const auto r=frames%hz;
        const auto tail=(r*target+hz-1)/hz;
        if(q>(std::numeric_limits<std::int64_t>::max()-tail)/target)
            return invalid<std::int64_t>("MONITOR_FRAME_MAP_OVERFLOW");
        return core::Result<std::int64_t>::success(q*target+tail);
    }

    core::SampleRate masterRate_;
    core::SampleRate deviceRate_;
    core::FrameCount masterFrames_;
    core::FrameCount deviceFrames_;
};
} // namespace rgsml::audio
