// Native end-to-end read-only M20 presenter inputs, no QML math.
#include "internal/m20_presentation_candidate.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <vector>
using namespace rgsml::analysis::m20_private;
int main() {
 for(std::uint32_t fs:{44100U,48000U,88200U,96000U,176400U,192000U}) {
  const auto geometry=spectral_geometry(fs);assert(geometry);
  M20ScopeCandidate s;
  s.absolute_scope_start=3333U;
  s.scope_frames=static_cast<std::uint64_t>(fs);
  s.spectrum.diagnostic=Diagnostic::valid;
  s.spectrum.geometry=*geometry;
  s.spectrum.all_frames_fallback=true;
  s.spectrum.average_psd.resize(
    static_cast<std::size_t>(geometry->fft/2U+1U),1e-6);
  s.spectrum.average_psd[10]=0;
  s.width.diagnostic=Diagnostic::valid;
  s.width.ratio=2.5;
  s.width.used_all_scope_fallback=true;
  s.correlation.diagnostic=Diagnostic::valid;
  s.correlation.window_frames=round_even_div(2ULL*fs,5);
  s.correlation.p10=-0.8;s.correlation.p50=0;s.correlation.p90=0.9;
  s.correlation.series.push_back({3333,Diagnostic::valid,0.0});
  s.correlation.series.push_back({3333+fs/10,Diagnostic::insufficient_content,{}});
  const auto b=fractional_band_geometry(fs,12);
  std::vector<std::uint8_t> eligibility(b.size(),std::uint8_t{1});
  if(b.size()>4)eligibility[4]=std::uint8_t{2};
  const auto p=make_profile_candidate(s.spectrum.average_psd,
    fs,geometry->fft,12,eligibility);
  assert(p.diagnostic==ProfileDiagnostic::valid);
  auto ui=make_m20_readonly_view_candidate(s,&p);
  assert(ui.any_actual_content&&ui.has_spectrum&&ui.has_profile&&
         ui.has_width&&ui.has_correlation);
  assert(ui.width_text=="2.50"&&ui.p10_text=="-0.80"&&
         ui.p50_text=="0.00"&&ui.p90_text=="0.90");
  assert(ui.support_text.find("ALL_FRAMES")!=std::string::npos);
  assert(ui.support_text.find("ALL_SCOPE")!=std::string::npos);
  assert(!ui.spectrum_points.empty()&&!ui.profile_bands.empty());
  assert(ui.correlation_recent.size()==2);
  assert(ui.correlation_recent[0].valid);
  assert(!ui.correlation_recent[1].valid);
  assert(std::any_of(ui.spectrum_points.begin(),ui.spectrum_points.end(),
    [](const M20PlotPointCandidate& x){return x.genuine_zero&&x.valid;}));
  bool has_low_support=false;
  for(const auto& x:ui.profile_bands) {
    assert(x.plot.x>=0&&x.plot.x<=1&&x.plot.y>=0&&x.plot.y<=1);
    if(x.support=="LOW_SUPPORT"){
      has_low_support=true;
      assert(!x.plot.valid);
    }
  }
  assert(has_low_support);
  s.width.diagnostic=Diagnostic::not_applicable;s.width.ratio.reset();
  s.width.used_all_scope_fallback=false;
  s.correlation.diagnostic=Diagnostic::not_applicable;
  s.correlation.p10.reset();s.correlation.p50.reset();s.correlation.p90.reset();
  auto mono=make_m20_readonly_view_candidate(s,&p);
  assert(mono.mono_not_applicable&&!mono.has_width&&!mono.has_correlation);
  assert(mono.width_text=="N/A · mono");
  s.spectrum.diagnostic=Diagnostic::insufficient_content;
  s.spectrum.average_psd.clear();
  auto missing=make_m20_readonly_view_candidate(s);
  assert(!missing.any_actual_content&&!missing.has_spectrum);
  assert(missing.width_text=="N/A · mono");
 }
 std::cout<<"M20_READONLY_QML_PRESENTER_SIX_RATE_PASS_NOT_GATE_G\n";
}
