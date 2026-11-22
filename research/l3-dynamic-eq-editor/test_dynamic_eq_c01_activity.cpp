// Authentic C01 observation parity — isolated post-L3 Dynamic EQ research.
// This builds the REAL production C01 engine and its frozen coefficients.
// It is NOT a Qt UI test and does not edit Product/AutoMatch.
#include "../../dsp/src/internal/dynamic_eq_c01_engine.hpp"
#include <rgsml/dsp/dynamic_eq_parameters.hpp>
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <numbers>
#include <span>
#include <vector>

using namespace rgsml;
using namespace rgsml::dsp;
using namespace rgsml::dsp::internal;

static core::Uuid uuid(const char* id) {
    auto parsed=core::Uuid::parse(id);
    assert(parsed);
    return *parsed.value();
}
static DynamicEqParameters params(bool stereo) {
    DynamicEqBandSpec a;
    a.band_id=uuid("11111111-1111-4111-8111-111111111111");
    a.control.base_gain_db=1.5;
    a.control.range_db=-8.0;
    a.control.threshold_dbfs=-60.0;
    a.routing=EqRouting::STEREO;
    auto aa=DynamicEqBandParameters::create(a); assert(aa);
    if(!stereo) {
        auto p=DynamicEqParameters::create({*aa.value()}); assert(p);
        return *p.value();
    }
    DynamicEqBandSpec b;
    b.band_id=uuid("22222222-2222-4222-8222-222222222222");
    b.filter_type=DynamicEqFilterType::LOW_SHELF;
    b.shape=DynamicEqShelfShape{800.0,0.8};
    b.routing=EqRouting::LEFT;
    b.control.range_db=5.0;
    b.control.threshold_dbfs=-50.0;
    auto bb=DynamicEqBandParameters::create(b); assert(bb);
    auto p=DynamicEqParameters::create({*aa.value(),*bb.value()}); assert(p);
    return *p.value();
}
struct EvidenceSink final:IC01ActivitySink {
    std::uint64_t next_frame{0};
    std::size_t blocks{0},frames{0};
    bool seen_nonzero{false},seen_tagged_zero{false};
    void on_successful_block(
        std::uint64_t frame,
        std::span<const C01FrameActivitySample> observed) noexcept override {
        assert(frame==next_frame);
        assert(!observed.empty());
        for(const auto& s:observed) {
            assert(s.band_count==expected_bands);
            assert(s.bands[0].band_id==uuid_first);
            assert(s.bands[0].routing==EqRouting::STEREO);
            for(std::size_t i=0;i<s.band_count;++i) {
                const auto& b=s.bands[i];
                assert(std::isfinite(b.linked_power) && b.linked_power>=0.0);
                assert(std::isfinite(b.unsigned_target_db));
                assert(std::isfinite(b.signed_target_dynamic_gain_db));
                assert(std::isfinite(b.applied_dynamic_gain_db));
                assert(std::isfinite(b.effective_gain_db));
                assert(b.has_detector_level == (b.linked_power>0.0));
                if(b.has_detector_level)
                    assert(std::isfinite(b.detector_level_dbfs));
                else {
                    assert(b.detector_level_dbfs==0.0);
                    seen_tagged_zero=true;
                }
                assert(b.unsigned_target_db>=0.0);
                assert(std::abs(b.applied_dynamic_gain_db)<=9.000001);
                if(std::abs(b.applied_dynamic_gain_db)>1e-8)
                    seen_nonzero=true;
            }
            if(s.band_count==2) {
                assert(s.bands[1].band_id==uuid_second);
                assert(s.bands[1].routing==EqRouting::LEFT);
                assert(s.bands[1].signed_target_dynamic_gain_db>=0.0);
                assert(s.bands[0].signed_target_dynamic_gain_db<=0.0);
            }
        }
        next_frame+=observed.size();
        frames+=observed.size();
        ++blocks;
    }
    std::size_t expected_bands{1};
    core::Uuid uuid_first=uuid("11111111-1111-4111-8111-111111111111");
    core::Uuid uuid_second=uuid("22222222-2222-4222-8222-222222222222");
};
static bool identical(const std::vector<double>& a,
                      const std::vector<double>& b) {
    return a.size()==b.size()
        && std::memcmp(a.data(),b.data(),a.size()*sizeof(double))==0;
}
int main() {
    for(auto fs:{44100U,48000U,96000U,192000U})
        for(int channel:{1,2}) {
            const auto p=params(channel==2);
            auto regular=C01FrameEngine::create(p,fs,channel);
            auto observed=C01FrameEngine::create(p,fs,channel);
            assert(regular&&observed);
            constexpr std::size_t N=8201;
            constexpr std::size_t BLOCK=401;
            std::vector<double> l(N),r(channel==2?N:0U);
            for(std::size_t i=0;i<N;++i) {
                const auto t=static_cast<double>(i);
                l[i]=0.34*std::sin(2.0*std::numbers::pi*1011.0*t/fs);
                if(i%1291==0)l[i]+=0.41;
                if(channel==2)r[i]=0.22*std::cos(
                    2.0*std::numbers::pi*557.0*t/fs);
            }
            std::vector<double> plainL(N),plainR(r.size());
            std::vector<double> seenL(N),seenR(r.size());
            std::vector<C01FrameActivitySample> scratch(BLOCK);
            EvidenceSink sink;
            sink.expected_bands=channel==2?2U:1U;
            std::size_t at=0;
            while(at<N) {
                const auto n=std::min(BLOCK,N-at);
                const auto lr=std::span<const double>(l).subspan(at,n);
                const auto rr=channel==2
                    ?std::span<const double>(r).subspan(at,n)
                    :std::span<const double>{};
                auto ol=std::span<double>(plainL).subspan(at,n);
                auto orr=channel==2
                    ?std::span<double>(plainR).subspan(at,n)
                    :std::span<double>{};
                auto el=std::span<double>(seenL).subspan(at,n);
                auto er=channel==2
                    ?std::span<double>(seenR).subspan(at,n)
                    :std::span<double>{};
                assert(regular.value()->process_block(lr,rr,ol,orr));
                assert(observed.value()->process_block(
                    lr,rr,el,er,&sink,at,scratch));
                assert(regular.value()->states()==observed.value()->states());
                at+=n;
            }
            assert(sink.frames==N && sink.next_frame==N);
            assert(sink.blocks>1 && sink.seen_nonzero);
            assert(identical(plainL,seenL));
            if(channel==2)assert(identical(plainR,seenR));
            // Invalid input: neither state, output, nor evidence advances.
            const auto previous=observed.value()->states();
            const auto old_frames=sink.frames;
            std::array<double,3> bad{
                0.2,std::numeric_limits<double>::quiet_NaN(),0.1};
            std::array<double,3> bad_r{0.3,0.1,0.1};
            std::array<double,3> out{77,88,99},out_r{11,22,33};
            assert(!observed.value()->process_block(
                bad,channel==2?std::span<const double>(bad_r)
                              :std::span<const double>{},
                out,channel==2?std::span<double>(out_r)
                              :std::span<double>{},
                &sink,N,scratch));
            assert(observed.value()->states()==previous);
            assert(out[0]==77 && out[1]==88 && out[2]==99);
            assert(sink.frames==old_frames);
            // Insufficient preallocation or overflow: fail before audio.
            std::array<C01FrameActivitySample,1> tiny{};
            bad[1]=0.2;
            assert(!observed.value()->process_block(
                bad,channel==2?std::span<const double>(bad_r)
                              :std::span<const double>{},
                out,channel==2?std::span<double>(out_r)
                              :std::span<double>{},
                &sink,N,tiny));
            assert(!observed.value()->process_block(
                bad,channel==2?std::span<const double>(bad_r)
                              :std::span<const double>{},
                out,channel==2?std::span<double>(out_r)
                              :std::span<double>{},
                &sink,std::numeric_limits<std::uint64_t>::max()-1,scratch));
            assert(observed.value()->states()==previous);
            assert(sink.frames==old_frames);
        }
    std::cout<<"C01 authentic per-band activity: PASS (4 Fs x mono/stereo)\n";
}
