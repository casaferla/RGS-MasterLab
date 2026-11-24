#pragma once
// L3-M06 D07-D09 from the SAME canonical Phase6 detector event set.
// The detector authority owns event creation, rawFlux, robustZ, NMS,
// P04 rational timestamps and per-descriptor contexts. This function
// NEVER detects, synthesizes, pairs, deletes or time-aligns events.
// Missing edge/context descriptors remain in the support denominator.
#include "l3_m06_real_short_term_shape.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace rgsml::app::l3_automatch::m05m06_real {
struct CanonicalTransientEvent final {
    std::uint64_t canonical_event_ordinal{};
    // Diagnostic/source evidence only, NOT a new artistic metric.
    double raw_positive_spectral_flux{};
    double robust_z{};
    // Each independently missing descriptor stays NULL. Units dB,dB,ms.
    std::optional<double> early_minus_sustain_rms_db;
    std::optional<double> peak_minus_local_rms_db;
    std::optional<double> attack_rise_time_ms;
};
enum class TransientDescriptorFailure:std::uint8_t {
    ready,insufficient_events_or_active_scope,
    missing_detector_policy_or_source_identity,
    invalid_canonical_population,missing_valid_descriptor,
    invalid_descriptor_or_numeric
};
struct NativeTransientDescriptor final {
    std::array<std::optional<double>,3> canonical_type7_p10_p50_p90{};
    std::array<std::optional<double>,3> native_type7_p10_p50_p90{};
    std::uint64_t valid_descriptor_events{};
    std::uint64_t canonical_events{};
    std::optional<double> native_support;
    TransientDescriptorFailure reason{
        TransientDescriptorFailure::missing_detector_policy_or_source_identity};
    [[nodiscard]] bool reference_grade() const noexcept {
        return reason==TransientDescriptorFailure::ready;
    }
};
struct CanonicalTransientDistributions final {
    std::array<NativeTransientDescriptor,3> d07_d08_d09;
    [[nodiscard]] constexpr bool authorizes_m10() const noexcept {
        return false;
    }
};
[[nodiscard]] inline CanonicalTransientDistributions
observe_phase6_native_transient_descriptors(
    const std::vector<CanonicalTransientEvent>& canonical_detector_events,
    std::int64_t exact_active_frames,std::int64_t sample_rate_hz,
    bool exact_current_p04_detector_policy_and_event_population_attested)
{
    CanonicalTransientDistributions result;
    if(!exact_current_p04_detector_policy_and_event_population_attested||
       sample_rate_hz<=0||exact_active_frames<0) {
        for(auto& r:result.d07_d08_d09)
            r.reason=TransientDescriptorFailure::
                missing_detector_policy_or_source_identity;
        return result;
    }
    const auto count=canonical_detector_events.size();
    std::array<std::vector<double>,3> sorted;
    for(auto& s:sorted)s.reserve(count);
    std::optional<std::uint64_t> previous_ordinal;
    for(const auto& event:canonical_detector_events) {
        if((previous_ordinal&&
            event.canonical_event_ordinal<=*previous_ordinal)||
           !std::isfinite(event.raw_positive_spectral_flux)||
           event.raw_positive_spectral_flux<0.0||
           !std::isfinite(event.robust_z)) {
            for(auto& r:result.d07_d08_d09)
                r.reason=TransientDescriptorFailure::
                    invalid_canonical_population;
            return result;
        }
        previous_ordinal=event.canonical_event_ordinal;
        const std::array<std::optional<double>,3> values{{
            event.early_minus_sustain_rms_db,
            event.peak_minus_local_rms_db,
            event.attack_rise_time_ms
        }};
        for(std::size_t d=0;d<3U;++d) {
            if(!values[d])continue;
            if(!std::isfinite(*values[d])||
               (d==2U&&*values[d]<=0.0)) {
                for(auto& r:result.d07_d08_d09)
                    r.reason=TransientDescriptorFailure::
                        invalid_descriptor_or_numeric;
                return result;
            }
            sorted[d].push_back(*values[d]);
        }
    }
    for(std::size_t d=0;d<3U;++d) {
        auto& out=result.d07_d08_d09[d];
        out.canonical_events=count;
        out.valid_descriptor_events=sorted[d].size();
        if(count>0U)
            out.native_support=static_cast<double>(
                out.valid_descriptor_events)/static_cast<double>(count);
        if(count<8U||exact_active_frames<5*sample_rate_hz||
           out.valid_descriptor_events<8U) {
            out.reason=TransientDescriptorFailure::
                insufficient_events_or_active_scope;
            continue;
        }
        if(sorted[d].empty()) {
            out.reason=TransientDescriptorFailure::missing_valid_descriptor;
            continue;
        }
        std::sort(sorted[d].begin(),sorted[d].end());
        constexpr std::array<double,3> probabilities{{0.10,0.50,0.90}};
        bool good=true;
        for(std::size_t k=0;k<3U;++k) {
            const auto q=type7_percentile(sorted[d],probabilities[k]);
            if(!q) {
                good=false;
                break;
            }
            // D09: first percentile on actual positive rise time,
            // THEN frozen LOG2_POSITIVE with reference 1 ms.
            const double z=d==2U?std::log2(*q):*q;
            if(!std::isfinite(z)) {
                good=false;
                break;
            }
            out.canonical_type7_p10_p50_p90[k]=z;
            out.native_type7_p10_p50_p90[k]=*q;
        }
        out.reason=good?TransientDescriptorFailure::ready:
            TransientDescriptorFailure::invalid_descriptor_or_numeric;
        if(!good) {
            out.canonical_type7_p10_p50_p90={};
            out.native_type7_p10_p50_p90={};
        }
    }
    return result;
}
} // namespace rgsml::app::l3_automatch::m05m06_real
