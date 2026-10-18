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
#include <cfenv>
#include <span>
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
    void appliedGainWitnessReconstructsPcmAndIsNoninvasive();
    void strictFpRuntimeMode();
    void failedCallsAreAtomicAndCannotPublishWitness();
    void highRateChunkBoundednessAndBitIdentity();
    void controlAllCeilingsLookaheadBoundarySixRates();
    void oneSecond192kAnd44100StressWithBoundedCheckpoints();
    void adversarialStereoDetectorEightXOracle();
    void absoluteFrameOriginAndLinkedStereoWitness();
    void directEightXIndependentControllerReplayMatchesAppliedGain();
    void absoluteFrameLimitRejectsAtomicallyAcrossSixRates();
    void inPlaceBufferAliasIsBitIdenticalAndFailureAtomic();
    void resetAndReprepareOwnDistinctResourcesAcrossSixRates();
    void midDrainInvalidCallsPreservePcmAndGainAcrossSixRates();
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

struct AppliedGainTestCapture final {
    std::vector<dsp::TruePeakLimiterAppliedGainFrame> records;
    static void accept(void* context,
        std::span<const dsp::TruePeakLimiterAppliedGainFrame> batch) noexcept
    {
        auto& records=static_cast<AppliedGainTestCapture*>(context)->records;
        records.insert(records.end(),batch.begin(),batch.end());
    }
};
[[nodiscard]] std::vector<std::vector<double>> runLimiterAudio(
    std::int64_t fs, bool stereo, const std::vector<double>& samples,
    double ceiling, const std::vector<std::size_t>& partitions,
    std::vector<dsp::TruePeakLimiterAppliedGainFrame>* gainWitness = nullptr,
    std::int64_t origin=0, std::span<const double> rightOverride={},
    std::vector<std::size_t>* checkpointBytes = nullptr)
{
    if(!rightOverride.empty() && (!stereo || rightOverride.size()!=samples.size()))
        return {};
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
    AppliedGainTestCapture capture;
    if(gainWitness) {
        capture.records.reserve(samples.size()); // noexcept callback cannot allocate
        mod.set_applied_gain_sink(&AppliedGainTestCapture::accept,&capture);
    }
    std::vector<std::vector<double>> raw(stereo?2U:1U);
    std::size_t offset=0,block=0;
    while(offset<samples.size()) {
        const auto count=std::min(
            samples.size()-offset,partitions[block++%partitions.size()]);
        auto in=audio::AudioBuffer::create(format,audio::FrameDomainId::OUTPUT_RATE,
            core::FrameIndex{origin+static_cast<std::int64_t>(offset)},
            *core::FrameCount::create(static_cast<std::int64_t>(count)).value());
        auto out=audio::AudioBuffer::create(format,audio::FrameDomainId::OUTPUT_RATE,
            core::FrameIndex{origin+static_cast<std::int64_t>(offset)},
            *core::FrameCount::create(static_cast<std::int64_t>(count)).value());
        if(!in || !out)return {};
        for(std::size_t ch=0;ch<raw.size();++ch) {
            auto plane=in.value()->mutable_view().channel(ch);
            if(!plane)return {};
            for(std::size_t i=0;i<count;++i) {
                const auto value=samples[offset+i];
                (*plane.value())[i]=(ch==0)?value
                    :(rightOverride.empty()?-value*0.375:rightOverride[offset+i]);
            }
        }
        auto range=core::FrameRange::create(
            core::FrameIndex{origin+static_cast<std::int64_t>(offset)},
            core::FrameIndex{origin+static_cast<std::int64_t>(offset+count)});
        if(!range)return {};
        if(!mod.process(in.value()->view(),out.value()->mutable_view(),
            dsp::DspProcessContext{*range.value(),offset==0,false}))return {};
        if(checkpointBytes) {
            const auto state=mod.runtime_checkpoint();
            if(!state)return {};
            checkpointBytes->push_back(state.value()->payload.size());
        }
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
        const auto first=origin+static_cast<std::int64_t>(samples.size())+drain;
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
    if(gainWitness) *gainWitness=std::move(capture.records);
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


void TruePeakLimiterCoreTest::strictFpRuntimeMode()
{
    // Source-scoped /fp:strict or -fno-fast-math/-ffp-contract=off alone
    // is not proof of the runtime rounding and gradual-underflow context.
    QCOMPARE(std::fegetround(),FE_TONEAREST);
    volatile double smallestNormal=std::numeric_limits<double>::min();
    volatile double half=0.5;
    const volatile double subnormal=smallestNormal*half;
    QVERIFY(subnormal>0.0); // catches active flush-to-zero for this arithmetic
    QVERIFY(std::fpclassify(static_cast<double>(subnormal))==FP_SUBNORMAL);
}

void TruePeakLimiterCoreTest::appliedGainWitnessReconstructsPcmAndIsNoninvasive()
{
    // U1/U4/U5 read-only witness: no ticks/placeholders, exactly N real
    // frames, one stereo-linked base gain, no M16 global trim mixed in.
    // Reproduce every actual PCM multiplication or bitwise identity copy.
    constexpr std::size_t n=1103;
    std::vector<double> source(n,+0.0);
    source[0]=4.0; // prezero signed detector support is required
    source[17]=-2.5;
    source[256]=+0.7;
    source[n-1]=-0.25;
    for(const auto fs:{44100,48000,88200,96000,176400,192000}) {
        const std::int64_t latency=(fs/200)+8;
        for(const bool stereo:{false,true}) {
            std::vector<dsp::TruePeakLimiterAppliedGainFrame> witness;
            const auto withWitness=runLimiterAudio(
                fs,stereo,source,-12.0,{83,71,239},&witness);
            const auto withoutWitness=runLimiterAudio(
                fs,stereo,source,-12.0,{1103});
            QVERIFY(!withWitness.empty());
            QCOMPARE(withWitness.size(),stereo?std::size_t{2}:std::size_t{1});
            QCOMPARE(withWitness.size(),withoutWitness.size());
            QCOMPARE(witness.size(),n);
            bool actualReduction=false;
            for(std::size_t i=0;i<n;++i) {
                const auto& w=witness[i];
                QCOMPARE(w.programme_frame,static_cast<std::int64_t>(i));
                QCOMPARE(w.raw_output_frame,static_cast<std::int64_t>(i)+latency);
                QVERIFY(std::isfinite(w.gain) && w.gain>0.0 && w.gain<=1.0);
                QVERIFY(std::isfinite(w.reduction_db) && w.reduction_db>=0.0);
                if(w.gain<1.0)actualReduction=true;
                QCOMPARE(std::bit_cast<std::uint64_t>(w.gain),
                    std::bit_cast<std::uint64_t>(
                        w.reduction_db==0.0?1.0:
                        std::pow(10.0,-w.reduction_db/20.0)));
                for(std::size_t ch=0;ch<withWitness.size();++ch) {
                    const auto sample=ch==0?source[i]:-source[i]*0.375;
                    const double expected=w.gain==1.0?sample:sample*w.gain;
                    QCOMPARE(std::bit_cast<std::uint64_t>(withWitness[ch][i]),
                        std::bit_cast<std::uint64_t>(expected));
                    QCOMPARE(std::bit_cast<std::uint64_t>(withWitness[ch][i]),
                        std::bit_cast<std::uint64_t>(withoutWitness[ch][i]));
                }
            }
            QVERIFY(actualReduction);
        }
    }
}

void TruePeakLimiterCoreTest::failedCallsAreAtomicAndCannotPublishWitness()
{
    constexpr int fs=44100;
    const auto fmt=limiterFormat(fs,true);
    auto p=dsp::TruePeakLimiterParameters::create(-12.0);
    QVERIFY(p);
    auto instance=dsp::TruePeakLimiterModule::create(frozenLimiterDescriptor(),
        *p.value());
    QVERIFY(instance);
    auto& mod=**instance.value();
    QVERIFY(mod.prepare(dsp::DspProcessSpec{fmt,
        audio::FrameDomainId::OUTPUT_RATE,
        *core::FrameCount::create(512).value()}));
    AppliedGainTestCapture witness;
    witness.records.reserve(128);
    mod.set_applied_gain_sink(&AppliedGainTestCapture::accept,&witness);
    const auto initial=mod.runtime_checkpoint();
    QVERIFY(initial);

    constexpr int n=128;
    auto input=audio::AudioBuffer::create(fmt,audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{0},*core::FrameCount::create(n).value());
    auto output=audio::AudioBuffer::create(fmt,audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{0},*core::FrameCount::create(n).value());
    QVERIFY(input && output);
    for(std::size_t ch=0;ch<2;++ch) {
        auto in=input.value()->mutable_view().channel(ch);
        auto out=output.value()->mutable_view().channel(ch);
        QVERIFY(in && out);
        for(std::size_t i=0;i<static_cast<std::size_t>(n);++i) {
            (*in.value())[i]=i==0?4.0:0.0;
            (*out.value())[i]=-0.0;
        }
    }
    auto bad=input.value()->mutable_view().channel(1);
    QVERIFY(bad);
    (*bad.value())[n-1]=std::numeric_limits<double>::quiet_NaN();
    auto block=core::FrameRange::create(core::FrameIndex{0},core::FrameIndex{n});
    QVERIFY(block);
    QVERIFY(!mod.process(input.value()->view(),output.value()->mutable_view(),
        dsp::DspProcessContext{*block.value(),true,false}));
    QCOMPARE(witness.records.size(),std::size_t{0});
    const auto pristine=mod.runtime_checkpoint();
    QVERIFY(pristine);
    QCOMPARE(*pristine.value(),*initial.value());
    for(std::size_t ch=0;ch<2;++ch) {
        const auto plane=output.value()->view().channel(ch);
        QVERIFY(plane);
        for(const double v:*plane.value())
            QCOMPARE(std::bit_cast<std::uint64_t>(v),
                std::bit_cast<std::uint64_t>(-0.0));
    }
    (*bad.value())[n-1]=+0.0;
    QVERIFY(mod.process(input.value()->view(),output.value()->mutable_view(),
        dsp::DspProcessContext{*block.value(),true,false}));
    const auto afterValid=mod.runtime_checkpoint();
    QVERIFY(afterValid);
    QCOMPARE(witness.records.size(),std::size_t{0}); // input shorter than D

    auto discontinuousInput=audio::AudioBuffer::create(
        fmt,audio::FrameDomainId::OUTPUT_RATE,core::FrameIndex{n+1},
        *core::FrameCount::create(1).value());
    auto discontinuousOutput=audio::AudioBuffer::create(
        fmt,audio::FrameDomainId::OUTPUT_RATE,core::FrameIndex{n+1},
        *core::FrameCount::create(1).value());
    QVERIFY(discontinuousInput && discontinuousOutput);
    auto sentinel=discontinuousOutput.value()->mutable_view().channel(0);
    QVERIFY(sentinel);
    (*sentinel.value())[0]=-0.0;
    auto wrongRange=core::FrameRange::create(
        core::FrameIndex{n+1},core::FrameIndex{n+2});
    QVERIFY(wrongRange);
    QVERIFY(!mod.process(discontinuousInput.value()->view(),
        discontinuousOutput.value()->mutable_view(),
        dsp::DspProcessContext{*wrongRange.value(),false,false}));
    QCOMPARE(std::bit_cast<std::uint64_t>(
        (*discontinuousOutput.value()->view().channel(0).value())[0]),
        std::bit_cast<std::uint64_t>(-0.0));
    const auto stillValid=mod.runtime_checkpoint();
    QVERIFY(stillValid);
    QCOMPARE(*stillValid.value(),*afterValid.value());

    // An early claimed EOS must fail before any partial drain/witness.
    constexpr int premature=7;
    auto drained=audio::AudioBuffer::create(fmt,audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{n},*core::FrameCount::create(premature).value());
    QVERIFY(drained);
    auto drainRange=core::FrameRange::create(
        core::FrameIndex{n},core::FrameIndex{n+premature});
    QVERIFY(drainRange);
    QVERIFY(!mod.finalize(drained.value()->mutable_view(),
        dsp::DspProcessContext{*drainRange.value(),false,true}));
    QCOMPARE(witness.records.size(),std::size_t{0});
    const auto afterInvalidEos=mod.runtime_checkpoint();
    QVERIFY(afterInvalidEos);
    QCOMPARE(*afterInvalidEos.value(),*afterValid.value());
}

void TruePeakLimiterCoreTest::highRateChunkBoundednessAndBitIdentity()
{
    constexpr int fs=192000;
    std::vector<double> input(2049,+0.0);
    input[0]=+4.0;
    input[120]=-1.0;
    input[1024]=+2.5;
    input[2048]=-0.5;
    const auto single=runLimiterAudio(fs,true,input,-12.0,{4096});
    const auto chunked=runLimiterAudio(fs,true,input,-12.0,{73,127,301,19});
    QVERIFY(!single.empty() && !chunked.empty());
    QCOMPARE(single.size(),std::size_t{2});
    QCOMPARE(single.size(),chunked.size());
    for(std::size_t ch=0;ch<single.size();++ch) {
        QCOMPARE(single[ch].size(),input.size());
        QCOMPARE(chunked[ch].size(),input.size());
        for(std::size_t i=0;i<input.size();++i) {
            QCOMPARE(std::bit_cast<std::uint64_t>(single[ch][i]),
                std::bit_cast<std::uint64_t>(chunked[ch][i]));
            QVERIFY(std::isfinite(single[ch][i]));
        }
    }
    // A streaming checkpoint must remain safely below the canonical 32 MiB
    // serialization resource bound even at the maximum supported Fs.
    auto param=dsp::TruePeakLimiterParameters::create(-12.0);
    auto module=dsp::TruePeakLimiterModule::create(
        frozenLimiterDescriptor(),*param.value());
    QVERIFY(module);
    const auto fmt=limiterFormat(fs,true);
    QVERIFY(module.value()->get()->prepare(dsp::DspProcessSpec{
        fmt,audio::FrameDomainId::OUTPUT_RATE,
        *core::FrameCount::create(4096).value()}));
    auto source=audio::AudioBuffer::create(fmt,audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{0},*core::FrameCount::create(1024).value());
    auto dest=audio::AudioBuffer::create(fmt,audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{0},*core::FrameCount::create(1024).value());
    QVERIFY(source && dest);
    for(std::size_t ch=0;ch<2;++ch) {
        auto lane=source.value()->mutable_view().channel(ch);
        QVERIFY(lane);
        (*lane.value())[0]=ch==0?4.0:-1.5;
    }
    auto extent=core::FrameRange::create(
        core::FrameIndex{0},core::FrameIndex{1024});
    QVERIFY(extent);
    QVERIFY(module.value()->get()->process(
        source.value()->view(),dest.value()->mutable_view(),
        dsp::DspProcessContext{*extent.value(),true,false}));
    auto cp=module.value()->get()->runtime_checkpoint();
    QVERIFY(cp);
    QVERIFY(cp.value()->payload.size()>640U);
    QVERIFY(cp.value()->payload.size()<(32U*1024U*1024U));
}



void TruePeakLimiterCoreTest::controlAllCeilingsLookaheadBoundarySixRates()
{
    using dsp::internal::TruePeakLimiterControllerState;
    using dsp::internal::LimiterDetectorEvent;
    // A future peak at the inclusive horizon END contributes zero to the
    // raised-cosine attack, while a halfway event contributes half its demand.
    // The full six-rate/four-ceiling matrix is independent of audio chunk size.
    for(const int fs:{44100,48000,88200,96000,176400,192000}) {
        for(const double ceilingDb:{-12.0,-6.0,-1.0,0.0}) {
            auto deadline=TruePeakLimiterControllerState::create(fs,ceilingDb);
            QVERIFY(deadline);
            const auto m=deadline.value()->next_tick();
            const auto horizon=deadline.value()->lookahead_ticks();
            const std::array<LimiterDetectorEvent,1> end{{
                {m+horizon,8.0,true}
            }};
            const auto neutral=deadline.value()->advance(0.0,end);
            QVERIFY(neutral);
            QCOMPARE(neutral.value()->attack_floor_db,0.0);
            QCOMPARE(neutral.value()->gain,1.0);
            QCOMPARE(neutral.value()->density,0.0);

            auto mid=TruePeakLimiterControllerState::create(fs,ceilingDb);
            QVERIFY(mid);
            const std::array<LimiterDetectorEvent,2> horizonEvents{{
                {m+horizon/2,2.0,true},
                {m+horizon,8.0,true}
            }};
            const auto step=mid.value()->advance(0.0,horizonEvents);
            QVERIFY(step);
            const double c=std::pow(10.0,ceilingDb/20.0);
            const double demand=20.0*std::log10(2.0/c);
            const double w=0.5*(1.0+std::cos(std::numbers::pi_v<double>*0.5));
            QVERIFY(std::abs(step.value()->attack_floor_db-demand*w)<1e-12);
            QVERIFY(step.value()->reduction_db>0.0);
            QVERIFY(step.value()->gain<1.0);
            QCOMPARE(step.value()->hold_remaining,mid.value()->hold_ticks());

            const auto before=mid.value()->snapshot();
            const std::array<LimiterDetectorEvent,1> outside{{
                {mid.value()->next_tick()+horizon+1,1.0,true}
            }};
            QVERIFY(!mid.value()->advance(0.0,outside));
            QCOMPARE(mid.value()->next_tick(),before.next_tick);
            QCOMPARE(mid.value()->density(),before.density);
            QCOMPARE(mid.value()->previous_reduction_db(),before.previous_reduction_db);
            QCOMPARE(mid.value()->hold_remaining(),before.hold_remaining);
        }
    }
}

void TruePeakLimiterCoreTest::oneSecond192kAnd44100StressWithBoundedCheckpoints()
{
    // One complete second per rate, including 192k stereo and the 44.1k
    // ties-to-even H=220 case: sparse impulses, channel-dominance swaps,
    // short dense alternating bursts, and an adversarial final-frame peak.
    // This is deterministic functional/serialization stress, NOT a portable
    // CPU-time or resident-memory certification of the production renderer.
    constexpr std::int64_t origin=1'000'000'027;
    constexpr std::size_t maxCheckpointBytes=1024U*1024U;
    for(const int fs:{44100,192000}) {
        const auto count=static_cast<std::size_t>(fs);
        std::vector<double> left(count,0.03125),right(count,-0.046875);
        for(std::size_t i=0;i<count;++i) {
            if((i%8192U)<16U) {
                left[i]=(i&1U)?-0.875:+0.875;
                right[i]=(i&1U)?+1.125:-1.125;
            }
            if((i%4096U)==0U) {
                left[i]=(i%8192U)==0U?4.0:-2.0;
                right[i]=(i%8192U)==0U?-0.0625:3.0;
            }
        }
        left.back()=-3.0;
        right.back()=4.0;

        std::vector<dsp::TruePeakLimiterAppliedGainFrame> referenceGain;
        std::vector<dsp::TruePeakLimiterAppliedGainFrame> splitGain;
        std::vector<std::size_t> checkpointBytes;
        const auto whole=runLimiterAudio(fs,true,left,-12.0,{4096},
            &referenceGain,origin,std::span<const double>{right},
            &checkpointBytes);
        const auto split=runLimiterAudio(fs,true,left,-12.0,
            {3,47,257,4096,11,511},&splitGain,origin,
            std::span<const double>{right});
        QVERIFY2(!whole.empty() && !split.empty(),
            "One-second OUTPUT_RATE PCM, drain or checkpoint failed");
        QCOMPARE(whole.size(),std::size_t{2});
        QCOMPARE(split.size(),whole.size());
        QCOMPARE(referenceGain.size(),count);
        QCOMPARE(splitGain.size(),count);
        QVERIFY(checkpointBytes.size()>=count/4096U);
        for(const auto bytes:checkpointBytes) {
            QVERIFY(bytes>640U);
            QVERIFY2(bytes<maxCheckpointBytes,
                "Checkpoint serialization grew beyond 1 MiB in long stress");
        }
        std::size_t attenuated=0;
        for(std::size_t i=0;i<count;++i) {
            const auto& frame=referenceGain[i];
            QCOMPARE(frame.programme_frame,origin+static_cast<std::int64_t>(i));
            QCOMPARE(frame.raw_output_frame,
                origin+static_cast<std::int64_t>(i)+fs/200+8);
            QCOMPARE(frame.programme_frame,splitGain[i].programme_frame);
            QCOMPARE(frame.raw_output_frame,splitGain[i].raw_output_frame);
            QCOMPARE(std::bit_cast<std::uint64_t>(frame.gain),
                std::bit_cast<std::uint64_t>(splitGain[i].gain));
            QCOMPARE(std::bit_cast<std::uint64_t>(frame.reduction_db),
                std::bit_cast<std::uint64_t>(splitGain[i].reduction_db));
            QVERIFY(std::isfinite(frame.gain));
            QVERIFY(frame.gain>0.0 && frame.gain<=1.0);
            if(frame.gain<1.0)++attenuated;
            for(std::size_t ch=0;ch<2U;++ch) {
                QCOMPARE(whole[ch].size(),count);
                QCOMPARE(split[ch].size(),count);
                const double input=ch==0U?left[i]:right[i];
                const double reconstructed=frame.gain==1.0
                    ?input:input*frame.gain;
                QCOMPARE(std::bit_cast<std::uint64_t>(whole[ch][i]),
                    std::bit_cast<std::uint64_t>(reconstructed));
                QCOMPARE(std::bit_cast<std::uint64_t>(whole[ch][i]),
                    std::bit_cast<std::uint64_t>(split[ch][i]));
                QVERIFY(std::isfinite(whole[ch][i]));
            }
        }
        QVERIFY2(attenuated>0U,
            "Adversarial hot stereo fixture never exercised gain reduction");
    }
}

void TruePeakLimiterCoreTest::adversarialStereoDetectorEightXOracle()
{
    // Full finite 8x direct convolution, across M16 asset phase order,
    // verifies signed prezero, tail and stereo max after channel dominance
    // swaps. Independent oracle has no M17 detector/ring-buffer state.
    for(const int fs:{44100,48000,88200,96000,176400,192000}) {
        Q_UNUSED(fs); // all rates share per-frame physical 8x tick geometry
        for(const int shape:{0,1,2,3}) {
            const std::size_t n=47U+19U*static_cast<std::size_t>(shape);
            std::vector<double> left(n,+0.0),right(n,+0.0);
            std::uint64_t seed=0x4b1d8c726af92113ULL;
            for(std::size_t i=0;i<n;++i) {
                seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;
                const double noise=static_cast<double>((seed>>43)&1023U)
                    /1024.0-0.5;
                switch(shape) {
                case 0: // independent starting/ending impulses
                    left[i]=i==0?4.0:(i==n-1?-2.0:+0.0);
                    right[i]=i==0?-0.125:(i==n-1?6.0:+0.0);
                    break;
                case 1: // alternating near Nyquist
                    left[i]=(i&1U)?-0.875:+0.875;
                    right[i]=(i&1U)?+1.25:-1.25;
                    break;
                case 2: // deterministic non-periodic stereo
                    left[i]=noise;
                    right[i]=static_cast<double>((seed>>31)&2047U)/1024.0-1.0;
                    break;
                default: // dominant lane changes across programme
                    left[i]=(i%17U==0U)?3.0:-0.03125;
                    right[i]=(i%13U==0U)?-5.0:+0.0625;
                    break;
                }
            }
            for(const std::uint32_t channels:{1U,2U}) {
                TruePeakLimiterDetectorState d{channels};
                std::vector<Event> events;
                events.reserve(8U*n+110U);
                const auto append=[&](const LimiterDetectorQuarter& q) {
                    if(q.preceding_anchor.valid) events.push_back(
                        {q.preceding_anchor.tick,q.preceding_anchor.peak});
                    if(q.midpoint.valid) events.push_back(
                        {q.midpoint.tick,q.midpoint.peak});
                };
                for(std::size_t i=0;i<n;++i) {
                    const auto group=d.push_programme_frame(left[i],
                        channels==2U?right[i]:+0.0);
                    QVERIFY(group);
                    for(const auto& q:*group.value())append(q);
                }
                const auto tail=d.finalize_detector();
                QVERIFY(tail);
                for(const auto& q:*tail.value())append(q);
                const auto reference=m17_oracle::direct_events(left,
                    channels==2U?std::span<const double>{right}
                                :std::span<const double>{});
                QCOMPARE(events.size(),reference.size());
                QCOMPARE(events.size(),8U*n+110U);
                QCOMPARE(events.front().tick,std::int64_t{-62});
                QCOMPARE(events.back().tick,static_cast<std::int64_t>(8U*n+54U));
                for(std::size_t i=0;i<reference.size();++i) {
                    QCOMPARE(events[i].tick,reference[i].tick);
                    QCOMPARE(std::bit_cast<std::uint64_t>(events[i].peak),
                        std::bit_cast<std::uint64_t>(reference[i].peak));
                    QVERIFY(std::isfinite(events[i].peak));
                    QVERIFY(events[i].peak>=0.0);
                    if(i>0)QVERIFY(events[i].tick>events[i-1].tick);
                }
            }
        }
    }
}

void TruePeakLimiterCoreTest::absoluteFrameOriginAndLinkedStereoWitness()
{
    // Distinct absolute resource origins cannot change DSP audio or gain:
    // only provenance fields are translated by the exact origin.
    constexpr std::int64_t origin=10'000'027;
    std::vector<double> source(1091,+0.0);
    source[0]=4.0;source[47]=-1.5;source[271]=1.0;source.back()=-0.125;
    for(const int fs:{44100,96000,192000}) {
        for(const bool stereo:{false,true}) {
            std::vector<dsp::TruePeakLimiterAppliedGainFrame> zeroWitness,movedWitness;
            const auto zero=runLimiterAudio(fs,stereo,source,-12.0,
                {1091},&zeroWitness);
            const auto moved=runLimiterAudio(fs,stereo,source,-12.0,
                {1,7,59,131},&movedWitness,origin);
            QVERIFY(!zero.empty() && !moved.empty());
            QCOMPARE(zero.size(),moved.size());
            QCOMPARE(zeroWitness.size(),source.size());
            QCOMPARE(movedWitness.size(),source.size());
            for(std::size_t i=0;i<source.size();++i) {
                QCOMPARE(movedWitness[i].programme_frame,
                    origin+static_cast<std::int64_t>(i));
                QCOMPARE(movedWitness[i].raw_output_frame,
                    origin+static_cast<std::int64_t>(i)+fs/200+8);
                QCOMPARE(std::bit_cast<std::uint64_t>(zeroWitness[i].gain),
                    std::bit_cast<std::uint64_t>(movedWitness[i].gain));
                QCOMPARE(std::bit_cast<std::uint64_t>(zeroWitness[i].reduction_db),
                    std::bit_cast<std::uint64_t>(movedWitness[i].reduction_db));
                for(std::size_t ch=0;ch<zero.size();++ch)
                    QCOMPARE(std::bit_cast<std::uint64_t>(zero[ch][i]),
                        std::bit_cast<std::uint64_t>(moved[ch][i]));
            }
        }
    }
    // Only right channel drives the limiter, but both lanes must receive
    // one identical applied gain, and the quiet left mono stays unaffected.
    constexpr int fs=48000;
    std::vector<double> left(511,+0.0),right(511,+0.0);
    left[0]=0.125;left[50]=-0.0625;
    right[0]=4.0;right[50]=-2.0;right.back()=-1.0;
    std::vector<dsp::TruePeakLimiterAppliedGainFrame> witness;
    const auto linked=runLimiterAudio(fs,true,left,-12.0,{11,127,3,53},
        &witness,0,std::span<const double>{right});
    const auto noWitness=runLimiterAudio(fs,true,left,-12.0,{511},
        nullptr,0,std::span<const double>{right});
    const auto mono=runLimiterAudio(fs,false,left,-12.0,{511});
    QVERIFY(!linked.empty() && !noWitness.empty() && !mono.empty());
    QCOMPARE(linked.size(),std::size_t{2});
    QCOMPARE(witness.size(),left.size());
    QVERIFY(witness.front().gain<1.0);
    for(std::size_t i=0;i<left.size();++i) {
        const double inputs[]{left[i],right[i]};
        QCOMPARE(std::bit_cast<std::uint64_t>(mono[0][i]),
            std::bit_cast<std::uint64_t>(left[i]));
        QCOMPARE(witness[i].programme_frame,static_cast<std::int64_t>(i));
        QCOMPARE(witness[i].raw_output_frame,static_cast<std::int64_t>(i)+248);
        QVERIFY(witness[i].gain>0.0 && witness[i].gain<=1.0);
        for(std::size_t ch=0;ch<2;++ch) {
            const double expected=witness[i].gain==1.0
                ? inputs[ch]:inputs[ch]*witness[i].gain;
            QCOMPARE(std::bit_cast<std::uint64_t>(linked[ch][i]),
                std::bit_cast<std::uint64_t>(expected));
            QCOMPARE(std::bit_cast<std::uint64_t>(linked[ch][i]),
                std::bit_cast<std::uint64_t>(noWitness[ch][i]));
        }
    }
    QVERIFY(std::abs(linked[0][0])<std::abs(mono[0][0]));
}


void TruePeakLimiterCoreTest::directEightXIndependentControllerReplayMatchesAppliedGain()
{
    // U2/U3: independent physical event convolution + offline controller
    // recurrence -> one gain per ACTUAL frame (no invented prezero PCM).
    // Compare same-backend exact IEEE-754 bits, not a displayed GR estimate.
    constexpr std::int64_t origin=4'000'029;
    for(const int fs:{44100,48000,88200,96000,176400,192000}) {
        for(const double ceiling:{-12.0,-6.0,-1.0,0.0}) {
            for(const bool stereo:{false,true}) {
                for(const int pattern:{0,1}) {
                    const std::size_t n=pattern==0?73U:127U;
                    std::vector<double> left(n,+0.0),right(n,+0.0);
                    for(std::size_t i=0;i<n;++i) {
                        if(pattern==0) {
                            left[i]=i==0U?4.0:(i==n-1U?-2.0:
                                (i==31U?+0.5:+0.0));
                            right[i]=i==0U?-0.125:(i==n-1U?3.0:
                                (i==31U?-4.0:+0.0));
                        } else {
                            left[i]=(i&1U)?-0.8125:+0.8125;
                            right[i]=(i%19U<9U)?+1.25:-1.25;
                            if(i%37U==0U) {
                                left[i]=+3.0;
                                right[i]=-2.0;
                            }
                        }
                    }
                    const auto direct=m17_oracle::direct_events(left,
                        stereo?std::span<const double>{right}:
                            std::span<const double>{});
                    const auto expected=m17_oracle::replay_control_from_events(
                        direct,fs,ceiling,n);
                    QCOMPARE(expected.size(),n);
                    std::vector<dsp::TruePeakLimiterAppliedGainFrame> actual;
                    const auto pcm=runLimiterAudio(fs,stereo,left,ceiling,
                        {3,17,1,57},&actual,origin,
                        stereo?std::span<const double>{right}:
                            std::span<const double>{});
                    QVERIFY2(!pcm.empty(),"M17 output failed offline replay fixture");
                    QCOMPARE(actual.size(),n);
                    for(std::size_t i=0;i<n;++i) {
                        const auto& predicted=expected[i];
                        const auto& observed=actual[i];
                        QCOMPARE(predicted.frame,static_cast<std::int64_t>(i));
                        QCOMPARE(observed.programme_frame,
                            origin+static_cast<std::int64_t>(i));
                        QCOMPARE(observed.raw_output_frame,
                            origin+static_cast<std::int64_t>(i)+fs/200+8);
                        QCOMPARE(std::bit_cast<std::uint64_t>(observed.reduction_db),
                            std::bit_cast<std::uint64_t>(predicted.reduction_db));
                        QCOMPARE(std::bit_cast<std::uint64_t>(observed.gain),
                            std::bit_cast<std::uint64_t>(predicted.gain));
                        for(std::size_t ch=0;ch<pcm.size();++ch) {
                            const double input=ch==0U?left[i]:right[i];
                            const double reconstructed=predicted.gain==1.0?
                                input:input*predicted.gain;
                            QCOMPARE(std::bit_cast<std::uint64_t>(pcm[ch][i]),
                                std::bit_cast<std::uint64_t>(reconstructed));
                        }
                    }
                }
            }
        }
    }
}


void TruePeakLimiterCoreTest::absoluteFrameLimitRejectsAtomicallyAcrossSixRates()
{
    // U4 absolute OUTPUT_RATE clock contract: a valid input span is not
    // sufficient if the delayed raw stream's final exclusive end overflows.
    // Negative cases must not alter PCM, checkpoint or genuine GR observer;
    // the EXACT boundary end==INT64_MAX remains valid and can finalize.
    constexpr auto max=std::numeric_limits<std::int64_t>::max();
    for(const int fs:{44100,48000,88200,96000,176400,192000}) {
        const std::int64_t d=fs/200+8;
        for(const bool stereo:{false,true}) {
            const auto fmt=limiterFormat(fs,stereo);
            const auto params=dsp::TruePeakLimiterParameters::create(-12.0);
            QVERIFY(params);
            auto created=dsp::TruePeakLimiterModule::create(
                frozenLimiterDescriptor(),*params.value());
            QVERIFY(created);
            auto& module=**created.value();
            QVERIFY(module.prepare(dsp::DspProcessSpec{
                fmt,audio::FrameDomainId::OUTPUT_RATE,
                *core::FrameCount::create(4096).value()}));
            AppliedGainTestCapture witness;
            witness.records.reserve(2);
            module.set_applied_gain_sink(
                &AppliedGainTestCapture::accept,&witness);
            const auto clean=module.runtime_checkpoint();
            QVERIFY(clean);
            const auto processOne=[&](std::int64_t begin,bool first,
                bool shouldPass) {
                auto in=audio::AudioBuffer::create(
                    fmt,audio::FrameDomainId::OUTPUT_RATE,
                    core::FrameIndex{begin},
                    *core::FrameCount::create(1).value());
                auto out=audio::AudioBuffer::create(
                    fmt,audio::FrameDomainId::OUTPUT_RATE,
                    core::FrameIndex{begin},
                    *core::FrameCount::create(1).value());
                QVERIFY(in && out);
                for(std::size_t ch=0;ch<(stereo?2U:1U);++ch) {
                    auto input=in.value()->mutable_view().channel(ch);
                    auto output=out.value()->mutable_view().channel(ch);
                    QVERIFY(input && output);
                    (*input.value())[0]=ch==0U?4.0:-2.0;
                    (*output.value())[0]=-0.0;
                }
                const auto range=core::FrameRange::create(
                    core::FrameIndex{begin},core::FrameIndex{begin+1});
                QVERIFY(range);
                const auto result=module.process(
                    in.value()->view(),out.value()->mutable_view(),
                    dsp::DspProcessContext{*range.value(),first,false});
                QCOMPARE(bool(result),shouldPass);
                if(!shouldPass) {
                    for(std::size_t ch=0;ch<(stereo?2U:1U);++ch) {
                        auto output=out.value()->view().channel(ch);
                        QVERIFY(output);
                        QCOMPARE(std::bit_cast<std::uint64_t>((*output.value())[0]),
                            std::bit_cast<std::uint64_t>(-0.0));
                    }
                }
            };
            // A valid one-frame input near the positive limit cannot be
            // accepted when the mandatory D-frame drain would wrap.
            processOne(max-2,true,false);
            QCOMPARE(witness.records.size(),std::size_t{0});
            const auto afterRejectedStart=module.runtime_checkpoint();
            QVERIFY(afterRejectedStart);
            QCOMPARE(*afterRejectedStart.value(),*clean.value());

            const auto boundaryOrigin=max-d-1;
            processOne(boundaryOrigin,true,true);
            const auto beforeRejectedContinuation=module.runtime_checkpoint();
            QVERIFY(beforeRejectedContinuation);
            QCOMPARE(beforeRejectedContinuation.value()->next_input_frame,
                std::optional{core::FrameIndex{boundaryOrigin+1}});
            processOne(boundaryOrigin+1,false,false);
            QCOMPARE(witness.records.size(),std::size_t{0});
            const auto afterRejectedContinuation=module.runtime_checkpoint();
            QVERIFY(afterRejectedContinuation);
            QCOMPARE(*afterRejectedContinuation.value(),
                *beforeRejectedContinuation.value());

            // The extreme accepted stream must drain all D physical frames:
            // its exclusive output end is precisely INT64_MAX, never wrapped.
            auto out=audio::AudioBuffer::create(
                fmt,audio::FrameDomainId::OUTPUT_RATE,
                core::FrameIndex{boundaryOrigin+1},
                *core::FrameCount::create(d).value());
            QVERIFY(out);
            const auto tail=core::FrameRange::create(
                core::FrameIndex{boundaryOrigin+1},
                core::FrameIndex{max});
            QVERIFY(tail);
            QVERIFY(module.finalize(out.value()->mutable_view(),
                dsp::DspProcessContext{*tail.value(),false,true}));
            QCOMPARE(witness.records.size(),std::size_t{1});
            QCOMPARE(witness.records[0].programme_frame,boundaryOrigin);
            QCOMPARE(witness.records[0].raw_output_frame,
                boundaryOrigin+d);
            QVERIFY(std::isfinite(witness.records[0].gain));
            QVERIFY(!module.runtime_checkpoint());
        }
    }
}


void TruePeakLimiterCoreTest::inPlaceBufferAliasIsBitIdenticalAndFailureAtomic()
{
    // U4: the host can legitimately give one owning AudioBuffer as both
    // read-only source and writable destination. Stream must retain the
    // original PCM until every channel has been consumed, because its raw
    // output is delayed by D and may overwrite that block in-place.
    // Compare against the independent separate input/output harness at
    // every sample (not only the final True Peak scalar).
    constexpr std::int64_t origin=17'000'013;
    for(const int fs:{44100,48000,88200,96000,176400,192000}) {
        const int d=fs/200+8;
        for(const bool stereo:{false,true}) {
            for(const double ceiling:{-12.0,0.0}) {
                for(const int n:{1,2*d+31}) {
                    std::vector<double> programme(static_cast<std::size_t>(n),+0.0);
                    for(int i=0;i<n;++i)
                        programme[static_cast<std::size_t>(i)]=
                            i==0?4.0:(i==n-1?-3.0:
                                (i%89==0?2.125:(i%7==0?-0.0:+0.125)));
                    const auto reference=runLimiterAudio(fs,stereo,programme,
                        ceiling,{1,31,257,4096},nullptr,origin);
                    QVERIFY(!reference.empty());
                    const auto fmt=limiterFormat(fs,stereo);
                    const auto params=dsp::TruePeakLimiterParameters::create(ceiling);
                    QVERIFY(params);
                    auto created=dsp::TruePeakLimiterModule::create(
                        frozenLimiterDescriptor(),*params.value());
                    QVERIFY(created);
                    auto& module=**created.value();
                    QVERIFY(module.prepare(dsp::DspProcessSpec{
                        fmt,audio::FrameDomainId::OUTPUT_RATE,
                        *core::FrameCount::create(4096).value()}));
                    AppliedGainTestCapture witness;
                    witness.records.reserve(static_cast<std::size_t>(n));
                    module.set_applied_gain_sink(
                        &AppliedGainTestCapture::accept,&witness);
                    const auto pristine=module.runtime_checkpoint();
                    QVERIFY(pristine);
                    // A rejected aliased input must not mutate any channel
                    // or advance/emit even a single gain witness.
                    auto invalid=audio::AudioBuffer::create(
                        fmt,audio::FrameDomainId::OUTPUT_RATE,
                        core::FrameIndex{origin},
                        *core::FrameCount::create(1).value());
                    QVERIFY(invalid);
                    auto invalidLane=invalid.value()->mutable_view().channel(0);
                    QVERIFY(invalidLane);
                    (*invalidLane.value())[0]=
                        std::numeric_limits<double>::quiet_NaN();
                    const auto originalBits=std::bit_cast<std::uint64_t>(
                        (*invalidLane.value())[0]);
                    const auto invalidRange=core::FrameRange::create(
                        core::FrameIndex{origin},core::FrameIndex{origin+1});
                    QVERIFY(invalidRange);
                    QVERIFY(!module.process(invalid.value()->view(),
                        invalid.value()->mutable_view(),
                        dsp::DspProcessContext{*invalidRange.value(),true,false}));
                    QCOMPARE(std::bit_cast<std::uint64_t>((*invalidLane.value())[0]),
                        originalBits);
                    const auto unchanged=module.runtime_checkpoint();
                    QVERIFY(unchanged);
                    QCOMPARE(*unchanged.value(),*pristine.value());
                    QCOMPARE(witness.records.size(),std::size_t{0});

                    std::vector<std::vector<double>> raw(stereo?2U:1U);
                    const std::array<std::size_t,4> parts{1,31,257,4096};
                    std::size_t offset=0,step=0;
                    while(offset<programme.size()) {
                        const auto count=std::min(programme.size()-offset,
                            parts[step++%parts.size()]);
                        auto aliased=audio::AudioBuffer::create(
                            fmt,audio::FrameDomainId::OUTPUT_RATE,
                            core::FrameIndex{origin+static_cast<std::int64_t>(offset)},
                            *core::FrameCount::create(
                                static_cast<std::int64_t>(count)).value());
                        QVERIFY(aliased);
                        for(std::size_t ch=0;ch<raw.size();++ch) {
                            auto lane=aliased.value()->mutable_view().channel(ch);
                            QVERIFY(lane);
                            for(std::size_t i=0;i<count;++i) {
                                const double x=programme[offset+i];
                                (*lane.value())[i]=ch==0U?x:-x*0.375;
                            }
                        }
                        const auto range=core::FrameRange::create(
                            core::FrameIndex{origin+static_cast<std::int64_t>(offset)},
                            core::FrameIndex{origin+static_cast<std::int64_t>(offset+count)});
                        QVERIFY(range);
                        QVERIFY(module.process(aliased.value()->view(),
                            aliased.value()->mutable_view(),
                            dsp::DspProcessContext{*range.value(),offset==0,false}));
                        for(std::size_t ch=0;ch<raw.size();++ch) {
                            const auto lane=aliased.value()->view().channel(ch);
                            QVERIFY(lane);
                            raw[ch].insert(raw[ch].end(),
                                lane.value()->begin(),lane.value()->end());
                        }
                        offset+=count;
                    }
                    std::int64_t drained=0;
                    while(drained<d) {
                        const auto count=std::min<std::int64_t>(d-drained,71);
                        const auto frame=origin+static_cast<std::int64_t>(n)+drained;
                        auto output=audio::AudioBuffer::create(
                            fmt,audio::FrameDomainId::OUTPUT_RATE,
                            core::FrameIndex{frame},
                            *core::FrameCount::create(count).value());
                        QVERIFY(output);
                        const auto range=core::FrameRange::create(
                            core::FrameIndex{frame},core::FrameIndex{frame+count});
                        QVERIFY(range);
                        QVERIFY(module.finalize(output.value()->mutable_view(),
                            dsp::DspProcessContext{*range.value(),false,
                                drained+count==d}));
                        for(std::size_t ch=0;ch<raw.size();++ch) {
                            const auto lane=output.value()->view().channel(ch);
                            QVERIFY(lane);
                            raw[ch].insert(raw[ch].end(),
                                lane.value()->begin(),lane.value()->end());
                        }
                        drained+=count;
                    }
                    QCOMPARE(witness.records.size(),programme.size());
                    QVERIFY(!module.runtime_checkpoint());
                    for(std::size_t ch=0;ch<raw.size();++ch) {
                        QCOMPARE(raw[ch].size(),
                            programme.size()+static_cast<std::size_t>(d));
                        QCOMPARE(reference[ch].size(),programme.size());
                        for(std::size_t i=0;i<programme.size();++i) {
                            const auto actual=raw[ch][static_cast<std::size_t>(d)+i];
                            QCOMPARE(std::bit_cast<std::uint64_t>(actual),
                                std::bit_cast<std::uint64_t>(reference[ch][i]));
                        }
                    }
                    for(std::size_t i=0;i<witness.records.size();++i) {
                        const auto& w=witness.records[i];
                        QCOMPARE(w.programme_frame,origin+static_cast<std::int64_t>(i));
                        QCOMPARE(w.raw_output_frame,
                            origin+static_cast<std::int64_t>(i)+d);
                        QVERIFY(std::isfinite(w.gain) && w.gain>0.0 && w.gain<=1.0);
                        QVERIFY(std::isfinite(w.reduction_db) && w.reduction_db>=0.0);
                    }
                }
            }
        }
    }
}


void TruePeakLimiterCoreTest::resetAndReprepareOwnDistinctResourcesAcrossSixRates()
{
    // U4/U5 lifecycle: a prepared module can process an entire resource
    // with D drain, then reset OR prepare again for a new independent
    // resource, without leaking detector history, GR, frame origin or PCM.
    // Every run is compared to a separately prepared fresh module.
    for(const int fs:{44100,48000,88200,96000,176400,192000}) {
        const int d=fs/200+8;
        for(const bool stereo:{false,true}) {
            for(const double ceiling:{-12.0,0.0}) {
                const auto fmt=limiterFormat(fs,stereo);
                const auto params=dsp::TruePeakLimiterParameters::create(ceiling);
                QVERIFY(params);
                auto created=dsp::TruePeakLimiterModule::create(
                    frozenLimiterDescriptor(),*params.value());
                QVERIFY(created);
                auto& module=**created.value();
                const auto spec=dsp::DspProcessSpec{
                    fmt,audio::FrameDomainId::OUTPUT_RATE,
                    *core::FrameCount::create(4096).value()};
                QVERIFY(module.prepare(spec));
                const auto pristine=module.runtime_checkpoint();
                QVERIFY(pristine);
                QVERIFY(!pristine.value()->next_input_frame);
                AppliedGainTestCapture witness;
                witness.records.reserve(static_cast<std::size_t>(3*(d+130)));
                module.set_applied_gain_sink(
                    &AppliedGainTestCapture::accept,&witness);
                const std::array<std::int64_t,3> origins{0,17'000'029,37};
                for(int stage=0;stage<3;++stage) {
                    if(stage>0) {
                        // Cancel a NEW, deliberately hot resource after some
                        // output has been emitted; stage 1 cancels before EOS,
                        // stage 2 after an actual partial D drain. This is
                        // distinct from recycling a fully finalized module.
                        module.reset();
                        const int abandonedN=static_cast<int>(d)+23;
                        const std::int64_t abandonedOrigin=
                            29'000'000+stage*4096;
                        auto abandonedIn=audio::AudioBuffer::create(
                            fmt,audio::FrameDomainId::OUTPUT_RATE,
                            core::FrameIndex{abandonedOrigin},
                            *core::FrameCount::create(abandonedN).value());
                        auto abandonedOut=audio::AudioBuffer::create(
                            fmt,audio::FrameDomainId::OUTPUT_RATE,
                            core::FrameIndex{abandonedOrigin},
                            *core::FrameCount::create(abandonedN).value());
                        QVERIFY(abandonedIn && abandonedOut);
                        for(std::size_t ch=0;ch<(stereo?2U:1U);++ch) {
                            const auto lane=abandonedIn.value()->
                                mutable_view().channel(ch);
                            QVERIFY(lane);
                            for(int i=0;i<abandonedN;++i) {
                                const double x=i==0?4.0:
                                    (i==abandonedN-1?-3.0:
                                    (i%31==0?2.375:0.125));
                                (*lane.value())[static_cast<std::size_t>(i)]=
                                    ch==0U?x:-x*0.375;
                            }
                        }
                        const auto abandonedRange=core::FrameRange::create(
                            core::FrameIndex{abandonedOrigin},
                            core::FrameIndex{abandonedOrigin+abandonedN});
                        QVERIFY(abandonedRange);
                        const auto witnessBefore=witness.records.size();
                        QVERIFY(module.process(abandonedIn.value()->view(),
                            abandonedOut.value()->mutable_view(),
                            dsp::DspProcessContext{
                                *abandonedRange.value(),true,false}));
                        QVERIFY(witness.records.size()>witnessBefore);
                        const auto pending=module.runtime_checkpoint();
                        QVERIFY(pending);
                        QCOMPARE(pending.value()->next_input_frame,
                            std::optional{core::FrameIndex{
                                abandonedOrigin+abandonedN}});
                        if(stage==2) {
                            constexpr int partial=17;
                            const auto first=abandonedOrigin+abandonedN;
                            auto tail=audio::AudioBuffer::create(
                                fmt,audio::FrameDomainId::OUTPUT_RATE,
                                core::FrameIndex{first},
                                *core::FrameCount::create(partial).value());
                            QVERIFY(tail);
                            const auto tailRange=core::FrameRange::create(
                                core::FrameIndex{first},
                                core::FrameIndex{first+partial});
                            QVERIFY(tailRange);
                            QVERIFY(module.finalize(
                                tail.value()->mutable_view(),
                                dsp::DspProcessContext{
                                    *tailRange.value(),false,false}));
                            QVERIFY(!module.runtime_checkpoint());
                        }
                        // A user cancel / fresh item load has to clear all
                        // PCM, detector history, absolute origin and GR.
                        if(stage==1)module.reset();
                        else QVERIFY(module.prepare(spec));
                    }
                    const auto clean=module.runtime_checkpoint();
                    QVERIFY(clean);
                    QCOMPARE(*clean.value(),*pristine.value());
                    witness.records.clear();
                    const int n=d+41+stage*37;
                    std::vector<double> programme(static_cast<std::size_t>(n),+0.0);
                    for(int i=0;i<n;++i) {
                        programme[static_cast<std::size_t>(i)]=
                            i==0?(stage==1?-4.0:4.0):
                            (i==n-1?(stage==2?-3.0:3.0):
                            (i%97==0?2.25:(i%7==0?-0.0:+0.125)));
                    }
                    const std::int64_t origin=origins[static_cast<std::size_t>(stage)];
                    const auto reference=runLimiterAudio(fs,stereo,
                        programme,ceiling,{1,3,257,4096},nullptr,origin);
                    QVERIFY2(!reference.empty(),"New-instance oracle did not process.");
                    const std::array<std::size_t,3> partitions{31,1,4096};
                    std::vector<std::vector<double>> raw(stereo?2U:1U);
                    std::size_t offset=0,block=0;
                    while(offset<programme.size()) {
                        const auto count=std::min(programme.size()-offset,
                            partitions[block++%partitions.size()]);
                        const auto first=origin+static_cast<std::int64_t>(offset);
                        auto in=audio::AudioBuffer::create(fmt,
                            audio::FrameDomainId::OUTPUT_RATE,
                            core::FrameIndex{first},*core::FrameCount::create(
                                static_cast<std::int64_t>(count)).value());
                        auto out=audio::AudioBuffer::create(fmt,
                            audio::FrameDomainId::OUTPUT_RATE,
                            core::FrameIndex{first},*core::FrameCount::create(
                                static_cast<std::int64_t>(count)).value());
                        QVERIFY(in && out);
                        for(std::size_t ch=0;ch<raw.size();++ch) {
                            const auto lane=in.value()->mutable_view().channel(ch);
                            QVERIFY(lane);
                            for(std::size_t i=0;i<count;++i) {
                                const double x=programme[offset+i];
                                (*lane.value())[i]=ch==0U?x:-x*0.375;
                            }
                        }
                        const auto range=core::FrameRange::create(
                            core::FrameIndex{first},core::FrameIndex{
                                first+static_cast<std::int64_t>(count)});
                        QVERIFY(range);
                        QVERIFY(module.process(in.value()->view(),
                            out.value()->mutable_view(),
                            dsp::DspProcessContext{*range.value(),offset==0,false}));
                        for(std::size_t ch=0;ch<raw.size();++ch) {
                            const auto lane=out.value()->view().channel(ch);
                            QVERIFY(lane);
                            raw[ch].insert(raw[ch].end(),lane.value()->begin(),
                                lane.value()->end());
                        }
                        offset+=count;
                    }
                    std::int64_t drained=0;
                    while(drained<d) {
                        const std::int64_t count=std::min<std::int64_t>(d-drained,61);
                        const auto first=origin+n+drained;
                        auto out=audio::AudioBuffer::create(fmt,
                            audio::FrameDomainId::OUTPUT_RATE,
                            core::FrameIndex{first},*core::FrameCount::create(count).value());
                        QVERIFY(out);
                        const auto range=core::FrameRange::create(
                            core::FrameIndex{first},core::FrameIndex{first+count});
                        QVERIFY(range);
                        QVERIFY(module.finalize(out.value()->mutable_view(),
                            dsp::DspProcessContext{*range.value(),false,
                                drained+count==d}));
                        for(std::size_t ch=0;ch<raw.size();++ch) {
                            const auto lane=out.value()->view().channel(ch);
                            QVERIFY(lane);
                            raw[ch].insert(raw[ch].end(),lane.value()->begin(),
                                lane.value()->end());
                        }
                        drained+=count;
                    }
                    QVERIFY(!module.runtime_checkpoint());
                    QCOMPARE(witness.records.size(),programme.size());
                    for(std::size_t i=0;i<witness.records.size();++i) {
                        const auto& applied=witness.records[i];
                        QCOMPARE(applied.programme_frame,
                            origin+static_cast<std::int64_t>(i));
                        QCOMPARE(applied.raw_output_frame,
                            origin+static_cast<std::int64_t>(i)+d);
                        QVERIFY(std::isfinite(applied.reduction_db)
                            && applied.reduction_db>=0.0);
                        QVERIFY(applied.gain>0.0 && applied.gain<=1.0);
                    }
                    for(std::size_t ch=0;ch<raw.size();++ch) {
                        QCOMPARE(raw[ch].size(),programme.size()+static_cast<std::size_t>(d));
                        QCOMPARE(reference[ch].size(),programme.size());
                        for(std::size_t i=0;i<programme.size();++i)
                            QCOMPARE(std::bit_cast<std::uint64_t>(
                                raw[ch][static_cast<std::size_t>(d)+i]),
                                std::bit_cast<std::uint64_t>(reference[ch][i]));
                    }
                    // Terminal, finalized state cannot be silently reused
                    // without an explicit reset/reprepare and new origin.
                    auto wrong=audio::AudioBuffer::create(
                        fmt,audio::FrameDomainId::OUTPUT_RATE,
                        core::FrameIndex{origin+n+d},
                        *core::FrameCount::create(1).value());
                    QVERIFY(wrong);
                    const auto r=core::FrameRange::create(
                        core::FrameIndex{origin+n+d},
                        core::FrameIndex{origin+n+d+1});
                    QVERIFY(r);
                    QVERIFY(!module.process(wrong.value()->view(),
                        wrong.value()->mutable_view(),
                        dsp::DspProcessContext{*r.value(),true,false}));
                    QCOMPARE(witness.records.size(),programme.size());
                }
            }
        }
    }
}


void TruePeakLimiterCoreTest::midDrainInvalidCallsPreservePcmAndGainAcrossSixRates()
{
    // U4 true EOS transactional lifetime: mid-drain an invalid Finalize,
    // Process, or Prepare must not consume a tick, publish PCM/GR or
    // silently restore checkpoint eligibility. The accepted continuation
    // remains bitwise equal to an independently partitioned fresh run.
    constexpr std::int64_t origin=19'000'013;
    for(const int fs:{44100,48000,88200,96000,176400,192000}) {
        const std::int64_t d=fs/200+8;
        for(const bool stereo:{false,true}) {
            for(const double ceiling:{-12.0,0.0}) {
                const int n=static_cast<int>(d)+49;
                std::vector<double> samples(static_cast<std::size_t>(n),0.0);
                for(int i=0;i<n;++i)
                    samples[static_cast<std::size_t>(i)]=i==0?4.0:
                        (i==n-1?-3.0:(i%71==0?2.5:
                        (i%17==0?-0.0:0.125)));
                const auto reference=runLimiterAudio(fs,stereo,samples,
                    ceiling,{1,17,239,4096},nullptr,origin);
                QVERIFY(reference.size()==(stereo?2U:1U));
                const auto fmt=limiterFormat(fs,stereo);
                const auto param=dsp::TruePeakLimiterParameters::create(ceiling);
                QVERIFY(param);
                auto made=dsp::TruePeakLimiterModule::create(
                    frozenLimiterDescriptor(),*param.value());
                QVERIFY(made);
                auto& module=**made.value();
                const auto spec=dsp::DspProcessSpec{
                    fmt,audio::FrameDomainId::OUTPUT_RATE,
                    *core::FrameCount::create(4096).value()};
                QVERIFY(module.prepare(spec));
                AppliedGainTestCapture witness;
                witness.records.reserve(static_cast<std::size_t>(n));
                module.set_applied_gain_sink(
                    &AppliedGainTestCapture::accept,&witness);
                auto in=audio::AudioBuffer::create(fmt,
                    audio::FrameDomainId::OUTPUT_RATE,core::FrameIndex{origin},
                    *core::FrameCount::create(n).value());
                auto out=audio::AudioBuffer::create(fmt,
                    audio::FrameDomainId::OUTPUT_RATE,core::FrameIndex{origin},
                    *core::FrameCount::create(n).value());
                QVERIFY(in && out);
                std::vector<std::vector<double>> raw(stereo?2U:1U);
                for(std::size_t ch=0;ch<raw.size();++ch) {
                    auto lane=in.value()->mutable_view().channel(ch);
                    QVERIFY(lane);
                    for(int i=0;i<n;++i) {
                        const double x=samples[static_cast<std::size_t>(i)];
                        (*lane.value())[static_cast<std::size_t>(i)]=
                            ch==0U?x:-x*0.375;
                    }
                }
                const auto inputRange=core::FrameRange::create(
                    core::FrameIndex{origin},core::FrameIndex{origin+n});
                QVERIFY(inputRange);
                QVERIFY(module.process(in.value()->view(),
                    out.value()->mutable_view(),
                    dsp::DspProcessContext{*inputRange.value(),true,false}));
                for(std::size_t ch=0;ch<raw.size();++ch) {
                    const auto lane=out.value()->view().channel(ch);
                    QVERIFY(lane);
                    raw[ch].insert(raw[ch].end(),
                        lane.value()->begin(),lane.value()->end());
                }
                const auto beforeDrain=module.runtime_checkpoint();
                QVERIFY(beforeDrain);
                const std::int64_t firstCount=17;
                auto first=audio::AudioBuffer::create(fmt,
                    audio::FrameDomainId::OUTPUT_RATE,
                    core::FrameIndex{origin+n},
                    *core::FrameCount::create(firstCount).value());
                QVERIFY(first);
                const auto firstRange=core::FrameRange::create(
                    core::FrameIndex{origin+n},
                    core::FrameIndex{origin+n+firstCount});
                QVERIFY(firstRange);
                const std::size_t beforeWitness=witness.records.size();
                for(std::size_t ch=0;ch<raw.size();++ch) {
                    auto lane=first.value()->mutable_view().channel(ch);
                    QVERIFY(lane);
                    std::fill(lane.value()->begin(),lane.value()->end(),-0.0);
                }
                // Premature true-EOS claim fails without even zeroing output.
                QVERIFY(!module.finalize(first.value()->mutable_view(),
                    dsp::DspProcessContext{*firstRange.value(),false,true}));
                QCOMPARE(witness.records.size(),beforeWitness);
                const auto untouched=module.runtime_checkpoint();
                QVERIFY(untouched);
                QCOMPARE(*untouched.value(),*beforeDrain.value());
                for(std::size_t ch=0;ch<raw.size();++ch) {
                    const auto lane=first.value()->view().channel(ch);
                    QVERIFY(lane);
                    for(const auto x:*lane.value())
                        QCOMPARE(std::bit_cast<std::uint64_t>(x),
                            std::bit_cast<std::uint64_t>(-0.0));
                }
                QVERIFY(module.finalize(first.value()->mutable_view(),
                    dsp::DspProcessContext{*firstRange.value(),false,false}));
                for(std::size_t ch=0;ch<raw.size();++ch) {
                    const auto lane=first.value()->view().channel(ch);
                    QVERIFY(lane);
                    raw[ch].insert(raw[ch].end(),
                        lane.value()->begin(),lane.value()->end());
                }
                QVERIFY(!module.runtime_checkpoint());
                const auto witnessAfterFirst=witness.records.size();
                // A mismatched stream transition must not alter the pending
                // D frames; a valid continuation should still match fresh PCM.
                QVERIFY(!module.prepare(dsp::DspProcessSpec{
                    fmt,audio::FrameDomainId::SOURCE_PROCESSING_RATE,
                    *core::FrameCount::create(4096).value()}));
                const auto secondStart=origin+n+firstCount;
                auto wrong=audio::AudioBuffer::create(fmt,
                    audio::FrameDomainId::OUTPUT_RATE,
                    core::FrameIndex{secondStart},
                    *core::FrameCount::create(1).value());
                QVERIFY(wrong);
                const auto wrongRange=core::FrameRange::create(
                    core::FrameIndex{secondStart},
                    core::FrameIndex{secondStart+1});
                QVERIFY(wrongRange);
                auto wrongLane=wrong.value()->mutable_view().channel(0);
                QVERIFY(wrongLane);
                (*wrongLane.value())[0]=-0.0;
                QVERIFY(!module.process(wrong.value()->view(),
                    wrong.value()->mutable_view(),
                    dsp::DspProcessContext{*wrongRange.value(),false,false}));
                QCOMPARE(std::bit_cast<std::uint64_t>((*wrongLane.value())[0]),
                    std::bit_cast<std::uint64_t>(-0.0));
                QCOMPARE(witness.records.size(),witnessAfterFirst);
                const auto badDrain=core::FrameRange::create(
                    core::FrameIndex{secondStart+1},
                    core::FrameIndex{secondStart+2});
                QVERIFY(badDrain);
                QVERIFY(!module.finalize(wrong.value()->mutable_view(),
                    dsp::DspProcessContext{*badDrain.value(),false,false}));
                QCOMPARE(std::bit_cast<std::uint64_t>((*wrongLane.value())[0]),
                    std::bit_cast<std::uint64_t>(-0.0));
                QCOMPARE(witness.records.size(),witnessAfterFirst);
                std::int64_t drain=firstCount;
                while(drain<d) {
                    const auto count=std::min<std::int64_t>(
                        d-drain,drain%2==0?31:89);
                    const auto frame=origin+n+drain;
                    auto segment=audio::AudioBuffer::create(
                        fmt,audio::FrameDomainId::OUTPUT_RATE,
                        core::FrameIndex{frame},
                        *core::FrameCount::create(count).value());
                    QVERIFY(segment);
                    const auto range=core::FrameRange::create(
                        core::FrameIndex{frame},core::FrameIndex{frame+count});
                    QVERIFY(range);
                    QVERIFY(module.finalize(segment.value()->mutable_view(),
                        dsp::DspProcessContext{*range.value(),false,
                            drain+count==d}));
                    for(std::size_t ch=0;ch<raw.size();++ch) {
                        const auto lane=segment.value()->view().channel(ch);
                        QVERIFY(lane);
                        raw[ch].insert(raw[ch].end(),
                            lane.value()->begin(),lane.value()->end());
                    }
                    drain+=count;
                }
                QCOMPARE(witness.records.size(),samples.size());
                QVERIFY(!module.runtime_checkpoint());
                for(std::size_t ch=0;ch<raw.size();++ch) {
                    QCOMPARE(raw[ch].size(),samples.size()+static_cast<std::size_t>(d));
                    for(std::size_t i=0;i<samples.size();++i)
                        QCOMPARE(std::bit_cast<std::uint64_t>(
                            raw[ch][static_cast<std::size_t>(d)+i]),
                            std::bit_cast<std::uint64_t>(reference[ch][i]));
                }
                for(std::size_t i=0;i<witness.records.size();++i) {
                    const auto& frame=witness.records[i];
                    QCOMPARE(frame.programme_frame,
                        origin+static_cast<std::int64_t>(i));
                    QCOMPARE(frame.raw_output_frame,
                        origin+static_cast<std::int64_t>(i)+d);
                    QVERIFY(std::isfinite(frame.reduction_db)&&frame.reduction_db>=0.0);
                    QVERIFY(frame.gain>0.0&&frame.gain<=1.0);
                }
            }
        }
    }
}

} // namespace
} // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::TruePeakLimiterCoreTest)
#include "test_true_peak_limiter_core.moc"
