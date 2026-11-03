#ifndef RGSML_M20_PRIVATE_STEREO_CANDIDATE_HPP
#define RGSML_M20_PRIVATE_STEREO_CANDIDATE_HPP
// M20 PREPRODUCTION ONLY; NOT a public feature, UI, QTP or Gate G.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>
namespace rgsml::analysis::m20_private {
enum class Diagnostic {valid,not_applicable,invalid_input,insufficient_content,undefined_denominator};
struct Interval {std::uint64_t begin=0,end=0;}; // absolute half-open caller-authorized PCM interval
struct StereoScope {
  std::uint32_t sample_rate=0;
  std::uint64_t origin=0;
  std::span<const double> left,right; // Empty right means real mono; no artificial duplication
  bool gate_available=false;
  std::span<const Interval> retained_loudness_blocks; // AUTHORITATIVE retained blocks supplied by caller
};
struct WidthResult {
  Diagnostic diagnostic=Diagnostic::invalid_input;
  bool used_all_scope_fallback=false;
  std::uint64_t selected_frames=0;
  std::optional<double> ratio;
};
struct CorrelationWindow {
  std::uint64_t absolute_start=0;
  Diagnostic diagnostic=Diagnostic::insufficient_content;
  std::optional<double> rho;
};
struct CorrelationResult {
  Diagnostic diagnostic=Diagnostic::invalid_input;
  std::uint64_t window_frames=0;
  std::vector<CorrelationWindow> series;
  std::optional<double> p10,p50,p90;
};
[[nodiscard]] inline bool supported_rate(std::uint32_t fs) noexcept {
  return fs==44100 || fs==48000 || fs==88200 || fs==96000 || fs==176400 || fs==192000;
}
[[nodiscard]] inline std::uint64_t round_even_div(std::uint64_t a,std::uint64_t b) noexcept {
  const auto q=a/b,r=a%b;
  return q+(r>b/2 || (b%2==0 && r==b/2 && (q&1U)));
}
[[nodiscard]] inline std::optional<std::uint64_t> grid100_offset(std::uint64_t j,std::uint32_t fs) noexcept {
  if(fs==0 || (j/10)>std::numeric_limits<std::uint64_t>::max()/fs) return std::nullopt;
  const auto whole=(j/10)*static_cast<std::uint64_t>(fs);
  const auto fraction=round_even_div((j%10)*static_cast<std::uint64_t>(fs),10);
  if(whole>std::numeric_limits<std::uint64_t>::max()-fraction) return std::nullopt;
  return whole+fraction;
}
[[nodiscard]] inline bool valid_scope(const StereoScope& s) noexcept {
  if(!supported_rate(s.sample_rate)||s.left.empty()||
     (!s.right.empty()&&s.left.size()!=s.right.size())||
     s.left.size()>std::numeric_limits<std::uint64_t>::max()-s.origin) return false;
  for(double x:s.left) if(!std::isfinite(x)) return false;
  for(double x:s.right) if(!std::isfinite(x)) return false;
  return true;
}
[[nodiscard]] inline std::optional<std::vector<Interval>> selected_union(const StereoScope& s) {
  const auto last=s.origin+static_cast<std::uint64_t>(s.left.size());
  if(!s.gate_available) return std::vector<Interval>{{s.origin,last}};
  std::vector<Interval> v;
  for(const auto& block:s.retained_loudness_blocks) {
    if(block.end<block.begin) return std::nullopt;
    const auto a=std::max(block.begin,s.origin),b=std::min(block.end,last);
    if(a<b) v.push_back({a,b});
  }
  std::sort(v.begin(),v.end(),[](const Interval& a,const Interval& b){
    return a.begin<b.begin||(a.begin==b.begin&&a.end<b.end);
  });
  std::vector<Interval> merged;
  for(const auto& b:v) {
    if(merged.empty()||b.begin>merged.back().end) merged.push_back(b);
    else merged.back().end=std::max(merged.back().end,b.end);
  }
  return merged;
}
[[nodiscard]] inline WidthResult width_ratio(const StereoScope& s) {
  WidthResult out;
  if(!valid_scope(s)) return out;
  if(s.right.empty()){out.diagnostic=Diagnostic::not_applicable;return out;}
  out.used_all_scope_fallback=!s.gate_available;
  const auto intervals=selected_union(s);
  if(!intervals) return out;
  double scale=0;
  for(const auto& part:*intervals) {
    out.selected_frames+=part.end-part.begin;
    for(auto absolute=part.begin;absolute<part.end;++absolute){
      const auto i=static_cast<std::size_t>(absolute-s.origin);
      scale=std::max({scale,std::abs(s.left[i]),std::abs(s.right[i])});
    }
  }
  if(out.selected_frames==0){out.diagnostic=Diagnostic::insufficient_content;return out;}
  if(scale==0){out.diagnostic=Diagnostic::undefined_denominator;return out;}
  double mid=0,side=0;
  for(const auto& part:*intervals) for(auto absolute=part.begin;absolute<part.end;++absolute){
    const auto i=static_cast<std::size_t>(absolute-s.origin);
    const double l=s.left[i]/scale,r=s.right[i]/scale;
    const double m=l+r,d=l-r; // common 1/sqrt(2) cancels in ratio
    mid+=m*m;side+=d*d;
  }
  if(!std::isfinite(mid)||!std::isfinite(side)) return out;
  if(mid==0){out.diagnostic=Diagnostic::undefined_denominator;return out;}
  const double ratio=std::sqrt(side/mid);
  if(!std::isfinite(ratio)) return out;
  out.diagnostic=Diagnostic::valid;out.ratio=ratio;return out;
}
[[nodiscard]] inline double type7(std::vector<double> v,double p) {
  std::sort(v.begin(),v.end());
  const auto h=static_cast<double>(v.size()-1)*p;
  const auto k=static_cast<std::size_t>(h);
  const auto t=h-static_cast<double>(k);
  return k+1>=v.size()?v.back():v[k]+t*(v[k+1]-v[k]);
}
[[nodiscard]] inline CorrelationResult correlation_summary(const StereoScope& s){
  CorrelationResult out;
  if(!valid_scope(s)) return out;
  if(s.right.empty()){out.diagnostic=Diagnostic::not_applicable;return out;}
  out.window_frames=round_even_div(2ULL*s.sample_rate,5);
  if(s.left.size()<out.window_frames){out.diagnostic=Diagnostic::insufficient_content;return out;}
  std::vector<double> valid;
  for(std::uint64_t j=0;;++j){
    const auto off=grid100_offset(j,s.sample_rate);
    if(!off||*off>s.left.size()||out.window_frames>s.left.size()-*off) break;
    const auto first=static_cast<std::size_t>(*off);
    const double ol=s.left[first],orr=s.right[first];
    double sl=0,sr=0;
    for(std::uint64_t i=0;i<out.window_frames;++i){
      const auto idx=static_cast<std::size_t>(*off+i);
      sl+=s.left[idx]-ol;sr+=s.right[idx]-orr;
    }
    const double n=static_cast<double>(out.window_frames),ml=sl/n,mr=sr/n;
    double el=0,er=0,cross=0;
    for(std::uint64_t i=0;i<out.window_frames;++i){
      const auto idx=static_cast<std::size_t>(*off+i);
      const double l=(s.left[idx]-ol)-ml,r=(s.right[idx]-orr)-mr;
      el+=l*l;er+=r*r;cross+=l*r;
    }
    CorrelationWindow w{s.origin+*off,Diagnostic::insufficient_content,std::nullopt};
    if(std::isfinite(el)&&std::isfinite(er)&&std::isfinite(cross)&&el/n>=1e-10&&er/n>=1e-10){
      const double den=std::sqrt(el/n)*std::sqrt(er/n);
      if(std::isfinite(den)&&den>=1e-20&&den>0){
        const double rho=(cross/n)/den;
        if(std::isfinite(rho)&&rho>=-1&&rho<=1){
          w.diagnostic=Diagnostic::valid;w.rho=rho;valid.push_back(rho);
        }
      }
    }
    out.series.push_back(w);
    if(j==std::numeric_limits<std::uint64_t>::max()) break;
  }
  if(valid.empty()){out.diagnostic=Diagnostic::insufficient_content;return out;}
  out.p10=type7(valid,0.1);out.p50=type7(valid,0.5);out.p90=type7(valid,0.9);
  out.diagnostic=Diagnostic::valid;return out;
}
} // namespace rgsml::analysis::m20_private
#endif
