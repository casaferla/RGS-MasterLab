// M22 PRIVATE ADVERSARIAL MODEL. This models causal state history and a
// FULL-PROGRAMME terminal dependency; it is NOT EQ/compressor/M-S/M17 DSP.
#include "m22_request_plan_candidate.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace rgsml::m22_private;
#define REQUIRE(c) do{if(!(c))throw std::runtime_error(std::string("M22_REPLAY_FAIL_LINE_")+std::to_string(__LINE__));}while(0)

struct ArtificialHistory {
    std::int64_t iir{0};
    std::int64_t envelope{0};
    std::int64_t mono_bass{0};
    std::int64_t terminal_hold{0};
    std::int64_t advance(std::int64_t x) noexcept {
        iir=(3*iir+x)/4;
        envelope=(7*envelope+std::llabs(iir))/8;
        mono_bass=(5*mono_bass+iir)/6;
        terminal_hold=(9*terminal_hold+envelope)/10;
        return iir+mono_bass-envelope-terminal_hold;
    }
};
static std::vector<std::int64_t> input() {
    std::vector<std::int64_t> v(1100);
    for(std::size_t n=0;n<v.size();++n)
        v[n]=static_cast<std::int64_t>((n*41U)%167U)-80;
    v[5]=5000; // long pre-region memory
    v[950]=9000; // changes complete-programme terminal witness after middle region
    return v;
}
// The intentionally simple global terminal "witness" is NOT a peak limiter
// or correction algorithm. It only detects illegal truncation of full-programme
// terminal preflight/verification to a PREVIEW region.
static std::int64_t global_terminal_witness(
    const std::vector<std::int64_t>& v,std::uint64_t seen_end) {
    std::int64_t peak=0;
    for(std::uint64_t i=0;i<seen_end;++i)peak=std::max(peak,std::llabs(v[static_cast<std::size_t>(i)]));
    return peak;
}
static std::vector<std::int64_t> run(
    const std::vector<std::int64_t>& source,
    Window window,std::size_t chunk,
    bool reset_at_region_start=false,bool truncate_terminal_at_region_end=false) {
    ArtificialHistory history;
    const auto witness=global_terminal_witness(
        source,truncate_terminal_at_region_end?window.end:source.size());
    std::vector<std::int64_t> published;
    published.reserve(static_cast<std::size_t>(window.end-window.first));
    const std::uint64_t begin=reset_at_region_start?window.first:0;
    for(std::uint64_t block=begin;block<window.end;) {
        const std::uint64_t end=std::min<std::uint64_t>(window.end,block+chunk);
        for(std::uint64_t i=block;i<end;++i) {
            const auto sample=history.advance(source[static_cast<std::size_t>(i)]);
            if(i>=window.first) published.push_back(sample+witness);
        }
        block=end;
    }
    return published;
}
int main() {
 try {
    const auto source=input();
    const Window fullWindow{0,source.size()};
    const auto full=run(source,fullWindow,73);
    REQUIRE(full==run(source,fullWindow,1));
    REQUIRE(full==run(source,fullWindow,4096));
    for(const auto window:{Window{0,188},Window{173,803},Window{851,1100}}) {
        const auto exact=run(source,window,1);
        REQUIRE(exact.size()==window.end-window.first);
        REQUIRE(exact==run(source,window,11));
        REQUIRE(exact==run(source,window,73));
        REQUIRE(exact==run(source,window,4096));
        REQUIRE(std::equal(exact.begin(),exact.end(),
                          full.begin()+static_cast<std::ptrdiff_t>(window.first)));
        if(window.first>0)
            REQUIRE(run(source,window,73,true)!=exact); // wrong local zero state
        if(window.end<source.size())
            REQUIRE(run(source,window,73,false,true)!=exact); // wrong local terminal scope
    }
    // M22 plan must prohibit treating the region end as true physical EOS.
    for(auto fs:{44100U,48000U,88200U,96000U,176400U,192000U}) {
        Intent in;
        in.source_identity="source";in.source_revision="s1";
        in.prepared_identity="prepared";in.prepared_revision="p1";
        in.chain_identity="chain";in.chain_revision=2;
        in.source_frames=source.size();in.mode=Mode::PREVIEW;
        in.window={173,803};in.processing_rate_hz=fs;in.target_rate_hz=fs;
        in.channels=1;in.terminal_limiter_signature="real-M17-later";
        in.terminal_limiter_parameters="frozen";
        in.execution_policy_signature="fp64";
        in.format=Format::FLOAT32;
        Failure err{};
        auto req=ResolvedRenderRequest::freeze(in,err);
        REQUIRE(req);
        auto plan=RenderExecutionPlan::preflight(*req,err);
        REQUIRE(plan);
        REQUIRE(plan->replay_input_first_frame()==0);
        REQUIRE(plan->logical_output_frames()==630);
        REQUIRE(plan->required_terminal_processing_end_frame()==source.size());
        REQUIRE(plan->terminal_requires_true_source_eos());
    }
    std::cout<<"M22_PRIVATE_ADVERSARIAL_REPLAY_AND_FULL_TERMINAL_SCOPE_PASS_NOT_REAL_DSP\n";
    return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
