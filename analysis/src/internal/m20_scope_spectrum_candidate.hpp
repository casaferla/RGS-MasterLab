#ifndef RGSML_M20_PRIVATE_SCOPE_SPECTRUM_CANDIDATE_HPP
#define RGSML_M20_PRIVATE_SCOPE_SPECTRUM_CANDIDATE_HPP
// PREPRODUCTION ONLY: REAL canonical FFT/Hann/one-sided PSD; no new FFT.
// The authoritative retained-loudness frame mask MUST be supplied externally.
#include "m20_tonal_candidate.hpp"
#include <rgsml/analysis/fft.hpp>
#include <rgsml/analysis/hann.hpp>
#include <rgsml/analysis/psd.hpp>
#include <complex>
#include <cstdint>
#include <span>
#include <vector>
namespace rgsml::analysis::m20_private {
struct ScopeSpectrumInput {
  std::uint32_t fs=0;
  std::uint64_t absolute_scope_origin=0;
  std::span<const double> left,right;
  bool authoritative_active_gate_available=false;
  // EXACT one zero/one per complete STFT frame, from canonical M19 retained blocks.
  // No gate creation or unqualified frame center/endpoint convention in M20.
  std::span<const std::uint8_t> active_frame_mask;
};
struct ScopeSpectrumResult {
  Diagnostic diagnostic=Diagnostic::invalid_input;
  bool all_frames_fallback=false;
  std::uint64_t complete_frames=0,selected_frames=0;
  std::vector<std::uint64_t> selected_absolute_starts;
  SpectralGeometry geometry{};
  std::vector<double> average_psd; // power / Hz, valid iff diagnostic=valid
  struct BandPower {Band band;double measured_energy=0;};
  std::vector<BandPower> bands_1_12; // numerical energy/geometry, NOT accepted support or feature
};
[[nodiscard]] inline ScopeSpectrumResult scope_spectrum_candidate(const ScopeSpectrumInput& in){
  ScopeSpectrumResult out;
  const StereoScope scope{in.fs,in.absolute_scope_origin,in.left,in.right,false,{}};
  if(!valid_scope(scope)) return out;
  const auto g=spectral_geometry(in.fs);
  if(!g) return out;
  out.geometry=*g;
  const auto count=in.left.size()<g->window?0ULL:
    1ULL+static_cast<std::uint64_t>((in.left.size()-g->window)/g->hop);
  out.complete_frames=count;
  if(in.authoritative_active_gate_available){
    if(in.active_frame_mask.size()!=count) return out;
    for(const auto x:in.active_frame_mask) if(x!=0&&x!=1) return out;
  }else if(!in.active_frame_mask.empty()) return out;
  out.all_frames_fallback=!in.authoritative_active_gate_available;
  if(count==0){out.diagnostic=Diagnostic::insufficient_content;return out;}
  const auto hann=HannWindow::create(static_cast<std::size_t>(g->window));
  const auto fft_size=static_cast<std::size_t>(g->fft);
  Radix2Fft fft(fft_size);
  std::vector<double> weighted(static_cast<std::size_t>(g->window));
  std::vector<std::complex<double>> transformed(fft_size);
  std::vector<double> psd_left,psd_right;
  std::vector<std::vector<double>> frames;
  for(std::uint64_t j=0;j<count;++j){
    if(in.authoritative_active_gate_available&&in.active_frame_mask[j]==0) continue;
    const auto offset=j*g->hop; // complete-frame count bounds multiplication and slice
    const auto process_channel=[&](std::span<const double> pcm,std::vector<double>& psd){
      for(std::size_t i=0;i<weighted.size();++i)
        weighted[i]=pcm[static_cast<std::size_t>(offset)+i]*hann.window[i];
      fft.forward(weighted.data(),weighted.size(),transformed.data());
      compute_onesided_psd(transformed,static_cast<double>(in.fs),hann.sum_w2,psd);
    };
    process_channel(in.left,psd_left);
    if(!in.right.empty()) process_channel(in.right,psd_right);
    else psd_right.clear();
    auto combined=mean_channel_psd(psd_left,psd_right);
    if(!combined) return out;
    frames.push_back(*combined);
    out.selected_absolute_starts.push_back(in.absolute_scope_origin+offset);
  }
  out.selected_frames=static_cast<std::uint64_t>(frames.size());
  if(frames.empty()){out.diagnostic=Diagnostic::insufficient_content;return out;}
  const auto averaged=average_linear_psd(frames);
  if(!averaged) return out;
  out.average_psd=*averaged;
  for(const auto& band:fractional_band_geometry(in.fs,12)){
    const auto energy=band_energy(out.average_psd,in.fs,g->fft,band.covered_low,band.covered_high);
    if(!energy) return out;
    out.bands_1_12.push_back({band,*energy});
  }
  out.diagnostic=Diagnostic::valid;
  return out;
}
} // namespace rgsml::analysis::m20_private
#endif
