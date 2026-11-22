#ifndef RGSML_M20_PRIVATE_BOUNDED_SPECTRUM_HPP
#define RGSML_M20_PRIVATE_BOUNDED_SPECTRUM_HPP
// M20 isolated preproduction: actual shared FFT/Hann/PSD, bounded STFT ring.
// External active-gate mask is BORROWED, never inferred from M19 LUFS.
#include "m20_scope_spectrum_candidate.hpp"
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
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
      std::span<const std::uint8_t> mask={},
      TonalStereoSelectedFrameSink* selected_sink=nullptr,
      TonalStereoStereoFrameSink* stereo_sink=nullptr)
    : fs_(fs),channels_(channels),origin_(origin),frames_(frames),
      gate_available_(gate_available),mask_(mask),
      selected_sink_(selected_sink),stereo_sink_(stereo_sink)
  {
    const auto g=spectral_geometry(fs_);
    if(!g || frames_==0 || (channels_!=1&&channels_!=2) ||
       (stereo_sink_&&channels_!=2U) ||
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
    if(stereo_sink_) {
      left_fft_half_.resize(static_cast<std::size_t>(g->fft/2+1));
      cross_psd_.resize(left_fft_half_.size());
    }
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
                     std::vector<double>& psd,bool capture_left) {
      for(std::size_t k=0;k<weighted_.size();++k){
        const auto pos=static_cast<std::size_t>(
          (first+static_cast<std::uint64_t>(k))%geometry_.window);
        weighted_[k]=ring[pos]*hann_.window[k];
      }
      fft_->forward(weighted_.data(),weighted_.size(),spectrum_.data());
      if(capture_left)
        std::copy_n(spectrum_.begin(),left_fft_half_.size(),
                    left_fft_half_.begin());
      compute_onesided_psd(spectrum_,static_cast<double>(fs_),hann_.sum_w2,psd);
    };
    process(ring_l_,psd_l_,stereo_sink_!=nullptr);
    if(channels_==2)process(ring_r_,psd_r_,false);
    else psd_r_.clear();
    const auto mean=mean_channel_psd(psd_l_,psd_r_);
    if(!mean||mean->size()!=sums_.size())return false;
    if(stereo_sink_) {
      // Same canonical one-sided density as L/R auto PSD.
      const double denominator=static_cast<double>(fs_)*hann_.sum_w2;
      if(!std::isfinite(denominator)||denominator<=0.0 ||
         left_fft_half_.size()!=psd_l_.size()||
         cross_psd_.size()!=psd_r_.size())return false;
      const double inv=1.0/denominator;
      const auto nyquist=static_cast<std::size_t>(geometry_.fft/2);
      for(std::size_t k=0;k<cross_psd_.size();++k) {
        const auto cross=std::real(
            left_fft_half_[k]*std::conj(spectrum_[k]));
        cross_psd_[k]=cross*inv*(k==0U||k==nyquist?1.0:2.0);
        if(!std::isfinite(cross_psd_[k]))return false;
      }
      if(!stereo_sink_->on_selected_stereo_frame(
           frame,fs_,geometry_.fft,psd_l_,psd_r_,cross_psd_))
        return false;
    }
    // One source-native callback, BEFORE the original time-averaged
    // accumulation; callback failure aborts rather than dropping a
    // spectral frame and changing selected native support.
    if(selected_sink_&&!selected_sink_->on_selected_native_frame(
         frame,fs_,geometry_.fft,
         std::span<const double>{mean->data(),mean->size()}))
      return false;
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
  TonalStereoSelectedFrameSink* selected_sink_=nullptr;
  TonalStereoStereoFrameSink* stereo_sink_=nullptr;
  SpectralGeometry geometry_{};
  HannWindow hann_{};
  std::unique_ptr<Radix2Fft> fft_;
  std::vector<double> ring_l_,ring_r_,weighted_,sums_,psd_l_,psd_r_;
  std::vector<std::complex<double>> spectrum_,left_fft_half_;
  std::vector<double> cross_psd_;
  BoundedSpectrumCandidate out_{};
};
} // namespace rgsml::analysis::m20_private
#endif
