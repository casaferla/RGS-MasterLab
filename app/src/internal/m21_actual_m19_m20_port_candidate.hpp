#pragma once
// M21 PRIVATE typed import from ACTUAL M19/M20 preproduction DTOs.
// No Phase-6 FeatureStatus, ownership authority, qualified TP, or product binding.
#include "m21_metrics_comparison_candidate.hpp"
#include "internal/m19_basic_level_candidate.hpp"
#include "internal/m20_audio_scope_adapter_candidate.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
namespace rgsml::app::m21_private {
namespace m19=rgsml::analysis::internal::m19;
namespace m20=rgsml::analysis::m20_private;
struct ExternalProducerDisclosure {
  Column column{Column::prepared};
  std::uint64_t generation_token{};
  std::string exact_realization_id{};
  std::string scope_label{};
  bool qualified_owner_token_provided{false}; // Private test seam ONLY.
};
enum class PortProblem : std::uint8_t {
  none, missing_owner_authority, invalid_scope, source_mismatch,
  invalid_m19, invalid_m20, forbidden_unqualified_tp
};
struct PortResult {
  std::optional<Snapshot> snapshot;
  PortProblem problem{PortProblem::missing_owner_authority};
};
inline Cell metric_optional(const std::optional<double>& x,const char* reason) {
  if(!x)return {{},Quality::missing,reason};
  if(!std::isfinite(*x))return {{},Quality::invalid,"Non-finite upstream metric"};
  return {*x,Quality::present,{}};
}
inline Cell status_from_m20(m20::Diagnostic d,const std::optional<double>& value) {
  if(d==m20::Diagnostic::not_applicable)
    return {{},Quality::not_applicable,"Mono layout"};
  if(d==m20::Diagnostic::invalid_input)
    return {{},Quality::invalid,"Invalid upstream M20 status"};
  if(d==m20::Diagnostic::undefined_denominator)
    return {{},Quality::invalid,"Undefined stereo denominator"};
  if(d==m20::Diagnostic::insufficient_content)
    return {{},Quality::missing,"Insufficient in-scope support"};
  return metric_optional(value,"M20 status valid but feature missing");
}
inline PortResult import_actual_private_m19_m20(
  const m19::BasicLevelPreproductionSnapshot& l,
  const m20::M20ScopeCandidate& s,
  const ExternalProducerDisclosure& owner)
{
  auto fail=[](PortProblem x){return PortResult{{},x};};
  if(!owner.qualified_owner_token_provided || owner.generation_token==0 ||
     owner.exact_realization_id.empty() ||
     (owner.scope_label!="Full Track" && owner.scope_label!="Region"))
    return fail(PortProblem::missing_owner_authority);
  if(l.scope_frames==0 || l.sample_rate_hz==0 ||
     l.scope_begin_frame>(std::numeric_limits<std::uint64_t>::max)()-l.scope_frames ||
     l.channel_count<1 || l.channel_count>2)
    return fail(PortProblem::invalid_scope);
  if(l.scope_begin_frame!=s.absolute_scope_start ||
     l.scope_frames!=s.scope_frames ||
     l.sample_rate_hz!=s.spectrum.geometry.fs ||
     !m20::supported_rate(l.sample_rate_hz))
    return fail(PortProblem::source_mismatch);
  if(l.true_peak_db_tp.has_value())
    return fail(PortProblem::forbidden_unqualified_tp); // M16 not carried by candidate.
  if(l.loudness_state==m19::LoudnessPreproductionState::not_finalized)
    return fail(PortProblem::invalid_m19);
  if(s.spectrum.diagnostic==m20::Diagnostic::invalid_input ||
     s.width.diagnostic==m20::Diagnostic::invalid_input ||
     s.correlation.diagnostic==m20::Diagnostic::invalid_input)
    return fail(PortProblem::invalid_m20);
  if((l.channel_count==1 && (s.width.diagnostic!=m20::Diagnostic::not_applicable ||
                             s.correlation.diagnostic!=m20::Diagnostic::not_applicable)) ||
     (l.channel_count==2 && (s.width.diagnostic==m20::Diagnostic::not_applicable ||
                             s.correlation.diagnostic==m20::Diagnostic::not_applicable)))
    return fail(PortProblem::source_mismatch);
  Snapshot out;
  out.column=owner.column;out.lifecycle=Lifecycle::current;
  out.generation_token=owner.generation_token;
  out.resource_identity=owner.exact_realization_id;
  out.analysis_scope_label=owner.scope_label;
  out.absolute_scope_begin=l.scope_begin_frame;
  out.scope_frames=l.scope_frames;out.sample_rate_hz=l.sample_rate_hz;
  using Loud=m19::LoudnessPreproductionState;
  out.metrics[0]=l.loudness_state==Loud::available?
     metric_optional(l.integrated_lufs,"Missing gated loudness"):
     Cell{{},Quality::missing,"No valid gated loudness in scope"};
  out.metrics[1]=metric_optional(l.full_scope_rms_dbfs,"RMS unavailable");
  out.metrics[2]=metric_optional(l.sample_peak_dbfs,"Peak unavailable");
  out.metrics[3]={{},Quality::missing,"M16 qualified True Peak owner not bound"};
  out.metrics[4]=metric_optional(l.sample_peak_crest_db,"Crest unavailable");
  out.metrics[5]=status_from_m20(s.width.diagnostic,s.width.ratio);
  out.metrics[6]=status_from_m20(s.correlation.diagnostic,s.correlation.p10);
  out.metrics[7]=status_from_m20(s.correlation.diagnostic,s.correlation.p50);
  out.metrics[8]=status_from_m20(s.correlation.diagnostic,s.correlation.p90);
  if(s.spectrum.diagnostic==m20::Diagnostic::valid) {
    const auto fft=s.spectrum.geometry.fft;
    if(fft==0 || s.spectrum.average_psd.size()!=fft/2U+1U)
      return fail(PortProblem::invalid_m20);
    out.average_spectrum.reserve(s.spectrum.average_psd.size());
    for(std::size_t k=0;k<s.spectrum.average_psd.size();++k) {
      const auto p=s.spectrum.average_psd[k];
      if(!std::isfinite(p) || p<0)return fail(PortProblem::invalid_m20);
      const double hz=static_cast<double>(k)*out.sample_rate_hz/fft;
      if(!std::isfinite(hz))return fail(PortProblem::invalid_m20);
      out.average_spectrum.push_back({hz,{p,Quality::present,{}}});
    }
  }
  // M20 profile fractions are NOT normalized shape dB, so NEVER derive
  // M21 tonal dB by inventing a second spectral analysis/log conversion.
  // A future authenticated canonical shapeDb producer must supply them.
  out.profile_denominator=0;
  return {std::move(out),PortProblem::none};
}
}
