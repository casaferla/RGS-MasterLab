#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include "internal/m20_bounded_spectrum_candidate.hpp"
#include "internal/m20_normalized_profile_candidate.hpp"
#include <rgsml/core/error.hpp>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::analysis {
namespace {
using Diagnostic=m20_private::Diagnostic;
using State=TonalStereoMetricState;
[[nodiscard]] State state(Diagnostic diagnostic) noexcept {
    switch(diagnostic) {
    case Diagnostic::valid: return State::AVAILABLE;
    case Diagnostic::not_applicable: return State::NOT_APPLICABLE;
    case Diagnostic::insufficient_content: return State::INSUFFICIENT_CONTENT;
    case Diagnostic::undefined_denominator: return State::UNDEFINED_DENOMINATOR;
    case Diagnostic::invalid_input:
        return State::UNAVAILABLE_AUTHORITATIVE_SUPPORT;
    }
    return State::UNAVAILABLE_AUTHORITATIVE_SUPPORT;
}
[[nodiscard]] TonalStereoValue metric(
    std::optional<double> value,Diagnostic diagnostic) noexcept {
    return {value,state(diagnostic)};
}
[[nodiscard]] core::Error invalid(const char* name) {
    return core::Error{core::ErrorCode::InvalidState,name};
}
} // namespace

