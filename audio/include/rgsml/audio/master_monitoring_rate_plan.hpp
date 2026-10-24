#pragma once

#include <rgsml/core/error.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>

#include <cstdint>

namespace rgsml::audio {

// Immutable boundary between the chosen FINAL-MASTER rate (M17 -> M18)
// and the physical hardware rate. This only plans a monitoring path: it
// does NOT render SRC, claim a verified sink, or mutate the final PCM.
struct MasterMonitoringRatePlan final {
    core::SampleRate final_master_rate;
    core::SampleRate physical_device_rate;
    bool monitoring_src_required;
    bool can_install_master_pcm_directly;
};

[[nodiscard]] inline core::Result<MasterMonitoringRatePlan>
plan_master_monitoring_rate(
    core::SampleRate selected_final_master_rate,
    core::SampleRate observed_physical_device_rate)
{
    // The frozen export / terminal M16-M17 sample-rate set. A device's
    // independent hardware capabilities cannot expand this set.
    const auto hz=selected_final_master_rate.value();
    if(hz!=44100 && hz!=48000 && hz!=88200 && hz!=96000
       && hz!=176400 && hz!=192000) {
        return core::Result<MasterMonitoringRatePlan>::failure(core::Error{
            core::ErrorCode::UnsupportedOperation,
            "M17_M18_MASTER_RATE_UNSUPPORTED: final master rate must be one of six supported rates."});
    }

    const bool exact=selected_final_master_rate==
        observed_physical_device_rate;
    // When !exact, a separate, device-owned monitoring SRC is REQUIRED.
    // Never install master PCM at a mismatched device rate, and never
    // replace selected_final_master_rate with observed_physical_device_rate.
    return core::Result<MasterMonitoringRatePlan>::success({
        selected_final_master_rate,observed_physical_device_rate,
        !exact,exact});
}

} // namespace rgsml::audio
