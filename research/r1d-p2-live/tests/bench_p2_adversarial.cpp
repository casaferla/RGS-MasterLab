// RGS R1d-P2 -- research-only adversarial controller stress.
// No user WAV, no product changes, no realtime device. Captures failures
// explicitly rather than treating a synthetic hostile signal as "PASS".
#include "p2_callback_engine.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using rgsml::research::r1d_p2::CallbackEngine;
using rgsml::research::r1d_p2::Configuration;
using rgsml::research::r1d_p2::Frame;
using rgsml::research::r1d_p2::Status;
using Clock=std::chrono::steady_clock;
constexpr std::size_t kBlock=512;
constexpr double kPi=3.14159265358979323846264338327950288;
void need(bool v,const char* what){if(!v)throw std::runtime_error(what);}
double ms(Clock::time_point a,Clock::time_point b) {
    return std::chrono::duration<double,std::milli>{b-a}.count();
}
const char* status_name(Status rc) {
    switch(rc) {
    case Status::ok:return "OK";
    case Status::invalid_configuration:return "INVALID_CONFIGURATION";
    case Status::incompatible_fp:return "INCOMPATIBLE_FP";
    case Status::not_prepared:return "NOT_PREPARED";
    case Status::invalid_pcm:return "INVALID_PCM";
    case Status::output_size_mismatch:return "OUTPUT_SIZE_MISMATCH";
    case Status::safety_failure:return "SAFETY_FAILURE";
    case Status::fifo_overflow:return "FIFO_OVERFLOW";
    case Status::fifo_underflow:return "FIFO_UNDERFLOW";
    case Status::wrong_lifecycle:return "WRONG_LIFECYCLE";
    case Status::frame_limit:return "FRAME_LIMIT";
    }
    return "UNKNOWN_STATUS";
}
enum class Shape { MIXED, NYQUIST_ALTERNATION, IMPULSE_TRAIN };
struct Fixture {
    const char* name;
    Shape shape;
    double amplitude;
    double driveDb;
};
constexpr std::array<Fixture,5> kFixtures{{
    {"MIX_NORMAL",Shape::MIXED,1.0,0.0},
    {"MIX_AMPLIFIED4_DRIVE12",Shape::MIXED,4.0,12.0},
    {"MIX_AMPLIFIED16_DRIVE18",Shape::MIXED,16.0,18.0},
    {"NYQUIST_DRIVE24",Shape::NYQUIST_ALTERNATION,1.0,24.0},
    {"SPARSE_IMPULSES_DRIVE24",Shape::IMPULSE_TRAIN,20.0,24.0}
}};
Frame sample(int hz,std::int64_t i,Fixture fixture) {
    const auto t=static_cast<double>(i)/hz;
    if(fixture.shape==Shape::NYQUIST_ALTERNATION) {
        const double v=fixture.amplitude*(i%2?1.0:-1.0);
        return {v,-0.75*v};
    }
    if(fixture.shape==Shape::IMPULSE_TRAIN) {
        const auto period=static_cast<std::int64_t>(hz/7);
        const double v=(i%period==0)?fixture.amplitude:0.0;
        return {v,(i/period)%2?-v:v};
    }
    const double impulse=i%(hz/3)<60?0.47:0.;
    const double l=fixture.amplitude*(
        0.68*std::sin(2*kPi*79*t)
       +0.33*std::sin(2*kPi*7189*t)+impulse);
    return {l,-0.61*l+
        fixture.amplitude*0.28*std::sin(2*kPi*1103*t)};
}
void run_fixture(std::ofstream& out,int hz,int seconds,Fixture fixture)
{
    Configuration cfg{hz,2,2,0.8912509381337456,fixture.driveDb};
    CallbackEngine engine;
    const auto p=engine.prepare(cfg);
    need(p==Status::ok,"adversarial prepare rejected");
    std::array<Frame,kBlock> in{};
    std::array<Frame,kBlock> result{};
    std::vector<double> perBlock;
    perBlock.reserve((static_cast<std::size_t>(hz)*seconds+kBlock-1)/kBlock);
    double processingMs=0;
    Status state=Status::ok;
    std::int64_t processed=0;
    const std::int64_t total=static_cast<std::int64_t>(hz)*seconds;
    for(std::int64_t offset=0;offset<total;) {
        const auto size=static_cast<std::size_t>(
            std::min<std::int64_t>(kBlock,total-offset));
        for(std::size_t i=0;i<size;++i)
            in[i]=sample(hz,offset+static_cast<std::int64_t>(i),fixture);
        const auto t0=Clock::now();
        state=engine.process(
            std::span<const Frame>{in.data(),size},
            std::span<Frame>{result.data(),size});
        const auto t1=Clock::now();
        const auto duration=ms(t0,t1);
        processingMs+=duration;
        perBlock.push_back(duration);
        if(state!=Status::ok)break; // fail-closed, no further calls
        processed+=static_cast<std::int64_t>(size);
        offset+=static_cast<std::int64_t>(size);
    }
    double tailMs=0;
    if(state==Status::ok) {
        std::vector<Frame> tail(engine.latency_frames());
        const auto t0=Clock::now();
        state=engine.finish(tail);
        tailMs=ms(t0,Clock::now());
    }
    const auto counters=engine.perf_counters();
    std::sort(perBlock.begin(),perBlock.end());
    const auto quantile=[&](double q) {
        return perBlock.empty()?0.:perBlock[
            static_cast<std::size_t>((perBlock.size()-1)*q)];
    };
    std::uint64_t routeCount=0;
    for(const auto c:counters.pathCalls)routeCount+=c;
    for(std::size_t index=0;index<counters.pathCalls.size();++index) {
        out<<"I2-ADVERSARIAL-1\t"<<hz<<'\t'<<seconds<<'\t'
           <<fixture.name<<'\t'<<fixture.amplitude<<'\t'
           <<fixture.driveDb<<'\t'<<status_name(state)<<'\t'
           <<processed<<'\t'<<total<<'\t'
           <<::research::path_name(static_cast<::research::Path>(index))
           <<'\t'<<counters.pathCalls[index]<<'\t'
           <<counters.totalChecks<<'\t'
           <<counters.totalProjectionSweeps<<'\t'
           <<routeCount<<'\t'<<processingMs<<'\t'<<tailMs<<'\t'
           <<perBlock.size()<<'\t'<<quantile(0.50)<<'\t'
           <<quantile(0.95)<<'\t'
           <<(perBlock.empty()?0.:perBlock.back())<<'\n';
    }
    out.flush();
    need(bool(out),"adversarial data write failed");
    std::cout<<"ADVERSARIAL "<<hz<<' '<<fixture.name
             <<" RESULT="<<status_name(state)
             <<" committed="<<processed<<"/"<<total
             <<" repaired_calls="<<counters.pathCalls[5]
             <<" projection_sweeps="<<counters.totalProjectionSweeps
             <<" block_max_ms="<<(perBlock.empty()?0.:perBlock.back())
             <<'\n';
    // A non-OK result is an EXPLICIT failed-safety fixture in the report.
    // This research probe does not hide it, but must still collect other
    // independent fixtures in order to characterize the failure envelope.
}
int main(int argc,char** argv)
{
    try {
        bool quick=false,full=false;
        std::string path="P2_ADVERSARIAL_PATH_BENCH.tsv";
        for(int i=1;i<argc;++i) {
            const std::string_view a{argv[i]};
            if(a=="--quick") quick=true;
            else if(a=="--full") full=true;
            else if(a=="--output"&&i+1<argc)path=argv[++i];
            else throw std::runtime_error(
                "usage: [--quick|--full] [--output PATH]");
        }
        need(!(quick&&full),"quick/full are mutually exclusive");
        const int seconds=quick?1:(full?300:30);
        std::ofstream out{path,std::ios::out|std::ios::trunc};
        need(bool(out),"cannot open adversarial output");
        out<<"schema\trate_hz\tduration_s\tfixture\tamp_factor"
              "\tdrive_db\tstatus\tframes_accepted\tframes_requested"
              "\tpath\tpath_calls\tfull_checks\tprojection_sweeps"
              "\ttotal_path_calls\tprocess_ms\tfinalize_ms\tblock_calls"
              "\tblock_p50_ms\tblock_p95_ms\tblock_max_ms\n";
        out<<std::fixed<<std::setprecision(6);
        for(int fs:{44100,48000}) {
            for(const auto& f:kFixtures)run_fixture(out,fs,seconds,f);
        }
        std::cout<<"ADVERSARIAL_PROBE_COMPLETE: "<<path
                 <<" (offline; NEVER evidence of WASAPI callback safety)\n";
        return 0;
    } catch(const std::exception& ex) {
        std::cerr<<"ADVERSARIAL_PROBE_ERROR: "<<ex.what()<<'\n';
        return 1;
    }
}
