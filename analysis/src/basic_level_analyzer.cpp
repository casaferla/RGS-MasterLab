#include <rgsml/analysis/basic_level_analyzer.hpp>
#include <rgsml/analysis/true_peak_measurement.hpp>
#include "internal/m19_audio_scope_adapter_candidate.hpp"
#include <rgsml/core/error.hpp>
#include <cstdint>
#include <utility>

namespace rgsml::analysis {
namespace {
[[nodiscard]] BasicLevelValue mapped(
    std::optional<double> value,
    BasicLevelValueState unavailable=BasicLevelValueState::SILENCE) noexcept
{
    return BasicLevelValue{value,value?BasicLevelValueState::AVAILABLE:unavailable};
}
[[nodiscard]] BasicLevelValueState loudness_reason(
    internal::m19::LoudnessPreproductionState state) noexcept
{
    using S=internal::m19::LoudnessPreproductionState;
    switch(state) {
    case S::available: return BasicLevelValueState::AVAILABLE;
    case S::insufficient_duration:
        return BasicLevelValueState::INSUFFICIENT_DURATION;
    case S::no_absolute_gated_blocks:
        return BasicLevelValueState::BELOW_LOUDNESS_GATE;
    case S::not_finalized:
    case S::unsupported_k_weighting_rate:
    case S::k_numerically_undefined:
        return BasicLevelValueState::NUMERIC_UNDEFINED;
    }
    return BasicLevelValueState::NUMERIC_UNDEFINED;
}
} // namespace

core::Result<BasicLevelAnalysisResult> measure_basic_level_scope(
    const audio::AudioBufferView& resource,
    core::FrameIndex absoluteScopeBegin,
    core::FrameCount scopeFrames,
    audio::FrameDomainId expectedDomain)
{
    using R=core::Result<BasicLevelAnalysisResult>;
    auto measured=internal::m19::measure_selected_scope_candidate(
        resource,absoluteScopeBegin,scopeFrames,expectedDomain);
    if(!measured)return R::failure(*measured.error());
    const auto& s=*measured.value();
    BasicLevelAnalysisResult out;
    out.scope_begin_frame=absoluteScopeBegin.value();
    out.scope_frame_count=scopeFrames.value();
    out.sample_rate_hz=static_cast<std::int64_t>(s.sample_rate_hz);
    out.channel_count=static_cast<std::size_t>(s.channel_count);
    out.frame_domain=expectedDomain;
    out.k_weighting_bank_member_id=s.k_weighting_bank_member_id;
    out.k_weighting_bank_sha256=s.k_weighting_bank_sha256;
    // Convert only the original M19 gate's exact frame intervals.
    // No new gating threshold or independent loudness calculation.
    out.retained_block_disclosure_complete=
        s.retained_block_disclosure_complete;
    out.retained_block_union.reserve(s.retained_block_union.size());
    for(const auto& interval:s.retained_block_union)
        out.retained_block_union.push_back({interval.begin,interval.end});
    out.integrated_lufs=BasicLevelValue{
        s.integrated_lufs,loudness_reason(s.loudness_state)};
    out.full_scope_rms_dbfs=mapped(s.full_scope_rms_dbfs);
    out.sample_peak_dbfs=mapped(s.sample_peak_dbfs);
    out.sample_peak_crest_db=mapped(s.sample_peak_crest_db);
    // M16 is whole-resource only; never substitute Sample Peak or
    // resample/copy an arbitrary selected region to impersonate its
    // existing exact, owning-resource True Peak authority.
    out.true_peak_db_tp={std::nullopt,
        BasicLevelValueState::M16_SCOPE_UNAVAILABLE};
    return R::success(std::move(out));
}

core::Result<BasicLevelAnalysisResult> measure_basic_level_full_resource(
    const audio::AudioBuffer& resource)
{
    using R=core::Result<BasicLevelAnalysisResult>;
    const auto view=resource.view();
    auto result=measure_basic_level_scope(view,
        view.absolute_start_frame(),view.frame_count(),
        view.timebase().frame_domain_id());
    if(!result)return result;
    // Actual canonical M16: source-ordered FIR and final dBTP are
    // imported by one already-existing public method. Never copy the
    // algorithm, override its qualification or fabricate a peak.
    const auto peak=measure_true_peak_full_resource(resource);
    if(!peak)return R::failure(*peak.error());
    if(peak.value()->sample_rate_hz()!=result.value()->sample_rate_hz
       ||peak.value()->channel_count()!=result.value()->channel_count)
        return R::failure(core::Error{
            core::ErrorCode::InvalidState,
            "M19_REAL_M16_TRUE_PEAK_RESOURCE_IDENTITY_MISMATCH"});
    const auto qualification=peak.value()->qualification_status();
    if(qualification==TruePeakQualificationStatus::UNSUPPORTED) {
        result.value()->true_peak_db_tp={std::nullopt,
            BasicLevelValueState::M16_SCOPE_UNAVAILABLE};
    } else if(qualification==TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED) {
        result.value()->true_peak_db_tp={peak.value()->global_db_tp(),
            BasicLevelValueState::M16_SUPPORTED_UNQUALIFIED};
    } else {
        result.value()->true_peak_db_tp=mapped(
            peak.value()->global_db_tp());
    }
    return result;
}

} // namespace rgsml::analysis
