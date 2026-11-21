// M20 numerical normalization/fallback/common-bandwidth contract screening.
// Uses the ACTUAL shared FFT/Hann/PSD from rgsml_analysis. No new FFT or Qt.
#include "internal/m20_scope_spectrum_candidate.hpp"
#include "internal/m20_normalized_profile_candidate.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numbers>
#include <vector>
using namespace rgsml::analysis::m20_private;

static double fraction_sum(const NormalizedProfileCandidate& p) {
  double sum=0;
  for (const auto& b:p.bands) {
    if (b.fraction) {
      assert(b.supplied==SuppliedBandEligibility::eligible);
      if (p.common_comparison_bandwidth) assert(b.fully_in_common_comparison);
      sum+=*b.fraction;
    } else assert(!b.measured_energy);
  }
  return sum;
}
static std::vector<std::uint8_t> policy(const std::size_t n) {
  std::vector<std::uint8_t> v(n,1);
  for (std::size_t i=0;i<n;++i) {
    if (i%17==0) v[i]=0;  // external authority: missing
    else if (i%19==0) v[i]=2; // external authority: low support
  }
  return v;
}
static void must_be_missing(const NormalizedProfileCandidate& result, std::size_t index){
  assert(!result.bands[index].fraction);
  assert(!result.bands[index].measured_energy);
}
int main(){
  constexpr std::array<std::uint32_t,6> rates{44100,48000,88200,96000,176400,192000};
  for(const auto fs:rates){
    const auto sg=spectral_geometry(fs);
    assert(sg);
    const auto count=static_cast<std::size_t>(sg->window+2*sg->hop);
    std::vector<double> l(count),r(count);
    for(std::size_t i=0;i<count;++i){
      l[i]=0.30*std::sin(2*std::numbers::pi*41.0*static_cast<double>(i)/static_cast<double>(sg->fft));
      r[i]=-0.35*std::sin(2*std::numbers::pi*91.0*static_cast<double>(i)/static_cast<double>(sg->fft));
    }
    const auto scope=scope_spectrum_candidate(ScopeSpectrumInput{fs,17001,l,r,false,{}});
    assert(scope.diagnostic==Diagnostic::valid);
    assert(scope.complete_frames==3&&scope.selected_frames==3);
    const auto common=common_bandwidth(fs,44100);
    assert(common&&common->high==19845);
    for(const int denom:{12,3}){
      const auto geom=fractional_band_geometry(fs,denom);
      const auto codes=policy(geom.size());
      const auto p=make_profile_candidate(scope.average_psd,fs,sg->fft,denom,codes);
      assert(p.diagnostic==ProfileDiagnostic::valid);
      assert(p.bands.size()==geom.size());
      assert(std::abs(fraction_sum(p)-1)<1e-12);
      for(std::size_t i=0;i<p.bands.size();++i){
        if(codes[i]!=1)must_be_missing(p,i);
        assert(p.bands[i].fully_in_measurement==geom[i].fully_supported);
      }
      const auto comparison=make_profile_candidate(scope.average_psd,fs,sg->fft,denom,codes,common);
      assert(comparison.diagnostic==ProfileDiagnostic::valid);
      assert(comparison.common_comparison_bandwidth);
      assert(std::abs(fraction_sum(comparison)-1)<1e-12);
      bool partial=false;
      for(std::size_t i=0;i<comparison.bands.size();++i) {
        const auto& x=comparison.bands[i];
        if(!x.fully_in_common_comparison) {
          must_be_missing(comparison,i);
          partial=true;
        }
        if(x.fraction) assert(x.geometry.low>=common->low&&x.geometry.high<=common->high);
      }
      assert(partial);
      // Positive gain scales mean PSD by gain^2; normalized tonal fractions invariant.
      auto doubled=scope.average_psd;
      for(double& x:doubled) x*=4;
      const auto scaled=make_profile_candidate(doubled,fs,sg->fft,denom,codes,common);
      assert(scaled.diagnostic==ProfileDiagnostic::valid);
      for(std::size_t i=0;i<comparison.bands.size();++i){
        const auto& a=comparison.bands[i].fraction;
        const auto& b=scaled.bands[i].fraction;
        assert(a.has_value()==b.has_value());
        if(a)assert(std::abs(*a-*b)<1e-12);
      }
      auto none=std::vector<std::uint8_t>(codes.size(),0);
      const auto no_support=make_profile_candidate(scope.average_psd,fs,sg->fft,denom,none);
      assert(no_support.diagnostic==ProfileDiagnostic::no_eligible_power);
      assert(!no_support.eligible_power_sum);
      for(std::size_t i=0;i<none.size();++i)must_be_missing(no_support,i);
      const auto silence=make_profile_candidate(
        std::vector<double>(scope.average_psd.size(),0),fs,sg->fft,denom,codes);
      assert(silence.diagnostic==ProfileDiagnostic::no_eligible_power);
      assert(!silence.eligible_power_sum);
      assert(make_profile_candidate(scope.average_psd,fs,sg->fft,denom,
        std::vector<std::uint8_t>(geom.size()-1,1)).diagnostic==ProfileDiagnostic::invalid_input);
      auto bad=codes;bad[0]=3;
      assert(make_profile_candidate(scope.average_psd,fs,sg->fft,denom,bad).
        diagnostic==ProfileDiagnostic::invalid_input);
      assert(make_profile_candidate(scope.average_psd,fs,sg->fft,denom,codes,
        Bandwidth{1,20000}).diagnostic==ProfileDiagnostic::invalid_input);
      assert(make_profile_candidate(scope.average_psd,fs,sg->fft,denom,codes,
        Bandwidth{19000,18000}).diagnostic==ProfileDiagnostic::invalid_input);
      if(fs==44100){
        assert(make_profile_candidate(scope.average_psd,fs,sg->fft,denom,codes,
          Bandwidth{20,20000}).diagnostic==ProfileDiagnostic::invalid_input);
      }
      auto nonfinite=scope.average_psd;nonfinite[0]=std::numeric_limits<double>::infinity();
      assert(make_profile_candidate(nonfinite,fs,sg->fft,denom,codes).
        diagnostic==ProfileDiagnostic::invalid_input);
      nonfinite[0]=-1;
      assert(make_profile_candidate(nonfinite,fs,sg->fft,denom,codes).
        diagnostic==ProfileDiagnostic::invalid_input);
    }
  }
  std::cout<<"M20_SIX_RATE_NORMALIZED_PROFILE_AND_COMMON_SUPPORT_PASS_NOT_GATE_G\n";
}
