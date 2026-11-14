// RGS P2 research-only: independent component-attribution benchmark.
// Split the original pinned PredictiveMusic and Stream sequentially into
// block-timed stages, then compare EVERY emitted double bit against the
// unmodified CallbackEngine running the same source and epoch. No Product
// or pinned limiter source changes. This is offline, not WASAPI or WCET.
#include "p2_callback_engine.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace p2=rgsml::research::r1d_p2;
using Clock=std::chrono::steady_clock;
using Frame=p2::Frame;
constexpr std::size_t block=1024U;
constexpr std::size_t queue_size=8192U;
constexpr double pi=3.1415926535897932384626433832795;
void require(bool ok,const char* why) {if(!ok)throw std::runtime_error(why);}
[[nodiscard]] double ms(Clock::time_point a,Clock::time_point b) {
    return std::chrono::duration<double,std::milli>(b-a).count();
}
[[nodiscard]] bool same(Frame a,Frame b) {
    return std::bit_cast<std::uint64_t>(a[0])
            ==std::bit_cast<std::uint64_t>(b[0])
        &&std::bit_cast<std::uint64_t>(a[1])
            ==std::bit_cast<std::uint64_t>(b[1]);
}
[[nodiscard]] Frame stimulus(std::int64_t i,int hz,int scenario) {
    const double t=static_cast<double>(i)/hz;
    const double volume=scenario==0?0.08:1.;
    const double transient=(i%(hz/3)<60)?0.47:0.;
    const double left=volume*(0.68*std::sin(2*pi*79*t)
        +0.33*std::sin(2*pi*7189*t)+transient);
    return {left,volume*(-0.61*left+0.28*std::sin(2*pi*1103*t))};
}
struct Row {
    double callback_ms{},music_ms{},safety_ms{},finish_ms{};
    std::uint64_t compared_bits{};
    std::uint64_t committed{};
    std::uint64_t checks{};
    std::uint64_t sweeps{};
};
struct StageQueue {
    std::array<Frame,queue_size> pcm{};
    std::size_t head{},tail{},size{};
    void put(std::span<const Frame> frames) {
        require(frames.size()<=queue_size-size,"P2 component queue overflow");
        for(Frame f:frames){pcm[tail]=f;tail=(tail+1U)%queue_size;++size;}
    }
    Frame take() {
        require(size>0,"P2 component output underrun");
        auto f=pcm[head];head=(head+1U)%queue_size;--size;return f;
    }
};
Row run(int hz,int seconds,int scenario) {
    const std::int64_t count=static_cast<std::int64_t>(hz)*seconds;
    const double drive=scenario==2?6.:scenario==1?3.:0.;
    constexpr double ceiling=0.8912509381337456;
    const p2::Configuration conf{hz,2,2,ceiling,drive};
    p2::CallbackEngine reference;
    require(reference.prepare(conf)==p2::Status::ok,"reference prepare failed");
    ::research::PredictiveMusic controller{hz,2,drive,ceiling};
    ::research::Stream verifier;
    require(verifier.prepare({hz,2,2,ceiling,false,true})
        ==::research::Status::ok,"verifier prepare failed");
    const std::size_t latency=controller.delay()+verifier.horizon()
        +::research::quantum-1U;
    require(reference.latency_frames()==latency,"component latency mismatch");
    std::array<Frame,block> input{},referenceOutput{},controllerOutput{};
    std::array<Frame,::research::quantum> scratch{};
    std::array<double,::research::quantum> gains{};
    StageQueue q;
    Row result{};
    std::uint64_t emitted=0;
    auto capture=[&](const ::research::Step& step) {
        require(step.status==::research::Status::ok,"verifier rejected input");
        q.put({scratch.data(),step.frames});
        emitted+=step.frames;
        result.checks+=step.full_checks;
        result.sweeps+=step.sweeps;
    };
    auto compare=[&](Frame expected,std::uint64_t outIndex) {
        // The source-processing channel exposes D zero prefill frames,
        // followed by the complete verified sample stream.
        const Frame actual=outIndex<latency?Frame{}:q.take();
        require(same(expected,actual),"independent P2 stages differ from CallbackEngine PCM");
        result.compared_bits+=2U;
    };
    for(std::int64_t cursor=0;cursor<count;) {
        const std::size_t n=static_cast<std::size_t>(
            std::min<std::int64_t>(count-cursor,block));
        for(std::size_t i=0;i<n;++i)
            input[i]=stimulus(cursor+static_cast<std::int64_t>(i),hz,scenario);
        const auto t0=Clock::now();
        require(reference.process({input.data(),n},{referenceOutput.data(),n})
            ==p2::Status::ok,"reference process failed");
        const auto t1=Clock::now();
        std::size_t produced=0;
        for(std::size_t i=0;i<n;++i) {
            Frame f{};
            if(controller.push(input[i],f))
                controllerOutput[produced++]=f;
        }
        const auto t2=Clock::now();
        for(std::size_t i=0;i<produced;++i) {
            const auto step=verifier.submit(
                {controllerOutput.data()+i,1U},scratch,gains);
            capture(step);
        }
        const auto t3=Clock::now();
        result.callback_ms+=ms(t0,t1);
        result.music_ms+=ms(t1,t2);
        result.safety_ms+=ms(t2,t3);
        for(std::size_t i=0;i<n;++i)
            compare(referenceOutput[i],static_cast<std::uint64_t>(cursor)+i);
        cursor+=static_cast<std::int64_t>(n);
    }
    // Flush the controller's exact finite lookahead, then verifier drain.
    const auto t0=Clock::now();
    std::size_t produced=0;
    for(std::size_t i=0;i<controller.delay();++i) {
        Frame f{};
        if(controller.push(Frame{},f))
            controllerOutput[produced++]=f;
    }
    const auto t1=Clock::now();
    for(std::size_t i=0;i<produced;++i)
        capture(verifier.submit({controllerOutput.data()+i,1U},scratch,gains));
    for(;;) {
        const auto step=verifier.finish(scratch,gains);
        capture(step);
        if(!step.frames)break;
    }
    require(verifier.committed()==static_cast<std::uint64_t>(count)
        &&emitted==static_cast<std::uint64_t>(count),
        "verifier did not emit complete programme");
    std::array<Frame,::research::fir_tail> analyzerOnly{};
    const auto bridge=verifier.bridge(analyzerOnly);
    require(bridge.status==::research::Status::ok
        &&bridge.frames==::research::fir_tail,"verifier bridge failed");
    const auto t2=Clock::now();
    std::array<Frame,4096U> referenceTail{};
    require(latency<=referenceTail.size(),"reference latency too large");
    require(reference.finish({referenceTail.data(),latency})
        ==p2::Status::ok,"reference EOS failed");
    const auto t3=Clock::now();
    result.music_ms+=ms(t0,t1);
    result.safety_ms+=ms(t1,t2);
    result.finish_ms=ms(t2,t3);
    for(std::size_t i=0;i<latency;++i)
        compare(referenceTail[i],static_cast<std::uint64_t>(count)+i);
    require(q.size==0,"P2 verifier emitted extra PCM after EOS");
    require(result.compared_bits==
        (static_cast<std::uint64_t>(count)+latency)*2U,
        "P2 component test did not compare every output bit");
    result.committed=emitted;
    return result;
}
int main(int argc,char** argv) {
    try {
        bool quick=false;std::string path="P2_COMPONENT_COST_5MIN.tsv";
        for(int i=1;i<argc;++i) {
            const std::string_view arg{argv[i]};
            if(arg=="--quick")quick=true;
            else if(arg=="--output"&&i+1<argc)path=argv[++i];
            else throw std::runtime_error("usage: [--quick] [--output FILE]");
        }
        std::ofstream file{path,std::ios::trunc};
        require(bool(file),"cannot create component TSV");
        file<<"schema\trate_hz\tduration_s\tscenario"
            "\tcallback_ms\tpredictive_ms\tsafety_ms\teos_ms"
            "\tcompared_bits\tcommitted_frames\tchecks\tsweeps\tresult\n";
        file<<std::fixed<<std::setprecision(6);
        for(int hz:{44100,48000}) {
            const auto cases=quick?1:3,seconds=quick?2:300;
            for(int scenario=0;scenario<cases;++scenario) {
                const auto r=run(hz,seconds,scenario);
                const char* label=scenario==0?"QUIET":scenario==1?"NORMAL":"HOT_DRIVE6";
                file<<"I2-P2-COMPONENT-1\t"<<hz<<'\t'<<seconds<<'\t'
                    <<label<<'\t'<<r.callback_ms<<'\t'<<r.music_ms
                    <<'\t'<<r.safety_ms<<'\t'<<r.finish_ms<<'\t'
                    <<r.compared_bits<<'\t'<<r.committed<<'\t'
                    <<r.checks<<'\t'<<r.sweeps<<"\tBIT_EXACT\n";
                require(bool(file),"cannot write component row");
                std::cout<<"P2 components: "<<hz<<" "<<label
                         <<" callback_ms="<<r.callback_ms
                         <<" controller_ms="<<r.music_ms
                         <<" safety_ms="<<r.safety_ms
                         <<" BIT_EXACT\n";
            }
        }
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"P2 component profiler FAILED: "<<e.what()<<'\n';return 1;
    }
}
