// P2 exact execution-layer profiler — research-only, no algorithm changes.
// Measures CallbackEngine, EpochSession, planar bridge and genuine IModule
// process independently on identical PCM. Every sample and EOS tail is
// compared bit-for-bit to the direct engine on the same run.
// Timings: only process / finish; generated PCM, allocations and comparisons
// deliberately excluded. Not a WASAPI, WCET or AutoMatch throughput claim.
#include "p2_callback_engine.hpp"
#include "p2_epoch_session.hpp"
#include "p2_planar_host_bridge.hpp"
#include "research_p2_imodule.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/dsp/module_descriptor.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace p2 = rgsml::research::r1d_p2;
namespace audio = rgsml::audio;
namespace core = rgsml::core;
namespace dsp = rgsml::dsp;
using Clock = std::chrono::steady_clock;
using Frame = p2::Frame;
using Bridge = p2::PlanarHostBridge<p2::EpochSession,p2::Configuration,p2::EpochStatus>;
constexpr std::size_t kBlock=1024U;
constexpr double pi=3.1415926535897932384626433832795;

void require(bool valid,const char* message) {
    if(!valid)throw std::runtime_error(message);
}
double ms(Clock::time_point start,Clock::time_point end) {
    return std::chrono::duration<double,std::milli>{end-start}.count();
}
core::FrameCount fc(std::int64_t n) {
    auto r=core::FrameCount::create(n);
    require(bool(r),"FrameCount invalid");
    return *r.value();
}
core::FrameRange range(std::int64_t start,std::int64_t length) {
    auto r=core::FrameRange::create(core::FrameIndex{start},
                                    core::FrameIndex{start+length});
    require(bool(r),"FrameRange invalid");
    return *r.value();
}
dsp::ModuleDescriptor descriptor() {
    auto r=dsp::ModuleDescriptor::create({
        "rgsml.dsp.streaming-true-peak-limiter", "Streaming TP Limiter",
        {dsp::ModuleCategory::DYNAMICS}, {dsp::ProcessingStage::MASTER},
        {dsp::ChainSegment::MANUAL}, false, true,
        dsp::PlacementClass::INLINE_CHAIN,std::nullopt,{},{},{},{},
        true,true,std::string{"0.1.0-research"},
        std::string{"rgsml.dsp.streaming-true-peak-limiter.params/0.1.0"}});
    require(bool(r),"P2 descriptor invalid");
    return std::move(*r.value());
}
[[nodiscard]] bool bits_equal(double a,double b) {
    return std::bit_cast<std::uint64_t>(a)==std::bit_cast<std::uint64_t>(b);
}
void verify(const Frame& original, const Frame& candidate) {
    require(bits_equal(original[0],candidate[0])
        &&bits_equal(original[1],candidate[1]),
        "P2 execution layers disagree on actual binary64 PCM");
}
Frame signal(std::int64_t index,int hz,int scenario) {
    const auto t=static_cast<double>(index)/hz;
    const auto amp=scenario==0?0.12:1.0;
    const double transient=(index%(hz/3)<55)?0.48:0.0;
    const auto left=amp*(0.67*std::sin(2*pi*79*t)
        +0.35*std::sin(2*pi*7099*t)+transient);
    return {left,amp*(-0.61*left +0.28*std::sin(2*pi*1103*t))};
}
struct Accumulated {
    double directProcess{},epochProcess{},bridgeProcess{},moduleProcess{};
    double directFinish{},epochFinish{},bridgeFinish{},moduleFinish{};
    std::uint64_t comparedSamples{};
};
Accumulated measure(int hz,int seconds,int scenario) {
    const auto frames=static_cast<std::int64_t>(hz)*seconds;
    const p2::Configuration config{hz,2,2,0.8912509381337456,
                                   scenario==2?6.0:3.0};
    p2::CallbackEngine direct;
    p2::EpochSession epoch;
    Bridge bridge;
    p2::ResearchP2IModule module{descriptor(),config,false};
    const auto fmt=audio::AudioFormat::create(
        *core::SampleRate::create(hz).value(),audio::ChannelLayout::STEREO_LR);
    require(bool(fmt),"unsupported format");
    const dsp::DspProcessSpec spec{
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,fc(kBlock)};
    require(direct.prepare(config)==p2::Status::ok,"direct prepare failed");
    require(epoch.begin_epoch(100,config,false)==p2::EpochStatus::ok,
            "epoch prepare failed");
    require(bridge.begin_epoch(101,config,false,0)==p2::HostBridgeStatus::ok,
            "bridge prepare failed");
    require(bool(module.prepare(spec)),"IModule prepare failed");
    const auto delay=direct.latency_frames();
    require(delay==epoch.latency_frames()
        &&delay==bridge.latency_frames(),"layer latency mismatch");
    auto requirements=module.runtime_requirements(spec);
    require(bool(requirements)
        &&requirements.value()->algorithmic_latency_frames.value()
            ==static_cast<std::int64_t>(delay),"IModule latency mismatch");
    std::array<Frame,kBlock> in{},outDirect{},outEpoch{};
    std::array<double,kBlock> inL{},inR{},outL{},outR{};
    Accumulated acc{};

    for(std::int64_t cursor=0;cursor<frames;) {
        const auto size=static_cast<std::size_t>(
            std::min<std::int64_t>(frames-cursor,static_cast<std::int64_t>(kBlock)));
        // All test material and buffer creation occurs outside timed paths.
        for(std::size_t i=0;i<size;++i) {
            in[i]=signal(cursor+static_cast<std::int64_t>(i),hz,scenario);
            inL[i]=in[i][0];inR[i]=in[i][1];
        }
        auto input=audio::AudioBuffer::create(
            *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{cursor},fc(size));
        auto output=audio::AudioBuffer::create(
            *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{cursor},fc(size));
        require(bool(input)&&bool(output),"IModule buffer allocation failed");
        auto sourceView=input.value()->mutable_view();
        const auto srcL=sourceView.channel(0),srcR=sourceView.channel(1);
        require(bool(srcL)&&bool(srcR),"IModule source channels missing");
        std::copy_n(inL.data(),size,srcL.value()->begin());
        std::copy_n(inR.data(),size,srcR.value()->begin());

        const auto t0=Clock::now();
        auto dr=direct.process({in.data(),size},{outDirect.data(),size});
        const auto t1=Clock::now();
        auto er=epoch.process(100,{in.data(),size},{outEpoch.data(),size});
        const auto t2=Clock::now();
        auto br=bridge.process(
            101,cursor,{inL.data(),size},{inR.data(),size},
            {outL.data(),size},{outR.data(),size},
            cursor==0,cursor+static_cast<std::int64_t>(size)==frames);
        const auto t3=Clock::now();
        auto mr=module.process(
            input.value()->view(),output.value()->mutable_view(),
            {range(cursor,static_cast<std::int64_t>(size)),cursor==0,false});
        const auto t4=Clock::now();
        require(dr==p2::Status::ok && er==p2::EpochStatus::ok
            &&br==p2::HostBridgeStatus::ok &&bool(mr),
            "P2 layer rejected ordinary PCM");
        acc.directProcess+=ms(t0,t1);
        acc.epochProcess+=ms(t1,t2);
        acc.bridgeProcess+=ms(t2,t3);
        acc.moduleProcess+=ms(t3,t4);
        const auto actual=output.value()->view();
        const auto vL=actual.channel(0),vR=actual.channel(1);
        require(bool(vL)&&bool(vR),"IModule output channels missing");
        for(std::size_t i=0;i<size;++i) {
            verify(outDirect[i],outEpoch[i]);
            verify(outDirect[i],Frame{outL[i],outR[i]});
            verify(outDirect[i],Frame{(*vL.value())[i],(*vR.value())[i]});
            acc.comparedSamples+=6; // 2 channels * 3 independently checked paths
        }
        cursor+=static_cast<std::int64_t>(size);
    }

    std::vector<Frame> directTail(delay),epochTail(delay);
    std::vector<double> bridgeL(delay),bridgeR(delay);
    auto moduleTail=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{frames},fc(static_cast<std::int64_t>(delay)));
    require(bool(moduleTail),"IModule EOS buffer allocation failed");

    const auto t0=Clock::now();
    const auto dr=direct.finish(directTail);
    const auto t1=Clock::now();
    const auto er=epoch.finish(100,epochTail);
    const auto t2=Clock::now();
    const auto br=bridge.finish(101,frames,bridgeL,bridgeR);
    const auto t3=Clock::now();
    const auto mr=module.finalize(
        moduleTail.value()->mutable_view(),
        {range(frames,static_cast<std::int64_t>(delay)),false,true});
    const auto t4=Clock::now();
    require(dr==p2::Status::ok && er==p2::EpochStatus::ok
        &&br==p2::HostBridgeStatus::ok &&bool(mr),
        "P2 layer EOS failed");
    acc.directFinish=ms(t0,t1);
    acc.epochFinish=ms(t1,t2);
    acc.bridgeFinish=ms(t2,t3);
    acc.moduleFinish=ms(t3,t4);
    const auto tailL=moduleTail.value()->view().channel(0);
    const auto tailR=moduleTail.value()->view().channel(1);
    require(bool(tailL)&&bool(tailR),"IModule EOS channels missing");
    for(std::size_t i=0;i<delay;++i) {
        verify(directTail[i],epochTail[i]);
        verify(directTail[i],Frame{bridgeL[i],bridgeR[i]});
        verify(directTail[i],Frame{(*tailL.value())[i],(*tailR.value())[i]});
        acc.comparedSamples+=6;
    }
    require(acc.comparedSamples==6U*(static_cast<std::uint64_t>(frames)+delay),
            "P2 profiler did not verify complete PCM");
    return acc;
}
int main(int argc,char** argv) {
    try {
        bool quick=false;
        int selectedRate=0;
        std::string path="P2_EXECUTION_LAYERS_5MIN.tsv";
        for(int i=1;i<argc;++i) {
            const std::string_view arg{argv[i]};
            if(arg=="--quick")quick=true;
            else if(arg=="--rate" && i+1<argc)selectedRate=std::stoi(argv[++i]);
            else if(arg=="--output" && i+1<argc)path=argv[++i];
            else throw std::runtime_error("usage: [--quick] [--rate 44100|48000] [--output TSV]");
        }
        require(selectedRate==0||selectedRate==44100||selectedRate==48000,
                "unsupported selected rate");
        std::ofstream file{path,std::ios::trunc};
        require(bool(file),"cannot open P2 execution-layers TSV");
        file<<"schema\trate_hz\tduration_s\tscenario\tlayer\tprocess_ms"
              "\tfinalize_ms\tmeasured_ms\tcompared_sample_bits\tresult\n";
        file<<std::fixed<<std::setprecision(6);
        const auto seconds=quick?2:300;
        for(const int hz:{44100,48000}) {
            if(selectedRate!=0 && selectedRate!=hz)continue;
            const auto scenarioCount=quick?1:3;
            for(int scenario=0;scenario<scenarioCount;++scenario) {
                const auto r=measure(hz,seconds,scenario);
                const char* label=scenario==0?"QUIET":scenario==1?"NORMAL":"HOT_DRIVE6";
                const std::array<const char*,4> layers{
                    "CALLBACK_ENGINE","EPOCH_SESSION","PLANAR_BRIDGE","REAL_IMODULE"};
                const std::array<double,4> process{
                    r.directProcess,r.epochProcess,r.bridgeProcess,r.moduleProcess};
                const std::array<double,4> finish{
                    r.directFinish,r.epochFinish,r.bridgeFinish,r.moduleFinish};
                for(std::size_t i=0;i<4;++i) {
                    file<<"I2-P2-LAYERS-1\t"<<hz<<'\t'<<seconds<<'\t'<<label
                        <<'\t'<<layers[i]<<'\t'<<process[i]<<'\t'<<finish[i]
                        <<'\t'<<process[i]+finish[i]<<'\t'
                        <<r.comparedSamples<<"\tBIT_EXACT\n";
                }
                file.flush();
                require(bool(file),"P2 layers TSV write failure");
                std::cout<<"P2 layers exact: "<<hz<<"Hz / "<<label
                         <<" direct_ms="<<r.directProcess
                         <<" module_ms="<<r.moduleProcess
                         <<" compared_bits="<<r.comparedSamples<<'\n';
            }
        }
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"P2 layers FAIL: "<<e.what()<<'\n';
        return 1;
    }
}