core::Result<TonalStereoAnalysisResult> measure_tonal_stereo_scope(
    const audio::AudioBufferView& resource,
    core::FrameIndex absoluteScopeBegin,
    core::FrameCount scopeFrames,
    audio::FrameDomainId expectedDomain,
    const TonalStereoTrustedSupport& supplied)
{
    using R=core::Result<TonalStereoAnalysisResult>;
    if(resource.timebase().frame_domain_id()!=expectedDomain
       ||absoluteScopeBegin.value()<0||scopeFrames.value()<=0)
        return R::failure(core::Error{
            core::ErrorCode::InvalidFrameRange,
            "M20_REAL_SCOPE_DOMAIN_OR_RANGE_INVALID"});
    const auto selected=resource.subview(absoluteScopeBegin,scopeFrames);
    if(!selected)return R::failure(*selected.error());
    const auto& view=*selected.value();
    const auto rate=view.format().sample_rate().value();
    const auto channels=view.format().channel_count();
    if(rate<=0||!m20_private::supported_rate(
           static_cast<std::uint32_t>(rate)))
        return R::failure(core::Error{
            core::ErrorCode::UnsupportedOperation,
            "M20_REAL_SCOPE_UNSUPPORTED_SIX_RATE"});
    if(channels!=1U&&channels!=2U)
        return R::failure(core::Error{
            core::ErrorCode::UnsupportedAudioLayout,
            "M20_REAL_SCOPE_EXPECTS_MONO_OR_STEREO"});
    const auto leftResult=view.channel(0U);
    if(!leftResult)return R::failure(*leftResult.error());
    const auto left=*leftResult.value();
    std::span<const double> right;
    if(channels==2U) {
        const auto r=view.channel(1U);
        if(!r)return R::failure(*r.error());
        right=*r.value();
    }
    const auto fs=static_cast<std::uint32_t>(rate);
    const auto origin=static_cast<std::uint64_t>(absoluteScopeBegin.value());
    const auto frames=static_cast<std::uint64_t>(scopeFrames.value());
    std::vector<m20_private::Interval> retained;
    retained.reserve(supplied.retained_width_blocks.size());
    for(const auto& block:supplied.retained_width_blocks)
        retained.push_back({block.begin,block.end});
    const m20_private::StereoScope stereo{
        fs,origin,left,right,supplied.has_width_gate,retained};
    if(!m20_private::valid_scope(stereo))
        return R::failure(core::Error{
            core::ErrorCode::InvalidAudioSample,
            "M20_REAL_SCOPE_NONFINITE_OR_INVALID_PCM"});

    m20_private::BoundedSpectrumAccumulatorCandidate accumulator{
        fs,origin,frames,static_cast<std::uint32_t>(channels),
        supplied.has_spectral_gate,supplied.spectral_frame_mask};
    constexpr std::size_t kChunk=4096U;
    for(std::size_t offset=0;offset<left.size();) {
        const auto take=std::min(kChunk,left.size()-offset);
        if(!accumulator.push(left.subspan(offset,take),
             channels==2U?right.subspan(offset,take)
                        :std::span<const double>{}))
            return R::failure(invalid("M20_REAL_SPECTRUM_GATE_OR_PCM_INVALID"));
        offset+=take;
    }
    auto spectrum=accumulator.finalize();
    if(spectrum.diagnostic==Diagnostic::invalid_input)
        return R::failure(invalid("M20_REAL_BOUNDED_SPECTRUM_INVALID"));

    const auto width=m20_private::width_ratio(stereo);
    const auto correlation=m20_private::correlation_summary(stereo);
    if(width.diagnostic==Diagnostic::invalid_input
       ||correlation.diagnostic==Diagnostic::invalid_input)
        return R::failure(invalid("M20_REAL_STEREO_AUTHORITY_INVALID"));

    TonalStereoAnalysisResult result;
    result.scope_begin_frame=absoluteScopeBegin.value();
    result.scope_frame_count=scopeFrames.value();
    result.sample_rate_hz=rate;
    result.channel_count=channels;
    result.frame_domain=expectedDomain;
    result.complete_spectral_windows=spectrum.complete_frames;
    result.selected_spectral_windows=spectrum.selected_frames;
    result.spectral_all_frames_fallback=spectrum.all_frames_fallback;
    result.width_all_scope_fallback=width.used_all_scope_fallback;
    result.spectrum_state=state(spectrum.diagnostic);
    result.width_ratio=metric(width.ratio,width.diagnostic);
    result.correlation_p10=metric(correlation.p10,correlation.diagnostic);
    result.correlation_p50=metric(correlation.p50,correlation.diagnostic);
    result.correlation_p90=metric(correlation.p90,correlation.diagnostic);
    result.correlation_grid100.reserve(correlation.series.size());
    for(const auto& w:correlation.series)
        result.correlation_grid100.push_back({
            w.absolute_start,metric(w.rho,w.diagnostic)});

    result.primary_bands_1_12.reserve(spectrum.bands_1_12.size());
    for(const auto& item:spectrum.bands_1_12) {
        result.primary_bands_1_12.push_back({
            item.band.index,item.band.center,item.band.low,item.band.high,
            item.band.fully_supported,item.measured_energy,
            {std::nullopt,State::UNAVAILABLE_AUTHORITATIVE_SUPPORT}});
    }
    if(spectrum.diagnostic==Diagnostic::valid) {
        // The only normalized fractions are those authorized by the
        // separately supplied Phase-6 per-band eligibility. Never
        // assume "all fully supported" means authoritative approval.
        if(!supplied.authoritative_band_eligibility.empty()) {
            const auto profile=m20_private::make_profile_candidate(
                spectrum.average_psd,fs,spectrum.geometry.fft,12,
                supplied.authoritative_band_eligibility);
            if(profile.diagnostic==m20_private::ProfileDiagnostic::invalid_input
               ||profile.bands.size()!=result.primary_bands_1_12.size())
                return R::failure(invalid("M20_REAL_PROFILE_SUPPORT_INVALID"));
            for(std::size_t i=0;i<profile.bands.size();++i) {
                const auto& candidate=profile.bands[i];
                result.primary_bands_1_12[i].normalized_fraction={
                    candidate.fraction,
                    candidate.fraction?State::AVAILABLE:
                        (profile.diagnostic==m20_private::ProfileDiagnostic::no_eligible_power
                         ?State::INSUFFICIENT_CONTENT
                         :State::UNAVAILABLE_AUTHORITATIVE_SUPPORT)};
            }
        }
        result.mean_linear_psd=std::move(spectrum.average_psd);
    } else if(!supplied.authoritative_band_eligibility.empty()) {
        // A claimed support vector cannot invent power when no complete
        // analysis window is available. Fail closed, not a fake profile.
        return R::failure(invalid("M20_REAL_PROFILE_NO_COMPLETE_SPECTRAL_FRAME"));
    }
    return R::success(std::move(result));
}

} // namespace rgsml::analysis
