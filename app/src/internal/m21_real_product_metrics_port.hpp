#pragma once
// L1-M21 post-M20 Product DTO port. This adapter consumes the REAL public
// M19/M20 AnalysisScope results, not strings/demo data. It is deliberately
// not a canonical Phase-6 publisher or UI authorization mechanism:
// the caller-supplied owner/generation claim still needs reconciliation
// with actual SOURCE/PREPARED/GOLD/PROCESSED Product lifecycle.
#include "m21_metrics_comparison_candidate.hpp"
#include <rgsml/analysis/basic_level_analyzer.hpp>
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <internal/m20_tonal_candidate.hpp>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace rgsml::app::m21_private {

struct ProductMetricsOwnerClaim final {
    Column column{Column::prepared};
    std::uint64_t generation_token{0};
    std::string exact_realization_id;
    std::string scope_label; // "Full Track" or "Region"
    bool external_product_owner_confirmed{false};
};

enum class RealProductPortProblem : std::uint8_t {
    none,missing_authority,mismatched_scope,unsupported_rate,
    invalid_scalar,invalid_spectrum
};
struct RealProductPortResult final {
    std::optional<Snapshot> snapshot;
    RealProductPortProblem problem{RealProductPortProblem::missing_authority};
};

[[nodiscard]] inline Cell basic_level_cell(
    const analysis::BasicLevelValue& metric,
    const char* unavailable)
{
    using S=analysis::BasicLevelValueState;
    switch(metric.state) {
    case S::AVAILABLE:
        if(!metric.db||!std::isfinite(*metric.db))
            return {{},Quality::invalid,"Invalid available M19 scalar"};
        return {metric.db,Quality::present,{}};
    case S::M16_SUPPORTED_UNQUALIFIED:
        // A numerical true peak returned by the M16 public API is not
        // evidence of release qualification. NEVER silently elevate it.
        return {{},Quality::missing,"M16 TP public status unqualified"};
    case S::NUMERIC_UNDEFINED:
        return {{},Quality::invalid,"M19 numerical analysis undefined"};
    case S::SILENCE:
    case S::INSUFFICIENT_DURATION:
    case S::BELOW_LOUDNESS_GATE:
    case S::M16_SCOPE_UNAVAILABLE:
        return {{},Quality::missing,unavailable};
    }
    return {{},Quality::invalid,"Unknown M19 metric status"};
}

[[nodiscard]] inline Cell tonal_stereo_cell(
    const analysis::TonalStereoValue& metric,
    const char* unavailable)
{
    using S=analysis::TonalStereoMetricState;
    switch(metric.state) {
    case S::AVAILABLE:
        if(!metric.value||!std::isfinite(*metric.value))
            return {{},Quality::invalid,"Invalid available M20 scalar"};
        return {metric.value,Quality::present,{}};
    case S::NOT_APPLICABLE:
        return {{},Quality::not_applicable,"Mono: no stereo metric"};
    case S::INSUFFICIENT_CONTENT:
        return {{},Quality::missing,"Insufficient M20 scoped samples"};
    case S::UNDEFINED_DENOMINATOR:
        return {{},Quality::invalid,"Undefined M20 stereo denominator"};
    case S::UNAVAILABLE_AUTHORITATIVE_SUPPORT:
        return {{},Quality::missing,unavailable};
    }
    return {{},Quality::invalid,"Unknown M20 metric status"};
}

// Exactly nine rows, PREPARED/GOLD/PROCESSED only. This function never
// fabricates RAW as PREPARED, Gold alignment, M16 TP qualification or tonal
// shape dB from M20's normalized linear-power fractions. All row deltas
// remain the existing Gold - Current unrounded M21 comparator's job.
[[nodiscard]] inline RealProductPortResult import_real_product_metrics(
    const analysis::BasicLevelAnalysisResult& l,
    const analysis::TonalStereoAnalysisResult& s,
    const ProductMetricsOwnerClaim& owner)
{
    const auto fail=[](RealProductPortProblem p){
        return RealProductPortResult{{},p};
    };
    if(!owner.external_product_owner_confirmed
       ||owner.generation_token==0
       ||owner.exact_realization_id.empty()
       ||(owner.scope_label!="Full Track"&&owner.scope_label!="Region"))
        return fail(RealProductPortProblem::missing_authority);
    if(l.scope_begin_frame<0||l.scope_frame_count<=0
       ||l.scope_begin_frame!=s.scope_begin_frame
       ||l.scope_frame_count!=s.scope_frame_count
       ||l.sample_rate_hz!=s.sample_rate_hz
       ||l.channel_count!=s.channel_count
       ||l.frame_domain!=s.frame_domain
       ||l.channel_count<1||l.channel_count>2)
        return fail(RealProductPortProblem::mismatched_scope);
    if(l.sample_rate_hz<=0||
       l.sample_rate_hz>(std::numeric_limits<std::uint32_t>::max)()||
       !supported_private_rate(static_cast<std::uint32_t>(l.sample_rate_hz)))
        return fail(RealProductPortProblem::unsupported_rate);

    Snapshot out;
    out.column=owner.column;
    out.generation_token=owner.generation_token;
    out.resource_identity=owner.exact_realization_id;
    out.analysis_scope_label=owner.scope_label;
    out.absolute_scope_begin=static_cast<std::uint64_t>(
        l.scope_begin_frame);
    out.scope_frames=static_cast<std::uint64_t>(l.scope_frame_count);
    out.sample_rate_hz=static_cast<std::uint32_t>(l.sample_rate_hz);
    out.lifecycle=Lifecycle::current;
    out.metrics[0]=basic_level_cell(l.integrated_lufs,"No gated LUFS");
    out.metrics[1]=basic_level_cell(l.full_scope_rms_dbfs,"RMS missing");
    out.metrics[2]=basic_level_cell(l.sample_peak_dbfs,"Sample peak missing");
    out.metrics[3]=basic_level_cell(l.true_peak_db_tp,"True peak missing");
    out.metrics[4]=basic_level_cell(l.sample_peak_crest_db,"Crest missing");
    out.metrics[5]=tonal_stereo_cell(s.width_ratio,"Width support missing");
    out.metrics[6]=tonal_stereo_cell(s.correlation_p10,"Correlation P10 missing");
    out.metrics[7]=tonal_stereo_cell(s.correlation_p50,"Correlation P50 missing");
    out.metrics[8]=tonal_stereo_cell(s.correlation_p90,"Correlation P90 missing");
    for(const auto& cell:out.metrics)
        if(!valid_cell_storage(cell))
            return fail(RealProductPortProblem::invalid_scalar);

    // Static PSD is carried only if M20 supplied a valid real spectrum.
    // Frequency bins are derived from the same canonical M20 geometry,
    // never a guessed bin count or log-frequency UI display value.
    if(s.spectrum_state==analysis::TonalStereoMetricState::AVAILABLE) {
        const auto g=analysis::m20_private::spectral_geometry(
            out.sample_rate_hz);
        if(!g||g->fft<2
           ||s.mean_linear_psd.size()!=g->fft/2U+1U)
            return fail(RealProductPortProblem::invalid_spectrum);
        out.average_spectrum.reserve(s.mean_linear_psd.size());
        for(std::size_t k=0;k<s.mean_linear_psd.size();++k) {
            const auto power=s.mean_linear_psd[k];
            if(!std::isfinite(power)||power<0)
                return fail(RealProductPortProblem::invalid_spectrum);
            const auto hz=static_cast<double>(k)
                *static_cast<double>(out.sample_rate_hz)
                /static_cast<double>(g->fft);
            out.average_spectrum.push_back({
                hz,{power,Quality::present,{}}});
        }
    }else if(!s.mean_linear_psd.empty()) {
        return fail(RealProductPortProblem::invalid_spectrum);
    }
    out.profile_denominator=0; // linear fractions != shape dB
    return {std::move(out),RealProductPortProblem::none};
}
} // namespace rgsml::app::m21_private
