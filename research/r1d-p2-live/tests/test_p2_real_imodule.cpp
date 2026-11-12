#include "research_p2_imodule.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/dsp/module_descriptor.hpp>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

static std::atomic<std::size_t> g_callback_allocs{0};
static std::atomic<bool> g_tracking{false};
static std::uint64_t g_hash{14695981039346656037ULL};
#if defined(_MSC_VER)
#define R1D_NOINLINE __declspec(noinline)
#else
#define R1D_NOINLINE __attribute__((noinline))
#endif
static R1D_NOINLINE void* alloc_witness(std::size_t n){
    if(g_tracking.load(std::memory_order_relaxed))g_callback_allocs.fetch_add(1,std::memory_order_relaxed);
    return std::malloc(n?n:1);
}
void* operator new(std::size_t n){if(auto* p=alloc_witness(n))return p;throw std::bad_alloc();}
void* operator new(std::size_t n,const std::nothrow_t&) noexcept {return alloc_witness(n);}
static R1D_NOINLINE void free_witness(void* p) noexcept {std::free(p);}
void operator delete(void* p) noexcept {free_witness(p);}
void operator delete(void* p,std::size_t) noexcept {free_witness(p);}
void operator delete(void* p,const std::nothrow_t&) noexcept {free_witness(p);}

