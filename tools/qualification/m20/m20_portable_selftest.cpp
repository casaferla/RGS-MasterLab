// M20 independent native candidate tests: no Qt, no M17/M18/M19 mutation.
#include "internal/m20_stereo_candidate.hpp"
#include "internal/m20_tonal_candidate.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>
using namespace rgsml::analysis::m20_private;
static bool near(double a,double b,double e=1e-9){return std::abs(a-b)<=e;}
int main(){
  constexpr std::array<std::uint32_t,6> fs{44100,48000,88200,96000,176400,192000};
  constexpr std::array<std::uint64_t,6> window{3763,4096,7526,8192,15053,16384};
  constexpr std::array<std::uint64_t,6> hop{941,1024,1882,2048,3763,4096};
  constexpr std::array<std::uint64_t,6> fft{4096,4096,8192,8192,16384,16384};
  for(std::size_t i=0;i<fs.size();++i){
    const auto g=spectral_geometry(fs[i]);
    assert(g&&g->window==window[i]&&g->hop==hop[i]&&g->fft==fft[i]);
    for(int b:{3,12}){
      const auto bands=fractional_band_geometry(fs[i],b);assert(!bands.empty());
      for(const auto& x:bands)
        assert(x.covered_high>x.covered_low&&x.covered_low>=20 &&
               x.covered_high<=available_bandwidth(fs[i]).high);
    }
  }
  assert(!spectral_geometry(32000));
  auto common=common_bandwidth(44100,48000);
  assert(common&&common->high==19845.0);
  common=common_bandwidth(48000,96000,Bandwidth{100,10000});
  assert(common&&common->low==100&&common->high==10000);
  assert(!common_bandwidth(48000,48000,Bandwidth{22000,23000}));
  constexpr std::array<double,2> a{1,3},b{3,5};
  auto m=mean_channel_psd(a,b);assert(m&&(*m)[0]==2&&(*m)[1]==4);
  m=mean_channel_psd(a,{});assert(m&&(*m)[0]==1&&(*m)[1]==3);
  const std::vector<std::vector<double>> frames{{1,100},{100,1}};
  auto avg=average_linear_psd(frames);assert(avg&&(*avg)[0]==50.5&&(*avg)[1]==50.5);
  const std::array<double,5> psd{1,2,3,4,5};
  auto total=band_energy(psd,48000,8,0,24000);
  auto lo=band_energy(psd,48000,8,0,12000);
  auto hi=band_energy(psd,48000,8,12000,24000);
  assert(total&&lo&&hi&&*total==72000&&near(*lo+*hi,*total));
  constexpr std::array<double,3> energies{2,1,0};
  constexpr std::array<bool,3> eligible{true,true,false},none{false,false,false};
  auto f=normalize_eligible(energies,eligible);
  assert(f&&(*f)[0]&&(*f)[1]&&!(*f)[2]&&near(*(*f)[0],2.0/3.0));
  assert(!normalize_eligible(energies,none));
  assert(!normalize_eligible(std::array<double,3>{0,0,0},eligible));
  std::vector<double> left(48000,1.0),right(48000,1.0);
  StereoScope s{48000,12345,left,right,false,{}};
  auto w=width_ratio(s);
  assert(w.ratio&&*w.ratio==0&&w.selected_frames==48000&&w.used_all_scope_fallback);
  for(auto& x:right)x=0;
  w=width_ratio(s);assert(w.ratio&&near(*w.ratio,1));
  for(auto& x:left)x=3;
  for(auto& x:right)x=-1;
  w=width_ratio(s);assert(w.ratio&&near(*w.ratio,2));
  for(auto& x:left)x=1e308;
  for(auto& x:right)x=-1e308;
  w=width_ratio(s);assert(w.diagnostic==Diagnostic::undefined_denominator&&!w.ratio);
  for(auto& x:right)x=0;
  w=width_ratio(s);assert(w.ratio&&near(*w.ratio,1));
  for(auto& x:left)x=1;
  for(std::size_t i=0;i<right.size();++i)right[i]=i<24000?1:-1;
  const std::array<Interval,3> blocks{{{12345,20345},{18000,36345},{0,12345}}};
  s.gate_available=true;s.retained_loudness_blocks=blocks;
  w=width_ratio(s);
  assert(w.ratio&&*w.ratio==0&&w.selected_frames==24000&&!w.used_all_scope_fallback);
  s.gate_available=false;w=width_ratio(s);assert(w.ratio&&near(*w.ratio,1));
  const std::array<Interval,1> outside{{{0,1}}};
  s.gate_available=true;s.retained_loudness_blocks=outside;
  w=width_ratio(s);assert(w.diagnostic==Diagnostic::insufficient_content&&!w.ratio);
  s.gate_available=false;s.retained_loudness_blocks={};
  for(std::size_t i=0;i<left.size();++i){
    left[i]=1099511627776.0+((i&1)?1.0:-1.0);
    right[i]=100.0+((i&1)?3.0:-3.0);
  }
  auto c=correlation_summary(s);
  assert(c.diagnostic==Diagnostic::valid&&c.series.size()==7&&c.window_frames==19200);
  for(std::size_t i=0;i<c.series.size();++i)
    assert(c.series[i].absolute_start==12345+i*4800&&c.series[i].rho&&
           near(*c.series[i].rho,1,1e-6));
  assert(c.p10&&c.p50&&c.p90&&near(*c.p10,1,1e-6)&&near(*c.p90,1,1e-6));
  for(auto& x:right)x=-x;
  c=correlation_summary(s);assert(c.p10&&near(*c.p10,-1,1e-6));
  for(auto& x:right)x=42;
  c=correlation_summary(s);assert(c.series.size()==7&&!c.p50);
  left.resize(19199);right.resize(19199);s.left=left;s.right=right;
  c=correlation_summary(s);assert(c.series.empty());
  left.assign(19200,1);right.assign(19200,1);
  left[100]=std::numeric_limits<double>::quiet_NaN();
  s.left=left;s.right=right;
  assert(width_ratio(s).diagnostic==Diagnostic::invalid_input);
  assert(correlation_summary(s).diagnostic==Diagnostic::invalid_input);
  left[100]=1;s.right={};
  assert(width_ratio(s).diagnostic==Diagnostic::not_applicable);
  assert(correlation_summary(s).diagnostic==Diagnostic::not_applicable);
  assert(grid100_offset(0,44105)==0&&grid100_offset(1,44105)==4410);
  assert(grid100_offset(3,44105)==13232&&grid100_offset(7,44105)==30874);
  assert(!grid100_offset(std::numeric_limits<std::uint64_t>::max(),44100));
  assert(round_even_div(5,2)==2&&round_even_div(7,2)==4);
  assert(near(type7(std::vector<double>{-1,1},0.1),-0.8));
  assert(near(type7(std::vector<double>{-1,1},0.9),0.8));
  std::cout<<"M20_NATIVE_PREPRODUCTION_PASS_NOT_GATE_G\n";
}
