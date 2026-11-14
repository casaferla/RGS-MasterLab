// P2 RESEARCH ONLY: bit-exact experiment using the ORIGINAL pinned
// PredictiveMusic + ORIGINAL pinned Stream, feeding Stream in 64-frame quanta.
// The normal CallbackEngine::process is an independent fresh-epoch oracle.
// The experimental method is compiled only here, never Product or M16/M17.
// Full delivered PCM, latency and EOS are verified as binary64 bit patterns.
// No realtime/WASAPI, worst-case, dropout or 5-min SLA certification.
#ifndef RESEARCH_P2_BATCHED_CALLBACK
#error This benchmark must never silently run without the research feature gate
#endif
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
#include <vector>

namespace p2=rgsml::research::r1d_p2;
using Frame=p2::Frame;
using Clock=std::chrono::steady_clock;
constexpr std::size_t maxBlock=4096U;
constexpr double pi=3.14159265358979323846264338327950288;
void require(bool ok,const char* reason) {
    if(!ok)throw std::runtime_error(reason);
}
[[nodiscard]] double elapsed_ms(Clock::time_point begin,Clock::time_point end) {
    return std::chrono::duration<double,std::milli>{end-begin}.count();
}
[[nodiscard]] Frame stimulus(std::int64_t index,int hz,int scenario,int channels) {
    const auto t=static_cast<double>(index)/hz;
    const auto noise = static_cast<double>((static_cast<std::uint64_t>(index)
        *1664525U+1013904223U)&0xFFFFU)/65536.-.5;
    if(scenario==5) {
        // The pinned isolated adversarial suite's sparse impulse family.
        const auto period=static_cast<std::int64_t>(hz/7);
        const double v=index%period==0?20.0:0.0;
        return {v,channels==2?((index/period)%2?-v:v):0.0};
    }
    if(scenario==3||scenario==4) {
        // Nyquist alternation reproduces a genuinely difficult safety case.
        const double v=index%2?1.0:-1.0;
        return {v,channels==2?-0.75*v:0.0};
    }
    const double level=scenario==0?.065:scenario==2?1.8:
        scenario==6?16.0:1.0;
    const double transient=index%(hz/3)<60?.47:0.;
    const double l=level*(.68*std::sin(2*pi*79*t)
               +.33*std::sin(2*pi*7189*t)+.17*noise+transient);
    const double r=-0.61*l+level*.28*std::sin(2*pi*1103*t);
    return {l,channels==2?r:0.0};
}
[[nodiscard]] bool equal_bits(Frame a,Frame b) {
    return std::bit_cast<std::uint64_t>(a[0])
        == std::bit_cast<std::uint64_t>(b[0])
        && std::bit_cast<std::uint64_t>(a[1])
        == std::bit_cast<std::uint64_t>(b[1]);
}
struct Result {
    double baseline_ms{},batched_ms{};
    double baseline_eos_ms{},batched_eos_ms{};
    std::uint64_t compared_frames{};
    std::int64_t latency{};
    double measuredTruePeak{};
};
Result run(int hz,int channels,int seconds,int scenario,std::size_t size) {
    const double drive=scenario==0?0.0:scenario==2?6.:
        scenario==3?12.:scenario==4||scenario==5?24.:
        scenario==6?18.:3.;
    const p2::Configuration cfg{hz,channels,2,0.8912509381337456,drive};
    p2::CallbackEngine original,experiment;
    require(original.prepare(cfg)==p2::Status::ok
        &&experiment.prepare(cfg)==p2::Status::ok,"P2 prepare rejected");
    const auto latency=original.latency_frames();
    require(latency==experiment.latency_frames(),"P2 experimental latency drift");
    std::array<Frame,maxBlock> input{},observed{},candidate{};
    const auto frames=static_cast<std::int64_t>(hz)*seconds;
    Result result{};
    result.latency=static_cast<std::int64_t>(latency);
    // Independent ITU 4x meter on ACTUAL emitted experimental PCM.
    // Verification and PCM comparison are outside timed process regions.
    ::research::Meter outputMeter{};
    for(std::int64_t offset=0;offset<frames;) {
        const auto n=static_cast<std::size_t>(
            std::min<std::int64_t>(frames-offset,static_cast<std::int64_t>(size)));
        for(std::size_t i=0;i<n;++i)
            input[i]=stimulus(offset+static_cast<std::int64_t>(i),
                              hz,scenario,channels);
        const auto t0=Clock::now();
        auto a=original.process({input.data(),n},{observed.data(),n});
        const auto t1=Clock::now();
        auto b=experiment.process_batched(
            {input.data(),n},{candidate.data(),n});
        const auto t2=Clock::now();
        require(a==p2::Status::ok&&b==p2::Status::ok,
                "P2 original or batched path rejected a test fixture");
        result.baseline_ms+=elapsed_ms(t0,t1);
        result.batched_ms+=elapsed_ms(t1,t2);
        for(std::size_t i=0;i<n;++i) {
            require(equal_bits(observed[i],candidate[i]),
                    "P2 batch/original PCM differs at real output sample");
            outputMeter.push(candidate[i],channels);
        }
        result.compared_frames+=n;
        offset+=static_cast<std::int64_t>(n);
    }
    std::vector<Frame> baselineTail(latency),batchedTail(latency);
    const auto t0=Clock::now();
    auto a=original.finish(baselineTail);
    const auto t1=Clock::now();
    auto b=experiment.finish(batchedTail);
    const auto t2=Clock::now();
    require(a==p2::Status::ok&&b==p2::Status::ok,"P2 batch EOS rejected");
    result.baseline_eos_ms=elapsed_ms(t0,t1);
    result.batched_eos_ms=elapsed_ms(t1,t2);
    for(std::size_t i=0;i<latency;++i) {
        require(equal_bits(baselineTail[i],batchedTail[i]),
                "P2 batch/original EOS PCM differs");
        outputMeter.push(batchedTail[i],channels);
    }
    for(std::size_t i=0;i<::research::fir_tail;++i)
        outputMeter.push(Frame{},channels);
    require(std::isfinite(outputMeter.peak)
        &&outputMeter.peak<=cfg.ceiling_linear,
        "P2 batch violates independent post-gain ITU True Peak ceiling");
    result.compared_frames+=latency;
    result.measuredTruePeak=outputMeter.peak;
    require(original.input_frames()==static_cast<std::uint64_t>(frames)
         &&experiment.input_frames()==original.input_frames()
         &&original.output_frames()==experiment.output_frames()
         &&original.output_frames()==result.compared_frames,
         "P2 experimental frame-domain or output-clock drift");
    return result;
}
int main(int argc,char** argv) {
    try {
        bool quick=false;
        std::string path="P2_BATCHED_SAFETY_5MIN.tsv";
        for(int i=1;i<argc;++i) {
            const std::string_view arg{argv[i]};
            if(arg=="--quick")quick=true;
            else if(arg=="--output"&&i+1<argc)path=argv[++i];
            else throw std::runtime_error("usage: [--quick] [--output TSV]");
        }
        std::ofstream file{path,std::ios::trunc};
        require(bool(file),"cannot create P2 batched safety TSV");
        file<<"schema\trate_hz\tduration_s\tchannels\tscenario\tblock_frames"
            "\tbaseline_ms\tbatched_ms\tbaseline_eos_ms\tbatched_eos_ms"
            "\tcompared_frames\tlatency_frames\tmeasured_true_peak_linear\tresult\n";
        file<<std::fixed<<std::setprecision(6);
        const int seconds=quick?1:300;
        const auto scenarios=quick?7:3;
        const std::array<std::size_t,7> blocks{
            1U,7U,64U,511U,512U,1024U,4096U};
        const std::array<std::size_t,2> longBlocks{512U,4096U};
        for(int hz:{44100,48000,88200,96000,176400,192000}) {
            if(!quick && hz!=44100 && hz!=48000)continue;
            for(int channels:{1,2}) {
                if(!quick&&channels!=2)continue;
                for(int scenario=0;scenario<scenarios;++scenario) {
                    for(std::size_t i=0;i<(quick?blocks.size():longBlocks.size());++i) {
                        const auto blockFrames=quick?blocks[i]:longBlocks[i];
                        const auto r=run(hz,channels,seconds,scenario,blockFrames);
                        const char* label=scenario==0?"QUIET":
                            scenario==1?"NORMAL":
                            scenario==2?"HOT_DRIVE6":
                            scenario==3?"NYQUIST_DRIVE12":
                            scenario==4?"NYQUIST_DRIVE24":
                            scenario==5?"SPARSE_IMPULSES_DRIVE24":
                            "MIX_AMPLIFIED16_DRIVE18";
                        file<<"I2-P2-BATCHED-1\t"<<hz<<'\t'<<seconds<<'\t'
                            <<channels<<'\t'<<label<<'\t'<<blockFrames<<'\t'
                            <<r.baseline_ms<<'\t'<<r.batched_ms<<'\t'
                            <<r.baseline_eos_ms<<'\t'<<r.batched_eos_ms<<'\t'
                            <<r.compared_frames<<'\t'<<r.latency
                            <<'\t'<<r.measuredTruePeak<<"\tBIT_EXACT_AND_TP_SAFE\n";
                        require(bool(file),"P2 batched TSV write rejected");
                        if(quick ||blockFrames==longBlocks.front())
                            std::cout<<"P2 batch "<<hz<<"Hz c="<<channels
                                <<" "<<label<<" block="<<blockFrames
                                <<" baseline_ms="<<r.baseline_ms
                                <<" experimental_ms="<<r.batched_ms
                                <<" PCM_BIT_EXACT\n";
                    }
                }
            }
        }
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"P2 batched safety STOP: "<<e.what()<<'\n';
        return 1;
    }
}
