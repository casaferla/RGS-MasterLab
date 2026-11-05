#include <rgsml/analysis/true_peak_measurement.hpp>

#include "internal/true_peak_full_resource.hpp"

#include <cstddef>
#include <utility>

namespace rgsml::analysis {

core::Result<TruePeakMeasurement>
measure_true_peak_full_resource(const audio::AudioBuffer& resource)
{
    // Same exact production FIR, strict-FP source and 11-frame finalization:
    // no parallel algorithm, dB conversion, DC filter or PCM mutation.
    auto evidence = internal::measure_unqualified_true_peak_full_resource(resource);
    if (!evidence) {
        return core::Result<TruePeakMeasurement>::failure(*evidence.error());
    }
    const auto& e = *evidence.value();
    TruePeakMeasurement m;
    m.sample_rate_hz_ = e.sample_rate_hz;
    m.channel_count_ = e.channel_count;
    m.per_channel_linear_peak_ = e.per_channel_linear_peak;
    m.per_channel_db_tp_ = e.per_channel_db_tp;
    m.global_linear_peak_ = e.global_linear_peak;
    m.global_db_tp_ = e.global_db_tp;
    m.is_exact_silence_ = e.is_exact_silence;

    // Metadata derived from already finalized peaks, never re-measured.
    if (e.is_exact_silence) {
        m.global_peak_channel_ = TruePeakPeakChannel::NONE;
    } else if (e.channel_count == std::size_t{1}) {
        m.global_peak_channel_ = TruePeakPeakChannel::MONO_C;
    } else if (e.per_channel_linear_peak[0] > e.per_channel_linear_peak[1]) {
        m.global_peak_channel_ = TruePeakPeakChannel::LEFT;
    } else if (e.per_channel_linear_peak[1] > e.per_channel_linear_peak[0]) {
        m.global_peak_channel_ = TruePeakPeakChannel::RIGHT;
    } else {
        m.global_peak_channel_ = TruePeakPeakChannel::TIE;
    }
    // qualification_status_ is deliberately left SUPPORTED_UNQUALIFIED:
    // no product-release Gate G evidence/resolver is accepted yet.
    return core::Result<TruePeakMeasurement>::success(std::move(m));
}

}  // namespace rgsml::analysis
