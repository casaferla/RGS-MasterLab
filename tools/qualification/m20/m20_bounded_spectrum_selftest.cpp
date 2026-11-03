// M20 #8: genuinely bounded DSP accumulation vs original whole-scope baseline.
#include "internal/m20_bounded_spectrum_candidate.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numbers>
#include <vector>
using namespace rgsml::analysis::m20_private;
static void same(const BoundedSpectrumCandidate& s,const ScopeSpectrumResult& w){
 assert(s.diagnostic==w.diagnostic);
 assert(s.complete_frames==w.complete_frames);
 assert(s.selected_frames==w.selected_frames);
 assert(s.all_frames_fallback==w.all_frames_fallback);
 assert(s.average_psd.size()==w.average_psd.size());
 for(std::size_t k=0;k<s.average_psd.size();++k){
  const double a=s.average_psd[k],b=w.average_psd[k];
  assert(std::isfinite(a)&&std::isfinite(b));
  assert(std::abs(a-b)<=1e-13*std::max({1e-10,std::abs(a),std::abs(b)}));
 }
 assert(s.bands_1_12.size()==w.bands_1_12.size());
 for(std::size_t k=0;k<s.bands_1_12.size();++k){
  const double a=s.bands_1_12[k].measured_energy;
  const double b=w.bands_1_12[k].measured_energy;
  assert(std::abs(a-b)<=1e-13*std::max({1e-10,std::abs(a),std::abs(b)}));
 }
}
int main(){
 for(std::uint32_t fs:{44100U,48000U,88200U,96000U,176400U,192000U}){
  const auto g=spectral_geometry(fs);assert(g);
  const auto count=static_cast<std::size_t>(2*g->window+3*g->hop);
  std::vector<double> a(count),b(count);
  for(std::size_t i=0;i<count;++i){
    a[i]=0.37*std::sin(2.0*std::numbers::pi*500.0*
        static_cast<double>(i)/static_cast<double>(fs));
    b[i]=-0.23*std::cos(2.0*std::numbers::pi*800.0*
        static_cast<double>(i)/static_cast<double>(fs));
  }
  const auto complete=1+(count-g->window)/g->hop;
  std::vector<std::uint8_t> mask(complete,std::uint8_t{0});
  for(std::size_t i=0;i<mask.size();++i)
    if(i%3==0)mask[i]=std::uint8_t{1};
  for(const bool stereo:{false,true})for(const bool gated:{false,true}){
    const auto right=stereo?
       std::span<const double>(b):std::span<const double>{};
    const auto selected=gated?
       std::span<const std::uint8_t>(mask):std::span<const std::uint8_t>{};
    const auto w=scope_spectrum_candidate(ScopeSpectrumInput{
       fs,312345,a,right,gated,selected});
    assert(w.diagnostic==Diagnostic::valid);
    for(std::size_t chunk:{1U,7U,31U,127U,4096U}){
      BoundedSpectrumAccumulatorCandidate acc(
          fs,312345,count,stereo?2U:1U,gated,selected);
      for(std::size_t pos=0;pos<count;){
        const auto take=std::min(chunk,count-pos);
        const auto l=std::span<const double>(a).subspan(pos,take);
        const auto r=stereo?
          std::span<const double>(b).subspan(pos,take):
          std::span<const double>{};
        assert(acc.push(l,r));
        pos+=take;
      }
      const auto s=acc.finalize();
      same(s,w);
      assert(s.working_double_equivalents<
           3*static_cast<std::size_t>(g->window)+
           4*static_cast<std::size_t>(g->fft));
      assert(acc.finalize().diagnostic==Diagnostic::invalid_input);
    }
  }
  std::fill(mask.begin(),mask.end(),std::uint8_t{0});
  BoundedSpectrumAccumulatorCandidate empty(fs,312345,count,1,true,mask);
  assert(empty.push(a));
  const auto missing=empty.finalize();
  assert(missing.diagnostic==Diagnostic::insufficient_content);
  assert(!missing.all_frames_fallback);
  BoundedSpectrumAccumulatorCandidate guard(fs,312345,count,1);
  auto invalid=a; invalid[3]=std::numeric_limits<double>::infinity();
  assert(!guard.push(std::span<const double>(invalid).subspan(0,7)));
  assert(guard.push(a));
  assert(guard.finalize().diagnostic==Diagnostic::valid);
  const std::array<std::uint8_t,1> malformed{{2}};
  BoundedSpectrumAccumulatorCandidate badMask(fs,312345,count,1,true,malformed);
  assert(!badMask.push(a));
 }
 std::cout<<"M20_BOUNDED_MEMORY_CHUNK_INVARIANCE_SIX_RATE_PASS_NOT_GATE_G\n";
}
