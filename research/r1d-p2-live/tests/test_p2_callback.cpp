#include "p2_callback_engine.hpp"
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <new>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

// Test-only allocation witness; the production callback bridge itself has
// no allocation hooks or instrumentation.
static std::atomic<std::uint64_t> g_allocations{0};
#if defined(_MSC_VER)
#define R1D_NOINLINE __declspec(noinline)
#else
#define R1D_NOINLINE __attribute__((noinline))
#endif
static R1D_NOINLINE void* counted_alloc(std::size_t n) {
    g_allocations.fetch_add(1,std::memory_order_relaxed);
    return std::malloc(n ? n:1);
}
static R1D_NOINLINE void counted_free(void* p) { std::free(p); }
void* operator new(std::size_t n) {
    if (void* p=counted_alloc(n))return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { counted_free(p); }
void operator delete(void* p,std::size_t) noexcept { counted_free(p); }

using namespace rgsml::research::r1d_p2;
using Frame = rgsml::research::r1d_p2::Frame;

static void require(bool success, const char* message) {
    if (!success) throw std::runtime_error(message);
}
static double native_tp(std::span<const Frame> pcm,int channels) {
    ::research::Meter meter;
    for(Frame frame:pcm) meter.push(frame,channels);
    for(int i=0;i<11;++i) meter.push({},channels);
    return meter.peak;
}
static std::vector<Frame> signal(std::size_t n,int fs,int channels,bool stressed) {
    std::vector<Frame> result(n);
    for(std::size_t i=0;i<n;++i) {
        const double t=double(i)/fs;
        const double amplitude=stressed ? 1.38 : 0.13;
        result[i][0]=amplitude*(0.54*std::sin(2*3.141592653589793*277*t)
                       +0.40*std::sin(2*3.141592653589793*7291*t));
        result[i][1]=channels==2 ? result[i][0]*-.83 : 0.;
    }
    return result;
}
static std::vector<Frame> run(const std::vector<Frame>& src,
                              Configuration config,std::size_t block) {
    CallbackEngine engine;
    require(engine.prepare(config)==Status::ok,"engine prepare");
    require(engine.latency_frames()>0,"missing latency");
    std::vector<Frame> output(src.size()+engine.latency_frames());
    std::size_t offset=0;
    while(offset<src.size()) {
        const std::size_t n=std::min(block,src.size()-offset);
        require(engine.process(std::span(src).subspan(offset,n),
                               std::span(output).subspan(offset,n))==Status::ok,
                "callback error");
        offset+=n;
    }
    require(engine.finish(std::span(output).subspan(src.size()))==Status::ok,
            "finish error");
    require(engine.is_finished(),"finish state");
    require(engine.output_frames()==output.size(),"output frame count");
    // Analyzer-only latency compensation: strip the initial zeros.
    for(std::size_t i=0;i<engine.latency_frames();++i)
        require(output[i]==Frame{},"incorrect prefill");
    return std::vector<Frame>(output.begin()+engine.latency_frames(),output.end());
}
static std::vector<Frame> offline_reference(const std::vector<Frame>& src,
                                            Configuration cfg) {
    ::research::PredictiveMusic music(cfg.sample_rate,cfg.channels,cfg.drive_db,
                                      cfg.ceiling_linear);
    ::research::Stream safety;
    require(safety.prepare({cfg.sample_rate,cfg.channels,cfg.margin_percent,
            cfg.ceiling_linear,false,true})==::research::Status::ok,
            "reference prepare failed");
    std::array<Frame,64> staging{},output{};
    std::array<double,64> gains{};
    std::vector<Frame> pcm;
    std::size_t staged=0;
    auto submit=[&](bool force) {
        if (staged==64 || (force && staged)) {
            const auto step=safety.submit(std::span(staging).first(staged),output,gains);
            require(step.status==::research::Status::ok,"reference submit failed");
            pcm.insert(pcm.end(),output.begin(),output.begin()+step.frames);
            staged=0;
        }
    };
    for (auto frame:src) {
        Frame next{};
        if (music.push(frame,next)) { staging[staged++]=next; submit(false); }
    }
    for(std::size_t i=0;i<music.delay();++i) {
        Frame next{};
        if (music.push(Frame{},next)) { staging[staged++]=next; submit(false); }
    }
    submit(true);
    for (;;) {
        const auto step=safety.finish(output,gains);
        require(step.status==::research::Status::ok,"reference drain failed");
        pcm.insert(pcm.end(),output.begin(),output.begin()+step.frames);
        if (!step.frames)break;
    }
    require(pcm.size()==src.size(),"reference lost frames");
    std::array<Frame,11> analyzer_only{};
    require(safety.bridge(analyzer_only).status==::research::Status::ok,
            "reference M16 bridge failed");
    return pcm;
}
static std::uint64_t fnv_pcm(const std::vector<Frame>& data) {
    std::uint64_t hash=14695981039346656037ULL;
    const auto* bytes=reinterpret_cast<const unsigned char*>(data.data());
    for(std::size_t i=0;i<data.size()*sizeof(Frame);++i) {
        hash^=bytes[i]; hash*=1099511628211ULL;
    }
    return hash;
}
static void test_case(int fs,int channels,bool stressed,std::size_t n) {
    Configuration cfg{fs,channels,2,0.8912509381337456,0.0};
    const auto input=signal(n,fs,channels,stressed);
    const auto ref=offline_reference(input,cfg);
    for(std::size_t block : {std::size_t{7},std::size_t{64},std::size_t{511},
                             std::size_t{512},std::size_t{1024}}) {
        const auto actual=run(input,cfg,block);
        require(actual.size()==n,"actual programme length");
        require(std::memcmp(actual.data(),ref.data(),n*sizeof(Frame))==0,
                "callback-block partition changed PCM");
    }
    const double tp=native_tp(ref,channels);
    require(tp<=cfg.ceiling_linear,"M16 output above ceiling");
    if(!stressed) {
        require(std::memcmp(ref.data(),input.data(),n*sizeof(Frame))==0,
                "INTERIOR unity no longer exact");
    }
    std::cout << "ok fs="<<fs<<" channels="<<channels<<" stress="<<stressed
              <<" frames="<<n<<" truepeak="<<tp
              <<" fnv="<<std::hex<<fnv_pcm(ref)<<std::dec<<"\n";
}
static void test_allocation_free() {
    CallbackEngine engine;
    require(engine.prepare({192000,2,2,0.8912509381337456,0.0})==Status::ok,
            "allocation test prepare");
    const auto n=engine.latency_frames();
    const auto in=signal(512*32,192000,2,true);
    std::vector<Frame> output(in.size());
    std::vector<Frame> tail(n);
    const auto before=g_allocations.load(std::memory_order_relaxed);
    for(std::size_t i=0;i<in.size();i+=512) {
        const auto a=engine.process(std::span(in).subspan(i,512),
                                    std::span(output).subspan(i,512));
        require(a==Status::ok,"allocation test callback");
    }
    require(engine.finish(tail)==Status::ok,"allocation test finish");
    const auto after=g_allocations.load(std::memory_order_relaxed);
    require(before==after,"heap allocation inside callback or drain");
    std::cout<<"ok zero_heap_allocations_after_prepare"<<"\n";
}
static void test_invalid() {
    CallbackEngine invalid;
    auto cfg=Configuration{};
    cfg.drive_db=std::numeric_limits<double>::quiet_NaN();
    require(invalid.prepare(cfg)==Status::invalid_configuration,"NaN drive accepted");
    CallbackEngine engine;
    require(engine.prepare({})==Status::ok,"prepare invalid test");
    std::array<Frame,3> samples{{Frame{.1,.1},Frame{NAN,.1},Frame{.1,.1}}};
    std::array<Frame,3> output{{Frame{7,7},Frame{7,7},Frame{7,7}}};
    require(engine.process(samples,output)==Status::invalid_pcm,"NaN sample accepted");
    for(auto f:output) require(f==Frame{},"fault not silent");
    require(engine.process(samples,output)==Status::not_prepared,"fault state resumed");
    std::cout << "ok fault_closed\n";
}
int main() try {
    // All six supported sample rates, both layouts, unaltered INTERIOR.
    for(int fs : {44100,48000,88200,96000,176400,192000})
        for(int channels:{1,2})
            test_case(fs,channels,false,1130);
    // Stress with history-sensitive smoothing, EOS and >1 sample domains.
    for(int fs : {48000,192000})
        for(int channels:{1,2})
            test_case(fs,channels,true,1507);
    // A second already-researched eta, with explicit drive stress.
    for(int fs : {48000,192000}) {
        Configuration cfg{fs,2,5,0.8912509381337456,5.0};
        const auto x=signal(3337,fs,2,true);
        const auto ref=offline_reference(x,cfg);
        const auto streamed=run(x,cfg,512);
        require(std::memcmp(ref.data(),streamed.data(),x.size()*sizeof(Frame))==0,
                "eta5/drive5 callback != independent offline reference");
        require(native_tp(streamed,2)<=cfg.ceiling_linear,
                "eta5/drive5 above M16 ceiling");
        std::cout << "ok eta5/drive5 fs="<<fs<<"\n";
    }
    for(std::size_t n:{std::size_t{1},std::size_t{5},std::size_t{121}})
        test_case(48000,2,true,n);
    test_invalid();
    test_allocation_free();
    std::cout << "R1d-P2 I0 CALLBACK ADAPTER: PASS (RESEARCH ONLY)\n";
    return 0;
} catch(const std::exception& e) {
    std::cerr << "R1d-P2 I0 CALLBACK ADAPTER: FAIL "<<e.what()<<'\n';
    return 1;
}
