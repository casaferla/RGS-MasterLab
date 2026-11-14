#include "p2_epoch_session.hpp"
#include <algorithm>
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

static std::atomic<std::uint64_t> g_allocs{0};
#if defined(_MSC_VER)
#define R1D_NOINLINE __declspec(noinline)
#else
#define R1D_NOINLINE __attribute__((noinline))
#endif
static R1D_NOINLINE void* counted_alloc(std::size_t n) {
    g_allocs.fetch_add(1,std::memory_order_relaxed);
    return std::malloc(n?n:1);
}
static R1D_NOINLINE void counted_free(void* p) { std::free(p); }
void* operator new(std::size_t n) { if(void* p=counted_alloc(n))return p; throw std::bad_alloc(); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { return counted_alloc(n); }
void operator delete(void* p) noexcept { counted_free(p); }
void operator delete(void* p,std::size_t) noexcept { counted_free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { counted_free(p); }
using namespace rgsml::research::r1d_p2;
static void expect(bool ok,const char* what) { if(!ok) throw std::runtime_error(what); }
static std::vector<Frame> make_signal(std::size_t n,int fs,int channels,double amplitude) {
    std::vector<Frame> out(n);
    for(std::size_t i=0;i<n;++i) {
        double t=double(i)/fs;
        out[i][0]=amplitude*(.49*std::sin(2*3.141592653589793*71*t)
                   +.43*std::sin(2*3.141592653589793*7001*t));
        out[i][1]=channels==2 ? -.731*out[i][0] : 0.;
    }
    return out;
}
static bool equal_pcm(const std::vector<Frame>& a,const std::vector<Frame>& b) {
    return a.size()==b.size() && (a.empty() ||
        std::memcmp(a.data(),b.data(),a.size()*sizeof(Frame))==0);
}
static std::vector<Frame> run_direct(const std::vector<Frame>& input,Configuration cfg,std::size_t block) {
    CallbackEngine engine;
    expect(engine.prepare(cfg)==Status::ok,"direct prepare");
    std::vector<Frame> out(input.size()+engine.latency_frames());
    for(std::size_t off=0;off<input.size();) {
        const auto n=std::min(block,input.size()-off);
        expect(engine.process(std::span(input).subspan(off,n),std::span(out).subspan(off,n))==Status::ok,"direct process");
        off+=n;
    }
    expect(engine.finish(std::span(out).subspan(input.size()))==Status::ok,"direct finish");
    return out;
}
static std::vector<Frame> run_epoch(const std::vector<Frame>& input,Configuration cfg,bool bypass,
                                     std::size_t block,std::uint64_t id) {
    EpochSession session;
    expect(session.begin_epoch(id,cfg,bypass)==EpochStatus::ok,"epoch begin");
    const auto delay=session.latency_frames();
    std::vector<Frame> out(input.size()+delay);
    for(std::size_t off=0;off<input.size();) {
        auto n=std::min(block,input.size()-off);
        expect(session.process(id,std::span(input).subspan(off,n),
                         std::span(out).subspan(off,n))==EpochStatus::ok,"epoch process");
        off+=n;
    }
    expect(session.finish(id,std::span(out).subspan(input.size()))==EpochStatus::ok,"epoch finish");
    expect(session.input_frames()==input.size(),"input ledger");
    expect(session.output_frames()==out.size(),"output ledger");
    for(std::size_t i=0;i<delay;++i)expect(out[i]==Frame{},"nonzero latency prefill");
    if(bypass) {
        std::vector<Frame> programme(out.begin()+delay,out.end());
        expect(equal_pcm(programme,input),"bypass not bit-exact unity at aligned latency");
    }
    // Stale completion and double-finish must fail silently, without rewriting ledger.
    std::vector<Frame> stale(delay,Frame{1.,1.});
    expect(session.finish(id,stale)==EpochStatus::already_finished,"double finish accepted");
    for(const auto& f:stale) expect(f==Frame{},"double finish not silent");
    return out;
}
static void all_layouts() {
    for(int fs:{44100,48000,88200,96000,176400,192000})
      for(int channels:{1,2}) {
        Configuration cfg{fs,channels,2,0.8912509381337456,0.0};
        for(std::size_t n:{std::size_t{0},std::size_t{1},std::size_t{5},std::size_t{121},std::size_t{512},std::size_t{1697}}) {
          auto data=make_signal(n,fs,channels, n>121?1.36:.13);
          const auto direct=run_direct(data,cfg,512);
          ::research::Meter measured;
          for(std::size_t i=direct.size()-n;i<direct.size();++i)
              measured.push(direct[i],channels);
          for(int i=0;i<11;++i)measured.push(Frame{},channels);
          expect(measured.peak<=cfg.ceiling_linear,"epoch M16 true peak exceeded");
          for(auto block:{std::size_t{1},std::size_t{7},std::size_t{64},std::size_t{511},std::size_t{512},std::size_t{1024}}) {
            const auto selected=run_epoch(data,cfg,false,block,3);
            expect(equal_pcm(direct,selected),"P2 epoch != direct callback PCM");
          }
          const auto bypass=run_epoch(data,cfg,true,7,4);
          expect(bypass.size()==direct.size(),"bypass latency differs from active");
        }
        std::cout<<"ok all layouts fs="<<fs<<" channels="<<channels<<"\n";
      }
}
static void transitions() {
    EpochSession s;
    Configuration cfg{};
    std::array<Frame,1> in{Frame{.2,-.1}};
    std::array<Frame,1> output{};
    expect(s.process(1,in,output)==EpochStatus::not_prepared,"preprepare accepted");
    expect(s.begin_epoch(0,cfg,false)==EpochStatus::invalid_epoch,"zero epoch accepted");
    expect(s.begin_epoch(11,cfg,false)==EpochStatus::ok,"initial epoch failed");
    const auto first_latency=s.latency_frames();
    expect(s.process(11,in,output)==EpochStatus::ok,"first callback failed");
    expect(s.input_frames()==1,"input count first");
    auto bad=cfg;bad.sample_rate=123456;
    expect(s.begin_epoch(12,bad,true)==EpochStatus::invalid_configuration,"bad next config accepted");
    expect(s.epoch()==11,"bad reconfigure replaced active epoch");
    expect(s.process(11,in,output)==EpochStatus::ok,"bad reconfigure damaged active epoch");
    expect(s.begin_epoch(11,cfg,true)==EpochStatus::invalid_epoch,"duplicate epoch accepted");
    // Transport seek/loop/reconfigure: old delayed PCM discarded before new
    // playback starts. The host must gate the discontinuity itself.
    expect(s.begin_epoch(12,cfg,true)==EpochStatus::ok,"seek/bypass rearm failed");
    expect(s.previous_abandoned(),"dropped tail not marked abandoned");
    expect(s.latency_frames()==first_latency,"bypass latency changed");
    expect(s.input_frames()==0 && s.output_frames()==0,"new epoch inherited old counts");
    output[0]=Frame{17,18};
    expect(s.process(11,in,output)==EpochStatus::stale_epoch,"stale callback accepted");
    expect(output[0]==Frame{},"stale callback leaked old PCM");
    expect(s.input_frames()==0,"stale callback advanced counters");
    expect(s.process(12,in,output)==EpochStatus::ok,"new bypass callback failed");
    expect(output[0]==Frame{},"new epoch leaked unaligned first PCM");
    expect(s.cancel_epoch(11)==EpochStatus::stale_epoch,"stale cancel succeeded");
    expect(s.cancel_epoch(12)==EpochStatus::ok,"cancel failed");
    expect(s.process(12,in,output)==EpochStatus::not_prepared,"cancelled callback resumed");
    expect(s.begin_epoch(12,cfg,false)==EpochStatus::invalid_epoch,"cancel reused token");
    expect(s.begin_epoch(13,cfg,false)==EpochStatus::ok,"loop rewind fresh epoch failed");
    expect(s.process(13,in,output)==EpochStatus::ok,"fresh loop callback failed");
    expect(output[0]==Frame{},"loop leaked history");
    expect(s.cancel_epoch(13)==EpochStatus::ok,"final cancel failed");
    // Parameter change is a fresh epoch, never a mutation of frames already emitted.
    cfg.drive_db = 3.; cfg.margin_percent = 5;
    expect(s.begin_epoch(14,cfg,false)==EpochStatus::ok,"new parameter epoch failed");
    expect(s.process(14,in,output)==EpochStatus::ok,"param epoch first callback");
    expect(output[0]==Frame{},"parameter epoch leaked old history");
    std::cout<<"ok stale/seek/loop/cancel/reconfigure and transactional config\n";
}
static void invalid_inputs() {
    EpochSession s;
    Configuration cfg{};
    expect(s.begin_epoch(7,cfg,true)==EpochStatus::ok,"invalid test prepare");
    std::array<Frame,4> raw{{Frame{.1,.1},Frame{.1,.1},Frame{NAN,.1},Frame{.1,.1}}};
    std::array<Frame,4> out{};
    for(auto& f:out)f=Frame{9,9};
    expect(s.process(7,raw,out)==EpochStatus::invalid_pcm,"bypass NaN accepted");
    for(auto f:out) expect(f==Frame{},"bypass invalid not silent");
    expect(s.process(7,raw,out)==EpochStatus::wrong_lifecycle,"bypass fault resumed");
    expect(s.begin_epoch(8,cfg,true)==EpochStatus::ok,"bypass fault recovery failed");
    raw[2]=Frame{.2,.1};
    expect(s.process(8,raw,out)==EpochStatus::ok,"bypass reinit failed");
    expect(s.begin_epoch(9,cfg,false)==EpochStatus::ok,"live rearm failed");
    raw[2]=Frame{INFINITY,.1};
    expect(s.process(9,raw,out)==EpochStatus::engine_fault,"live infinity accepted");
    expect(s.last_engine_status()==Status::invalid_pcm,"live detailed error incorrect");
    for(auto f:out)expect(f==Frame{},"live error not silent");
    expect(s.begin_epoch(10,cfg,false)==EpochStatus::ok,"live fault recovery failed");
    std::array<Frame,2> mismatch{};
    expect(s.process(10,raw,mismatch)==EpochStatus::output_size_mismatch,"invalid spans accepted");
    for(auto f:mismatch)expect(f==Frame{},"span mismatch not silent");
    std::cout<<"ok nonfinite/span fail-closed and recovery\n";
}
static void no_allocations_callbacks() {
    for(bool bypass:{false,true}) {
        EpochSession s;
        Configuration cfg{192000,2,2,.8912509381337456,0.};
        expect(s.begin_epoch(20,cfg,bypass)==EpochStatus::ok,"no-alloc begin");
        const auto src=make_signal(512*40,192000,2,1.36);
        std::vector<Frame> out(src.size());
        std::vector<Frame> tail(s.latency_frames());
        const auto before=g_allocs.load(std::memory_order_relaxed);
        for(std::size_t i=0;i<src.size();i+=512) {
            const auto rc=s.process(20,std::span(src).subspan(i,512),std::span(out).subspan(i,512));
            if(rc!=EpochStatus::ok)throw std::runtime_error("no-alloc callback failed");
        }
        const auto rc=s.finish(20,tail);
        if(rc!=EpochStatus::ok)throw std::runtime_error("no-alloc drain failed");
        const auto after=g_allocs.load(std::memory_order_relaxed);
        expect(before==after,"callback or drain allocated heap");
        std::cout<<"ok no_heap mode="<<(bypass?"bypass":"P2")<<"\n";
    }
}
int main()try {
    all_layouts();
    transitions();
    invalid_inputs();
    no_allocations_callbacks();
    std::cout<<"R1d-P2 I1 EPOCH/TRANSPORT RESEARCH: PASS\n";
    return 0;
} catch(const std::exception& ex){
    std::cerr<<"R1d-P2 I1 EPOCH/TRANSPORT RESEARCH: FAIL "<<ex.what()<<"\n";
    return 1;
}
