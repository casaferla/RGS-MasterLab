// RGS R1d-P2 research-only CPU / path probe.
// Explicit RESEARCH_P2_PERF_COUNTERS enables observational counters ONLY
// for this separate executable; no Product/callback/limiter code changes.
// Timings are OFFLINE Windows process timings, never WCET or dropout proof.
#include "p2_callback_engine.hpp"
#include <algorithm>
#include <array>
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

using rgsml::research::r1d_p2::CallbackEngine;
using rgsml::research::r1d_p2::Configuration;
using rgsml::research::r1d_p2::Frame;
using rgsml::research::r1d_p2::Status;
using Clock = std::chrono::steady_clock;
constexpr double kPi = 3.14159265358979323846264338327950288;
constexpr std::size_t kBlock = 512;

void require(bool v, const char* detail)
{
    if (!v) throw std::runtime_error(detail);
}
double elapsed_ms(Clock::time_point a, Clock::time_point b)
{
    return std::chrono::duration<double, std::milli>{b - a}.count();
}
std::vector<Frame> make_signal(int hz, int seconds)
{
    const auto n = static_cast<std::size_t>(hz) *
                   static_cast<std::size_t>(seconds);
    std::vector<Frame> pcm(n);
    std::uint32_t noise = 0xC001D00DU;
    for (std::size_t i = 0; i < n; ++i) {
        noise = 1664525U * noise + 1013904223U;
        const double unit = static_cast<double>((noise >> 8) & 0xFFFFFFU)
                          / static_cast<double>(0x1000000U) - 0.5;
        const double t = static_cast<double>(i) / hz;
        const double transient = (i % static_cast<std::size_t>(hz / 3) < 60)
            ? 0.47 : 0.0;
        const double l = 0.68 * std::sin(2 * kPi * 79 * t)
                       + 0.33 * std::sin(2 * kPi * 7189 * t)
                       + 0.15 * unit + transient;
        pcm[i] = {l, -0.61 * l + 0.28 * std::sin(2 * kPi * 1103 * t)};
    }
    return pcm;
}
struct Scenario {
    const char* label;
    double amplitude;
    double drive;
};
struct Measured {
    CallbackEngine::PerfCounters counters;
    double process_ms{};
    double tail_ms{};
    double p50_call_ms{};
    double p95_call_ms{};
    double max_call_ms{};
    std::size_t calls{};
};
Measured measure(const std::vector<Frame>& input, int hz,
                 Scenario scenario)
{
    Configuration cfg{hz,2,2,0.8912509381337456,scenario.drive};
    CallbackEngine engine;
    require(engine.prepare(cfg) == Status::ok, "P2 prepare failed");
    std::array<Frame,kBlock> output{};
    std::vector<double> timings;
    timings.reserve((input.size() + kBlock - 1) / kBlock);
    std::vector<Frame> scaled(kBlock);
    // Synthetic signal scaling and output capture are NOT timed.
    double ms = 0.0;
    for (std::size_t offset = 0; offset < input.size();) {
        const auto count = std::min(kBlock, input.size() - offset);
        for (std::size_t i = 0; i < count; ++i) {
            scaled[i] = {scenario.amplitude * input[offset + i][0],
                         scenario.amplitude * input[offset + i][1]};
        }
        const auto t0 = Clock::now();
        const auto status = engine.process(
            std::span<const Frame>{scaled.data(),count},
            std::span<Frame>{output.data(),count});
        const auto t1 = Clock::now();
        require(status == Status::ok, "P2 process failed");
        const auto time = elapsed_ms(t0,t1);
        timings.push_back(time);
        ms += time;
        offset += count;
    }
    std::vector<Frame> tail(engine.latency_frames());
    const auto t0 = Clock::now();
    const auto status = engine.finish(tail);
    const auto t1 = Clock::now();
    require(status == Status::ok, "P2 finalize failed");
    require(engine.input_frames() == input.size(), "input cursor mismatch");
    require(engine.output_frames() ==
        input.size() + engine.latency_frames(), "output cursor mismatch");
    const auto counters = engine.perf_counters();
    require(counters.totalCommittedFrames == input.size(),
            "P2 inner stream did not commit full programme");
    require(!timings.empty(), "no callback timings");
    std::sort(timings.begin(),timings.end());
    const auto quantile = [&](double q) {
        return timings[static_cast<std::size_t>(
            (timings.size()-1) * q)];
    };
    return Measured{
        counters,ms,elapsed_ms(t0,t1),
        quantile(0.50),quantile(0.95),timings.back(),timings.size()};
}
int main(int argc,char** argv)
{
    try {
        bool quick=false;
        std::string path="P2_CALLBACK_PATH_5MIN_BENCH.tsv";
        for (int i=1;i<argc;++i) {
            const std::string_view arg{argv[i]};
            if (arg=="--quick") quick=true;
            else if(arg=="--output" && i+1<argc) path=argv[++i];
            else throw std::runtime_error("Usage: [--quick] [--output PATH]");
        }
        std::ofstream file{path,std::ios::out|std::ios::trunc};
        require(bool(file),"cannot create callback path TSV");
        file<<"schema\trate_hz\tduration_s\tscenario\tamp_factor"
               "\tdrive_db\tpath\tpath_calls\tcommitted_frames"
               "\tfull_checks\tprojection_sweeps\tprocess_ms"
               "\tfinalize_ms\tblock_calls\tblock_p50_ms"
               "\tblock_p95_ms\tblock_max_ms\n";
        file<<std::fixed<<std::setprecision(6);
        const int seconds=quick?2:300;
        constexpr std::array<Scenario,3> scenarios{{
            {"QUIET",0.08,0.0},
            {"NORMAL",1.0,0.0},
            {"HOT_DRIVE6",1.0,6.0}
        }};
        for(int hz:{44100,48000}) {
            const auto input=make_signal(hz,seconds);
            const std::size_t n=quick?2:scenarios.size();
            for(std::size_t j=0;j<n;++j) {
                const auto cfg=scenarios[j];
                const auto r=measure(input,hz,cfg);
                std::uint64_t called=0;
                for(const auto value:r.counters.pathCalls)called+=value;
                require(called>0,"no committed P2 routes");
                for(std::size_t k=0;k<r.counters.pathCalls.size();++k) {
                    const auto p=static_cast<::research::Path>(k);
                    file<<"I2-PATH-PERF-1\t"<<hz<<'\t'<<seconds<<'\t'
                        <<cfg.label<<'\t'<<cfg.amplitude<<'\t'
                        <<cfg.drive<<'\t'<<::research::path_name(p)<<'\t'
                        <<r.counters.pathCalls[k]<<'\t'
                        <<r.counters.totalCommittedFrames<<'\t'
                        <<r.counters.totalChecks<<'\t'
                        <<r.counters.totalProjectionSweeps<<'\t'
                        <<r.process_ms<<'\t'<<r.tail_ms<<'\t'
                        <<r.calls<<'\t'<<r.p50_call_ms<<'\t'
                        <<r.p95_call_ms<<'\t'<<r.max_call_ms<<'\n';
                }
                file.flush();
                require(bool(file),"cannot write callback path TSV");
                std::cout<<"RATE "<<hz<<" FIXTURE "<<cfg.label
                         <<" ROUTES "<<called
                         <<" PROCESSED "<<r.counters.totalCommittedFrames
                         <<" CPU_PROCESS_MS "<<r.process_ms
                         <<" BLOCK_P95_MS "<<r.p95_call_ms<<'\n';
            }
        }
        std::cout<<"P2 path probe COMPLETE: "<<path
                 <<" (instrumented offline timing, not realtime WCET)\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"P2 path probe FAIL: "<<e.what()<<'\n';
        return 1;
    }
}
