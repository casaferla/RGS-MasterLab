#include "internal/true_peak_limiter_state.hpp"
#include <rgsml/dsp/true_peak_limiter_module.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/core/frame_time.hpp>
#include "../../oracles/true_peak_limiter/direct_oracle.hpp"

#include <QtTest/QTest>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <vector>
#include <string>

namespace rgsml::tests {
namespace {

using dsp::internal::TruePeakLimiterDetectorState;
using dsp::internal::LimiterDetectorQuarter;
using m17_oracle::Event;

class TruePeakLimiterCoreTest final : public QObject {
    Q_OBJECT
private slots:
    void frozenAssetsMatchIndependentSources();
    void signedTimelineAndFirstFrameImpulse();
    void directOracleMatchesAllPhaseAndTailEvents();
    void nonfiniteAndFinalizationAreAtomic();
    void controlTimingAndParamValidation();
    void controlFirstFramePrezeroAndHoldRelease();
    void controlFutureAttackAndTransactionalFailure();
    void audioStreamingOneLatencyAndTrueEos();
    void audioStreamingChunkPartitionAndStrictInput();
};

void TruePeakLimiterCoreTest::frozenAssetsMatchIndependentSources()
{
    using namespace dsp::internal;
    for (std::size_t i=0;i<48;++i) {
        QCOMPARE(std::bit_cast<std::uint64_t>(kLimiterAnchorPhaseMajor[i]),
                 std::bit_cast<std::uint64_t>(
                     analysis::internal::kTruePeakItu4xPhaseMajor[i]));
    }
    for (std::size_t i=0;i<16;++i) {
        QCOMPARE(std::bit_cast<std::uint64_t>(kLimiterMidpointFir16[i]),
                 std::bit_cast<std::uint64_t>(m17_oracle::kReferenceMidpoint[i]));
    }
    QCOMPARE(std::string(kLimiterAnchorAssetSha256),
             std::string(analysis::internal::kTruePeakItu4xAssetSha256));
}

void TruePeakLimiterCoreTest::signedTimelineAndFirstFrameImpulse()
{
    TruePeakLimiterDetectorState d{1};
    const auto real = d.push_programme_frame(4.0,0.0);
    QVERIFY(real);
    QCOMPARE((*real.value())[0].midpoint.tick, std::int64_t{-62});
    QVERIFY(!(*real.value())[0].preceding_anchor.valid);
    QCOMPARE((*real.value())[3].midpoint.tick, std::int64_t{-56});
    const auto tail = d.finalize_detector();
    QVERIFY(tail);
    QCOMPARE((*tail.value()).size(), std::size_t{59});
    QCOMPARE((*tail.value())[58].midpoint.tick, std::int64_t{62});
    QCOMPARE(d.programme_frames(), std::uint64_t{1});
    QCOMPARE(d.next_quarter_index(), std::int64_t{63});

    // First-frame anchor j=13 has physical tick -21 and is above
    // -12 dBTP; it changes q/density before controller tick 0.
    bool found = false;
    for (const auto& q : *tail.value()) {
        if (q.preceding_anchor.valid && q.preceding_anchor.tick == -21) {
            QCOMPARE(q.preceding_anchor.peak, 4.0*730.0/8192.0);
            found = true;
        }
    }
    QVERIFY(found);
}

void TruePeakLimiterCoreTest::directOracleMatchesAllPhaseAndTailEvents()
{
    const std::array<std::vector<double>,4> inputs{{
        {1.0}, {0.0,-2.0}, {0.0,0.0,0.1,0.0,-0.75,0.0,0.0},
        {0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.9,0.0,0.0,0.0,
         0.0,0.0,0.0,-0.5}
    }};
    for (const auto& left : inputs) {
        for (const std::uint32_t channels : {1U,2U}) {
            std::vector<double> right(left.size(),+0.0);
            if (channels==2) {
                for (std::size_t i=0;i<right.size();++i)
                    right[i] = -left[i]*0.375;
            }
            TruePeakLimiterDetectorState d{channels};
            std::vector<Event> real;
            auto collect=[&real](const LimiterDetectorQuarter& q) {
                if (q.preceding_anchor.valid)
                    real.push_back(Event{
                        q.preceding_anchor.tick,q.preceding_anchor.peak});
                if (q.midpoint.valid)
                    real.push_back(Event{q.midpoint.tick,q.midpoint.peak});
            };
            for (std::size_t i=0;i<left.size();++i) {
                const auto result=d.push_programme_frame(left[i],right[i]);
                QVERIFY(result);
                for(const auto& q:*result.value())collect(q);
            }
            const auto tail=d.finalize_detector();
            QVERIFY(tail);
            for (const auto& q:*tail.value())collect(q);
            const auto reference=m17_oracle::direct_events(left,
                channels==2?std::span<const double>(right)
                           :std::span<const double>{});
            QCOMPARE(real.size(),reference.size());
            for(std::size_t i=0;i<real.size();++i) {
                QCOMPARE(real[i].tick,reference[i].tick);
                QCOMPARE(real[i].peak,reference[i].peak);
                if(i>0)QVERIFY(real[i].tick>real[i-1].tick);
            }
            QCOMPARE(real.front().tick,std::int64_t{-62});
            QCOMPARE(real.back().tick,
                static_cast<std::int64_t>(8*left.size()+54));
        }
    }
}

void TruePeakLimiterCoreTest::nonfiniteAndFinalizationAreAtomic()
{
    TruePeakLimiterDetectorState d{2};
    QVERIFY(!d.finalize_detector());
    QVERIFY(!d.push_programme_frame(
        1.0,std::numeric_limits<double>::quiet_NaN()));
    QCOMPARE(d.programme_frames(),std::uint64_t{0});
    QCOMPARE(d.next_quarter_index(),std::int64_t{0});
    QVERIFY(d.push_programme_frame(1.0,-1.0));
    QVERIFY(!d.push_programme_frame(
        std::numeric_limits<double>::infinity(),0.0));
    QCOMPARE(d.programme_frames(),std::uint64_t{1});
    QCOMPARE(d.next_quarter_index(),std::int64_t{4});
    QVERIFY(d.finalize_detector());
    QVERIFY(!d.finalize_detector());
    QVERIFY(!d.push_programme_frame(0.0,0.0));
    d.reset();
    QCOMPARE(d.next_quarter_index(),std::int64_t{0});
    QVERIFY(d.push_programme_frame(0.0,0.0));
}


void TruePeakLimiterCoreTest::controlTimingAndParamValidation()
{
    using dsp::internal::TruePeakLimiterControllerState;
    for (const auto [fs,h,d,lookahead,hold] : std::array<std::array<std::int64_t,5>,6>{{
        {{44100,220,228,1760,1764}},
        {{48000,240,248,1920,1920}},
        {{88200,441,449,3528,3528}},
        {{96000,480,488,3840,3840}},
        {{176400,882,890,7056,7056}},
        {{192000,960,968,7680,7680}}
    }}) {
        auto state=TruePeakLimiterControllerState::create(fs,-1.0);
        QVERIFY(state);
        QCOMPARE(state.value()->next_tick(),std::int64_t{-62});
        QCOMPARE(state.value()->lookahead_frames(),h);
        QCOMPARE(state.value()->delay_frames(),d);
        QCOMPARE(state.value()->lookahead_ticks(),lookahead);
        QCOMPARE(state.value()->hold_ticks(),hold);
        QCOMPARE(state.value()->density(),0.0);
        QCOMPARE(state.value()->previous_reduction_db(),0.0);
        const auto sample=state.value()->advance(0.0,{});
        QVERIFY(sample);
        QCOMPARE(sample.value()->tick,std::int64_t{-62});
        QCOMPARE(sample.value()->required_db,0.0);
        QCOMPARE(sample.value()->reduction_db,0.0);
        QCOMPARE(sample.value()->gain,1.0);
        QCOMPARE(state.value()->next_tick(),std::int64_t{-61});
    }
    for (const auto fs : {0,8000,384000}) {
        QVERIFY(!TruePeakLimiterControllerState::create(fs,-1.0));
    }
    for (const auto invalid : {-13.0,0.01,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity()}) {
        QVERIFY(!TruePeakLimiterControllerState::create(48000,invalid));
    }
}

void TruePeakLimiterCoreTest::controlFirstFramePrezeroAndHoldRelease()
{
    using dsp::internal::TruePeakLimiterControllerState;
    auto state=TruePeakLimiterControllerState::create(44100,-12.0);
    QVERIFY(state);
    QVERIFY(state.value()->advance(+0.0,{}));
    double first_density=0.0;
    for (int m=-61;m<=0;++m) {
        // Exact PO adversarial impulse has anchor at j=13, m=-21
        // with |4*730/8192| > -12dBTP before tick zero.
        const double peak=m==-21 ? 4.0*730.0/8192.0 : 0.0;
        const auto result=state.value()->advance(peak,{});
        QVERIFY(result);
        if (m==-21) {
            QVERIFY(result.value()->required_db>0.0);
            QVERIFY(result.value()->density>0.0);
            QVERIFY(result.value()->reduction_db>0.0);
            first_density=result.value()->density;
            QCOMPARE(result.value()->hold_remaining,std::int64_t{1764});
        }
        if(m==0) {
            QVERIFY(result.value()->density>0.0);
            QVERIFY(result.value()->density<first_density);
            QVERIFY(result.value()->reduction_db>0.0);
            QVERIFY(result.value()->gain<1.0);
            QCOMPARE(result.value()->hold_remaining,std::int64_t{1743});
        }
    }
    state.value()->reset();
    QCOMPARE(state.value()->next_tick(),std::int64_t{-62});
    QCOMPARE(state.value()->density(),0.0);
    QCOMPARE(state.value()->previous_reduction_db(),0.0);
    QCOMPARE(state.value()->hold_remaining(),std::int64_t{0});
}

void TruePeakLimiterCoreTest::controlFutureAttackAndTransactionalFailure()
{
    using dsp::internal::TruePeakLimiterControllerState;
    using dsp::internal::LimiterDetectorEvent;
    auto state=TruePeakLimiterControllerState::create(48000,-1.0);
    QVERIFY(state);
    const auto m=state.value()->next_tick();
    const auto L=state.value()->lookahead_ticks();
    const double demandPeak=2.0;
    const LimiterDetectorEvent near{m+L/2,demandPeak,true};
    const LimiterDetectorEvent end{m+L,demandPeak,true};
    auto step=state.value()->advance(0.0,std::array{near,end});
    QVERIFY(step);
    const double required=20.0*std::log10(
        demandPeak/std::pow(10.0,-1.0/20.0));
    const double w=0.5*(1.0+std::cos(std::numbers::pi_v<double>*0.5));
    QVERIFY(std::abs(step.value()->attack_floor_db-required*w)<1e-12);
    QCOMPARE(step.value()->gain,std::pow(10.0,-step.value()->reduction_db/20.0));
    QCOMPARE(state.value()->next_tick(),m+1);
    const auto oldDensity=state.value()->density();
    const auto oldHold=state.value()->hold_remaining();
    const auto oldReduction=state.value()->previous_reduction_db();
    const LimiterDetectorEvent beyond{m+1+L+1,1.0,true};
    QVERIFY(!state.value()->advance(0.0,std::array{beyond}));
    QVERIFY(!state.value()->advance(
        std::numeric_limits<double>::quiet_NaN(),{}));
    QVERIFY(!state.value()->advance(0.0,std::array{
        LimiterDetectorEvent{m+3,1.0,true},
        LimiterDetectorEvent{m+2,1.0,true}}));
    QCOMPARE(state.value()->next_tick(),m+1);
    QCOMPARE(state.value()->density(),oldDensity);
    QCOMPARE(state.value()->hold_remaining(),oldHold);
    QCOMPARE(state.value()->previous_reduction_db(),oldReduction);
    const auto valid=state.value()->advance(0.0,{});
    QVERIFY(valid);
    QCOMPARE(valid.value()->tick,m+1);
    QCOMPARE(valid.value()->reduction_db,oldReduction);
}



[[nodiscard]] dsp::ModuleDescriptor frozenLimiterDescriptor()
{
    using namespace dsp;
    auto result=ModuleDescriptor::create(ModuleDescriptorSpec{
        "rgsml.dsp.true-peak-limiter",
        "rgsml.dsp.true-peak-limiter.display-name",
        {ModuleCategory::DYNAMICS,ModuleCategory::OUTPUT},
        {ProcessingStage::MASTER},{ChainSegment::TERMINAL},false,true,
        PlacementClass::TERMINAL_SLOT,TerminalSlot::FINAL_TRUE_PEAK_LIMITER,
        {},{},{},{},false,true,
        "1.0.0","rgsml.dsp.true-peak-limiter.parameters/1.0.0"});
    return *result.value();
}
[[nodiscard]] audio::AudioFormat limiterFormat(std::int64_t fs,bool stereo)
{
    return *audio::AudioFormat::create(
        *core::SampleRate::create(fs).value(),
        stereo?audio::ChannelLayout::STEREO_LR:audio::ChannelLayout::MONO_C).value();
}

[[nodiscard]] std::vector<std::vector<double>> runLimiterAudio(
    std::int64_t fs, bool stereo, const std::vector<double>& samples,
    double ceiling, const std::vector<std::size_t>& partitions)
{
    const auto format=limiterFormat(fs,stereo);
    const auto desc=frozenLimiterDescriptor();
    auto params=dsp::TruePeakLimiterParameters::create(ceiling);
    if (!params) return {};
    auto module=dsp::TruePeakLimiterModule::create(desc,*params.value());
    if (!module)return {};
    auto& mod=**module.value();
    auto req=mod.runtime_requirements(dsp::DspProcessSpec{
        format,audio::FrameDomainId::OUTPUT_RATE,
        *core::FrameCount::create(4096).value()});
    if (!req)return {};
    const auto d=req.value()->algorithmic_latency_frames.value();
    if (!mod.prepare(dsp::DspProcessSpec{format,audio::FrameDomainId::OUTPUT_RATE,
        *core::FrameCount::create(4096).value()}))return {};
    std::vector<std::vector<double>> raw(stereo?2U:1U);
    std::size_t offset=0,block=0;
    while(offset<samples.size()) {
        const auto count=std::min(
            samples.size()-offset,partitions[block++%partitions.size()]);
        auto in=audio::AudioBuffer::create(format,audio::FrameDomainId::OUTPUT_RATE,
            core::FrameIndex{static_cast<std::int64_t>(offset)},
            *core::FrameCount::create(static_cast<std::int64_t>(count)).value());
        auto out=audio::AudioBuffer::create(format,audio::FrameDomainId::OUTPUT_RATE,
            core::FrameIndex{static_cast<std::int64_t>(offset)},
            *core::FrameCount::create(static_cast<std::int64_t>(count)).value());
        if(!in || !out)return {};
        for(std::size_t ch=0;ch<raw.size();++ch) {
            auto plane=in.value()->mutable_view().channel(ch);
            if(!plane)return {};
            for(std::size_t i=0;i<count;++i) {
                const auto value=samples[offset+i];
                (*plane.value())[i]=(ch==0)?value:-value*0.375;
            }
        }
        auto range=core::FrameRange::create(
            core::FrameIndex{static_cast<std::int64_t>(offset)},
            core::FrameIndex{static_cast<std::int64_t>(offset+count)});
        if(!range)return {};
        if(!mod.process(in.value()->view(),out.value()->mutable_view(),
            dsp::DspProcessContext{*range.value(),offset==0,false}))return {};
        for(std::size_t ch=0;ch<raw.size();++ch) {
            auto plane=out.value()->view().channel(ch);
            if(!plane)return {};
            raw[ch].insert(raw[ch].end(),plane.value()->begin(),plane.value()->end());
        }
        offset+=count;
    }
    std::int64_t drain=0;
    while(drain<d) {
        const auto count=std::min<std::int64_t>(d-drain,partitions[block++%partitions.size()]);
        const auto first=static_cast<std::int64_t>(samples.size())+drain;
        auto out=audio::AudioBuffer::create(format,audio::FrameDomainId::OUTPUT_RATE,
            core::FrameIndex{first},*core::FrameCount::create(count).value());
        if(!out)return {};
        auto range=core::FrameRange::create(core::FrameIndex{first},
            core::FrameIndex{first+count});
        if(!range)return {};
        if(!mod.finalize(out.value()->mutable_view(),
            dsp::DspProcessContext{*range.value(),false,drain+count==d}))return {};
        for(std::size_t ch=0;ch<raw.size();++ch) {
            auto plane=out.value()->view().channel(ch);
            if(!plane)return {};
            raw[ch].insert(raw[ch].end(),plane.value()->begin(),plane.value()->end());
        }
        drain+=count;
    }
    for(auto& channel:raw) {
        if (channel.size()!=samples.size()+static_cast<std::size_t>(d))return {};
        channel.erase(channel.begin(),channel.begin()+d);
    }
    return raw;
}

void TruePeakLimiterCoreTest::audioStreamingOneLatencyAndTrueEos()
{
    for(const auto fs:{44100,48000,88200,96000,176400,192000}) {
        const auto d=(fs/200)+8;
        for(const auto n:{1,7,static_cast<int>(d-1),
                             static_cast<int>(d+2)}) {
            std::vector<double> sample(static_cast<std::size_t>(n),+0.0);
            sample[0]=4.0; // 44.1kHz first-frame PO startup impulse
            for (const bool stereo:{false,true}) {
                auto result=runLimiterAudio(fs,stereo,sample,-12.0,{7,1,3,31});
                QVERIFY2(!result.empty(),"Streaming PCM length/EOS contract failed");
                QCOMPARE(result.size(),stereo?std::size_t{2}:std::size_t{1});
                for(const auto& lane:result) {
                    QCOMPARE(lane.size(),sample.size());
                    for(const double v:lane)QVERIFY(std::isfinite(v));
                    QVERIFY(std::abs(lane.front())<4.0);
                }
            }
        }
    }
}

void TruePeakLimiterCoreTest::audioStreamingChunkPartitionAndStrictInput()
{
    constexpr auto fs=44100;
    std::vector<double> sample(274,+0.0);
    sample[0]=4.0;sample[11]=2.5;sample[270]=-0.45;
    const auto unchunked=runLimiterAudio(fs,true,sample,-12.0,{4096});
    const auto chunked=runLimiterAudio(fs,true,sample,-12.0,{1,3,7,17});
    QVERIFY(!unchunked.empty());
    QCOMPARE(unchunked.size(),chunked.size());
    for(std::size_t ch=0;ch<unchunked.size();++ch) {
        QCOMPARE(unchunked[ch].size(),chunked[ch].size());
        for(std::size_t i=0;i<unchunked[ch].size();++i)
            QCOMPARE(std::bit_cast<std::uint64_t>(unchunked[ch][i]),
                     std::bit_cast<std::uint64_t>(chunked[ch][i]));
    }
    std::vector<double> quiet{+0.0,-0.0,0.0625,-0.125};
    const auto neutral=runLimiterAudio(fs,false,quiet,0.0,{1});
    QVERIFY(!neutral.empty());
    for(std::size_t i=0;i<quiet.size();++i)
        QCOMPARE(std::bit_cast<std::uint64_t>(neutral[0][i]),
                 std::bit_cast<std::uint64_t>(quiet[i]));
    // Core explicitly rejects SOURCE_PROCESSING_RATE; M17 never borrows
    // M16 audition SRC or silently relabels the audio-domain timebase.
    const auto desc=frozenLimiterDescriptor();
    const auto params=*dsp::TruePeakLimiterParameters::create_default().value();
    auto module=dsp::TruePeakLimiterModule::create(desc,params);
    QVERIFY(module);
    const auto format=limiterFormat(fs,true);
    auto wrong=module.value()->get()->runtime_requirements(
        dsp::DspProcessSpec{format,audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            *core::FrameCount::create(32).value()});
    QVERIFY(!wrong);
}

} // namespace
} // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::TruePeakLimiterCoreTest)
#include "test_true_peak_limiter_core.moc"
