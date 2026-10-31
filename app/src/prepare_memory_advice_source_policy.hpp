#pragma once
#include "final_src_candidate_policy.hpp"
#include <rgsml/audio/monitoring_rate_memory_advisor.hpp>
#include <rgsml/audio/monitoring_pcm_candidate.hpp>
#include <array>
#include <vector>
#include <cstdint>

namespace rgsml::app {
// Presentation-only classification. Product Final SRC admission remains
// exclusively owned by final_src_candidate_path(), not by RAM advice.
enum class PrepareMemoryDisplayAdmission : std::uint8_t {
    SOURCE_OR_RATE_UNAVAILABLE = 0,
    CANDIDATE_ADMISSIBLE = 1,
    FINAL_SRC_UNQUALIFIED = 2
};

[[nodiscard]] constexpr PrepareMemoryDisplayAdmission
prepare_memory_display_admission(std::int64_t source_hz,
                                  std::int64_t master_hz,
                                  bool authenticated_full_original_q0_pack=false) noexcept
{
    switch (final_src_candidate_path(source_hz,master_hz)) {
    case FinalSrcCandidatePath::IDENTITY:
    case FinalSrcCandidatePath::EXISTING_44K_48K_FIR:
        return PrepareMemoryDisplayAdmission::CANDIDATE_ADMISSIBLE;
    case FinalSrcCandidatePath::EXPERIMENTAL_UNQUALIFIED:
        return authenticated_full_original_q0_pack
            ? PrepareMemoryDisplayAdmission::CANDIDATE_ADMISSIBLE
            : PrepareMemoryDisplayAdmission::FINAL_SRC_UNQUALIFIED;
    default:
        return PrepareMemoryDisplayAdmission::SOURCE_OR_RATE_UNAVAILABLE;
    }
}
// A six-row memory estimate is only usable when it is coherent with ONE
// immutable SOURCE snapshot and ONE observed DEVICE/memory budget. The
// authoritative audio cost oracle is reused; no allocations or admissions.
[[nodiscard]] inline bool prepare_rate_advice_snapshot_valid(
    const std::vector<audio::MonitoringRateMemoryAdvice>& rows,
    core::SampleRate source_rate, core::FrameCount source_frames,
    audio::ChannelLayout layout)
{
    constexpr std::array<std::int64_t,6> kRates{
        44100,48000,88200,96000,176400,192000};
    if(rows.size()!=kRates.size() || source_frames.value()<=0
       || !m17_six_rate(source_rate.value()))return false;
    const auto device=rows.front().observed_device_rate;
    const auto observed=rows.front().memory_observation;
    const auto reconstructed=audio::advise_monitoring_master_rates_for_prepare(
        source_rate,source_frames,device,layout,observed);
    if(!reconstructed || reconstructed.value()->size()!=kRates.size())
        return false;
    for(std::size_t i=0;i<kRates.size();++i) {
        const auto& actual=rows[i];
        const auto& expected=(*reconstructed.value())[i];
        const auto& a=actual.resource;
        const auto& b=expected.resource;
        if(actual.proposed_master_rate.value()!=kRates[i]
           ||actual.observed_device_rate!=device
           ||actual.estimated_master_frames!=expected.estimated_master_frames
           ||actual.kind!=expected.kind
           ||actual.memory_headroom_after_reserve_bytes
               !=expected.memory_headroom_after_reserve_bytes
           ||actual.cpu_performance_qualified
           ||actual.memory_observation.available_physical_bytes
               !=observed.available_physical_bytes
           ||actual.memory_observation.available_commit_bytes
               !=observed.available_commit_bytes
           ||actual.memory_observation.reserved_other_prepare_bytes
               !=observed.reserved_other_prepare_bytes
           ||a.device_frames!=b.device_frames
           ||a.master_pcm_bytes!=b.master_pcm_bytes
           ||a.device_pcm_bytes!=b.device_pcm_bytes
           ||a.combined_pcm_bytes!=b.combined_pcm_bytes
           ||a.experimental_fir_scratch_bytes!=b.experimental_fir_scratch_bytes
           ||a.experimental_fir_mac_operations
               !=b.experimental_fir_mac_operations)
            return false;
    }
    return true;
}
} // namespace rgsml::app
