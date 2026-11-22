#include "dynamic_eq_activity_history.hpp"
#include <rgsml/dsp/dynamic_eq_parameters.hpp>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <span>
#include <vector>

using namespace rgsml;
using namespace rgsml::dsp;
using namespace rgsml::dsp::internal;
using namespace rgsml::research::dynamic_ui;

static DynamicEqParameters fixture() {
    DynamicEqBandSpec b;
    auto id=core::Uuid::parse("11111111-1111-4111-8111-111111111111");
    assert(id);b.band_id=*id.value();
    b.control.range_db=-8;
    b.control.threshold_dbfs=-50;
    const auto part=DynamicEqBandParameters::create(b);
    assert(part);
    const auto whole=DynamicEqParameters::create({*part.value()});
    assert(whole);
    return *whole.value();
}
int main() {
    for(auto fs:{44100U,48000U,96000U,192000U}) {
        auto engine=C01FrameEngine::create(fixture(),fs,1);
        assert(engine);
        C01ActivityHistoryCollector history(fs,0,300);
        constexpr std::size_t block=509;
        const std::size_t N=static_cast<std::size_t>(fs)*3U;
        std::vector<C01FrameActivitySample> scratch(block);
        std::vector<double> input(block),out(block);
        for(std::size_t at=0;at<N;) {
            const auto n=std::min(block,N-at);
            for(std::size_t i=0;i<n;++i) {
                const auto t=static_cast<double>(at+i);
                input[i]=0.32*std::sin(2*std::numbers::pi*500*t/fs);
                if((at+i)%1817==0) input[i]+=0.45;
            }
            assert(engine.value()->process_block(
                std::span<const double>(input.data(),n),{},
                std::span<double>(out.data(),n),{},&history,at,scratch));
            at+=n;
        }
        assert(history.status()==C01HistoryStatus::OK);
        assert(history.next_frame()==N);
        assert(history.bucket_count()==150U);
        // Unfinished history is not yet a trustworthy PCM sidecar.
        assert(history.visible_window(N,fs*5U).empty());
        assert(history.finish());
        const auto full=history.visible_window(N,fs*5U);
        assert(full.size()==150U);
        assert(full.front().frame_begin==0);
        assert(full.back().frame_end_exclusive==N);
        double dynamic_max=0;
        for(const auto& b:full) {
            assert(b.band_count==1);
            assert(b.bands[0].gain_min_db<=b.bands[0].gain_last_db);
            assert(b.bands[0].gain_last_db<=b.bands[0].gain_max_db);
            assert(b.bands[0].detector_power_max>=b.bands[0].detector_power_last);
            dynamic_max=std::max(dynamic_max,
                                 std::abs(b.bands[0].gain_last_db));
        }
        assert(dynamic_max>0);
        // Only fully played buckets: no knowledge from the future.
        assert(history.visible_window(fs/50U-1,fs*5U).empty());
        assert(history.visible_window(fs/50U,fs*5U).size()==1);
        // Seek back and forward: same immutable track, no new DSP pass.
        assert(history.visible_window(fs,fs/50U).size()==1);
        assert(history.visible_window(N,fs/50U).size()==1);
    }
    {
        // An observation discontinuity fails only the telemetry, not C01.
        C01ActivityHistoryCollector history(48000,0,3);
        C01FrameActivitySample sample;
        sample.band_count=1;
        auto uid=core::Uuid::parse("11111111-1111-4111-8111-111111111111");
        assert(uid);sample.bands[0].band_id=*uid.value();
        std::array<C01FrameActivitySample,1> one{sample};
        history.on_successful_block(1,one);
        assert(history.status()==C01HistoryStatus::FRAME_DISCONTINUITY);
        assert(!history.finish());
    }
    {
        // No allocation growth: a full history fails closed.
        C01ActivityHistoryCollector history(48000,0,1);
        C01FrameActivitySample sample;
        sample.band_count=1;
        auto uid=core::Uuid::parse("11111111-1111-4111-8111-111111111111");
        assert(uid);sample.bands[0].band_id=*uid.value();
        std::vector<C01FrameActivitySample> block(961,sample);
        history.on_successful_block(0,block);
        assert(history.status()==C01HistoryStatus::CAPACITY_EXCEEDED);
        assert(!history.finish());
    }
    std::cout<<"C01 activity 20ms full history: PASS (4 rates/seek/capacity)\n";
}
