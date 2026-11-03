// REAL existing shared FFT, Hann and one-sided PSD reuse, not an invented M20 FFT.
#include "internal/m20_tonal_candidate.hpp"
#include <rgsml/analysis/fft.hpp>
#include <rgsml/analysis/hann.hpp>
#include <rgsml/analysis/psd.hpp>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <vector>
using namespace rgsml::analysis;
using namespace rgsml::analysis::m20_private;
int main(){
  for(std::uint32_t fs:{44100U,48000U,88200U,96000U,176400U,192000U}){
    const auto g=spectral_geometry(fs);assert(g);
    const auto hann=HannWindow::create(static_cast<std::size_t>(g->window));
    const auto n=static_cast<std::size_t>(g->fft);
    Radix2Fft fft(n);
    std::vector<double> pcm(g->window);
    for(std::size_t i=0;i<pcm.size();++i)
      pcm[i]=0.5*std::sin(2.0*std::numbers::pi*64.0*static_cast<double>(i)/static_cast<double>(n));
    for(std::size_t i=0;i<pcm.size();++i) pcm[i]*=hann.window[i];
    std::vector<std::complex<double>> bins(n);
    fft.forward(pcm.data(),pcm.size(),bins.data());
    std::vector<double> psd;
    compute_onesided_psd(bins,static_cast<double>(fs),hann.sum_w2,psd);
    assert(psd.size()==n/2+1);
    auto mean=mean_channel_psd(psd,psd);assert(mean&&mean->size()==psd.size());
    const double bw=available_bandwidth(fs).high;
    auto all=band_energy(*mean,fs,g->fft,20,bw);
    auto lo=band_energy(*mean,fs,g->fft,20,1000);
    auto hi=band_energy(*mean,fs,g->fft,1000,bw);
    assert(all&&lo&&hi&&*all>0&&std::isfinite(*all)&&std::abs(*lo+*hi-*all)<1e-10);
  }
  std::cout<<"M20_REAL_EXISTING_FFT_HANN_PSD_REUSE_PASS_NOT_GATE_G\n";
}