using namespace rgsml;
using namespace rgsml::research::r1d_p2;
static void require(bool v,const char* s){if(!v)throw std::runtime_error(s);}
static core::FrameCount frames(std::int64_t n){auto r=core::FrameCount::create(n);require(bool(r),"frame count");return *r.value();}
static core::FrameRange range(std::int64_t at,std::int64_t n){auto r=core::FrameRange::create(core::FrameIndex{at},core::FrameIndex{at+n});require(bool(r),"range");return *r.value();}
static audio::AudioFormat format(int fs,int ch){
 auto rate=core::SampleRate::create(fs);require(bool(rate),"sample rate");
 auto r=audio::AudioFormat::create(*rate.value(),ch==1?audio::ChannelLayout::MONO_C:audio::ChannelLayout::STEREO_LR);
 require(bool(r),"format");return *r.value();
}
static dsp::ModuleDescriptor descriptor(){
 dsp::ModuleDescriptorSpec s{
   "rgsml.dsp.streaming-true-peak-limiter", "Streaming True Peak Limiter",
   {dsp::ModuleCategory::DYNAMICS},{dsp::ProcessingStage::MASTER},
   {dsp::ChainSegment::MANUAL},false,true,dsp::PlacementClass::INLINE_CHAIN,
   std::nullopt,{},{},{},{},true,true,
   std::string{"0.1.0-research"},
   std::string{"rgsml.dsp.streaming-true-peak-limiter.params/0.1.0"}
 };
 auto r=dsp::ModuleDescriptor::create(std::move(s));require(bool(r),"descriptor");return *std::move(r.value());
}
static std::vector<Frame> input_pcm(std::size_t n,int fs,int ch,bool hot){
 std::vector<Frame> a(n);const double gain=hot?1.5:0.12;
 for(std::size_t i=0;i<n;i++){
  const auto t=static_cast<double>(i)/fs;
  a[i][0]=gain*(.61*std::sin(2*3.141592653589793*71*t)+.44*std::sin(2*3.141592653589793*7587*t));
  a[i][1]=ch==2?-.72*a[i][0]:0.;
 }return a;
}
static void test_rgs_imodule(int fs,int ch,std::size_t n,std::size_t block,bool bypass,bool hot,bool processEos){
 const auto fmt=format(fs,ch);
 Configuration cfg{fs,ch,2,.8912509381337456,0.};
 ResearchP2IModule module(descriptor(),cfg,bypass);
 dsp::IModule& base=module;
 const dsp::DspProcessSpec spec{fmt,audio::FrameDomainId::SOURCE_PROCESSING_RATE,frames(block)};
 auto requirements=base.runtime_requirements(spec);require(bool(requirements),"req");
 const auto d=requirements.value()->algorithmic_latency_frames.value();
 require(d>0 && requirements.value()->effective_tail_frames.value()==d,"latency/eos requirements");
 require(requirements.value()->execution_model==dsp::DspExecutionModel::STREAMING_CAUSAL,"execution model");
 require(base.prepare(spec).has_value(),"prepare real ABI");
 constexpr std::int64_t origin=97; // Nonzero, must remain sample-accurate
 auto ib=audio::AudioBuffer::create(fmt,audio::FrameDomainId::SOURCE_PROCESSING_RATE,core::FrameIndex{origin},frames(n));
 auto ob=audio::AudioBuffer::create(fmt,audio::FrameDomainId::SOURCE_PROCESSING_RATE,core::FrameIndex{origin},frames(n+static_cast<std::size_t>(d)));
 require(bool(ib)&&bool(ob),"create actual RGS AudioBuffer");
 const auto pcm=input_pcm(n,fs,ch,hot);
 for(std::size_t c=0;c<static_cast<std::size_t>(ch);++c){
  auto p=ib.value()->mutable_view().channel(c);require(bool(p),"in plane");
  for(std::size_t i=0;i<n;++i)(*p.value())[i]=pcm[i][c];
 }
 for(std::int64_t off=0;off<static_cast<std::int64_t>(n);){
  auto part=std::min<std::int64_t>(static_cast<std::int64_t>(block),static_cast<std::int64_t>(n)-off);
  auto view=ib.value()->view().subview(core::FrameIndex{origin+off},frames(part));
  auto out=ob.value()->mutable_view().subview(core::FrameIndex{origin+off},frames(part));
  require(bool(view)&&bool(out),"actual subview");
  const auto ctx=dsp::DspProcessContext{range(origin+off,part),off==0,processEos && off+part==static_cast<std::int64_t>(n)};
  const auto before=g_callback_allocs.load();g_tracking.store(true);
  const auto rc=base.process(*view.value(),*out.value(),ctx);
  g_tracking.store(false);
  require(bool(rc),"real IModule process failed");
  require(g_callback_allocs.load()==before,"allocation in real IModule process");
  off+=part;
 }
 for(std::int64_t pos=0;pos<d;){
  auto part=std::min<std::int64_t>(static_cast<std::int64_t>(block),d-pos);
  auto out=ob.value()->mutable_view().subview(core::FrameIndex{origin+static_cast<std::int64_t>(n)+pos},frames(part));
  require(bool(out),"real tail subview");
  auto ctx=dsp::DspProcessContext{range(origin+static_cast<std::int64_t>(n)+pos,part),false,pos+part==d};
  const auto before=g_callback_allocs.load();g_tracking.store(true);
  const auto rc=base.finalize(*out.value(),ctx);
  g_tracking.store(false);
  require(bool(rc),"real IModule finalize failed");
  require(g_callback_allocs.load()==before,"allocation in IModule finalize");
  pos+=part;
 }
 EpochSession direct;
 require(direct.begin_epoch(1,cfg,bypass)==EpochStatus::ok,"direct begin");
 std::vector<Frame> raw(n+static_cast<std::size_t>(d));
 for(std::size_t off=0;off<n;){
  const auto part=std::min(block,n-off);
  require(direct.process(1,{pcm.data()+off,part},{raw.data()+off,part})==EpochStatus::ok,"direct process");
  off+=part;
 }
 require(direct.finish(1,{raw.data()+n,static_cast<std::size_t>(d)})==EpochStatus::ok,"direct finish");
 for(const auto& frame:raw) {
  const auto* bytes=reinterpret_cast<const unsigned char*>(&frame);
  for(std::size_t j=0;j<sizeof(frame);++j){g_hash^=bytes[j];g_hash*=1099511628211ULL;}
 }
 for(std::size_t c=0;c<static_cast<std::size_t>(ch);++c){
  auto p=ob.value()->view().channel(c);require(bool(p),"output plane");
  for(std::size_t i=0;i<raw.size();++i){
   if(std::memcmp(&(*p.value())[i],&raw[i][c],sizeof(double))!=0)
      throw std::runtime_error("real IModule PCM != direct real P2 at frame "+std::to_string(i));
  }
 }
 if(!bypass){
  ::research::Meter meter;
  for(std::size_t i=static_cast<std::size_t>(d);i<raw.size();++i)meter.push(raw[i],ch);
  for(int i=0;i<11;i++)meter.push({},ch);
  require(meter.peak<=cfg.ceiling_linear,"true peak exceeded");
 }
 auto more=ob.value()->mutable_view().subview(core::FrameIndex{origin+static_cast<std::int64_t>(n)},frames(1));
 require(bool(more),"repeat finalize view");
 auto bad=base.finalize(*more.value(),{range(origin+static_cast<std::int64_t>(n),1),false,true});
 require(!bad,"repeat finalize accepted");
}
static void test_bad_input_and_alias(){
 const auto fmt=format(48000,2);
 const dsp::DspProcessSpec spec{fmt,audio::FrameDomainId::SOURCE_PROCESSING_RATE,frames(64)};
 ResearchP2IModule m(descriptor(),Configuration{},false);
 require(bool(m.prepare(spec)),"fault test prepare");
 auto in=audio::AudioBuffer::create(fmt,audio::FrameDomainId::SOURCE_PROCESSING_RATE,core::FrameIndex{0},frames(64));
 auto out=audio::AudioBuffer::create(fmt,audio::FrameDomainId::SOURCE_PROCESSING_RATE,core::FrameIndex{0},frames(64));
 require(bool(in)&&bool(out),"fault test buffers");
 auto lane=in.value()->mutable_view().channel(0);require(bool(lane),"lane");
 (*lane.value())[5]=std::numeric_limits<double>::quiet_NaN();
 auto rc=m.process(in.value()->view(),out.value()->mutable_view(),{range(0,64),true,false});
 require(!rc,"nonfinite accepted");
 for(int c=0;c<2;++c){const auto p=out.value()->view().channel(c);require(bool(p),"fault out lane");for(double x:*p.value())require(x==0.0,"fault not silent");}
 (*lane.value())[5]=0.0;
 require(!m.process(in.value()->view(),out.value()->mutable_view(),{range(0,64),true,false}),"fault resumed");
 m.reset();
 require(!m.process(in.value()->view(),in.value()->mutable_view(),{range(0,64),true,false}),"aliased buffers accepted");
 std::cout<<"ok nonfinite/alias fail-closed and reset epoch\n";
}
static void test_wrong_domain_and_reset(){
 const auto fmt=format(48000,2);
 ResearchP2IModule m(descriptor(),Configuration{},false);
 const dsp::DspProcessSpec bad{fmt,audio::FrameDomainId::OUTPUT_RATE,frames(128)};
 require(!m.prepare(bad),"accepted wrong-domain specification");
 const dsp::DspProcessSpec good{fmt,audio::FrameDomainId::SOURCE_PROCESSING_RATE,frames(128)};
 require(bool(m.prepare(good)),"prepare-after-bad");
 m.reset(); // off-callback rearm
 auto ib=audio::AudioBuffer::create(fmt,audio::FrameDomainId::SOURCE_PROCESSING_RATE,core::FrameIndex{41},frames(2));
 auto ob=audio::AudioBuffer::create(fmt,audio::FrameDomainId::SOURCE_PROCESSING_RATE,core::FrameIndex{41},frames(2));
 require(bool(ib)&&bool(ob),"reset buffers");
 require(bool(m.process(ib.value()->view(),ob.value()->mutable_view(),{range(41,2),true,false})),"reset epoch on nonzero origin");
 require(!m.runtime_checkpoint(),"fake checkpoint accepted");
}
int main(){try{
 for(int fs:{44100,48000,88200,96000,176400,192000}){
  for(int ch:{1,2}){
   for(auto block:{std::size_t{1},std::size_t{7},std::size_t{64},std::size_t{512}}){
    test_rgs_imodule(fs,ch,121,block,false,false,false);
    test_rgs_imodule(fs,ch,807,block,false,true,true);
    test_rgs_imodule(fs,ch,121,block,true,true,false);
   }
  }
  std::cout<<"ok fs="<<fs<<" true_RGS_IModule mono_stereo split_EOS_BYPASS\n";
 }
 test_wrong_domain_and_reset();
 test_bad_input_and_alias();
 std::cout<<"FNV64="<<std::hex<<g_hash<<std::dec<<"\n";
 std::cout<<"R1d-P2 I1.6 REAL RGS IModule ABI: PASS\n";
 return 0;
}catch(const std::exception& e){
 g_tracking.store(false);
 std::cerr<<"R1d-P2 I1.6 FAIL: "<<e.what()<<"\n";
 return 1;
}}
