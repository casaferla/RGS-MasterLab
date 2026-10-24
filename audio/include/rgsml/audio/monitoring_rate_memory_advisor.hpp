#pragma once

#include <rgsml/audio/monitoring_pcm_candidate.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

namespace rgsml::audio {

// ADVISORY ONLY: for initial Prepare estimates, NOT a memory reservation,
// filesystem/export-rate decision, successful allocation or live CPU gate.
// The physically available and committable amounts are independent Windows
// observations; both must be known, or the answer remains UNKNOWN.
enum class MonitoringMemoryAdviceKind {
    UNKNOWN,
    RECOMMENDED_MEMORY_ONLY,
    CAUTION_LIMITED_HEADROOM,
    DISCOURAGED_INSUFFICIENT_HEADROOM,
};

struct MonitoringMemoryObservation final {
    std::uint64_t available_physical_bytes{0};
    std::uint64_t available_commit_bytes{0};
    std::uint64_t reserved_other_prepare_bytes{0};
};

struct MonitoringRateMemoryAdvice final {
    core::SampleRate proposed_master_rate;
    core::SampleRate observed_device_rate;
    core::FrameCount estimated_master_frames;
    MonitoringPcmResourceProjection resource;
    MonitoringMemoryAdviceKind kind{MonitoringMemoryAdviceKind::UNKNOWN};
    std::uint64_t memory_headroom_after_reserve_bytes{0};
    bool cpu_performance_qualified{false}; // always false
};

// Budgeting policy: use the smaller of available physical and available
// commit. Reserve caller-owned Prepare/DSP bytes separately, AND retain
// 25% of observed available memory as a safety cushion (at least 512 MiB).
// In this advisory model, both full-track PCM buffers count as new usage:
// conservative at the start of Prepare; NEVER silently drop one of them.
// Fits in reserved-safe budget: CAUTION; at most half of safe budget:
// RECOMMENDED_MEMORY_ONLY. CPU cost has not been measured.
[[nodiscard]] inline MonitoringMemoryAdviceKind classify_monitoring_memory(
    std::uint64_t combined_pcm_bytes,
    MonitoringMemoryObservation observed,
    std::uint64_t* headroom=nullptr) noexcept
{
    if(headroom)*headroom=0;
    if(observed.available_physical_bytes==0
       ||observed.available_commit_bytes==0)
        return MonitoringMemoryAdviceKind::UNKNOWN;
    const auto available=std::min(
        observed.available_physical_bytes,
        observed.available_commit_bytes);
    constexpr std::uint64_t minimumReserve=UINT64_C(512)*1024*1024;
    const auto safetyReserve=std::max(minimumReserve,available/4);
    if(observed.reserved_other_prepare_bytes>available
       ||safetyReserve>available-observed.reserved_other_prepare_bytes)
        return MonitoringMemoryAdviceKind::DISCOURAGED_INSUFFICIENT_HEADROOM;
    const auto budget=available-observed.reserved_other_prepare_bytes
        -safetyReserve;
    if(headroom)*headroom=budget;
    if(combined_pcm_bytes>budget)
        return MonitoringMemoryAdviceKind::DISCOURAGED_INSUFFICIENT_HEADROOM;
    if(combined_pcm_bytes<=budget/2)
        return MonitoringMemoryAdviceKind::RECOMMENDED_MEMORY_ONLY;
    return MonitoringMemoryAdviceKind::CAUTION_LIMITED_HEADROOM;
}

// All six master frequencies are assessed at the ORIGINAL SOURCE duration.
// ceil(duration * proposed_master_rate) avoids floating-point rounding and
// never changes or writes any source/master PCM. Each proposal is tested
// against one observed device frequency (not hypothetical DAC support).
[[nodiscard]] inline core::Result<std::vector<MonitoringRateMemoryAdvice>>
advise_monitoring_master_rates_for_prepare(
    core::SampleRate source_rate,
    core::FrameCount source_frames,
    core::SampleRate observed_device_rate,
    ChannelLayout layout,
    MonitoringMemoryObservation observation)
{
    using AdviceSet=std::vector<MonitoringRateMemoryAdvice>;
    constexpr std::array<std::int64_t,6> proposedRates{
        44100,48000,88200,96000,176400,192000};
    if(source_frames.value()<=0||source_rate.value()<=0)
        return core::Result<AdviceSet>::failure(core::Error{
            core::ErrorCode::InvalidArgument,
            "MONITOR_PREPARE_SOURCE_DURATION_INVALID"});
    AdviceSet result;
    result.reserve(proposedRates.size());
    // The rate and source frame domain are separate from the master's
    // proposed OUTPUT_RATE domain, with ceil-to-cover duration.
    for(std::size_t i=0;i<proposedRates.size();++i) {
        const auto hz=proposedRates[i];
        const auto sourceHz=source_rate.value();
        const auto inputFrames=source_frames.value();
        const auto q=inputFrames/sourceHz;
        const auto r=inputFrames%sourceHz;
        const auto maxFrames=std::numeric_limits<std::int64_t>::max();
        if(r>(maxFrames-sourceHz+1)/hz)
            return core::Result<AdviceSet>::failure(core::Error{
                core::ErrorCode::IntegerOverflow,
                "MONITOR_PREPARE_DURATION_PRODUCT_OVERFLOW"});
        const auto tail=(r*hz+sourceHz-1)/sourceHz;
        if(q>(std::numeric_limits<std::int64_t>::max()-tail)/hz)
            return core::Result<AdviceSet>::failure(core::Error{
                core::ErrorCode::IntegerOverflow,
                "MONITOR_PREPARE_MASTER_DURATION_OVERFLOW"});
        const auto masterFrameCount=core::FrameCount::create(q*hz+tail);
        if(!masterFrameCount)
            return core::Result<AdviceSet>::failure(*masterFrameCount.error());
        const auto masterRate=core::SampleRate::create(hz);
        if(!masterRate)
            return core::Result<AdviceSet>::failure(*masterRate.error());
        auto resource=project_monitoring_pcm_resource_cost(
            *masterRate.value(),*masterFrameCount.value(),
            observed_device_rate,layout);
        if(!resource)
            return core::Result<AdviceSet>::failure(*resource.error());
        std::uint64_t headroom=0;
        const auto advice=classify_monitoring_memory(
            resource.value()->combined_pcm_bytes,observation,&headroom);
        result.push_back(MonitoringRateMemoryAdvice{
            *masterRate.value(),observed_device_rate,
            *masterFrameCount.value(),std::move(*resource.value()),
            advice,headroom,false});
    }
    return core::Result<AdviceSet>::success(std::move(result));
}

} // namespace rgsml::audio
