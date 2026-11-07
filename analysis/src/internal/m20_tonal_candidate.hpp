#ifndef RGSML_M20_PRIVATE_TONAL_CANDIDATE_HPP
#define RGSML_M20_PRIVATE_TONAL_CANDIDATE_HPP
// M20 private preproduction: reuse existing canonical PSD/FFT; no new public registry.
#include "m20_stereo_candidate.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <span>
#include <vector>
namespace rgsml::analysis::m20_private {
struct SpectralGeometry {std::uint32_t fs;std::uint64_t window,fft,hop;};
struct Band {int index;double center,low,high,covered_low,covered_high;bool fully_supported;};
struct Bandwidth {double low=20.0,high=0.0;};
[[nodiscard]] inline std::optional<SpectralGeometry> spectral_geometry(std::uint32_t fs){
  if(!supported_rate(fs)) return std::nullopt;
  const auto n=round_even_div(4096ULL*fs,48000);
  std::uint64_t fft=1;
  while(fft<n) fft<<=1;
  return SpectralGeometry{fs,n,fft,round_even_div(n,4)};
}
[[nodiscard]] inline Bandwidth available_bandwidth(std::uint32_t fs){
  return supported_rate(fs)?Bandwidth{20.0,std::min(20000.0,0.45*static_cast<double>(fs))}:Bandwidth{};
}
[[nodiscard]] inline std::optional<Bandwidth> common_bandwidth(
  std::uint32_t a,std::uint32_t b,std::optional<Bandwidth> restriction=std::nullopt){
  const auto A=available_bandwidth(a),B=available_bandwidth(b);
  if(A.high<=A.low||B.high<=B.low) return std::nullopt;
  Bandwidth out{std::max(A.low,B.low),std::min(A.high,B.high)};
  if(restriction){out.low=std::max(out.low,restriction->low);out.high=std::min(out.high,restriction->high);}
  return out.high>out.low?std::optional<Bandwidth>{out}:std::nullopt;
}
[[nodiscard]] inline std::vector<Band> fractional_band_geometry(std::uint32_t fs,int b){
  if(!supported_rate(fs)||(b!=3&&b!=12)) return {};
  const auto bw=available_bandwidth(fs);
  std::vector<Band> bands;
  for(int k=-128;k<=128;++k){
    const double center=1000*std::exp2(static_cast<double>(k)/b);
    const double lo=1000*std::exp2((static_cast<double>(k)-0.5)/b);
    const double hi=1000*std::exp2((static_cast<double>(k)+0.5)/b);
    if(hi<=bw.low||lo>=bw.high) continue;
    bands.push_back({k,center,lo,hi,std::max(lo,bw.low),std::min(hi,bw.high),
      lo>=bw.low&&hi<=bw.high});
  }
  return bands;
}
// Phase-6 P02 mean-channel-power; never PCM channel downmix nor BS.1770 weight.
[[nodiscard]] inline std::optional<std::vector<double>> mean_channel_psd(
  std::span<const double> a,std::span<const double> b){
  if(a.empty()||(!b.empty()&&a.size()!=b.size())) return std::nullopt;
  std::vector<double> out(a.size());
  for(std::size_t i=0;i<a.size();++i){
    if(!std::isfinite(a[i])||a[i]<0||(!b.empty()&&(!std::isfinite(b[i])||b[i]<0)))
      return std::nullopt;
    out[i]=b.empty()?a[i]:std::midpoint(a[i],b[i]);
    if(!std::isfinite(out[i])) return std::nullopt;
  }
  return out;
}
// Frozen Phase-6 PSD cell integration; do not force DC/Nyquist half cells to Parseval.
[[nodiscard]] inline std::optional<double> band_energy(
  std::span<const double> psd,std::uint32_t fs,std::uint64_t fft,double lo,double hi){
  if(!supported_rate(fs)||fft<2||(fft&(fft-1))!=0||psd.size()!=fft/2+1||
     !std::isfinite(lo)||!std::isfinite(hi)||!(hi>lo)||lo<0||hi>fs/2.0) return std::nullopt;
  const double df=static_cast<double>(fs)/static_cast<double>(fft);
  double energy=0;
  for(std::size_t k=0;k<psd.size();++k){
    const double p=psd[k];
    if(!std::isfinite(p)||p<0) return std::nullopt;
    const double cell_lo=std::max(0.0,(static_cast<double>(k)-0.5)*df);
    const double cell_hi=std::min(fs/2.0,(static_cast<double>(k)+0.5)*df);
    const double overlap=std::max(0.0,std::min(cell_hi,hi)-std::max(cell_lo,lo));
    energy+=p*overlap;
  }
  return std::isfinite(energy)?std::optional<double>{energy}:std::nullopt;
}
// Valid-band support threshold not available: caller MUST supply authoritative eligibility.
[[nodiscard]] inline std::optional<std::vector<std::optional<double>>> normalize_eligible(
  std::span<const double> energy,std::span<const bool> eligible){
  if(energy.empty()||energy.size()!=eligible.size()) return std::nullopt;
  double total=0;
  for(std::size_t i=0;i<energy.size();++i){
    if(!std::isfinite(energy[i])||energy[i]<0) return std::nullopt;
    if(eligible[i]) total+=energy[i];
  }
  if(!std::isfinite(total)||total<=0) return std::nullopt;
  std::vector<std::optional<double>> result(energy.size());
  for(std::size_t i=0;i<energy.size();++i) if(eligible[i]) result[i]=energy[i]/total;
  return result;
}
// Input frames must have been selected by authoritative gating; no invented mask.
[[nodiscard]] inline std::optional<std::vector<double>> average_linear_psd(
  std::span<const std::vector<double>> frames){
  if(frames.empty()||frames[0].empty()) return std::nullopt;
  const auto bins=frames[0].size();
  std::vector<double> sum(bins,0);
  for(const auto& frame:frames){
    if(frame.size()!=bins) return std::nullopt;
    for(std::size_t i=0;i<bins;++i){
      if(!std::isfinite(frame[i])||frame[i]<0) return std::nullopt;
      sum[i]+=frame[i];
      if(!std::isfinite(sum[i])) return std::nullopt;
    }
  }
  for(double& x:sum) x/=static_cast<double>(frames.size());
  return sum;
}
} // namespace rgsml::analysis::m20_private
#endif
