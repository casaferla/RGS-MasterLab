#include "internal/m20_scope_spectrum_candidate.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <vector>
using namespace rgsml::analysis::m20_private;
int main(){
  constexpr std::array<std::uint32_t,6> rates{44100,48000,88200,96000,176400,192000};
  for(const auto fs:rates){
    const auto g=spectral_geometry(fs);assert(g);
    const std::size_t n=static_cast<std::size_t>(g->window+3*g->hop+g->hop/2);
    std::vector<double> left(n),right(n);
    for(std::size_t i=0;i<n;++i){
      left[i]=0.2*std::sin(2*std::numbers::pi*59.0*static_cast<double>(i)/g->fft);
      right[i]=left[i];
    }
    ScopeSpectrumInput in{fs,900001,left,right,false,{}};
    const auto baseline=scope_spectrum_candidate(in);
    assert(baseline.diagnostic==Diagnostic::valid&&baseline.all_frames_fallback&&
      baseline.complete_frames==4&&baseline.selected_frames==4);
    assert(baseline.selected_absolute_starts.size()==4&&baseline.selected_absolute_starts[0]==900001);
    assert(baseline.selected_absolute_starts.back()==900001+3*g->hop);
    assert(baseline.average_psd.size()==g->fft/2+1&&!baseline.bands_1_12.empty());
    // Logical-scope reconstruction from independent feeder sizes; bitwise numerical identity.
    // This is NOT a claim of a production bounded-memory streaming architecture.
    for(std::size_t chunk:{1U,7U,31U,127U,4096U}){
      std::vector<double> ll,rr;ll.reserve(n);rr.reserve(n);
      for(std::size_t i=0;i<n;i+=chunk){
        const auto end=std::min(n,i+chunk);
        ll.insert(ll.end(),left.begin()+static_cast<std::ptrdiff_t>(i),
                  left.begin()+static_cast<std::ptrdiff_t>(end));
        rr.insert(rr.end(),right.begin()+static_cast<std::ptrdiff_t>(i),
                  right.begin()+static_cast<std::ptrdiff_t>(end));
      }
      const auto reassembled=scope_spectrum_candidate(ScopeSpectrumInput{fs,900001,ll,rr,false,{}});
      assert(reassembled.diagnostic==Diagnostic::valid&&
        reassembled.average_psd==baseline.average_psd&&
        reassembled.selected_absolute_starts==baseline.selected_absolute_starts);
    }
    const auto mono=scope_spectrum_candidate(ScopeSpectrumInput{fs,900001,left,{},false,{}});
    assert(mono.diagnostic==Diagnostic::valid&&mono.average_psd==baseline.average_psd);
    const std::array<std::uint8_t,4> mask{0,1,0,1};
    in.authoritative_active_gate_available=true;in.active_frame_mask=mask;
    const auto selected=scope_spectrum_candidate(in);
    assert(selected.diagnostic==Diagnostic::valid&&!selected.all_frames_fallback&&selected.selected_frames==2);
    assert(selected.selected_absolute_starts[0]==900001+g->hop);
    const std::array<std::uint8_t,4> empty{0,0,0,0};in.active_frame_mask=empty;
    const auto no_active=scope_spectrum_candidate(in);
    assert(no_active.diagnostic==Diagnostic::insufficient_content&&
      !no_active.all_frames_fallback&&no_active.average_psd.empty());
    in.active_frame_mask={};
    assert(scope_spectrum_candidate(in).diagnostic==Diagnostic::invalid_input);
    in.authoritative_active_gate_available=false;
    const std::array<std::uint8_t,4> contradictory{1,1,1,1};in.active_frame_mask=contradictory;
    assert(scope_spectrum_candidate(in).diagnostic==Diagnostic::invalid_input);
    in.active_frame_mask={};
    left.resize(static_cast<std::size_t>(g->window-1));right.resize(left.size());
    in.left=left;in.right=right;
    const auto short_scope=scope_spectrum_candidate(in);
    assert(short_scope.diagnostic==Diagnostic::insufficient_content&&short_scope.complete_frames==0);
  }
  std::cout<<"M20_REAL_SCOPE_SPECTRUM_AND_FEEDER_PASS_NOT_GATE_G\n";
}
