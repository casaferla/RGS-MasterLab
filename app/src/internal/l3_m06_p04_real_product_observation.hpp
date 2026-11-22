#pragma once
// L3-M06 one REAL Phase6 P04 detector and three actual PCM D07-D09
// descriptors from ONE full-track owner. P04 uses a distinct frozen
// 20ms/5ms FFT (not M20), but shares candidate RenderPreview PCM.
// Detector event set, null contexts and native support are preserved;
// neither component grants M07 Phase6 source checksum or M13 status.
#include "l3_m06_p04_native_attack_rise.hpp"
#include "l3_m06_native_transient_descriptors.hpp"
#include <rgsml/audio/audio_buffer_view.hpp>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace rgsml::app::l3_automatch::m05m06_real {
enum class P04ProductObservationReason:std::uint8_t {
    ready,missing_current_phase6_owner,
    invalid_native_flux,invalid_detector,
    missing_exact_pcm_descriptor_source,
    invalid_aggregated_native_distribution
};
struct P04ProductObservation final {
    P04RawFluxPopulation actual_flux;
    P04DetectedNativePopulation detector;
    P04D09NativeResult actual_d07_d08_d09_events;
    CanonicalTransientDistributions original_d07_d08_d09_type7;
    P04ProductObservationReason reason{
        P04ProductObservationReason::missing_current_phase6_owner};
    [[nodiscard]] constexpr bool authorizes_m10_or_publish()
        const noexcept{return false;}
};
[[nodiscard]] inline P04ProductObservation
measure_product_p04_d07_d09(
    const audio::AudioBufferView& actual_rendered_pcm,
    std::int64_t exact_phase6_active_frames,
    bool current_full_phase6_owner_and_p04_policy_attested)
{
    P04ProductObservation out;
    if(!current_full_phase6_owner_and_p04_policy_attested||
       exact_phase6_active_frames<0||
       exact_phase6_active_frames>
           actual_rendered_pcm.frame_count().value())
        return out;
    out.actual_flux=measure_p04_native_positive_flux(
        actual_rendered_pcm);
    if(out.actual_flux.reason!=P04NativeReason::ready) {
        out.reason=P04ProductObservationReason::invalid_native_flux;
        return out;
    }
    out.detector=select_p04_canonical_native_events(out.actual_flux);
    if(out.detector.reason!=P04SelectionReason::ready) {
        out.reason=P04ProductObservationReason::invalid_detector;
        return out;
    }
    const auto contexts=derive_real_p04_d07_d08_contexts(
        actual_rendered_pcm,out.detector,
        {true,out.actual_flux.sample_rate_hz});
    if(!contexts.source_valid) {
        out.reason=P04ProductObservationReason::
            missing_exact_pcm_descriptor_source;
        return out;
    }
    out.actual_d07_d08_d09_events=derive_real_p04_d09_attack_rise(
        actual_rendered_pcm,out.detector,contexts,true);
    if(!out.actual_d07_d08_d09_events.
            exact_source_and_canonical_selection_current) {
        out.reason=P04ProductObservationReason::
            missing_exact_pcm_descriptor_source;
        return out;
    }
    out.original_d07_d08_d09_type7=
        observe_phase6_native_transient_descriptors(
            out.actual_d07_d08_d09_events.same_detector_event_population,
            exact_phase6_active_frames,
            static_cast<std::int64_t>(out.actual_flux.sample_rate_hz),
            true);
    for(const auto& descriptor:out.original_d07_d08_d09_type7.
            d07_d08_d09) {
        if(descriptor.reason==TransientDescriptorFailure::
               invalid_canonical_population||
           descriptor.reason==TransientDescriptorFailure::
               invalid_descriptor_or_numeric) {
            out.reason=P04ProductObservationReason::
                invalid_aggregated_native_distribution;
            return out;
        }
    }
    // Zero or insufficient actual transients are VALID detector
    // observations but NOT a matching/reference-grade feature.
    out.reason=P04ProductObservationReason::ready;
    return out;
}
} // namespace rgsml::app::l3_automatch::m05m06_real
