// Adversarial stereo M20 math and temporal support: private preproduction tests.
#include "internal/m20_stereo_candidate.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>
using namespace rgsml::analysis::m20_private;
static bool near(double a,double b,double t=1e-8){return std::abs(a-b)<=t;}
static void test_all_rates(){
  for(std::uint32_t rate:{44100U,48000U,88200U,96000U,176400U,192000U}){
    const std::uint64_t nc=round_even_div(2ULL*rate,5);
    const auto h=grid100_offset(1,rate);assert(h);
    const std::uint64_t n=nc+2*(*h);
    const std::uint64_t absolute_origin=1234567;
    std::vector<double> a(static_cast<std::size_t>(n)),b(static_cast<std::size_t>(n));
    for(std::size_t i=0;i<a.size();++i){a[i]=(i&1)?0.125:-0.125;b[i]=a[i];}
    StereoScope s{rate,absolute_origin,a,b,false,{}};
    const auto c=correlation_summary(s);
    assert(c.diagnostic==Diagnostic::valid);
    assert(c.window_frames==nc);
    assert(c.series.size()==3);
    for(std::size_t j=0;j<c.series.size();++j){
      const auto expected=grid100_offset(j,rate);
      assert(expected&&c.series[j].absolute_start==absolute_origin+*expected);
      assert(c.series[j].diagnostic==Diagnostic::valid);
      assert(c.series[j].rho&&near(*c.series[j].rho,1.0));
    }
    assert(c.p10&&c.p50&&c.p90&&near(*c.p10,1)&&near(*c.p50,1)&&near(*c.p90,1));
    auto w=width_ratio(s);
    assert(w.diagnostic==Diagnostic::valid&&w.ratio&&near(*w.ratio,0));
    std::swap(s.left,s.right);
    const auto exchanged=correlation_summary(s);
    assert(exchanged.p10==c.p10&&exchanged.p50==c.p50&&exchanged.p90==c.p90);
    std::swap(s.left,s.right);
    for(std::size_t i=0;i<b.size();++i)b[i]=-a[i];
    const auto inverse=correlation_summary(s);
    assert(inverse.p10&&inverse.p50&&inverse.p90&&near(*inverse.p10,-1));
    assert(near(*inverse.p50,-1)&&near(*inverse.p90,-1));
    w=width_ratio(s);
    assert(w.diagnostic==Diagnostic::undefined_denominator&&!w.ratio);
    for(std::size_t i=0;i<a.size();++i){a[i]=3.0*((i&1)?1:-1);b[i]=-1.0*((i&1)?1:-1);}
    w=width_ratio(s);assert(w.ratio&&near(*w.ratio,2.0));
    for(double& v:a)v*=0.5;
    for(double& v:b)v*=0.5;
    const auto w2=width_ratio(s);assert(w2.ratio&&near(*w2.ratio,2.0));
    // When the authority supplies overlapping blocks, the union counts frames once.
    const std::array<Interval,4> overlap{{{absolute_origin,absolute_origin+100},
      {absolute_origin+40,absolute_origin+200},{0,absolute_origin},
      {absolute_origin+180,absolute_origin+300}}};
    s.gate_available=true;s.retained_loudness_blocks=overlap;
    w=width_ratio(s);
    assert(w.diagnostic==Diagnostic::valid&&w.selected_frames==300&&w.ratio&&near(*w.ratio,2.0));
    s.gate_available=false;
    s.retained_loudness_blocks={};
    // Insufficient content if the entire window is below the AC floor.
    for(std::size_t i=0;i<a.size();++i){a[i]=(i&1)?1e-6:-1e-6;b[i]=a[i];}
    const auto tooQuiet=correlation_summary(s);
    assert(tooQuiet.diagnostic==Diagnostic::insufficient_content);
    assert(!tooQuiet.p10&&!tooQuiet.p50&&!tooQuiet.p90);
    for(std::size_t i=0;i<a.size();++i){a[i]=(i&1)?2e-5:-2e-5;b[i]=a[i];}
    const auto supported=correlation_summary(s);
    assert(supported.diagnostic==Diagnostic::valid&&supported.p50&&near(*supported.p50,1));
    const auto before=std::span<const double>(a.data(),static_cast<std::size_t>(nc-1));
    const auto shortR=std::span<const double>(b.data(),static_cast<std::size_t>(nc-1));
    const auto shortResult=correlation_summary(StereoScope{rate,absolute_origin,before,shortR,false,{}});
    assert(shortResult.diagnostic==Diagnostic::insufficient_content&&shortResult.series.empty());
    const auto atBoundary=correlation_summary(StereoScope{rate,absolute_origin,
      std::span<const double>(a.data(),static_cast<std::size_t>(nc)),
      std::span<const double>(b.data(),static_cast<std::size_t>(nc)),false,{}});
    assert(atBoundary.series.size()==1);
    if(*h>0){
      const auto before2=correlation_summary(StereoScope{rate,absolute_origin,
        std::span<const double>(a.data(),static_cast<std::size_t>(nc+*h-1)),
        std::span<const double>(b.data(),static_cast<std::size_t>(nc+*h-1)),false,{}});
      assert(before2.series.size()==1);
      const auto at2=correlation_summary(StereoScope{rate,absolute_origin,
        std::span<const double>(a.data(),static_cast<std::size_t>(nc+*h)),
        std::span<const double>(b.data(),static_cast<std::size_t>(nc+*h)),false,{}});
      assert(at2.series.size()==2);
    }
  }
}
static void test_edge_cases(){
  const std::array<double,4> x{1,1,1,1},minus{-1,-1,-1,-1},zero{0,0,0,0};
  const StereoScope anti{48000,0,x,minus,false,{}};
  assert(width_ratio(anti).diagnostic==Diagnostic::undefined_denominator);
  assert(width_ratio(StereoScope{48000,0,x,zero,false,{}}).ratio==1.0);
  assert(width_ratio(StereoScope{48000,0,x,{},false,{}}).diagnostic==Diagnostic::not_applicable);
  assert(correlation_summary(StereoScope{48000,0,x,{},false,{}}).diagnostic==Diagnostic::not_applicable);
  const std::array<Interval,1> inverted{{{3,2}}};
  assert(width_ratio(StereoScope{48000,0,x,minus,true,inverted}).diagnostic==Diagnostic::invalid_input);
  const std::array<Interval,1> none{{{1234,2345}}};
  assert(width_ratio(StereoScope{48000,0,x,minus,true,none}).diagnostic==Diagnostic::insufficient_content);
  const std::array<double,2> huge{1e308,1e308},small{0,0};
  auto large=width_ratio(StereoScope{48000,0,huge,small,false,{}});
  assert(large.diagnostic==Diagnostic::valid&&large.ratio&&near(*large.ratio,1));
  const std::array<double,2> nonfinite{0,std::numeric_limits<double>::infinity()};
  assert(width_ratio(StereoScope{48000,0,nonfinite,small,false,{}}).
    diagnostic==Diagnostic::invalid_input);
  assert(!grid100_offset(std::numeric_limits<std::uint64_t>::max(),192000));
  for(std::uint32_t fs:{44100U,48000U,88200U,96000U,176400U,192000U}){
    for(std::uint64_t j:{0ULL,1ULL,3ULL,7ULL,31ULL,101ULL,100001ULL}){
      const auto start=grid100_offset(j,fs);
      assert(start&&*start>=grid100_offset(0,fs).value());
    }
  }
}
int main(){test_all_rates();test_edge_cases();std::cout<<"M20_STEREO_ADVERSARIAL_SIX_RATE_PASS_NOT_GATE_G\n";}
