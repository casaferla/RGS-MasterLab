#include "transient_c00_activity_sink.hpp"
#include <rgsml/dsp/transient_shaper_parameters.hpp>
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <numbers>
#include <span>
#include <vector>

using rgsml::dsp::TransientShaperParameters;
using rgsml::dsp::internal::TransientShaperC00Engine;
using namespace rgsml::research::transient_ui;

static std::vector<double> programme(std::size_t n, std::uint32_t fs) {
    std::vector<double> x(n);
    for (std::size_t i=0; i<n; ++i) {
        const double t=static_cast<double>(i);
        x[i]=0.31*std::sin(2.0*std::numbers::pi*190.0*t/fs)
            +0.21*std::sin(2.0*std::numbers::pi*2201.0*t/fs);
        if (i%901==0) x[i]+=0.4;
    }
    return x;
}
static bool bits_equal(std::span<const double> a, std::span<const double> b) {
    return a.size()==b.size()
        && std::memcmp(a.data(),b.data(),a.size_bytes())==0;
}

int main() {
    const auto p=TransientShaperParameters::create(8.0,-5.0);
    assert(p);
    for (std::uint32_t fs : {44100U,48000U,96000U,192000U})
        for (std::uint32_t channels : {1U,2U}) {
            constexpr std::size_t N=10347;
            auto normal=TransientShaperC00Engine::create(
                *p.value(),fs,channels,4096);
            auto observed=TransientShaperC00Engine::create(
                *p.value(),fs,channels,4096,true);
            assert(normal && observed);
            TransientActivityCollector collector(fs,0,20);
            TransientC00ActivitySink sink(collector);
            const auto left=programme(N,fs);
            auto right=programme(N,fs);
            std::reverse(right.begin(),right.end());
            std::vector<double> refL(N),refR(channels==2U?N:0U);
            std::vector<double> gotL(N),gotR(channels==2U?N:0U);
            std::size_t offset=0,step=0;
            constexpr std::array<std::size_t,7> sizes{{
                1,11,63,257,1024,2048,4096}};
            while (offset<N) {
                const auto n=std::min(sizes[step++%sizes.size()],N-offset);
                const auto l=std::span<const double>(left.data()+offset,n);
                const auto r=channels==2U
                    ?std::span<const double>(right.data()+offset,n)
                    :std::span<const double>{};
                auto nL=std::span<double>(refL.data()+offset,n);
                auto nR=channels==2U
                    ?std::span<double>(refR.data()+offset,n)
                    :std::span<double>{};
                auto oL=std::span<double>(gotL.data()+offset,n);
                auto oR=channels==2U
                    ?std::span<double>(gotR.data()+offset,n)
                    :std::span<double>{};
                assert(normal.value()->process_block(l,r,nL,nR));
                assert(observed.value()->process_block(
                    l,r,oL,oR,&sink,offset));
                assert(normal.value()->state()==observed.value()->state());
                assert(bits_equal(nL,oL));
                if (channels==2U)assert(bits_equal(nR,oR));
                offset+=n;
            }
            assert(sink.observed_frames()==N);
            assert(!sink.observation_failed());
            assert(sink.last_attack_db() == observed.value()->state().g_attack_db);
            assert(sink.last_sustain_db() == observed.value()->state().g_sustain_db);
            // Fully completed buckets are returned; the trailing partial
            // bucket is made visible only after an explicit successful finish.
            const auto before=collector.completed_buckets();
            assert(before==N/(fs/50U));
            assert(collector.finish());
            assert(collector.completed_buckets()==before+1);
            const auto window=collector.visible_window(N,fs*5U);
            assert(window.size()==collector.completed_buckets());
            assert(!window.empty());
            assert(window.back().frame_end_exclusive==N);
            assert(window.back().attack_last_db==sink.last_attack_db());
            assert(window.back().sustain_last_db==sink.last_sustain_db());
        }
    // Unobserved C00 never allocates activity scratch and refuses a tap
    // without altering output, state or the sink.
    {
        auto p1=TransientShaperParameters::create(5.0,-2.0);
        assert(p1);
        auto engine=TransientShaperC00Engine::create(*p1.value(),48000,1,32);
        assert(engine);
        TransientActivityCollector c(48000);
        TransientC00ActivitySink sink(c);
        std::array<double,2> input{{0.3,0.2}},output{{77.0,88.0}};
        const auto before=engine.value()->state();
        assert(!engine.value()->process_block(
            input,{},output,{},&sink,0));
        assert(engine.value()->state()==before);
        assert(output[0]==77.0 && output[1]==88.0);
        assert(sink.observed_frames()==0);
    }
    // A rejected audio block must not publish speculative observations.
    {
        auto p1=TransientShaperParameters::create(5.0,-2.0);
        assert(p1);
        auto engine=TransientShaperC00Engine::create(
            *p1.value(),48000,1,32,true);
        assert(engine);
        TransientActivityCollector c(48000);
        TransientC00ActivitySink sink(c);
        std::array<double,2> input{{0.3,
            std::numeric_limits<double>::quiet_NaN()}};
        std::array<double,2> output{{77.0,88.0}};
        const auto before=engine.value()->state();
        assert(!engine.value()->process_block(
            input,{},output,{},&sink,0));
        assert(engine.value()->state()==before);
        assert(output[0]==77.0 && output[1]==88.0);
        assert(sink.observed_frames()==0);
        input[1]=0.2;
        assert(engine.value()->process_block(
            input,{},output,{},&sink,0));
        assert(sink.observed_frames()==2);
    }
    // A failed telemetry observer never changes committed PCM or C00 state.
    {
        auto p1=TransientShaperParameters::create(7.0,2.0);
        assert(p1);
        auto engine=TransientShaperC00Engine::create(
            *p1.value(),48000,1,64,true);
        assert(engine);
        TransientActivityCollector c(48000,5,1);
        TransientC00ActivitySink sink(c);
        std::array<double,2> input{{0.5,0.2}},output{{0.0,0.0}};
        assert(engine.value()->process_block(
            input,{},output,{},&sink,0));
        assert(sink.observation_failed());
        assert(std::isfinite(output[0]));
        assert(engine.value()->state().f>0.0);
    }
    // Overflowing observation frame domain is rejected before processing.
    {
        auto p1=TransientShaperParameters::create(7.0,2.0);
        assert(p1);
        auto engine=TransientShaperC00Engine::create(
            *p1.value(),48000,1,64,true);
        assert(engine);
        TransientActivityCollector c(48000);
        TransientC00ActivitySink sink(c);
        std::array<double,2> input{{0.5,0.2}},output{{2.0,3.0}};
        assert(!engine.value()->process_block(
            input,{},output,{},&sink,
            std::numeric_limits<std::uint64_t>::max()-1U));
        assert(output[0]==2.0 && output[1]==3.0);
        assert(sink.observed_frames()==0);
    }
    std::cout << "C00 authentic observation parity: PASS (four rates; mono/stereo)\n";
}
