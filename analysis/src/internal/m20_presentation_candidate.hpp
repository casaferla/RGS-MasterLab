#ifndef RGSML_M20_PRIVATE_PRESENTATION_CANDIDATE_HPP
#define RGSML_M20_PRIVATE_PRESENTATION_CANDIDATE_HPP
// M20 PRIVATE READ-ONLY PREVIEW DTO. Proposed visual mapping (NOT PO approved).
// PCM/PSD/support belong to analysis; QML receives ONLY already plotted coords
// and complete textual statuses, never computes feature math.
#include "m20_m19_phase6_gate_port.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace rgsml::analysis::m20_private {
struct M20PlotPointCandidate {
  double x=0, y=0; // normalized UI coordinates, NOT canonical feature values.
  bool valid=false;
  bool genuine_zero=false; // measured zero distinct from unsupported/missing.
};
struct M20BandViewCandidate {
  M20PlotPointCandidate plot{};
  std::string label;
  std::string support; // "VALID"/"LOW_SUPPORT"/"MISSING" private labels.
};
struct M20ReadOnlyViewCandidate {
  bool has_spectrum=false,has_profile=false,has_width=false,has_correlation=false;
  bool mono_not_applicable=false;
  bool spectral_all_frames_fallback=false,width_all_scope_fallback=false;
  bool any_actual_content=false;
  int profile_denominator=0;
  std::string headline="BASIC TONAL + STEREO";
  std::string support_text="No supported selected scope";
  std::string width_text="—";
  std::string p10_text="—",p50_text="—",p90_text="—";
  std::vector<M20PlotPointCandidate> spectrum_points;
  std::vector<M20BandViewCandidate> profile_bands;
  std::vector<M20PlotPointCandidate> correlation_recent; // last 5s static tail only.
};
[[nodiscard]] inline std::string decimal_label(double x,int places=2){
  if(!std::isfinite(x)) return "—";
  std::ostringstream os;
  os.imbue(std::locale::classic());
  os<<std::fixed<<std::setprecision(places)<<x;
  return os.str();
}
[[nodiscard]] inline M20ReadOnlyViewCandidate make_m20_readonly_view_candidate(
    const M20ScopeCandidate& scope,
    const NormalizedProfileCandidate* profile=nullptr)
{
  M20ReadOnlyViewCandidate out;
  out.spectral_all_frames_fallback=scope.spectrum.all_frames_fallback;
  out.width_all_scope_fallback=scope.width.used_all_scope_fallback;
  out.mono_not_applicable =
    scope.width.diagnostic==Diagnostic::not_applicable;
  if(out.mono_not_applicable) out.width_text="N/A · mono";

  const auto& sp=scope.spectrum;
  if(sp.diagnostic==Diagnostic::valid &&
     supported_rate(sp.geometry.fs) &&
     sp.geometry.fft>0 &&
     sp.average_psd.size()==sp.geometry.fft/2U+1U) {
    const auto high=available_bandwidth(sp.geometry.fs).high;
    const double base=std::log2(high/20.0);
    for(std::size_t k=1;k<sp.average_psd.size();++k) {
      const double hz=static_cast<double>(k)*sp.geometry.fs/sp.geometry.fft;
      if(hz<20.0||hz>high)continue;
      const double p=sp.average_psd[k];
      if(!std::isfinite(p)||p<0.0)continue;
      M20PlotPointCandidate point;
      point.x=std::log2(hz/20.0)/base;
      point.valid=true;
      point.genuine_zero=(p==0.0);
      // PRIVATE display-only candidate y mapping: fixed -140..0 dB(power/Hz).
      // This is neither a feature floor nor a final product UI decision.
      point.y=point.genuine_zero?0.0:
        std::clamp((10.0*std::log10(p)+140.0)/140.0,0.0,1.0);
      out.spectrum_points.push_back(point);
    }
    out.has_spectrum=!out.spectrum_points.empty();
  }
  if(profile && profile->diagnostic==ProfileDiagnostic::valid &&
     supported_rate(profile->sample_rate) &&
     (profile->denominator==12||profile->denominator==3) &&
     profile->sample_rate==sp.geometry.fs) {
    out.profile_denominator=profile->denominator;
    const auto high=available_bandwidth(profile->sample_rate).high;
    const double xbase=std::log2(high/20.0);
    double max_fraction=0.0;
    for(const auto& band:profile->bands)
      if(band.fraction && std::isfinite(*band.fraction))
        max_fraction=std::max(max_fraction,*band.fraction);
    if(max_fraction>0.0) {
      for(const auto& band:profile->bands) {
        M20BandViewCandidate item;
        item.label=decimal_label(band.geometry.center,0)+" Hz";
        item.support=band.supplied==SuppliedBandEligibility::eligible
          ? "VALID" : band.supplied==SuppliedBandEligibility::low_support
            ? "LOW_SUPPORT":"MISSING";
        item.plot.x=std::clamp(std::log2(band.geometry.center/20.0)/xbase,0.0,1.0);
        if(band.fraction && std::isfinite(*band.fraction) &&
           *band.fraction>=0.0 && *band.fraction<=1.0) {
          item.plot.valid=true;
          item.plot.genuine_zero=(*band.fraction==0.0);
          item.plot.y=std::clamp(*band.fraction/max_fraction,0.0,1.0);
        }
        out.profile_bands.push_back(item);
      }
      out.has_profile=true;
    }
  }
  if(scope.width.diagnostic==Diagnostic::valid &&
     scope.width.ratio && std::isfinite(*scope.width.ratio)) {
    out.has_width=true;
    out.width_text=decimal_label(*scope.width.ratio);
  }
  if(scope.correlation.diagnostic==Diagnostic::valid &&
     scope.correlation.p10 &&scope.correlation.p50&&scope.correlation.p90) {
    out.has_correlation=true;
    out.p10_text=decimal_label(*scope.correlation.p10);
    out.p50_text=decimal_label(*scope.correlation.p50);
    out.p90_text=decimal_label(*scope.correlation.p90);
    // Candidate 5-second retrospective from the FROZEN complete Grid100 series.
    // No independent temporal grid, interpolation, or invented invalid zero.
    const auto end=scope.absolute_scope_start+scope.scope_frames;
    const auto fs=scope.correlation.window_frames>0 ?
      scope.spectrum.geometry.fs : std::uint32_t{0};
    const auto tail=fs>0&&scope.scope_frames>5ULL*fs ?
      end-5ULL*fs : scope.absolute_scope_start;
    const auto duration=end>tail?end-tail:0;
    for(const auto& w:scope.correlation.series) {
      if(w.absolute_start<tail||w.absolute_start>=end)continue;
      M20PlotPointCandidate point;
      point.x=duration>0?
        static_cast<double>(w.absolute_start-tail)/static_cast<double>(duration):0.0;
      point.valid=w.diagnostic==Diagnostic::valid&&w.rho.has_value();
      if(point.valid) {
        point.y=std::clamp((*w.rho+1.0)/2.0,0.0,1.0);
        point.genuine_zero=(*w.rho==0);
      }
      out.correlation_recent.push_back(point);
    }
  }
  out.any_actual_content=
      out.has_spectrum||out.has_profile||out.has_width||out.has_correlation;
  if(out.any_actual_content) {
    out.support_text=out.spectral_all_frames_fallback
      ? "Tonal: ALL_FRAMES fallback (retained gate unavailable)"
      : "Tonal: retained-gate selection";
    if(out.width_all_scope_fallback && !out.mono_not_applicable)
      out.support_text+=" · Stereo: ALL_SCOPE fallback";
  }
  return out;
}
} // namespace rgsml::analysis::m20_private
#endif
