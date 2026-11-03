#ifndef RGSML_M20_PRIVATE_BOUNDED_SPECTRUM_HPP
#define RGSML_M20_PRIVATE_BOUNDED_SPECTRUM_HPP
// M20 isolated preproduction: actual shared FFT/Hann/PSD, bounded STFT ring.
// External active-gate mask is BORROWED, never inferred from M19 LUFS.
#include "m20_scope_spectrum_candidate.hpp"
#include <complex>
#include <memory>
#include <span>
namespace rgsml::analysis::m20_private {
struct BoundedSpectrumCandidate {
  Diagnostic diagnostic=Diagnostic::invalid_input;
  bool all_frames_fallback=false;
  SpectralGeometry geometry{};
  std::uint64_t complete_frames=0,selected_frames=0;
  std::vector<double> average_psd;
  std::vector<ScopeSpectrumResult::BandPower> bands_1_12;
  std::size_t working_double_equivalents=0;
};
class BoundedSpectrumAccumulatorCandidate final {
public:
  BoundedSpectrumAccumulatorCandidate(
      std::uint32_t fs,std::uint64_t origin,std::uint64_t frames,
      std::uint32_t channels,bool gate_available=false,
      std::span<const std::uint8_t> mask={})
    : fs_(fs),origin_(origin),frames_(frames),channels_(channels),
      gate_available_(gate_available),mask_(mask)
  {
    const auto g=spectral_geometry(fs_);
    if(!g || frames_==0 || (channels_!=1&&channels_!=2) ||
       origin_>std::numeric_limits<std::uint64_t>::max()-frames_) return;
    geometry_=*g;
    out_.geometry=*g;
    out_.complete_frames=frames_<g->window?0:
      1+(frames_-g->window)/g->hop;
    if(gate_available_){
      if(mask_.size()!=out_.complete_frames)return;
      for(auto x:mask_)if(x>1)return;
    }else if(!mask_.empty())return;
    out_.all_frames_fallback=!gate_available_;
    hann_=HannWindow::create(static_cast<std::size_t>(g->window));
    fft_=std::make_unique<Radix2Fft>(static_cast<std::size_t>(g->fft));
    ring_l_.resize(static_cast<std::size_t>(g->window));
    if(channels_==2) ring_r_.resize(ring_l_.size());
    weighted_.resize(ring_l_.size());
    spectrum_.resize(static_cast<std::size_t>(g->fft));
    sums_.resize(static_cast<std::size_t>(g->fft/2+1),0.0);
    out_.working_double_equivalents=ring_l_.size()+ring_r_.size()+
      weighted_.size()+2*spectrum_.size()+3*sums_.size();
    valid_=true;
  }
  [[nodiscard]] bool push(std::span<const double> l,
                          std::span<const double> r={})
  {
    if(!valid_||finalized_||l.empty()||
       (channels_==1&&!r.empty())||
       (channels_==2&&l.size()!=r.size())||
       l.size()>frames_-consumed_)return false;
    for(double x:l)if(!std::isfinite(x))return false;
    for(double x:r)if(!std::isfinite(x))return false;
    for(std::size_t i=0;i<l.size();++i){
      const auto slot=static_cast<std::size_t>(
          consumed_%geometry_.window);
      ring_l_[slot]=l[i];
      if(channels_==2)ring_r_[slot]=r[i];
      ++consumed_;
      if(consumed_>=geometry_.window &&
         (consumed_-geometry_.window)%geometry_.hop==0){
        const auto index=(consumed_-geometry_.window)/geometry_.hop;
        if(!gate_available_||mask_[static_cast<std::size_t>(index)]==1){
          if(!accumulate(index)){valid_=false;return false;}
        }
      }
    }
    return true;
  }
  [[nodiscard]] BoundedSpectrumCandidate finalize(){
    if(!valid_||finalized_||consumed_!=frames_)return {};
    finalized_=true;
    if(out_.selected_frames==0){
      out_.diagnostic=Diagnostic::insufficient_content;
      return out_;
    }
    out_.average_psd=sums_;
    for(double& x:out_.average_psd) {
      x/=static_cast<double>(out_.selected_frames);
      if(!std::isfinite(x)||x<0)return {};
    }
    for(const auto& b:fractional_band_geometry(fs_,12)){
      const auto e=band_energy(out_.average_psd,fs_,geometry_.fft,
                               b.covered_low,b.covered_high);
      if(!e)return {};
      out_.bands_1_12.push_back({b,*e});
    }
    out_.diagnostic=Diagnostic::valid;
    return out_;
  }
private:
  [[nodiscard]] bool accumulate(std::uint64_t frame){
    const auto first=frame*geometry_.hop;
    auto process=[&](const std::vector<double>& ring,
                     std::vector<double>& psd) {
      for(std::size_t k=0;k<weighted_.size();++k){
        const auto pos=static_cast<std::size_t>(
          (first+static_cast<std::uint64_t>(k))%geometry_.window);
        weighted_[k]=ring[pos]*hann_.window[k];
      }
      fft_->forward(weighted_.data(),weighted_.size(),spectrum_.data());
      compute_onesided_psd(spectrum_,static_cast<double>(fs_),hann_.sum_w2,psd);
    };
    process(ring_l_,psd_l_);
    if(channels_==2)process(ring_r_,psd_r_);
    else psd_r_.clear();
    const auto mean=mean_channel_psd(psd_l_,psd_r_);
    if(!mean||mean->size()!=sums_.size())return false;
    for(std::size_t k=0;k<sums_.size();++k){
      sums_[k]+=(*mean)[k];
      if(!std::isfinite(sums_[k]))return false;
    }
    ++out_.selected_frames;
    return true;
  }
  std::uint32_t fs_=0,channels_=0;
  std::uint64_t origin_=0,frames_=0,consumed_=0;
  bool gate_available_=false,valid_=false,finalized_=false;
  std::span<const std::uint8_t> mask_; // never owned or mutated
  SpectralGeometry geometry_{};
  HannWindow hann_{};
  std::unique_ptr<Radix2Fft> fft_;
  std::vector<double> ring_l_,ring_r_,weighted_,sums_,psd_l_,psd_r_;
  std::vector<std::complex<double>> spectrum_;
  BoundedSpectrumCandidate out_{};
};
} // namespace rgsml::analysis::m20_private
#endif
