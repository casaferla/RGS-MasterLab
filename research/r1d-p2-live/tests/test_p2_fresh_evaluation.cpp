// RGS R1d-P2 I1.8 RESEARCH ONLY: fresh evaluation per AutoMatch candidate.
// Exercises real RGS IModule and AudioBuffer. NOT canonical L3 solver, Product,
// Windows callback qualification, M17, Final SRC or runtime checkpoint.
#include "research_p2_imodule.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/dsp/module_descriptor.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace core = rgsml::core;
namespace audio = rgsml::audio;
namespace dsp = rgsml::dsp;
using rgsml::research::r1d_p2::ResearchP2IModule;
using rgsml::research::r1d_p2::Configuration;
using Clock = std::chrono::steady_clock;

static void need(bool x, const char* message){if(!x) throw std::runtime_error(message);}
static core::FrameCount count(std::int64_t n){auto r=core::FrameCount::create(n);need(bool(r),"frame count");return *r.value();}
static core::FrameRange range(std::int64_t a,std::int64_t n){auto r=core::FrameRange::create(core::FrameIndex{a},core::FrameIndex{a+n});need(bool(r),"frame range");return *r.value();}
static audio::AudioFormat format(int fs,int ch){
 auto rate=core::SampleRate::create(fs);need(bool(rate),"sample rate");
 auto fmt=audio::AudioFormat::create(*rate.value(), ch==1?audio::ChannelLayout::MONO_C:audio::ChannelLayout::STEREO_LR);
 need(bool(fmt),"audio format");return *fmt.value();
}
static dsp::ModuleDescriptor descriptor(){
 dsp::ModuleDescriptorSpec s{
  "rgsml.dsp.streaming-true-peak-limiter", "Streaming True Peak Limiter",
  {dsp::ModuleCategory::DYNAMICS}, {dsp::ProcessingStage::MASTER},
  {dsp::ChainSegment::MANUAL}, false, true, dsp::PlacementClass::INLINE_CHAIN,
  std::nullopt, {}, {}, {}, {}, true, true,
  std::string{"0.1.0-research"},
  std::string{"rgsml.dsp.streaming-true-peak-limiter.params/0.1.0"}};
 auto r=dsp::ModuleDescriptor::create(std::move(s));need(bool(r),"descriptor");return std::move(*r.value());
}
static std::uint64_t mix(std::uint64_t hash, double x){
 std::uint64_t bits{};std::memcpy(&bits,&x,sizeof(bits));
 for(int i=0;i<8;++i){hash^=(bits>>(8*i))&255U;hash*=1099511628211ULL;}
 return hash;
}
struct Receipt {
 std::vector<std::array<double,2>> pcm;
 std::int64_t delay{};
 std::int64_t prep_us{}, process_us{}, tail_us{};
 std::uint64_t fingerprint{};
};
static Receipt evaluate(int fs, int ch, double drive, std::size_t n, std::size_t maxBlock){
 constexpr std::int64_t origin=937;
 const auto fmt=format(fs,ch);
 const dsp::DspProcessSpec spec{fmt,audio::FrameDomainId::SOURCE_PROCESSING_RATE,count(static_cast<std::int64_t>(maxBlock))};
 Configuration cfg{fs,ch,2,0.8912509381337456,drive};
 ResearchP2IModule p2(descriptor(),cfg,false);
 dsp::IModule& module=p2;
 const auto t0=Clock::now();
 auto req=module.runtime_requirements(spec);need(bool(req),"requirements");
 const auto d=req.value()->algorithmic_latency_frames.value();
 need(d>0 && req.value()->effective_tail_frames.value()==d,"delay and EOS must agree");
 need(bool(module.prepare(spec)),"fresh prepare");
 const auto t1=Clock::now();
 auto in=audio::AudioBuffer::create(fmt,audio::FrameDomainId::SOURCE_PROCESSING_RATE,core::FrameIndex{origin},count(n));
 auto out=audio::AudioBuffer::create(fmt,audio::FrameDomainId::SOURCE_PROCESSING_RATE,core::FrameIndex{origin},count(n+d));
 need(bool(in)&&bool(out),"canonical buffers");
 const auto pi=3.14159265358979323846;
 for(std::size_t i=0;i<n;++i){
   const double t=static_cast<double>(i)/static_cast<double>(fs);
   const double v=1.42*(0.54*std::sin(2*pi*73.0*t)+0.38*std::sin(2*pi*9701.0*t));
   (*in.value()->mutable_view().channel(0).value())[i]=v;
   if(ch==2)(*in.value()->mutable_view().channel(1).value())[i]=-.77*v;
 }
 const auto t2=Clock::now();
 for(std::size_t offset=0;offset<n;){
   const auto len=std::min(maxBlock,n-offset);
   const auto pos=origin+static_cast<std::int64_t>(offset);
   auto iv=in.value()->view().subview(core::FrameIndex{pos},count(len));
   auto ov=out.value()->mutable_view().subview(core::FrameIndex{pos},count(len));
   need(bool(iv)&&bool(ov),"process subviews");
   need(bool(module.process(*iv.value(),*ov.value(),{range(pos,len),offset==0,false})),"process failed");
   offset+=len;
 }
 const auto t3=Clock::now();
 for(std::int64_t offset=0;offset<d;){
   const auto len=std::min<std::int64_t>(static_cast<std::int64_t>(maxBlock),d-offset);
   const auto pos=origin+static_cast<std::int64_t>(n)+offset;
   auto ov=out.value()->mutable_view().subview(core::FrameIndex{pos},count(len));
   need(bool(ov),"tail subview");
   need(bool(module.finalize(*ov.value(),{range(pos,len),false,offset+len==d})),"finalize failed");
   offset+=len;
 }
 const auto t4=Clock::now();
 Receipt r;
 r.delay=d;r.pcm.resize(n+static_cast<std::size_t>(d));
 r.fingerprint=14695981039346656037ULL;
 for(int c=0;c<ch;++c){
   auto lane=out.value()->view().channel(c);need(bool(lane),"channel");
   for(std::size_t i=0;i<r.pcm.size();++i){
      const double value=(*lane.value())[i];
      need(std::isfinite(value),"non-finite PCM");
      r.pcm[i][static_cast<std::size_t>(c)]=value;
      r.fingerprint=mix(r.fingerprint,value);
   }
 }
 r.prep_us=std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
 r.process_us=std::chrono::duration_cast<std::chrono::microseconds>(t3-t2).count();
 r.tail_us=std::chrono::duration_cast<std::chrono::microseconds>(t4-t3).count();
 return r;
}
static void identical(const Receipt& a,const Receipt& b){
 need(a.delay==b.delay && a.pcm.size()==b.pcm.size(),"evaluation latency/size mismatch");
 for(std::size_t i=0;i<a.pcm.size();++i)
  for(std::size_t c=0;c<2;++c)
    need(std::memcmp(&a.pcm[i][c],&b.pcm[i][c],sizeof(double))==0,"fresh evaluation drift");
 need(a.fingerprint==b.fingerprint,"hash mismatch");
}
static void scenario(int fs,int ch){
 constexpr std::size_t n=8192,block=512;
 // A, B, C, D, A: return to previous setting after intervening candidates.
 const std::array<double,5> drives{0.0,1.5,3.0,5.0,0.0};
 auto baseline=evaluate(fs,ch,drives[0],n,block);
 auto maximum=baseline.process_us;
 std::int64_t maxPrep=baseline.prep_us, maxTail=baseline.tail_us;
 std::array<std::uint64_t,5> hashes{};hashes[0]=baseline.fingerprint;
 for(std::size_t i=1;i<drives.size();++i){
  auto r=evaluate(fs,ch,drives[i],n,block);
  hashes[i]=r.fingerprint;
  if(i==4)identical(baseline,r);
  maximum=std::max(maximum,r.process_us);
  maxPrep=std::max(maxPrep,r.prep_us);
  maxTail=std::max(maxTail,r.tail_us);
 }
 // On a fixed input, differing drive settings must be observable in PCM.
 need(hashes[0]!=hashes[3],"drive parameter inactive in stress fixture");
 std::cout << "fresh epoch fs=" << fs << " channels=" << ch
   << " candidates=" << drives.size() << " frames=" << n
   << " origin=937 block=" << block << " latency=" << baseline.delay
   << " repeat_A_bit_exact=yes" << " A_pcm_hash=" << std::hex << baseline.fingerprint << std::dec << " max_prepare_us=" << maxPrep
   << " max_process_us=" << maximum << " max_finalize_us=" << maxTail
   << " (local synthetic measurement, not WCET)\n";
}
int main() try {
 for(int fs:{48000,192000})for(int ch:{1,2})scenario(fs,ch);
 std::cout << "RGS R1d-P2 I1.8 fresh evaluation: PASS; checkpoint not required for full-epoch replay\n";
 return 0;
}catch(const std::exception& e){
 std::cerr << "RGS R1d-P2 I1.8 FAIL: " << e.what() << '\n';return 1;
}
