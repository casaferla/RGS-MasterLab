#include "../../oracles/true_peak_limiter/core_test_support.hpp"
#include "internal/true_peak_limiter_state.hpp"
#include "internal/experimental/true_peak_limiter_selector.hpp"
#include "internal/experimental/true_peak_limiter_rolling_scheduler.hpp"
#include "internal/experimental/true_peak_limiter_lazy_controller.hpp"
#include "internal/experimental/true_peak_limiter_bounded_detector_feed.hpp"

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/dsp/module_descriptor.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/render/true_peak_limiter_terminal_candidate.hpp>
#include <rgsml/core/frame_time.hpp>

#include <QtTest/QTest>
#include <QDebug>

#include <bit>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <numbers>
#include <numeric>
#include <vector>
#include <algorithm>
#include <utility>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string_view>
#include <optional>
#include <array>
#include <span>

namespace rgsml::tests {
namespace {
[[nodiscard]] audio::AudioBuffer buffer(int fs, bool stereo, int frames,
    std::int64_t origin=0)
{
    auto fmt=audio::AudioFormat::create(
        *core::SampleRate::create(fs).value(),
        stereo?audio::ChannelLayout::STEREO_LR:audio::ChannelLayout::MONO_C);
    return std::move(*audio::AudioBuffer::create(*fmt.value(),
        audio::FrameDomainId::OUTPUT_RATE,core::FrameIndex{origin},
        *core::FrameCount::create(frames).value()).value());
}
[[nodiscard]] std::string_view category(const core::Error& e)
{
    for(const auto& d:e.details())if(d.key=="category")return d.value;
    return {};
}

// Test-only owning full-resource renderer: process M17 through its real
// IModule entrypoints, discard D placeholders, and retain EXACTLY N frames.
// Not a qualified product renderer, nor a bypass/ownership implementation.
[[nodiscard]] std::optional<audio::AudioBuffer> renderRealM17(
    const audio::AudioBuffer& source,double ceilingDb,
    std::span<const std::size_t> chunks,
    std::optional<std::int64_t> resumeAt = std::nullopt)
{
    if(chunks.empty())return std::nullopt;
    for(const auto chunk:chunks)if(chunk==0 || chunk>4096)return std::nullopt;
    auto descriptor=dsp::ModuleDescriptor::create(dsp::ModuleDescriptorSpec{
        "rgsml.dsp.true-peak-limiter","m17-verification-test",
        {dsp::ModuleCategory::DYNAMICS,dsp::ModuleCategory::OUTPUT},
        {dsp::ProcessingStage::MASTER},{dsp::ChainSegment::TERMINAL},
        false,true,dsp::PlacementClass::TERMINAL_SLOT,
        dsp::TerminalSlot::FINAL_TRUE_PEAK_LIMITER,
        {},{},{},{},false,true,"1.0.0",
        "rgsml.dsp.true-peak-limiter.parameters/1.0.0"});
    auto params=dsp::TruePeakLimiterParameters::create(ceilingDb);
    if(!descriptor || !params)return std::nullopt;
    auto module=dsp::TruePeakLimiterModule::create(*descriptor.value(),*params.value());
    if(!module)return std::nullopt;
    auto active=std::move(*module.value());
    const auto view=source.view();
    const auto fmt=view.format();
    const auto n=view.frame_count().value();
    const auto origin=view.absolute_range().begin().value();
    if(n<=0 || (resumeAt && (*resumeAt<=0 || *resumeAt>=n)))return std::nullopt;
    const dsp::DspProcessSpec spec{fmt,audio::FrameDomainId::OUTPUT_RATE,
        *core::FrameCount::create(4096).value()};
    const auto requirements=active->runtime_requirements(spec);
    if(!requirements || !active->prepare(spec))return std::nullopt;
    const auto d=requirements.value()->algorithmic_latency_frames.value();
    std::array<std::vector<double>,2> raw;
    std::size_t chunkIndex=0;
    const auto append=[&](const audio::AudioBuffer& output)->bool {
        for(std::size_t ch=0;ch<fmt.channel_count();++ch) {
            const auto samples=output.view().channel(ch);
            if(!samples)return false;
            raw[ch].insert(raw[ch].end(),samples.value()->begin(),
                samples.value()->end());
        }
        return true;
    };
    for(std::int64_t pos=0;pos<n;) {
        auto count=std::min<std::int64_t>(n-pos,
            static_cast<std::int64_t>(chunks[chunkIndex++%chunks.size()]));
        if(resumeAt && pos<*resumeAt)
            count=std::min(count,*resumeAt-pos);
        const auto first=origin+pos;
        auto in=audio::AudioBuffer::create(fmt,audio::FrameDomainId::OUTPUT_RATE,
            core::FrameIndex{first},*core::FrameCount::create(count).value());
        auto out=audio::AudioBuffer::create(fmt,audio::FrameDomainId::OUTPUT_RATE,
            core::FrameIndex{first},*core::FrameCount::create(count).value());
        auto range=core::FrameRange::create(core::FrameIndex{first},
            core::FrameIndex{first+count});
        if(!in || !out || !range)return std::nullopt;
        for(std::size_t ch=0;ch<fmt.channel_count();++ch) {
            const auto src=view.channel(ch);
            const auto dst=in.value()->mutable_view().channel(ch);
            if(!src || !dst)return std::nullopt;
            for(std::int64_t i=0;i<count;++i)
                (*dst.value())[static_cast<std::size_t>(i)]=
                    (*src.value())[static_cast<std::size_t>(pos+i)];
        }
        if(!active->process(in.value()->view(),out.value()->mutable_view(),
            dsp::DspProcessContext{*range.value(),pos==0,false})
            || !append(*out.value()))return std::nullopt;
        pos+=count;
        if(resumeAt && pos==*resumeAt) {
            const auto cp=active->runtime_checkpoint();
            if(!cp || cp.value()->next_input_frame !=
                std::optional{core::FrameIndex{origin+pos}})
                return std::nullopt;
            const auto owned=m17_verifier::bind_core_checkpoint(
                *cp.value(),"whole-resource-m17-test",source,17U);
            if(!owned)return std::nullopt;
            auto replacement=dsp::TruePeakLimiterModule::create(
                *descriptor.value(),*params.value());
            if(!replacement)return std::nullopt;
            auto successor=std::move(*replacement.value());
            if(!successor->prepare(spec))return std::nullopt;
            const auto pristine=successor->runtime_checkpoint();
            if(!pristine)return std::nullopt;
            if(m17_verifier::restore_bound_checkpoint(
                *successor,*owned.value(),"foreign-resource",source,17U))
                return std::nullopt;
            const auto failed=successor->runtime_checkpoint();
            if(!failed || *failed.value()!=*pristine.value())
                return std::nullopt;
            if(!m17_verifier::restore_bound_checkpoint(
                *successor,*owned.value(),"whole-resource-m17-test",source,17U))
                return std::nullopt;
            const auto restored=successor->runtime_checkpoint();
            if(!restored || *restored.value()!=*cp.value())
                return std::nullopt;
            active=std::move(successor);
        }
    }
    for(std::int64_t drain=0;drain<d;) {
        const auto count=std::min<std::int64_t>(d-drain,127);
        const auto first=origin+n+drain;
        auto out=audio::AudioBuffer::create(fmt,audio::FrameDomainId::OUTPUT_RATE,
            core::FrameIndex{first},*core::FrameCount::create(count).value());
        auto range=core::FrameRange::create(core::FrameIndex{first},
            core::FrameIndex{first+count});
        if(!out || !range || !active->finalize(out.value()->mutable_view(),
            dsp::DspProcessContext{*range.value(),false,drain+count==d})
            || !append(*out.value()))return std::nullopt;
        drain+=count;
    }
    auto owned=audio::AudioBuffer::create(fmt,audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{origin},*core::FrameCount::create(n).value());
    if(!owned)return std::nullopt;
    for(std::size_t ch=0;ch<fmt.channel_count();++ch) {
        if(raw[ch].size()!=static_cast<std::size_t>(n+d))return std::nullopt;
        const auto dst=owned.value()->mutable_view().channel(ch);
        if(!dst)return std::nullopt;
        for(std::int64_t i=0;i<n;++i)
            (*dst.value())[static_cast<std::size_t>(i)]=
                raw[ch][static_cast<std::size_t>(i+d)];
    }
    return std::move(*owned.value());
}

class TruePeakLimiterVerificationTest final : public QObject {
    Q_OBJECT
private slots:
    void exactBinary64TrimThreshold();
    void genuineM16WholeResourceQuietAndFalsifiableHot();
    void rejectWrongCeilingAndPropagateM16Failures();
    void actualM16ReverificationUsesOneGlobalStereoScalar();
    void renderedM17FullResourceMeasuredAndVerifiedByM16();
    void shortAndSplitEosDoesNotCreatePhantomPcm();
    void inactiveBypassIsZeroLatencyBitCopyAboveCeilingSixRates();
    void bypassAndEnabledLimiterAreObservablyDistinct();
    void failedVerificationNeverMutatesResourceAcrossSixRates();
    void fullResourceSixRateChunkMatrixRemainsBitIdentical();
    void fullResourceResumeGatesAreBitwiseIdentical();
    void boundedActualM17CostProbe();
    void boundedLookaheadMemoizationProbe();
    void detectorSourcedLookaheadMemoizationProbe();
    void experimentalIsolatedSelectorMatchesFrozenControllerAndPcm();
    void experimentalRollingBoundedBatchPcmOracle();
    void experimentalLiveDetectorBoundedPcmOracle();
    void experimentalLongResourceRenderBenchmark();
    void experimental64FrameRealtimeQuantumProbe();
    void experimentalLazyControllerBitwiseOracle();
    void experimentalIntegratedLazyQuantumBenchmark();
    void experimentalSelectorWorkProfileFast();
    void experimentalDetectorSourcedSelectorProfileFast();
    void optionalPhaseInstrumentationIsNonInvasive();
};
void TruePeakLimiterVerificationTest::exactBinary64TrimThreshold()
{
    const double edge=m17_verifier::kMinimumGlobalTrimGain;
    QCOMPARE(std::bit_cast<std::uint64_t>(edge),
             std::bit_cast<std::uint64_t>(0x1.fa23a197d2819p-1));
    QVERIFY(m17_verifier::trim_within_guard(edge));
    QVERIFY(!m17_verifier::trim_within_guard(
        std::nextafter(edge,0.0)));
    QVERIFY(m17_verifier::trim_within_guard(
        std::nextafter(edge,1.0)));
    QVERIFY(m17_verifier::trim_within_guard(1.0));
    QVERIFY(!m17_verifier::trim_within_guard(
        std::nextafter(1.0,2.0)));
    QVERIFY(!m17_verifier::trim_within_guard(
        std::numeric_limits<double>::quiet_NaN()));
}
void TruePeakLimiterVerificationTest::genuineM16WholeResourceQuietAndFalsifiableHot()
{
    for(const auto fs:{44100,48000,88200,96000,176400,192000}) {
        for(const bool stereo:{false,true}) {
            auto quiet=buffer(fs,stereo,809);
            for(std::size_t ch=0;ch<quiet.view().format().channel_count();++ch) {
                auto channel=quiet.mutable_view().channel(ch);
                QVERIFY(channel);
                for(std::size_t i=0;i<channel.value()->size();++i)
                    (*channel.value())[i]=((i&1U)?-1.0:1.0)*0.015625;
            }
            const auto original=quiet.view().channel(0);
            QVERIFY(original);
            const auto before=std::bit_cast<std::uint64_t>((*original.value())[0]);
            auto verified=m17_verifier::verify_full_resource(quiet,-1.0);
            QVERIFY(verified);
            QCOMPARE(verified.value()->global_trim_gain,1.0);
            QVERIFY(!verified.value()->trim_used);
            QVERIFY(verified.value()->before_peak<=std::pow(10.0,-1.0/20.0));
            QCOMPARE(verified.value()->status,
                analysis::TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED);
            const auto after=quiet.view().channel(0);
            QCOMPARE(std::bit_cast<std::uint64_t>((*after.value())[0]),before);
            auto hot=buffer(fs,stereo,19);
            for(std::size_t ch=0;ch<hot.view().format().channel_count();++ch){
                auto lane=hot.mutable_view().channel(ch);
                QVERIFY(lane);
                (*lane.value())[0]=4.0;
            }
            const auto failed=m17_verifier::verify_full_resource(hot,-12.0);
            QVERIFY(!failed);
            QCOMPARE(category(*failed.error()),
                std::string_view{"LIMITER_VERIFICATION_GUARD_EXCEEDED"});
            auto unchanged=hot.view().channel(0);
            QCOMPARE((*unchanged.value())[0],4.0);
        }
    }
}
void TruePeakLimiterVerificationTest::rejectWrongCeilingAndPropagateM16Failures()
{
    auto good=buffer(44100,true,4);
    QVERIFY(!m17_verifier::verify_full_resource(good,-12.01));
    QVERIFY(!m17_verifier::verify_full_resource(good,0.0001));
    QVERIFY(!m17_verifier::verify_full_resource(good,
        std::numeric_limits<double>::quiet_NaN()));
    auto zeroLength=audio::AudioBuffer::create(
        *audio::AudioFormat::create(*core::SampleRate::create(48000).value(),
            audio::ChannelLayout::MONO_C).value(),
        audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{0},*core::FrameCount::create(0).value());
    if(zeroLength) {
        // M16 rejects empty full-resource input categorically.
        QVERIFY(!m17_verifier::verify_full_resource(*zeroLength.value(),-1.0));
    }
}
void TruePeakLimiterVerificationTest::actualM16ReverificationUsesOneGlobalStereoScalar()
{
    // The positive branch must call the REAL M16 full-resource meter both
    // before and after ONE common stereo/mono binary64 scalar application.
    // Force an approximately 0.04dB violation: trim is necessary and stays
    // inside the frozen 0.10dB verification guard.
    constexpr int count=303;
    constexpr double ceilingDb=-1.0;
    const double ceiling=std::pow(10.0,ceilingDb/20.0);
    for(const int fs:{44100,48000,88200,96000,176400,192000}) {
        for(const bool stereo:{false,true}) {
            auto resource=buffer(fs,stereo,count);
            for(std::size_t ch=0;ch<resource.view().format().channel_count();++ch) {
                auto plane=resource.mutable_view().channel(ch);
                QVERIFY(plane);
                for(int i=0;i<count;++i) {
                    const auto x=i==13?1.0:
                        (i%19==0?-0.4:std::sin(double(i)*0.213)*0.3);
                    (*plane.value())[static_cast<std::size_t>(i)]=
                        ch==0?x:x*(-0.87);
                }
            }
            const auto reference=analysis::measure_true_peak_full_resource(resource);
            QVERIFY(reference);
            QVERIFY(reference.value()->global_linear_peak()>0.0);
            const double scale=ceiling/reference.value()->global_linear_peak()*1.005;
            std::vector<std::vector<double>> before(
                resource.view().format().channel_count());
            for(std::size_t ch=0;ch<before.size();++ch) {
                auto data=resource.mutable_view().channel(ch);
                QVERIFY(data);
                before[ch].reserve(count);
                for(double& x:*data.value()) {
                    x*=scale;
                    before[ch].push_back(x);
                }
            }
            const auto peakBefore=analysis::measure_true_peak_full_resource(resource);
            QVERIFY(peakBefore);
            const double p=peakBefore.value()->global_linear_peak();
            QVERIFY2(p>ceiling,"Positive trim must not accidentally be neutral");
            const double expectedGain=std::nextafter(ceiling/p,0.0);
            QVERIFY(m17_verifier::trim_within_guard(expectedGain));
            auto checked=m17_verifier::verify_full_resource(resource,ceilingDb);
            QVERIFY(checked);
            QVERIFY(checked.value()->trim_used);
            QCOMPARE(std::bit_cast<std::uint64_t>(
                checked.value()->global_trim_gain),
                std::bit_cast<std::uint64_t>(expectedGain));
            QCOMPARE(std::bit_cast<std::uint64_t>(
                checked.value()->before_peak),
                std::bit_cast<std::uint64_t>(p));
            QVERIFY(checked.value()->after_peak<=ceiling);
            for(std::size_t ch=0;ch<before.size();++ch) {
                const auto after=resource.view().channel(ch);
                QVERIFY(after);
                for(int i=0;i<count;++i) {
                    const double expected=
                        before[ch][static_cast<std::size_t>(i)]*expectedGain;
                    QCOMPARE(std::bit_cast<std::uint64_t>(
                        (*after.value())[static_cast<std::size_t>(i)]),
                        std::bit_cast<std::uint64_t>(expected));
                }
            }
            const auto measuredAfter=analysis::measure_true_peak_full_resource(resource);
            QVERIFY(measuredAfter);
            QCOMPARE(std::bit_cast<std::uint64_t>(
                checked.value()->after_peak),
                std::bit_cast<std::uint64_t>(
                    measuredAfter.value()->global_linear_peak()));
            QCOMPARE(checked.value()->status,
                analysis::TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED);
        }
    }
}



void TruePeakLimiterVerificationTest::renderedM17FullResourceMeasuredAndVerifiedByM16()
{
    // Crosses the real process/finalize boundary, not a fabricated input to M16.
    // N=1, N>2D, six Fs, mono/stereo, absolute origins and three Ceilings.
    for(const int fs:{44100,48000,88200,96000,176400,192000}) {
        const int d=fs/200+8;
        for(const bool stereo:{false,true})
            for(const double ceilingDb:{-12.0,-6.0,0.0})
                for(const int n:{1,2*d+41}) {
                    auto input=buffer(fs,stereo,n,stereo?20'000'003:0);
                    for(std::size_t ch=0;ch<input.view().format().channel_count();++ch) {
                        const auto lane=input.mutable_view().channel(ch);
                        QVERIFY(lane);
                        for(int i=0;i<n;++i) {
                            const double x=i==0?4.0:(i==n-1?-3.0:
                                (i%97==0?2.25:((i&1)?-0.1875:0.1875)));
                            (*lane.value())[static_cast<std::size_t>(i)]=
                                ch==0?x:(i%211<105?-x*0.8:x*1.25);
                        }
                    }
                    const std::array<std::size_t,4> chunks{1,19,127,511};
                    auto real=renderRealM17(input,ceilingDb,chunks);
                    QVERIFY2(real.has_value(),"Real M17 process/finalize failed");
                    QCOMPARE(real->view().frame_count().value(),
                        static_cast<std::int64_t>(n));
                    QCOMPARE(real->view().absolute_range().begin().value(),
                        input.view().absolute_range().begin().value());
                    auto before=analysis::measure_true_peak_full_resource(*real);
                    QVERIFY(before);
                    auto verdict=m17_verifier::verify_full_resource(*real,ceilingDb);
                    QVERIFY2(verdict,"Real M17 required >0.10dB final M16 trim");
                    QCOMPARE(verdict.value()->status,
                        analysis::TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED);
                    QCOMPARE(std::bit_cast<std::uint64_t>(verdict.value()->before_peak),
                        std::bit_cast<std::uint64_t>(
                            before.value()->global_linear_peak()));
                    QVERIFY(m17_verifier::trim_within_guard(
                        verdict.value()->global_trim_gain));
                    auto after=analysis::measure_true_peak_full_resource(*real);
                    QVERIFY(after);
                    QVERIFY2(after.value()->global_linear_peak()<=
                        std::pow(10.0,ceilingDb/20.0),
                        "Full-resource final M16 measurement exceeds Ceiling");
                    QCOMPARE(std::bit_cast<std::uint64_t>(verdict.value()->after_peak),
                        std::bit_cast<std::uint64_t>(
                            after.value()->global_linear_peak()));
                }
    }
}
void TruePeakLimiterVerificationTest::shortAndSplitEosDoesNotCreatePhantomPcm()
{
    // End-of-stream and compensated output are chunk-independent even when
    // programme length is shorter than the core latency.
    for(const int fs:{44100,192000}) {
        const int d=fs/200+8;
        for(const int n:{1,d-1,d+3}) {
            auto source=buffer(fs,true,n,11'000'027);
            for(std::size_t ch=0;ch<2U;++ch) {
                const auto lane=source.mutable_view().channel(ch);
                QVERIFY(lane);
                for(int i=0;i<n;++i)
                    (*lane.value())[static_cast<std::size_t>(i)]=
                        i==0?(ch==0?4.0:-3.0):
                        (i==n-1?(ch==0?-1.0:2.0):((i%7==0)?0.25:-0.125));
            }
            const std::array<std::size_t,1> one{4096};
            const std::array<std::size_t,5> many{1,7,23,113,301};
            auto flat=renderRealM17(source,-12.0,one);
            auto split=renderRealM17(source,-12.0,many);
            QVERIFY(flat.has_value() && split.has_value());
            for(std::size_t ch=0;ch<2U;++ch) {
                const auto a=flat->view().channel(ch);
                const auto b=split->view().channel(ch);
                QVERIFY(a && b);
                QCOMPARE(a.value()->size(),static_cast<std::size_t>(n));
                QCOMPARE(b.value()->size(),a.value()->size());
                for(int i=0;i<n;++i)
                    QCOMPARE(std::bit_cast<std::uint64_t>(
                        (*a.value())[static_cast<std::size_t>(i)]),
                        std::bit_cast<std::uint64_t>(
                            (*b.value())[static_cast<std::size_t>(i)]));
            }
            const auto ma=analysis::measure_true_peak_full_resource(*flat);
            const auto mb=analysis::measure_true_peak_full_resource(*split);
            QVERIFY(ma && mb);
            QCOMPARE(std::bit_cast<std::uint64_t>(ma.value()->global_linear_peak()),
                std::bit_cast<std::uint64_t>(mb.value()->global_linear_peak()));
        }
    }
}


void TruePeakLimiterVerificationTest::inactiveBypassIsZeroLatencyBitCopyAboveCeilingSixRates()
{
    // AC-06: no limiter, no M16 trim, no PCM padding/drain and no GR when
    // inactive. An OVER-CEILING bypass remains deliberately over Ceiling:
    // M16 measurement is observational and NEVER a certification in BYP.
    constexpr std::int64_t origin=31'000'027;
    for(const int fs:{44100,48000,88200,96000,176400,192000}) {
        for(const bool stereo:{false,true}) {
            for(const double ceilingDb:{-12.0,-6.0,-1.0,0.0}) {
                for(const int n:{1,fs/200+17}) {
                    auto input=buffer(fs,stereo,n,origin);
                    for(std::size_t ch=0;ch<input.view().format().channel_count();++ch) {
                        auto lane=input.mutable_view().channel(ch);
                        QVERIFY(lane);
                        for(int i=0;i<n;++i) {
                            const double x=i==0?4.0:
                                (i==n-1?-3.0:(i%13==0?+0.5:-0.25));
                            (*lane.value())[static_cast<std::size_t>(i)]=
                                ch==0?x:-x*0.75;
                        }
                        if(n>3) {
                            (*lane.value())[1]=-0.0;
                            (*lane.value())[2]=+0.0;
                            (*lane.value())[3]=std::numeric_limits<double>::denorm_min();
                        }
                    }
                    const auto before=analysis::measure_true_peak_full_resource(input);
                    QVERIFY(before);
                    const double ceiling=std::pow(10.0,ceilingDb/20.0);
                    QVERIFY2(before.value()->global_linear_peak()>ceiling,
                        "Hot bypass fixture unexpectedly below Ceiling");
                    const auto bypass=m17_verifier::bypass_full_resource(input);
                    QVERIFY(bypass);
                    QCOMPARE(bypass.value()->view().format(),input.view().format());
                    QCOMPARE(bypass.value()->view().frame_count(),input.view().frame_count());
                    QCOMPARE(bypass.value()->view().absolute_range(),
                        input.view().absolute_range());
                    QCOMPARE(bypass.value()->view().timebase().frame_domain_id(),
                        audio::FrameDomainId::OUTPUT_RATE);
                    for(std::size_t ch=0;ch<input.view().format().channel_count();++ch) {
                        const auto original=input.view().channel(ch);
                        const auto passthrough=bypass.value()->view().channel(ch);
                        QVERIFY(original && passthrough);
                        for(int i=0;i<n;++i)
                            QCOMPARE(std::bit_cast<std::uint64_t>(
                                (*passthrough.value())[static_cast<std::size_t>(i)]),
                                std::bit_cast<std::uint64_t>(
                                    (*original.value())[static_cast<std::size_t>(i)]));
                        if(n>3) {
                            QCOMPARE(std::bit_cast<std::uint64_t>(
                                (*passthrough.value())[1]),
                                std::bit_cast<std::uint64_t>(-0.0));
                            QCOMPARE(std::bit_cast<std::uint64_t>(
                                (*passthrough.value())[2]),
                                std::bit_cast<std::uint64_t>(+0.0));
                        }
                    }
                    const auto observed=analysis::measure_true_peak_full_resource(
                        *bypass.value());
                    QVERIFY(observed);
                    QCOMPARE(std::bit_cast<std::uint64_t>(
                        observed.value()->global_linear_peak()),
                        std::bit_cast<std::uint64_t>(
                            before.value()->global_linear_peak()));
                    QVERIFY(observed.value()->global_linear_peak()>ceiling);
                }
            }
        }
    }
}
void TruePeakLimiterVerificationTest::bypassAndEnabledLimiterAreObservablyDistinct()
{
    // Deliberately unbounded first/last impulses. The disabled path remains
    // untouched while the enabled Core must attenuate and may then use the
    // one-shot M16 verifier. Bypass must NOT accidentally invoke this verifier.
    const int fs=44100;
    constexpr double ceilingDb=-12.0;
    auto programme=buffer(fs,true,319,19'000'021);
    for(std::size_t ch=0;ch<2;++ch) {
        auto lane=programme.mutable_view().channel(ch);
        QVERIFY(lane);
        for(int i=0;i<319;++i)
            (*lane.value())[static_cast<std::size_t>(i)]=
                i==0?(ch==0?4.0:-4.0):
                (i==318?(ch==0?-3.0:3.0):+0.0);
    }
    const auto inactive=m17_verifier::bypass_full_resource(programme);
    QVERIFY(inactive);
    const std::array<std::size_t,3> partition{1,37,301};
    auto enabled=renderRealM17(programme,ceilingDb,partition);
    QVERIFY(enabled.has_value());
    const auto before=analysis::measure_true_peak_full_resource(
        *inactive.value());
    QVERIFY(before);
    QVERIFY(before.value()->global_linear_peak()>
        std::pow(10.0,ceilingDb/20.0));
    const auto verdict=m17_verifier::verify_full_resource(*enabled,ceilingDb);
    QVERIFY(verdict);
    QVERIFY(verdict.value()->after_peak<=std::pow(10.0,ceilingDb/20.0));
    bool diverged=false;
    for(std::size_t ch=0;ch<2;++ch) {
        const auto bypassLane=inactive.value()->view().channel(ch);
        const auto activeLane=enabled->view().channel(ch);
        const auto source=programme.view().channel(ch);
        QVERIFY(bypassLane && activeLane && source);
        for(int i=0;i<319;++i) {
            const auto old=std::bit_cast<std::uint64_t>(
                (*source.value())[static_cast<std::size_t>(i)]);
            QCOMPARE(std::bit_cast<std::uint64_t>(
                (*bypassLane.value())[static_cast<std::size_t>(i)]),old);
            diverged |= std::bit_cast<std::uint64_t>(
                (*activeLane.value())[static_cast<std::size_t>(i)])!=old;
        }
    }
    QVERIFY2(diverged,"Enabled M17 had no observable change over hot bypass");
    // A direct M16 verification on BYP would need >0.10dB of global
    // attenuation and is deliberately NOT performed by the inactive path.
    auto verifierAttempt=m17_verifier::bypass_full_resource(programme);
    QVERIFY(verifierAttempt);
    const auto rejected=m17_verifier::verify_full_resource(
        *verifierAttempt.value(),ceilingDb);
    QVERIFY(!rejected);
    QCOMPARE(category(*rejected.error()),
        std::string_view{"LIMITER_VERIFICATION_GUARD_EXCEEDED"});
    for(std::size_t ch=0;ch<2;++ch) {
        const auto lane=verifierAttempt.value()->view().channel(ch);
        const auto src=programme.view().channel(ch);
        QVERIFY(lane && src);
        for(int i=0;i<319;++i)
            QCOMPARE(std::bit_cast<std::uint64_t>(
                (*lane.value())[static_cast<std::size_t>(i)]),
                std::bit_cast<std::uint64_t>(
                    (*src.value())[static_cast<std::size_t>(i)]));
    }
}


void TruePeakLimiterVerificationTest::failedVerificationNeverMutatesResourceAcrossSixRates()
{
    // Before success every path is fail-closed. In particular a failed M16
    // validation may not leak a left-lane trim when the right lane fails.
    const auto bits=[](const audio::AudioBuffer& resource) {
        std::vector<std::vector<std::uint64_t>> image(
            resource.view().format().channel_count());
        for(std::size_t ch=0;ch<image.size();++ch) {
            const auto lane=resource.view().channel(ch);
            if(!lane)return image;
            for(const double x:*lane.value())
                image[ch].push_back(std::bit_cast<std::uint64_t>(x));
        }
        return image;
    };
    for(const int fs:{44100,48000,88200,96000,176400,192000}) {
        for(const bool stereo:{false,true}) {
            auto hot=buffer(fs,stereo,319,5'000'029);
            for(std::size_t ch=0;ch<hot.view().format().channel_count();++ch) {
                auto lane=hot.mutable_view().channel(ch);
                QVERIFY(lane);
                for(std::size_t i=0;i<lane.value()->size();++i)
                    (*lane.value())[i]=i==0U?(ch==0U?4.0:-3.0):
                        (i==1U?-0.0:(i%31U==0U?0.375:-0.125));
            }
            const auto original=bits(hot);
            QVERIFY(!m17_verifier::verify_full_resource(hot,-12.01));
            QVERIFY(bits(hot)==original);
            QVERIFY(!m17_verifier::verify_full_resource(hot,0.01));
            QVERIFY(bits(hot)==original);
            const auto exceeded=m17_verifier::verify_full_resource(hot,-12.0);
            QVERIFY(!exceeded);
            QCOMPARE(category(*exceeded.error()),
                std::string_view{"LIMITER_VERIFICATION_GUARD_EXCEEDED"});
            QVERIFY(bits(hot)==original);
            auto lane=hot.mutable_view().channel(stereo?1U:0U);
            QVERIFY(lane);
            const auto saved=(*lane.value())[67];
            (*lane.value())[67]=std::numeric_limits<double>::quiet_NaN();
            const auto nonfiniteBefore=bits(hot);
            QVERIFY(!m17_verifier::verify_full_resource(hot,-6.0));
            QVERIFY(bits(hot)==nonfiniteBefore);
            (*lane.value())[67]=saved;
            QVERIFY(bits(hot)==original);

            // The M16 meter itself can accept supported numeric values in
            // other domains; Core-first verification MUST reject the domain
            // mismatch before mutating the supplied audio.
            auto wrong=audio::AudioBuffer::create(
                hot.view().format(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
                core::FrameIndex{5'000'029},
                *core::FrameCount::create(3).value());
            QVERIFY(wrong);
            for(std::size_t ch=0;ch<wrong.value()->view().format().channel_count();++ch) {
                auto dst=wrong.value()->mutable_view().channel(ch);
                QVERIFY(dst);
                (*dst.value())[0]=4.0;
                (*dst.value())[1]=-0.0;
                (*dst.value())[2]=-1.0;
            }
            const auto wrongBefore=bits(*wrong.value());
            const auto rejected=m17_verifier::verify_full_resource(
                *wrong.value(),-12.0);
            QVERIFY(!rejected);
            QCOMPARE(category(*rejected.error()),
                std::string_view{"INVALID_M17_VERIFY_DOMAIN"});
            QVERIFY(bits(*wrong.value())==wrongBefore);
        }
    }
}

void TruePeakLimiterVerificationTest::fullResourceSixRateChunkMatrixRemainsBitIdentical()
{
    // U4/U6: six supported Fs, stereo linked and mono, two extreme ceilings
    // and two nontrivial partition schedules. Compare all PCM before AND
    // after the permitted M16 full-resource verification, including the
    // final 8x tail, without attributing the global scalar to base M17 GR.
    for(const int fs:{44100,48000,88200,96000,176400,192000}) {
        const int d=fs/200+8;
        for(const bool stereo:{false,true}) {
            for(const double ceilingDb:{-12.0,0.0}) {
                const int n=2*d+37;
                auto input=buffer(fs,stereo,n,stereo?9'000'029:0);
                for(std::size_t ch=0;ch<input.view().format().channel_count();++ch) {
                    auto lane=input.mutable_view().channel(ch);
                    QVERIFY(lane);
                    for(int i=0;i<n;++i) {
                        const double x=i==0?4.0:(i==n-1?-3.0:
                            (i%113==0?2.25:((i&1)==0?0.1875:-0.1875)));
                        (*lane.value())[static_cast<std::size_t>(i)]=
                            ch==0U?x:(i%71<35?-x*0.8:x*1.25);
                    }
                }
                const std::array<std::size_t,1> flat{4096};
                const std::array<std::size_t,4> irregular{3,47,257,4096};
                auto a=renderRealM17(input,ceilingDb,flat);
                auto b=renderRealM17(input,ceilingDb,irregular);
                QVERIFY(a.has_value() && b.has_value());
                QCOMPARE(a->view().frame_count(),b->view().frame_count());
                QCOMPARE(a->view().absolute_range(),b->view().absolute_range());
                for(std::size_t ch=0;ch<a->view().format().channel_count();++ch) {
                    const auto first=a->view().channel(ch);
                    const auto second=b->view().channel(ch);
                    QVERIFY(first && second);
                    for(int i=0;i<n;++i)
                        QCOMPARE(std::bit_cast<std::uint64_t>(
                            (*first.value())[static_cast<std::size_t>(i)]),
                            std::bit_cast<std::uint64_t>(
                                (*second.value())[static_cast<std::size_t>(i)]));
                }
                const auto ma=analysis::measure_true_peak_full_resource(*a);
                const auto mb=analysis::measure_true_peak_full_resource(*b);
                QVERIFY(ma && mb);
                QCOMPARE(std::bit_cast<std::uint64_t>(
                    ma.value()->global_linear_peak()),
                    std::bit_cast<std::uint64_t>(
                        mb.value()->global_linear_peak()));
                const auto va=m17_verifier::verify_full_resource(*a,ceilingDb);
                const auto vb=m17_verifier::verify_full_resource(*b,ceilingDb);
                QVERIFY2(va && vb,"Valid M17 output exceeded frozen M16 trim guard");
                QCOMPARE(va.value()->trim_used,vb.value()->trim_used);
                QCOMPARE(std::bit_cast<std::uint64_t>(
                    va.value()->global_trim_gain),
                    std::bit_cast<std::uint64_t>(
                        vb.value()->global_trim_gain));
                QCOMPARE(va.value()->status,
                    analysis::TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED);
                for(std::size_t ch=0;ch<a->view().format().channel_count();++ch) {
                    const auto first=a->view().channel(ch);
                    const auto second=b->view().channel(ch);
                    QVERIFY(first && second);
                    for(int i=0;i<n;++i)
                        QCOMPARE(std::bit_cast<std::uint64_t>(
                            (*first.value())[static_cast<std::size_t>(i)]),
                            std::bit_cast<std::uint64_t>(
                                (*second.value())[static_cast<std::size_t>(i)]));
                }
                const auto finalA=analysis::measure_true_peak_full_resource(*a);
                const auto finalB=analysis::measure_true_peak_full_resource(*b);
                QVERIFY(finalA && finalB);
                QCOMPARE(std::bit_cast<std::uint64_t>(
                    finalA.value()->global_linear_peak()),
                    std::bit_cast<std::uint64_t>(
                        finalB.value()->global_linear_peak()));
                QVERIFY(finalA.value()->global_linear_peak()<=
                    std::pow(10.0,ceilingDb/20.0));
            }
        }
    }
}

void TruePeakLimiterVerificationTest::fullResourceResumeGatesAreBitwiseIdentical()
{
    // U4+U5+U6 end-to-end; full PCM owner is TEST-ONLY, never a product claim.
    // Actual module process -> owner-bound same-backend resume -> D drain ->
    // exact N frames -> full M16 peak -> transactional final trim.
    for(const int fs:{44100,48000,88200,96000,176400,192000}) {
        const int d=fs/200+8;
        for(const bool stereo:{false,true})
            for(const double ceilingDb:{-12.0,0.0})
                for(const int n:{d-1,2*d+37}) {
                    auto input=buffer(fs,stereo,n,stereo?17'000'029:0);
                    for(std::size_t ch=0;ch<input.view().format().channel_count();++ch) {
                        const auto lane=input.mutable_view().channel(ch);
                        QVERIFY(lane);
                        for(int i=0;i<n;++i) {
                            const double x=i==0?4.0:(i==n-1?-3.0:
                                (i%113==0?2.25:((i&1)==0?0.1875:-0.1875)));
                            (*lane.value())[static_cast<std::size_t>(i)]=
                                ch==0U?x:(i%71<35?-x*0.8:x*1.25);
                        }
                    }
                    const std::array<std::size_t,1> flat{4096};
                    const std::array<std::size_t,5> irregular{3,47,257,4096,11};
                    auto original=renderRealM17(input,ceilingDb,flat);
                    QVERIFY(original.has_value());
                    const auto peak=analysis::measure_true_peak_full_resource(*original);
                    QVERIFY(peak);
                    for(const std::int64_t at:{std::int64_t{1},
                            static_cast<std::int64_t>(n-1)}) {
                        auto resumed=renderRealM17(input,ceilingDb,irregular,at);
                        QVERIFY2(resumed.has_value(),
                            "Owner-bound midstream checkpoint failed");
                        QCOMPARE(resumed->view().absolute_range(),
                            original->view().absolute_range());
                        QCOMPARE(resumed->view().frame_count(),
                            original->view().frame_count());
                        for(std::size_t ch=0;ch<resumed->view().format().channel_count();++ch) {
                            const auto left=original->view().channel(ch);
                            const auto right=resumed->view().channel(ch);
                            QVERIFY(left && right);
                            for(int i=0;i<n;++i)
                                QCOMPARE(std::bit_cast<std::uint64_t>(
                                    (*left.value())[static_cast<std::size_t>(i)]),
                                    std::bit_cast<std::uint64_t>(
                                        (*right.value())[static_cast<std::size_t>(i)]));
                        }
                        const auto verdict=m17_verifier::verify_full_resource(
                            *resumed,ceilingDb);
                        QVERIFY2(verdict,"Resumed full M16 verification failed");
                        QCOMPARE(verdict.value()->status,
                            analysis::TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED);
                        QCOMPARE(std::bit_cast<std::uint64_t>(
                            verdict.value()->before_peak),
                            std::bit_cast<std::uint64_t>(
                                peak.value()->global_linear_peak()));
                        QVERIFY(m17_verifier::trim_within_guard(
                            verdict.value()->global_trim_gain));
                        const auto measured=analysis::measure_true_peak_full_resource(
                            *resumed);
                        QVERIFY(measured);
                        QCOMPARE(std::bit_cast<std::uint64_t>(
                            measured.value()->global_linear_peak()),
                            std::bit_cast<std::uint64_t>(
                                verdict.value()->after_peak));
                        QVERIFY(measured.value()->global_linear_peak()<=
                            std::pow(10.0,ceilingDb/20.0));
                        for(std::size_t ch=0;ch<resumed->view().format().channel_count();++ch) {
                            const auto before=original->view().channel(ch);
                            const auto after=resumed->view().channel(ch);
                            QVERIFY(before && after);
                            for(int i=0;i<n;++i) {
                                const double x=(*before.value())[static_cast<std::size_t>(i)];
                                const double y=verdict.value()->global_trim_gain==1.0
                                    ?x:x*verdict.value()->global_trim_gain;
                                QCOMPARE(std::bit_cast<std::uint64_t>(
                                    (*after.value())[static_cast<std::size_t>(i)]),
                                    std::bit_cast<std::uint64_t>(y));
                            }
                        }
                    }
                }
    }
}


void TruePeakLimiterVerificationTest::optionalPhaseInstrumentationIsNonInvasive()
{
    // The production checked-candidate renderer emits per-STAGE wall-time
    // diagnostics only. It may not affect PCM, GR, final M16 evidence, or
    // turn any bypass meter into an illicit Ceiling certificate.
    auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    auto params=dsp::TruePeakLimiterParameters::create(-1.0);
    auto block=core::FrameCount::create(64);
    QVERIFY(registry && params && block);
    auto input=buffer(44100,true,257);
    for(std::size_t ch=0;ch<2;++ch) {
        auto lane=input.mutable_view().channel(ch);
        QVERIFY(lane);
        for(int i=0;i<257;++i)
            (*lane.value())[static_cast<std::size_t>(i)]
                =0.12*std::sin(2.0*std::numbers::pi_v<double>*(ch?1301.0:991.0)
                    *static_cast<double>(i)/44100.0);
    }
    for(const bool bypassed:{true,false}) {
        render::TerminalRenderStageTimings stages{};
        const auto instrumented=render::render_true_peak_limiter_checked_candidate(
            input.view(),*params.value(),*registry.value(),*block.value(),
            bypassed,{},&stages);
        const auto uninstrumented=render::render_true_peak_limiter_checked_candidate(
            input.view(),*params.value(),*registry.value(),*block.value(),
            bypassed);
        QVERIFY(instrumented && uninstrumented);
        QVERIFY(stages.m17_core_us>=0);
        QCOMPARE(stages.src_us,std::int64_t{-1});
        QCOMPARE(stages.verification_trim_us,std::int64_t{-1});
        QCOMPARE(stages.m16_secondary_us,std::int64_t{-1});
        if(bypassed) {
            QCOMPARE(stages.m16_primary_us,std::int64_t{-1});
            QVERIFY(!instrumented.value()->final_check);
        } else {
            QVERIFY(stages.m16_primary_us>=0);
            QVERIFY(instrumented.value()->final_check.has_value());
        }
        QCOMPARE(instrumented.value()->final_check.has_value(),
            uninstrumented.value()->final_check.has_value());
        const auto lhs=instrumented.value()->terminal.compensated_pcm.view();
        const auto rhs=uninstrumented.value()->terminal.compensated_pcm.view();
        QCOMPARE(lhs.frame_count(),rhs.frame_count());
        for(std::size_t ch=0;ch<2;++ch) {
            const auto x=lhs.channel(ch);
            const auto y=rhs.channel(ch);
            QVERIFY(x && y);
            for(std::size_t i=0;i<x.value()->size();++i)
                QCOMPARE(std::bit_cast<std::uint64_t>((*x.value())[i]),
                    std::bit_cast<std::uint64_t>((*y.value())[i]));
        }
        QCOMPARE(instrumented.value()->terminal.applied_gain_frames.size(),
            uninstrumented.value()->terminal.applied_gain_frames.size());
    }
}

void TruePeakLimiterVerificationTest::boundedActualM17CostProbe()
{
    // MEASUREMENT ONLY: exact production M17 DSP module via the already
    // established test renderer, then exact M16 full-resource measurement.
    // This does not change the frozen recurrence or enforce timing budgets.
    // N=1024 intentionally keeps Windows/Linux Focused CI bounded; never
    // launch the 12-second, 7-billion-event case inside normal CTest.
    // Qt environment lookup is portable under strict MSVC /WX (no C4996).
    const auto summaryPath=qEnvironmentVariable("GITHUB_STEP_SUMMARY").toStdString();
    // Bounded machine-readable timings, separate from full workflow logs.
    const auto probePath=qEnvironmentVariable("RGS_M17_CPU_PROBE_FILE").toStdString();
    if(!probePath.empty()) {
        std::ofstream probe(probePath,std::ios::trunc);
        QVERIFY2(probe.good(),"M17 CPU probe artifact could not be opened");
        probe<<"sample_rate_hz,ceiling_dbtp,m17_renderer_us,m16_meter_us,frames\n";
    }
    if(!summaryPath.empty()) {
        std::ofstream summary(summaryPath,std::ios::app);
        if(summary) summary
            << "\n### M17 bounded CPU profile (diagnostic only)\n\n"
            << "Source: synthetic 1024-frame stereo signal, actual DSP module. "
               "Durations are runner-specific and NOT FQ/physical audio PASS.\n\n"
            << "| Sample rate | Ceiling | M17 + test renderer | M16 full measurement |\n"
            << "|---|---:|---:|---:|\n";
    }
    constexpr int n=1024;
    constexpr double pi=std::numbers::pi_v<double>;
    const std::array<std::size_t,1> chunks{1024};
    for(const int fs:{44100,48000}) {
        auto input=buffer(fs,true,n);
        for(std::size_t ch=0;ch<2;++ch) {
            auto lane=input.mutable_view().channel(ch);
            QVERIFY(lane);
            for(int i=0;i<n;++i) {
                const double t=static_cast<double>(i)/static_cast<double>(fs);
                (*lane.value())[static_cast<std::size_t>(i)]=
                    0.70*std::sin(2.0*pi*(ch==0?991.0:1301.0)*t)
                    +0.15*std::sin(2.0*pi*(ch==0?2351.0:1783.0)*t+0.25);
            }
        }
        for(const double ceiling:{0.0,-1.0,-6.0,-12.0}) {
            const auto start=std::chrono::steady_clock::now();
            const auto processed=renderRealM17(input,ceiling,chunks);
            const auto dspEnd=std::chrono::steady_clock::now();
            QVERIFY(processed.has_value());
            const auto measured=analysis::measure_true_peak_full_resource(
                *processed);
            const auto m16End=std::chrono::steady_clock::now();
            QVERIFY(measured);
            const auto dspUs=std::chrono::duration_cast<
                std::chrono::microseconds>(dspEnd-start).count();
            const auto meterUs=std::chrono::duration_cast<
                std::chrono::microseconds>(m16End-dspEnd).count();
            qInfo().nospace()<<"RGS M17 CPU probe Fs="<<fs
                <<" ceiling="<<ceiling<<" M17_us="<<dspUs
                <<" M16_us="<<meterUs<<" frames="<<n;
            if(!probePath.empty()) {
                std::ofstream probe(probePath,std::ios::app);
                QVERIFY2(probe.good(),"M17 CPU probe append failed");
                probe<<fs<<','<<ceiling<<','<<dspUs<<','
                    <<meterUs<<','<<n<<'\n';
            }
            if(!summaryPath.empty()) {
                std::ofstream summary(summaryPath,std::ios::app);
                if(summary)summary<<"| "<<fs<<" Hz | "<<ceiling
                    <<" dBTP | "<<dspUs/1000.0<<" ms | "
                    <<meterUs/1000.0<<" ms |\n";
            }
        }
    }

    // Phase-1 performance diagnostic: isolate block/transactional overhead
    // from the frozen controller's dense lookahead workload. This is NOT an
    // algorithm switch or a timing acceptance gate. Both partitions must
    // produce the identical binary64 PCM at the same difficult Ceiling.
    //
    // 2048 frames keeps Focused CI bounded, independently of test runner.
    // In particular, never extrapolate this micro-probe into a real-time
    // certification or a full-track completion promise.
    constexpr std::int64_t profileFrames=2048;
    const std::array<std::size_t,1> fineBlocks{128};
    const std::array<std::size_t,1> coarseBlocks{2048};
    const auto partitionPath=qEnvironmentVariable(
        "RGS_M17_PARTITION_PROBE_FILE").toStdString();
    if(!partitionPath.empty()) {
        std::ofstream probe(partitionPath,std::ios::trunc);
        QVERIFY2(probe.good(),"M17 partition probe artifact cannot be opened");
        probe<<"sample_rate_hz,ceiling_dbtp,frames,fine_block_frames,"
                "coarse_block_frames,fine_us,coarse_us,ratio_fine_over_coarse\n";
    }
    if(!summaryPath.empty()) {
        std::ofstream summary(summaryPath,std::ios::app);
        if(summary)summary
            << "\n### M17 block partition diagnostic (test-only)\n\n"
            << "2048 frames stereo; identical samples with 128- and "
               "2048-frame processing partitions. No DSP math edits; "
               "runner-specific timings only.\n\n"
            << "| Rate | Ceiling | 128-frame blocks | 2048-frame blocks | "
               "Fine/coarse cost ratio |\n"
            << "|---|---:|---:|---:|---:|\n";
    }
    for(const int fs : {44100,48000}) {
        auto input=buffer(fs,true,static_cast<int>(profileFrames));
        for(std::size_t ch=0;ch<2;++ch) {
            auto lane=input.mutable_view().channel(ch);
            QVERIFY(lane);
            for(std::int64_t frame=0;frame<profileFrames;++frame) {
                const double time=static_cast<double>(frame)
                    /static_cast<double>(fs);
                (*lane.value())[static_cast<std::size_t>(frame)]=
                    0.70*std::sin(2.0*pi*(ch==0?991.0:1301.0)*time)
                    +0.15*std::sin(2.0*pi*(ch==0?2351.0:1783.0)*time+0.25);
            }
        }
        for(const double ceiling : {-1.0,-6.0,-12.0}) {
            const auto started=std::chrono::steady_clock::now();
            const auto fine=renderRealM17(input,ceiling,fineBlocks);
            const auto mid=std::chrono::steady_clock::now();
            const auto coarse=renderRealM17(input,ceiling,coarseBlocks);
            const auto finished=std::chrono::steady_clock::now();
            QVERIFY(fine.has_value() && coarse.has_value());
            const auto fineUs=std::chrono::duration_cast<
                std::chrono::microseconds>(mid-started).count();
            const auto coarseUs=std::chrono::duration_cast<
                std::chrono::microseconds>(finished-mid).count();
            const auto a=fine->view(),b=coarse->view();
            QCOMPARE(a.frame_count(),b.frame_count());
            for(std::size_t ch=0;ch<2;++ch) {
                auto lhs=a.channel(ch),rhs=b.channel(ch);
                QVERIFY(lhs && rhs);
                QCOMPARE(lhs.value()->size(),rhs.value()->size());
                for(std::size_t frame=0;frame<lhs.value()->size();++frame)
                    QCOMPARE(std::bit_cast<std::uint64_t>((*lhs.value())[frame]),
                             std::bit_cast<std::uint64_t>((*rhs.value())[frame]));
            }
            const double ratio=coarseUs>0
                ?static_cast<double>(fineUs)/static_cast<double>(coarseUs)
                :0.0;
            qInfo().nospace()<<"RGS M17 block_partition Fs="<<fs
                <<" ceiling="<<ceiling<<" fine_us="<<fineUs
                <<" coarse_us="<<coarseUs<<" ratio="<<ratio;
            if(!partitionPath.empty()) {
                std::ofstream probe(partitionPath,std::ios::app);
                QVERIFY2(probe.good(),"M17 partition probe append failed");
                probe<<fs<<','<<ceiling<<','<<profileFrames<<','
                    <<fineBlocks[0]<<','<<coarseBlocks[0]<<','
                    <<fineUs<<','<<coarseUs<<','<<ratio<<'\n';
            }
            if(!summaryPath.empty()) {
                std::ofstream summary(summaryPath,std::ios::app);
                if(summary)summary<<"| "<<fs<<" Hz | "<<ceiling
                    <<" dBTP | "<<fineUs/1000.0<<" ms | "
                    <<coarseUs/1000.0<<" ms | "<<ratio<<"x |\n";
            }
        }
    }
}


void TruePeakLimiterVerificationTest::boundedLookaheadMemoizationProbe()
{
    // EXPERIMENTAL TEST ONLY. The M17 controller is unmodified. Compare the
    // original per-event demand_db/log10 + Hann/cos attack-floor scan against
    // a cache of the SAME binary64 expressions, in the SAME event order.
    // Identity of attack floors is necessary, not sufficient, for any
    // future production qualification. Nothing here renders PCM.
    const auto outPath=qEnvironmentVariable(
        "RGS_M17_LOOKAHEAD_PROBE_FILE").toStdString();
    if(!outPath.empty()) {
        std::ofstream csv(outPath,std::ios::trunc);
        QVERIFY2(csv.good(),"M17 lookahead probe artifact cannot be created");
        csv<<"profile,sample_rate_hz,ceiling_dbtp,controller_ticks,"
              "evaluated_future_events,original_us,memoized_us,"
              "ratio_original_over_memoized,attack_floor_bitwise,"
              "valid_detector_events,active_detector_events,"
              "max_events_per_horizon,controller_attack_floor_bitwise\n";
    }
    const auto summaryPath=qEnvironmentVariable("GITHUB_STEP_SUMMARY").toStdString();
    if(!summaryPath.empty()) {
        std::ofstream summary(summaryPath,std::ios::app);
        if(summary)summary
            << "\n### M17 frozen-controller lookahead memoization experiment "
               "(TEST ONLY)\n\n"
            << "128 consecutive controller ticks, dense synthetic event stream. "
               "Original log10+cos expression versus a cache of the same "
               "expressions, unchanged event order. These are attack floors, "
               "NOT a proposed DSP implementation or full PCM oracle.\n\n"
            << "| Fs | Ceiling | Original | Memoized incl. setup | Ratio | "
               "Binary64 identical |\n"
            << "|---|---:|---:|---:|---:|:---:|\n";
    }

    constexpr std::int64_t ticksToProbe=128;
    constexpr double pi=std::numbers::pi_v<double>;
    for(const std::int64_t fs:{44100,48000}) {
        for(const double ceilingDb:{-1.0,-6.0,-12.0}) {
            const auto control=dsp::internal::TruePeakLimiterControllerState::create(
                fs,ceilingDb);
            QVERIFY(control);
            const auto horizon=control.value()->lookahead_ticks();
            QVERIFY(horizon>0);
            const double ceilingLinear=std::pow(10.0,ceilingDb/20.0);
            const auto demand=[ceilingLinear](double peak) {
                return peak>ceilingLinear
                    ? 20.0*std::log10(peak/ceilingLinear) : +0.0;
            };
            // One event at every physical 8x tick: an upper-density stress
            // envelope, not an assumption about typical detector output.
            const auto end=horizon+ticksToProbe;
            std::vector<double> peaks(static_cast<std::size_t>(end+1));
            for(std::int64_t tick=0;tick<=end;++tick)
                peaks[static_cast<std::size_t>(tick)]=
                    0.20+static_cast<double>((tick*37+11)%107)/100.0;

            std::vector<double> original;
            std::vector<double> memoized;
            original.reserve(static_cast<std::size_t>(ticksToProbe));
            memoized.reserve(static_cast<std::size_t>(ticksToProbe));
            std::int64_t evaluations=0;
            const auto start=std::chrono::steady_clock::now();
            for(std::int64_t m=0;m<ticksToProbe;++m) {
                double attack=demand(peaks[static_cast<std::size_t>(m)]);
                for(std::int64_t tick=m+1;tick<=m+horizon;++tick) {
                    ++evaluations;
                    const double required=demand(peaks[static_cast<std::size_t>(tick)]);
                    if(required>0.0) {
                        const double distance=static_cast<double>(tick-m);
                        const double weight=0.5*(1.0+std::cos(
                            pi*distance/static_cast<double>(horizon)));
                        attack=std::max(attack,required*weight);
                    }
                }
                original.push_back(attack);
            }
            const auto baselineEnd=std::chrono::steady_clock::now();

            // Setup time is INCLUDED in the cached candidate's timing.
            const auto cacheStart=std::chrono::steady_clock::now();
            std::vector<double> cachedDemands(peaks.size());
            for(std::size_t i=0;i<peaks.size();++i)
                cachedDemands[i]=demand(peaks[i]);
            std::vector<double> cachedWeights(static_cast<std::size_t>(horizon+1));
            for(std::int64_t lag=1;lag<=horizon;++lag) {
                const double distance=static_cast<double>(lag);
                cachedWeights[static_cast<std::size_t>(lag)]=
                    0.5*(1.0+std::cos(
                        pi*distance/static_cast<double>(horizon)));
            }
            for(std::int64_t m=0;m<ticksToProbe;++m) {
                double attack=cachedDemands[static_cast<std::size_t>(m)];
                for(std::int64_t tick=m+1;tick<=m+horizon;++tick) {
                    const double required=
                        cachedDemands[static_cast<std::size_t>(tick)];
                    if(required>0.0) {
                        const auto lag=static_cast<std::size_t>(tick-m);
                        attack=std::max(attack,
                            required*cachedWeights[lag]);
                    }
                }
                memoized.push_back(attack);
            }
            const auto cachedEnd=std::chrono::steady_clock::now();
            QCOMPARE(original.size(),memoized.size());
            for(std::size_t m=0;m<original.size();++m)
                QCOMPARE(std::bit_cast<std::uint64_t>(original[m]),
                         std::bit_cast<std::uint64_t>(memoized[m]));
            const auto originalUs=std::chrono::duration_cast<
                std::chrono::microseconds>(baselineEnd-start).count();
            const auto cachedUs=std::chrono::duration_cast<
                std::chrono::microseconds>(cachedEnd-cacheStart).count();
            const double ratio=cachedUs>0
                ? static_cast<double>(originalUs)/static_cast<double>(cachedUs)
                : 0.0;
            qInfo().nospace()<<"RGS M17 TEST-ONLY lookahead Fs="<<fs
                <<" ceiling="<<ceilingDb<<" ticks="<<ticksToProbe
                <<" events="<<evaluations<<" original_us="<<originalUs
                <<" cached_us="<<cachedUs<<" ratio="<<ratio;
            if(!outPath.empty()) {
                std::ofstream csv(outPath,std::ios::app);
                QVERIFY2(csv.good(),"M17 lookahead probe append failed");
                csv<<"synthetic_8x,"<<fs<<','<<ceilingDb<<','<<ticksToProbe<<','
                    <<evaluations<<','<<originalUs<<','<<cachedUs
                    <<','<<ratio<<",PASS,,,,\n";
            }
            if(!summaryPath.empty()) {
                std::ofstream summary(summaryPath,std::ios::app);
                if(summary)summary<<"| "<<fs<<" | "<<ceilingDb<<" dBTP | "
                    <<originalUs/1000.0<<" ms | "<<cachedUs/1000.0
                    <<" ms | "<<ratio<<"x | YES |\n";
            }
        }
    }
}


// TEST ONLY: feed the very same signed 4x+midpoint detector used by M17.
// The reference is the unmodified controller.advance(), not an approximate
// mathematical model. A quiet, impulse-sparse, and dense source distinguish
// event geometry from an artificial one-event-per-tick upper-density envelope.
// All ratios are diagnostic, not realtime/FQ gates. No product DSP is changed.
void TruePeakLimiterVerificationTest::detectorSourcedLookaheadMemoizationProbe()
{
    using Event=dsp::internal::LimiterDetectorEvent;
    // Bound test-only input to the certified horizon even at 192kHz.
    constexpr int testedTicks=128;
    constexpr double pi=std::numbers::pi_v<double>;
    struct Window {
        std::int64_t tick{0};
        std::size_t first{0};
        std::size_t last{0};
        double current{0.0};
    };
    const auto artifact=qEnvironmentVariable(
        "RGS_M17_LOOKAHEAD_PROBE_FILE").toStdString();
    const auto boundsPath=qEnvironmentVariable(
        "RGS_M17_BOUND_PROBE_FILE").toStdString();
    if(!boundsPath.empty()) {
        std::ofstream csv(boundsPath,std::ios::trunc);
        QVERIFY2(csv.good(),"M17 bound probe artifact cannot be created");
        csv<<"profile,sample_rate_hz,ceiling_dbtp,controller_ticks,"
              "active_detector_events,future_evaluations,original_us,"
              "memoized_us,bounded_us,ratio_original_over_bounded,"
              "ratio_memoized_over_bounded,groups_checked,"
              "visited_events,skipped_events,attack_floor_bitwise,"
              "controller_attack_floor_bitwise,adaptive_us,"
              "ratio_original_over_adaptive,ratio_bounded_over_adaptive,"
              "adaptive_bound_windows,adaptive_fallback_windows,"
              "adaptive_attack_floor_bitwise\n";
    }
    const auto summaryPath=qEnvironmentVariable(
        "GITHUB_STEP_SUMMARY").toStdString();
    if(!summaryPath.empty()) {
        std::ofstream summary(summaryPath,std::ios::app);
        if(summary)summary
            << "\n### M17 REAL detector lookahead experiment (TEST ONLY)\n\n"
            << "Rate-dependent stereo input (at least 320 frames); "
               "128 controller ticks (256 for peak-then-quiet); six rates and five profiles; "
               "genuine M17 4x + FIR16 detector. The reference controller "
               "is advanced normally; candidate attack-floor scans never "
               "modify production state. Setup is included in cached time. "
               "This is NOT a PCM/render speedup or qualification.\n\n"
            << "| Fs | Ceiling | Input | Above-ceiling events | "
               "Future evaluations | Original | Cached | Ratio | "
               "Bitwise attack-floor | Controller oracle |\n"
            << "|---|---:|---|---:|---:|---:|---:|---:|:---:|:---:|\n";
    }
    for(const std::int64_t fs:{44100,48000,88200,96000,176400,192000}) {
        for(const double ceilingDb:{-1.0,-6.0,-12.0}) {
            const auto reference=dsp::internal::
                TruePeakLimiterControllerState::create(fs,ceilingDb);
            QVERIFY(reference);
            const auto horizon=reference.value()->lookahead_ticks();
            const int frames=std::max(320,static_cast<int>(
                (horizon+testedTicks+208)/8));
            const auto linearCeiling=std::pow(10.0,ceilingDb/20.0);
            const auto demand=[linearCeiling](const double peak) {
                return peak>linearCeiling
                    ? 20.0*std::log10(peak/linearCeiling) : +0.0;
            };
            for(int profile=0;profile<5;++profile) {
                const char* profileName=profile==0?"quiet":
                    (profile==1?"impulse_sparse":
                     profile==2?"music_like_dense":
                     profile==3?"mixed_transients":"peak_then_quiet");
                // Explicitly outlive the early transient's entire
                // detector/lookahead overlap to exercise both paths.
                const int profileTicks=profile==4?256:testedTicks;
                dsp::internal::TruePeakLimiterDetectorState detector{2};
                std::vector<Event> events;
                events.reserve(frames*8U);
                std::int64_t lastValidTick=std::numeric_limits<std::int64_t>::min();
                std::int64_t validCount=0;
                const auto accept=[&](const Event& event) {
                    if(!event.valid)return true;
                    if(event.tick<=lastValidTick
                       || !std::isfinite(event.peak) || event.peak<0.0)
                        return false;
                    lastValidTick=event.tick;
                    ++validCount;
                    if(event.peak>linearCeiling)events.push_back(event);
                    return true;
                };
                for(int frame=0;frame<frames;++frame) {
                    const double t=static_cast<double>(frame)/
                        static_cast<double>(fs);
                    double left=+0.0,right=+0.0;
                    if(profile==1 && frame==41) {
                        left=1.25;
                        right=-1.20;
                    } else if(profile==2) {
                        left=0.91*std::sin(2.0*pi*991.0*t)
                            +0.37*std::sin(2.0*pi*2351.0*t+0.25);
                        right=0.95*std::sin(2.0*pi*1301.0*t)
                            -0.32*std::sin(2.0*pi*1783.0*t+0.41);
                    } else if(profile==3) {
                        // Signed FIR cancellations and varying peak density.
                        left=0.38*std::sin(2.0*pi*599.0*t)
                            +(frame%11==0?1.32:-0.11);
                        right=0.43*std::sin(2.0*pi*877.0*t)
                            +(frame%13==0?-1.27:0.07);
                    } else if(profile==4 && frame<18) {
                        left=1.26*std::sin(2.0*pi*1193.0*t)+0.48;
                        right=-1.28*std::sin(2.0*pi*1477.0*t)-0.46;
                    }
                    auto quarters=detector.push_programme_frame(left,right);
                    QVERIFY(quarters);
                    for(const auto& quarter:*quarters.value()) {
                        QVERIFY(accept(quarter.preceding_anchor));
                        QVERIFY(accept(quarter.midpoint));
                    }
                }
                QVERIFY(lastValidTick>=-62+(profileTicks-1)+horizon);
                std::vector<Window> windows;
                windows.reserve(profileTicks);
                std::size_t cursor=0;
                std::int64_t evaluated=0;
                std::size_t maxPerHorizon=0;
                for(int i=0;i<profileTicks;++i) {
                    const std::int64_t m=-62+i;
                    while(cursor<events.size() && events[cursor].tick<m)
                        ++cursor;
                    const bool currentIsEvent=
                        cursor<events.size() && events[cursor].tick==m;
                    const std::size_t first=cursor+(currentIsEvent?1U:0U);
                    std::size_t last=first;
                    while(last<events.size()
                          && events[last].tick<=m+horizon)++last;
                    windows.push_back(Window{m,first,last,currentIsEvent
                        ? events[cursor].peak : +0.0});
                    const auto count=last-first;
                    maxPerHorizon=std::max(maxPerHorizon,count);
                    evaluated+=static_cast<std::int64_t>(count);
                }

                // Reference expression and ascending event order, timed.
                std::vector<double> original;
                original.reserve(windows.size());
                const auto originalStart=std::chrono::steady_clock::now();
                for(const auto& window:windows) {
                    double attack=demand(window.current);
                    for(std::size_t idx=window.first;idx<window.last;++idx) {
                        const double required=demand(events[idx].peak);
                        if(required>0.0) {
                            const double d=static_cast<double>(
                                events[idx].tick-window.tick);
                            const double weight=0.5*(1.0+std::cos(
                                std::numbers::pi_v<double>*d/
                                static_cast<double>(horizon)));
                            attack=std::max(attack,required*weight);
                        }
                    }
                    original.push_back(attack);
                }
                const auto originalEnd=std::chrono::steady_clock::now();

                // Independently validate against the REAL frozen controller.
                auto controller=dsp::internal::
                    TruePeakLimiterControllerState::create(fs,ceilingDb);
                QVERIFY(controller);
                for(std::size_t i=0;i<windows.size();++i) {
                    const auto& window=windows[i];
                    const std::span<const Event> future=window.first==window.last
                        ? std::span<const Event>{}
                        : std::span<const Event>{events.data()+window.first,
                                                window.last-window.first};
                    auto result=controller.value()->advance(window.current,future);
                    QVERIFY(result);
                    QCOMPARE(result.value()->tick,window.tick);
                    QCOMPARE(std::bit_cast<std::uint64_t>(
                                 result.value()->attack_floor_db),
                             std::bit_cast<std::uint64_t>(original[i]));
                }
                QCOMPARE(controller.value()->next_tick(),
                         std::int64_t{-62+profileTicks});

                // Same exact binary64 expressions, precomputed once; cache
                // construction IS INCLUDED in the candidate wall time.
                const auto cacheStart=std::chrono::steady_clock::now();
                std::vector<double> cachedDemands(events.size());
                for(std::size_t i=0;i<events.size();++i)
                    cachedDemands[i]=demand(events[i].peak);
                std::vector<double> cachedWeights(
                    static_cast<std::size_t>(horizon+1));
                for(std::int64_t lag=1;lag<=horizon;++lag) {
                    const double distance=static_cast<double>(lag);
                    cachedWeights[static_cast<std::size_t>(lag)]=
                        0.5*(1.0+std::cos(
                            std::numbers::pi_v<double>*distance/
                            static_cast<double>(horizon)));
                }
                std::vector<double> memoized;
                memoized.reserve(windows.size());
                for(const auto& window:windows) {
                    double attack=demand(window.current);
                    for(std::size_t idx=window.first;idx<window.last;++idx) {
                        const double required=cachedDemands[idx];
                        if(required>0.0) {
                            const auto lag=static_cast<std::size_t>(
                                events[idx].tick-window.tick);
                            attack=std::max(attack,
                                required*cachedWeights[lag]);
                        }
                    }
                    memoized.push_back(attack);
                }
                const auto cacheEnd=std::chrono::steady_clock::now();
                QCOMPARE(memoized.size(),original.size());
                for(std::size_t i=0;i<original.size();++i)
                    QCOMPARE(std::bit_cast<std::uint64_t>(original[i]),
                             std::bit_cast<std::uint64_t>(memoized[i]));
                // TEST ONLY: certified conservative upper envelope.
                // A 32-event group can be skipped only if even its maximum
                // demand multiplied by the maximum actual cached Hann weight
                // over ALL remaining lags is STRICTLY below attack.
                // Superset group maxima and suffix maxima are intentionally
                // conservative, not heuristic. Binary64 multiplication of
                // finite nonnegative inputs is monotone in both operands.
                // Surviving events retain ORIGINAL ascending order and FP ops.
                constexpr std::size_t kBoundGroup=32U;
                const auto boundStart=std::chrono::steady_clock::now();
                std::vector<double> boundDemands(events.size());
                for(std::size_t i=0;i<events.size();++i)
                    boundDemands[i]=demand(events[i].peak);
                std::vector<double> boundWeights(
                    static_cast<std::size_t>(horizon+1));
                for(std::int64_t lag=1;lag<=horizon;++lag) {
                    const double d=static_cast<double>(lag);
                    boundWeights[static_cast<std::size_t>(lag)]=
                        0.5*(1.0+std::cos(
                            std::numbers::pi_v<double>*d/
                            static_cast<double>(horizon)));
                }
                std::vector<double> suffixMaximum(
                    static_cast<std::size_t>(horizon+2),+0.0);
                for(std::int64_t lag=horizon;lag>=1;--lag) {
                    const auto idx=static_cast<std::size_t>(lag);
                    suffixMaximum[idx]=std::max(
                        suffixMaximum[idx+1],boundWeights[idx]);
                }
                std::vector<double> groupMaximum(
                    (events.size()+kBoundGroup-1)/kBoundGroup,+0.0);
                for(std::size_t i=0;i<boundDemands.size();++i)
                    groupMaximum[i/kBoundGroup]=std::max(
                        groupMaximum[i/kBoundGroup],boundDemands[i]);
                std::int64_t visited=0,skipped=0,checked=0;
                std::vector<double> bounded;
                bounded.reserve(windows.size());
                for(const auto& window:windows) {
                    double attack=demand(window.current);
                    std::size_t scanIndex=window.first;
                    while(scanIndex<window.last) {
                        const auto group=scanIndex/kBoundGroup;
                        const auto end=std::min(window.last,
                            (group+1)*kBoundGroup);
                        const auto lag=static_cast<std::size_t>(
                            events[scanIndex].tick-window.tick);
                        const double upper=groupMaximum[group]
                            *suffixMaximum[lag];
                        ++checked;
                        if(upper<attack) {
                            skipped+=static_cast<std::int64_t>(end-scanIndex);
                            scanIndex=end;
                            continue;
                        }
                        for(;scanIndex<end;++scanIndex) {
                            ++visited;
                            const double required=boundDemands[scanIndex];
                            if(required>0.0) {
                                const auto d=static_cast<std::size_t>(
                                    events[scanIndex].tick-window.tick);
                                attack=std::max(attack,
                                    required*boundWeights[d]);
                            }
                        }
                    }
                    bounded.push_back(attack);
                }
                const auto boundEnd=std::chrono::steady_clock::now();
                QCOMPARE(visited+skipped,evaluated);
                QCOMPARE(bounded.size(),original.size());
                for(std::size_t i=0;i<original.size();++i)
                    QCOMPARE(std::bit_cast<std::uint64_t>(original[i]),
                             std::bit_cast<std::uint64_t>(bounded[i]));
                const auto boundUs=std::chrono::duration_cast<
                    std::chrono::microseconds>(boundEnd-boundStart).count();
                const auto baselineUs=std::chrono::duration_cast<
                    std::chrono::microseconds>(originalEnd-originalStart).count();
                const auto memoUs=std::chrono::duration_cast<
                    std::chrono::microseconds>(cacheEnd-cacheStart).count();
                const double ratio=memoUs>0
                    ? static_cast<double>(baselineUs)/static_cast<double>(memoUs)
                    : 0.0;

                // TEST ONLY, no change to frozen controller. Build bound
                // structures only for dense detector windows. Fallback
                // executes the same ordered floating-point expressions as
                // the original without allocation or precomputation.
                constexpr std::size_t kAdaptiveMinEvents=64U;
                const auto adaptiveStart=std::chrono::steady_clock::now();
                const bool adaptiveBound=std::any_of(windows.begin(),
                    windows.end(),[](const Window& w) {
                        return w.last-w.first>=kAdaptiveMinEvents;
                    });
                std::int64_t boundWindowCount=0, directWindowCount=0;
                std::vector<double> adaptive;
                adaptive.reserve(windows.size());
                if(!adaptiveBound) {
                    for(const auto& window:windows) {
                        ++directWindowCount;
                        double attack=demand(window.current);
                        for(std::size_t index=window.first;
                            index<window.last;++index) {
                            const double required=demand(events[index].peak);
                            if(required>0.0) {
                                const double delta=static_cast<double>(
                                    events[index].tick-window.tick);
                                const double weight=0.5*(1.0+std::cos(
                                    std::numbers::pi_v<double>*delta/
                                    static_cast<double>(horizon)));
                                attack=std::max(attack,required*weight);
                            }
                        }
                        adaptive.push_back(attack);
                    }
                } else {
                    // Include full cache setup, not the previous test's
                    // precomputation or controller state.
                    std::vector<double> adaptiveDemand(events.size());
                    for(std::size_t i=0;i<events.size();++i)
                        adaptiveDemand[i]=demand(events[i].peak);
                    std::vector<double> adaptiveWeight(
                        static_cast<std::size_t>(horizon+1));
                    for(std::int64_t lag=1;lag<=horizon;++lag) {
                        const double delta=static_cast<double>(lag);
                        adaptiveWeight[static_cast<std::size_t>(lag)]=
                            0.5*(1.0+std::cos(
                                std::numbers::pi_v<double>*delta/
                                static_cast<double>(horizon)));
                    }
                    std::vector<double> adaptiveSuffix(
                        static_cast<std::size_t>(horizon+2),+0.0);
                    for(std::int64_t lag=horizon;lag>=1;--lag) {
                        const auto offset=static_cast<std::size_t>(lag);
                        adaptiveSuffix[offset]=std::max(
                            adaptiveSuffix[offset+1],adaptiveWeight[offset]);
                    }
                    std::vector<double> adaptiveMaximum(
                        (events.size()+kBoundGroup-1)/kBoundGroup,+0.0);
                    for(std::size_t i=0;i<events.size();++i)
                        adaptiveMaximum[i/kBoundGroup]=std::max(
                            adaptiveMaximum[i/kBoundGroup],
                            adaptiveDemand[i]);
                    for(const auto& window:windows) {
                        if(window.last-window.first<kAdaptiveMinEvents) {
                            ++directWindowCount;
                            double attack=demand(window.current);
                            for(std::size_t i=window.first;i<window.last;++i) {
                                const double required=demand(events[i].peak);
                                if(required>0.0) {
                                    const double delta=static_cast<double>(
                                        events[i].tick-window.tick);
                                    const double weight=0.5*(1.0+std::cos(
                                        std::numbers::pi_v<double>*delta/
                                        static_cast<double>(horizon)));
                                    attack=std::max(attack,required*weight);
                                }
                            }
                            adaptive.push_back(attack);
                            continue;
                        }
                        ++boundWindowCount;
                        double attack=demand(window.current);
                        std::size_t adaptiveIndex=window.first;
                        while(adaptiveIndex<window.last) {
                            const auto group=adaptiveIndex/kBoundGroup;
                            const auto stop=std::min(window.last,
                                (group+1)*kBoundGroup);
                            const auto offset=static_cast<std::size_t>(
                                events[adaptiveIndex].tick-window.tick);
                            const double upper=adaptiveMaximum[group]
                                *adaptiveSuffix[offset];
                            if(upper<attack) {
                                adaptiveIndex=stop;
                                continue;
                            }
                            for(;adaptiveIndex<stop;++adaptiveIndex) {
                                const double required=
                                    adaptiveDemand[adaptiveIndex];
                                if(required>0.0) {
                                    const auto delta=static_cast<std::size_t>(
                                        events[adaptiveIndex].tick-window.tick);
                                    attack=std::max(attack,
                                        required*adaptiveWeight[delta]);
                                }
                            }
                        }
                        adaptive.push_back(attack);
                    }
                }
                const auto adaptiveEnd=std::chrono::steady_clock::now();
                QCOMPARE(boundWindowCount+directWindowCount,
                         std::int64_t{profileTicks});
                // A passing bitwise check with 256 windows is insufficient
                // unless the local density decision actually switches.
                if(profile==4) {
                    QVERIFY2(boundWindowCount>0,
                        "Peak-then-quiet never selected bounded scan");
                    QVERIFY2(directWindowCount>0,
                        "Peak-then-quiet never returned to direct scan");
                }
                QCOMPARE(adaptive.size(),original.size());
                for(std::size_t i=0;i<original.size();++i)
                    QCOMPARE(std::bit_cast<std::uint64_t>(original[i]),
                             std::bit_cast<std::uint64_t>(adaptive[i]));
                const auto adaptiveUs=std::chrono::duration_cast<
                    std::chrono::microseconds>(
                        adaptiveEnd-adaptiveStart).count();
                const double adaptiveRatio=adaptiveUs>0
                    ? static_cast<double>(baselineUs)
                        /static_cast<double>(adaptiveUs) : 0.0;
                const double boundAdaptiveRatio=adaptiveUs>0
                    ? static_cast<double>(boundUs)
                        /static_cast<double>(adaptiveUs) : 0.0;
                qInfo().nospace()<<"RGS M17 adaptive Fs="<<fs
                    <<" ceiling="<<ceilingDb<<" profile="<<profileName
                    <<" adaptive_us="<<adaptiveUs
                    <<" original_ratio="<<adaptiveRatio
                    <<" bound_ratio="<<boundAdaptiveRatio
                    <<" bound_windows="<<boundWindowCount
                    <<" fallback_windows="<<directWindowCount
                    <<" attack_floor_bitwise=PASS";
                const double boundRatio=boundUs>0
                    ? static_cast<double>(baselineUs)/static_cast<double>(boundUs)
                    : 0.0;
                const double memoBoundRatio=boundUs>0
                    ? static_cast<double>(memoUs)/static_cast<double>(boundUs)
                    : 0.0;
                qInfo().nospace()<<"RGS M17 group_bound Fs="<<fs
                    <<" ceiling="<<ceilingDb<<" profile="<<profileName
                    <<" future_evaluations="<<evaluated
                    <<" groups_checked="<<checked<<" visited="<<visited
                    <<" skipped="<<skipped<<" bounded_us="<<boundUs
                    <<" ratio_original_over_bounded="<<boundRatio
                    <<" ratio_memoized_over_bounded="<<memoBoundRatio
                    <<" floor_bitwise=PASS";
                if(!boundsPath.empty()) {
                    std::ofstream csv(boundsPath,std::ios::app);
                    QVERIFY2(csv.good(),"M17 bound probe append failed");
                    csv<<profileName<<','<<fs<<','<<ceilingDb<<','
                        <<profileTicks<<','<<events.size()<<','<<evaluated<<','
                        <<baselineUs<<','<<memoUs<<','<<boundUs<<','
                        <<boundRatio<<','<<memoBoundRatio<<','<<checked
                        <<','<<visited<<','<<skipped<<",PASS,PASS,"
                        <<adaptiveUs<<','<<adaptiveRatio<<','
                        <<boundAdaptiveRatio<<','
                        <<boundWindowCount<<','
                        <<directWindowCount<<",PASS\n";
                }
                qInfo().nospace()<<"RGS M17 detector lookahead Fs="<<fs
                    <<" ceiling="<<ceilingDb<<" profile="<<profileName
                    <<" active_events="<<events.size()
                    <<" future_evaluations="<<evaluated
                    <<" max_horizon="<<maxPerHorizon
                    <<" original_us="<<baselineUs
                    <<" memoized_us="<<memoUs<<" ratio="<<ratio
                    <<" attack_floor_bitwise=PASS controller_oracle=PASS";
                if(!artifact.empty()) {
                    std::ofstream csv(artifact,std::ios::app);
                    QVERIFY2(csv.good(),"M17 real detector probe append failed");
                    csv<<"detector_"<<profileName<<','<<fs<<','<<ceilingDb<<','
                        <<profileTicks<<','<<evaluated<<','<<baselineUs<<','
                        <<memoUs<<','<<ratio<<",PASS,"
                        <<validCount<<','<<events.size()<<','
                        <<maxPerHorizon<<",PASS\n";
                }
                if(!summaryPath.empty()) {
                    std::ofstream summary(summaryPath,std::ios::app);
                    if(summary)summary<<"| "<<fs<<" | "<<ceilingDb
                        <<" | "<<profileName<<" | "<<events.size()
                        <<" | "<<evaluated<<" | "<<baselineUs/1000.0
                        <<" ms | "<<memoUs/1000.0<<" ms | "<<ratio
                        <<"x | PASS | PASS |\n";
                }
            }
        }
    }
}


void TruePeakLimiterVerificationTest::
experimentalIsolatedSelectorMatchesFrozenControllerAndPcm()
{
    using Event=dsp::internal::LimiterDetectorEvent;
    using Detector=dsp::internal::TruePeakLimiterDetectorState;
    using Controller=dsp::internal::TruePeakLimiterControllerState;
    using Selector=dsp::internal::experimental::M17LookaheadSelector;
    constexpr int nFrames=192;
    constexpr double pi=std::numbers::pi_v<double>;
    const auto csvPath=qEnvironmentVariable(
        "RGS_M17_EXPERIMENT_PROBE_FILE").toStdString();
    if(!csvPath.empty()) {
        std::ofstream csv(csvPath,std::ios::trunc);
        QVERIFY2(csv.good(),"experimental M17 PCM report unavailable");
        csv<<"sample_rate_hz,channels,ceiling_dbtp,frames,"
              "active_detector_events,reference_controller_us,"
              "candidate_select_and_controller_us,selector_setup_us,"
              "bounded_ticks,direct_ticks,events_examined,events_skipped,"
              "full_pcm_bitwise,controller_fields_bitwise,checkpoint_state_bitwise,"
              "reference_controller_ns,candidate_select_and_controller_ns,"
              "rolling_select_and_controller_ns,rolling_setup_ns,"
              "rolling_batches,rolling_pcm_bitwise,"
              "rolling_controller_bitwise,rolling_checkpoint_bitwise\n";
    }

    for(const int fs:{44100,48000,88200,96000,176400,192000}) {
        for(const bool stereo:{false,true}) {
            for(const double ceiling:{0.0,-1.0,-6.0,-12.0}) {
                auto source=buffer(fs,stereo,nFrames);
                for(std::size_t ch=0;ch<source.view().format().channel_count();
                    ++ch) {
                    auto lane=source.mutable_view().channel(ch);
                    QVERIFY(lane);
                    for(int n=0;n<nFrames;++n) {
                        const double t=static_cast<double>(n)/fs;
                        double sample=(ch==0?1.0:-1.0)*
                            (0.84*std::sin(2.0*pi*1061.0*t)
                             +0.39*std::sin(2.0*pi*3477.0*t+0.23));
                        if(n%29==0)sample+=ch==0?1.18:-1.12;
                        if(n>=109 && n<165)sample=+0.0;
                        (*lane.value())[static_cast<std::size_t>(n)]=sample;
                    }
                }

                // Authoritative PCM oracle: the actual frozen M17 IModule,
                // with nonuniform streaming chunks and physical D drain.
                const std::array<std::size_t,3> chunks{47,63,82};
                const auto referencePcm=renderRealM17(source,ceiling,chunks);
                QVERIFY2(referencePcm.has_value(),
                    "frozen M17 real-module PCM render failed");

                const auto linear=std::pow(10.0,ceiling/20.0);
                Detector detector(stereo?2U:1U);
                std::vector<Event> events;
                events.reserve(static_cast<std::size_t>(nFrames*8+118));
                std::int64_t lastTick=std::numeric_limits<std::int64_t>::min();
                const auto accept=[&](const Event& e) {
                    if(!e.valid)return true;
                    if(e.tick<=lastTick||!std::isfinite(e.peak)
                        ||e.peak<0.0)return false;
                    lastTick=e.tick;
                    if(e.peak>linear)events.push_back(e);
                    return true;
                };
                for(int n=0;n<nFrames;++n) {
                    const auto l=source.view().channel(0);
                    const auto r=stereo?source.view().channel(1):l;
                    QVERIFY(l&&r);
                    const auto left=(*l.value())[static_cast<std::size_t>(n)];
                    const auto right=stereo
                        ?(*r.value())[static_cast<std::size_t>(n)] : +0.0;
                    auto quarters=detector.push_programme_frame(left,right);
                    QVERIFY(quarters);
                    for(const auto& q:*quarters.value()) {
                        QVERIFY(accept(q.preceding_anchor));
                        QVERIFY(accept(q.midpoint));
                    }
                }
                auto tail=detector.finalize_detector();
                QVERIFY(tail);
                for(const auto& q:*tail.value()) {
                    QVERIFY(accept(q.preceding_anchor));
                    QVERIFY(accept(q.midpoint));
                }

                auto selection=Selector::create(fs,ceiling);
                auto oracle=Controller::create(fs,ceiling);
                auto candidate=Controller::create(fs,ceiling);
                auto rollingController=Controller::create(fs,ceiling);
                auto rollingSelection=Selector::create(fs,ceiling);
                QVERIFY(selection && oracle && candidate
                    && rollingController && rollingSelection);
                const auto cacheStart=std::chrono::steady_clock::now();
                QVERIFY(selection->prepare(events));
                const auto cacheEnd=std::chrono::steady_clock::now();
                QCOMPARE(selection->cached_event_count(),events.size());

                // Invalid input must never replace an accepted event cache.
                if(fs==44100 && !stereo && ceiling==-1.0) {
                    const std::array<Event,2> invalid{{
                        Event{15,1.3,true},Event{14,1.4,true}
                    }};
                    QVERIFY(!selection->prepare(invalid));
                    QCOMPARE(selection->cached_event_count(),events.size());
                }

                std::size_t search=0,boundedTicks=0,directTicks=0;
                std::size_t visited=0,skipped=0;
                std::int64_t referenceNanos=0,candidateNanos=0;
                std::int64_t rollingNanos=0,rollingSetupNanos=0;
                std::size_t rollingBatches=0;
                for(int n=0;n<nFrames;++n) {
                    if(n==0 || n==47 || n==110) {
                        // Independent complete lookahead, rebuilt every
                        // asymmetric PCM chunk. The previous cache is
                        // replaced, never appended across resource history.
                        if(n!=0) {
                            const auto snapshot=rollingController.value()->snapshot();
                            const auto original=oracle.value()->snapshot();
                            QCOMPARE(snapshot.next_tick,original.next_tick);
                            QCOMPARE(snapshot.hold_remaining,original.hold_remaining);
                            QCOMPARE(std::bit_cast<std::uint64_t>(snapshot.density),
                                     std::bit_cast<std::uint64_t>(original.density));
                            QCOMPARE(std::bit_cast<std::uint64_t>(
                                snapshot.previous_reduction_db),
                                std::bit_cast<std::uint64_t>(
                                    original.previous_reduction_db));
                            auto newController=Controller::create(fs,ceiling);
                            QVERIFY(newController);
                            QVERIFY(newController.value()->restore(snapshot));
                            rollingController=std::move(newController);
                        }
                        const auto firstTick=n==0?-62:8*(n-1)+1;
                        const auto lastFrame=n==0?46:(n==47?109:nFrames-1);
                        const auto finalTick=8*lastFrame
                            +oracle.value()->lookahead_ticks();
                        const auto first=std::lower_bound(
                            events.begin(),events.end(),firstTick,
                            [](const Event& ev,std::int64_t tick) {
                                return ev.tick<tick;
                            });
                        const auto last=std::upper_bound(
                            first,events.end(),finalTick,
                            [](std::int64_t tick,const Event& ev) {
                                return tick<ev.tick;
                            });
                        const std::span<const Event> batch=first==last
                            ?std::span<const Event>{}
                            :std::span<const Event>{
                                &*first,static_cast<std::size_t>(last-first)};
                        const auto setupStart=std::chrono::steady_clock::now();
                        QVERIFY(rollingSelection->prepare(batch));
                        const auto setupEnd=std::chrono::steady_clock::now();
                        rollingSetupNanos+=std::chrono::duration_cast<
                            std::chrono::nanoseconds>(
                                setupEnd-setupStart).count();
                        QCOMPARE(rollingSelection->cached_event_count(),
                                 batch.size());
                        ++rollingBatches;
                    }
                    double frameGain=1.0;
                    double rollingFrameGain=1.0;
                    for(std::int64_t m=n==0?-62:8*(n-1)+1;
                        m<=8*n;++m) {
                        while(search<events.size()&&events[search].tick<m)
                            ++search;
                        const bool onTick=search<events.size()
                            && events[search].tick==m;
                        const double current=onTick?events[search].peak:+0.0;
                        const auto first=search+(onTick?1U:0U);
                        auto last=first;
                        const auto deadline=m+oracle.value()->lookahead_ticks();
                        while(last<events.size()
                            && events[last].tick<=deadline)++last;
                        const std::span<const Event> full=first==last
                            ?std::span<const Event>{}
                            :std::span<const Event>{events.data()+first,last-first};

                        const auto a=std::chrono::steady_clock::now();
                        auto referenceStep=oracle.value()->advance(current,full);
                        const auto b=std::chrono::steady_clock::now();
                        QVERIFY(referenceStep);
                        auto choice=selection->choose(m,current);
                        QVERIFY(choice);
                        const auto& selected=choice->future_event;
                        const auto minimal=selected
                            ?std::span<const Event>{&*selected,1}
                            :std::span<const Event>{};
                        auto acceleratedStep=candidate.value()->advance(
                            current,minimal);
                        const auto c=std::chrono::steady_clock::now();
                        QVERIFY(acceleratedStep);
                        auto rollingChoice=rollingSelection->choose(m,current);
                        QVERIFY(rollingChoice);
                        const auto& rollingEvent=rollingChoice->future_event;
                        const auto rollingSpan=rollingEvent
                            ?std::span<const Event>{&*rollingEvent,1}
                            :std::span<const Event>{};
                        auto rollingStep=rollingController.value()->advance(
                            current,rollingSpan);
                        const auto d=std::chrono::steady_clock::now();
                        QVERIFY(rollingStep);
                        // Individual submicrosecond ticks must not be
                        // truncated to 0 us on fast Windows builds.
                        referenceNanos+=std::chrono::duration_cast<
                            std::chrono::nanoseconds>(b-a).count();
                        candidateNanos+=std::chrono::duration_cast<
                            std::chrono::nanoseconds>(c-b).count();
                        rollingNanos+=std::chrono::duration_cast<
                            std::chrono::nanoseconds>(d-c).count();
                        boundedTicks+=choice->bounded?1U:0U;
                        directTicks+=choice->bounded?0U:1U;
                        visited+=choice->visited;
                        skipped+=choice->skipped;
                        const auto& original=*referenceStep.value();
                        const auto& optimized=*acceleratedStep.value();
                        const auto& rolling=*rollingStep.value();
                        QCOMPARE(original.tick,rolling.tick);
                        QCOMPARE(original.hold_remaining,rolling.hold_remaining);
                        QCOMPARE(std::bit_cast<std::uint64_t>(
                            original.attack_floor_db),
                            std::bit_cast<std::uint64_t>(rolling.attack_floor_db));
                        QCOMPARE(std::bit_cast<std::uint64_t>(
                            original.reduction_db),
                            std::bit_cast<std::uint64_t>(rolling.reduction_db));
                        QCOMPARE(std::bit_cast<std::uint64_t>(
                            original.gain),
                            std::bit_cast<std::uint64_t>(rolling.gain));
                        QCOMPARE(std::bit_cast<std::uint64_t>(
                            original.density),
                            std::bit_cast<std::uint64_t>(rolling.density));
                        QCOMPARE(std::bit_cast<std::uint64_t>(
                            original.release_ms),
                            std::bit_cast<std::uint64_t>(rolling.release_ms));
                        QCOMPARE(original.tick,optimized.tick);
                        QCOMPARE(original.hold_remaining,optimized.hold_remaining);
                        QCOMPARE(std::bit_cast<std::uint64_t>(
                            original.required_db),
                            std::bit_cast<std::uint64_t>(optimized.required_db));
                        QCOMPARE(std::bit_cast<std::uint64_t>(
                            original.attack_floor_db),
                            std::bit_cast<std::uint64_t>(optimized.attack_floor_db));
                        QCOMPARE(std::bit_cast<std::uint64_t>(
                            choice->attack_floor_db),
                            std::bit_cast<std::uint64_t>(original.attack_floor_db));
                        QCOMPARE(std::bit_cast<std::uint64_t>(
                            original.reduction_db),
                            std::bit_cast<std::uint64_t>(optimized.reduction_db));
                        QCOMPARE(std::bit_cast<std::uint64_t>(
                            original.density),
                            std::bit_cast<std::uint64_t>(optimized.density));
                        QCOMPARE(std::bit_cast<std::uint64_t>(
                            original.release_ms),
                            std::bit_cast<std::uint64_t>(optimized.release_ms));
                        QCOMPARE(std::bit_cast<std::uint64_t>(
                            original.gain),
                            std::bit_cast<std::uint64_t>(optimized.gain));
                        if(m==8*n) {
                            frameGain=optimized.gain;
                            rollingFrameGain=rolling.gain;
                        }
                    }
                    for(std::size_t ch=0;
                        ch<source.view().format().channel_count();++ch) {
                        const auto original=source.view().channel(ch);
                        const auto output=referencePcm->view().channel(ch);
                        QVERIFY(original&&output);
                        const double pcm=(*original.value())[
                            static_cast<std::size_t>(n)];
                        const double predicted=frameGain==1.0
                            ?pcm:pcm*frameGain;
                        QCOMPARE(std::bit_cast<std::uint64_t>(predicted),
                            std::bit_cast<std::uint64_t>(
                                (*output.value())[static_cast<std::size_t>(n)]));
                        const double rollingPredicted=rollingFrameGain==1.0
                            ?pcm:pcm*rollingFrameGain;
                        QCOMPARE(std::bit_cast<std::uint64_t>(rollingPredicted),
                            std::bit_cast<std::uint64_t>(
                                (*output.value())[static_cast<std::size_t>(n)]));
                    }
                    if(n==nFrames/2) {
                        const auto snap=candidate.value()->snapshot();
                        const auto refSnap=oracle.value()->snapshot();
                        QCOMPARE(snap.next_tick,refSnap.next_tick);
                        QCOMPARE(snap.hold_remaining,refSnap.hold_remaining);
                        QCOMPARE(std::bit_cast<std::uint64_t>(snap.density),
                            std::bit_cast<std::uint64_t>(refSnap.density));
                        QCOMPARE(std::bit_cast<std::uint64_t>(
                            snap.previous_reduction_db),
                            std::bit_cast<std::uint64_t>(
                                refSnap.previous_reduction_db));
                        auto restored=Controller::create(fs,ceiling);
                        QVERIFY(restored);
                        QVERIFY(restored.value()->restore(snap));
                        QCOMPARE(restored.value()->snapshot().next_tick,
                            snap.next_tick);
                    }
                }
                // m=-62..0 gives 63 initial ticks, then 8 per
                // following frame. For N frames: 63+8*(N-1)=8*N+55.
                QCOMPARE(candidate.value()->next_tick(),
                         std::int64_t{8*(nFrames-1)+1});
                QCOMPARE(oracle.value()->next_tick(),
                         candidate.value()->next_tick());
                QCOMPARE(rollingController.value()->next_tick(),
                         oracle.value()->next_tick());
                QCOMPARE(rollingBatches,std::size_t{3});
                QCOMPARE(boundedTicks+directTicks,
                         std::size_t{8*nFrames+55});
                const auto cacheMicros=std::chrono::duration_cast<
                    std::chrono::microseconds>(cacheEnd-cacheStart).count();
                // Reporting only: preserving nanosecond sums avoids
                // platform-dependent microsecond truncation per tick.
                const double referenceMicros=referenceNanos/1000.0;
                const double candidateMicros=candidateNanos/1000.0;
                const double rollingMicros=rollingNanos/1000.0;
                qInfo().nospace()<<"M17 isolated-accelerator fs="<<fs
                    <<" stereo="<<stereo<<" ceiling="<<ceiling
                    <<" frames="<<nFrames<<" events="<<events.size()
                    <<" reference_controller_us="<<referenceMicros
                    <<" candidate_us="<<candidateMicros
                    <<" rolling_us="<<rollingMicros
                    <<" rolling_setup_us="<<rollingSetupNanos/1000.0
                    <<" setup_us="<<cacheMicros
                    <<" bounded_ticks="<<boundedTicks
                    <<" direct_ticks="<<directTicks
                    <<" visited="<<visited<<" skipped="<<skipped
                    <<" controller_and_pcm_bitwise=PASS";
                if(!csvPath.empty()) {
                    std::ofstream csv(csvPath,std::ios::app);
                    QVERIFY2(csv.good(),"experimental M17 report append failed");
                    csv<<fs<<','<<(stereo?2:1)<<','<<ceiling<<','
                        <<nFrames<<','<<events.size()<<','
                        <<referenceMicros<<','<<candidateMicros<<','
                        <<cacheMicros<<','<<boundedTicks<<','
                        <<directTicks<<','<<visited<<','<<skipped
                        <<",PASS,PASS,PASS,"
                        <<referenceNanos<<','<<candidateNanos<<','
                        <<rollingNanos<<','<<rollingSetupNanos<<','
                        <<rollingBatches<<",PASS,PASS,PASS\n";
                }
            }
        }
    }
}


void TruePeakLimiterVerificationTest::experimentalRollingBoundedBatchPcmOracle()
{
    using Event=dsp::internal::LimiterDetectorEvent;
    using Detector=dsp::internal::TruePeakLimiterDetectorState;
    using Controller=dsp::internal::TruePeakLimiterControllerState;
    using Rolling=dsp::internal::experimental::M17RollingBatchSelector;
    constexpr int kFrames=1536;
    constexpr int kBatchFrames=64;
    constexpr double kCeiling=-6.0;
    constexpr double pi=std::numbers::pi_v<double>;

    const auto csvPath=qEnvironmentVariable(
        "RGS_M17_ROLLING_PROBE_FILE").toStdString();
    if(!csvPath.empty()) {
        std::ofstream csv(csvPath,std::ios::trunc);
        QVERIFY2(csv.good(),"M17 rolling experiment artifact unavailable");
        csv<<"sample_rate_hz,channels,ceiling_dbtp,frames,all_active_events,"
              "batches,high_water_cached_events,cache_cap,"
              "rejected_incomplete_horizon,rejected_oversize,"
              "rejected_gap,rolling_pcm_bitwise,restored_state_bitwise,"
              "candidate_loop_ns,reference_render_ns\n";
    }

    for(const int fs:{44100,48000,88200,96000,176400,192000}) {
        for(const bool stereo:{false,true}) {
            auto source=buffer(fs,stereo,kFrames);
            const auto fmt=source.view().format();
            for(std::size_t ch=0;ch<fmt.channel_count();++ch) {
                auto lane=source.mutable_view().channel(ch);
                QVERIFY(lane);
                for(int n=0;n<kFrames;++n) {
                    const double t=static_cast<double>(n)/fs;
                    double sample=(ch==0?1.0:-1.0)*
                        (0.91*std::sin(2.0*pi*797.0*t)
                        +0.34*std::sin(2.0*pi*2171.0*t+0.13));
                    if(n%97==0)sample+=ch==0?1.14:-1.16;
                    if(n>=433 && n<673)sample=+0.0;
                    if(n>=1041 && n<1150)sample=+0.0;
                    (*lane.value())[static_cast<std::size_t>(n)]=sample;
                }
            }

            const std::array<std::size_t,4> chunks{31,127,19,251};
            const auto referenceStart=std::chrono::steady_clock::now();
            const auto pcm=renderRealM17(source,kCeiling,chunks);
            const auto referenceEnd=std::chrono::steady_clock::now();
            QVERIFY2(pcm.has_value(),"frozen M17 large PCM oracle failed");

            Detector detector(stereo?2U:1U);
            const double linear=std::pow(10.0,kCeiling/20.0);
            std::vector<Event> allEvents;
            allEvents.reserve(static_cast<std::size_t>(kFrames*8+118));
            std::int64_t detectorCovered=-63;
            const auto accept=[&](const Event& e) {
                if(!e.valid)return true;
                if(e.tick<=detectorCovered || !std::isfinite(e.peak)
                    || e.peak<0.0)return false;
                detectorCovered=e.tick;
                if(e.peak>linear)allEvents.push_back(e);
                return true;
            };
            for(int n=0;n<kFrames;++n) {
                const auto l=source.view().channel(0);
                const auto r=stereo?source.view().channel(1):l;
                QVERIFY(l&&r);
                const auto left=(*l.value())[static_cast<std::size_t>(n)];
                const auto right=stereo
                    ?(*r.value())[static_cast<std::size_t>(n)] : +0.0;
                auto q=detector.push_programme_frame(left,right);
                QVERIFY(q);
                for(const auto& quarter:*q.value()) {
                    QVERIFY(accept(quarter.preceding_anchor));
                    QVERIFY(accept(quarter.midpoint));
                }
            }
            const auto tail=detector.finalize_detector();
            QVERIFY(tail);
            for(const auto& quarter:*tail.value()) {
                QVERIFY(accept(quarter.preceding_anchor));
                QVERIFY(accept(quarter.midpoint));
            }
            QVERIFY(detector.finalized());

            auto rolling=Rolling::create(fs,kCeiling);
            auto controller=Controller::create(fs,kCeiling);
            QVERIFY(rolling && controller);
            const auto horizon=controller.value()->lookahead_ticks();
            QVERIFY(horizon>0);

            // First test intentionally incomplete lookahead certification:
            // a caller may not infer unseen future events from an empty span.
            const std::span<const Event> empty{};
            QVERIFY(!rolling->load(empty,-62,8*(kBatchFrames-1),
                8*(kBatchFrames-1)+horizon-1,false));
            QCOMPARE(rolling->batches(),std::size_t{0});
            QCOMPARE(rolling->next_tick(),std::int64_t{-62});

            // Cannot accept event storage above the explicit per-batch cap.
            std::vector<Event> over(Rolling::kMaxBatchEvents+1U);
            for(std::size_t i=0;i<over.size();++i)
                over[i]=Event{static_cast<std::int64_t>(i),
                    1.0,true};
            QVERIFY(!rolling->load(over,-62,8*(kBatchFrames-1),
                detectorCovered,true));
            QCOMPARE(rolling->batches(),std::size_t{0});
            over.clear();
            over.shrink_to_fit();

            // Product DSP remains the reference. The test runner holds
            // full offline detector events; ONLY the experimental selector
            // cache has a hard per-batch bound. This is not streaming
            // detector RAM qualification.
            std::size_t eventCursor=0;
            bool restoredState=false;
            const auto candidateStart=std::chrono::steady_clock::now();
            for(int startFrame=0;startFrame<kFrames;startFrame+=kBatchFrames) {
                const int lastFrame=std::min(kFrames,startFrame+kBatchFrames)-1;
                const std::int64_t firstM=startFrame==0
                    ?-62:8*(startFrame-1)+1;
                const std::int64_t lastM=8*lastFrame;
                const std::int64_t cutoff=lastM+horizon;
                const auto begin=std::lower_bound(
                    allEvents.begin(),allEvents.end(),firstM,
                    [](const Event& e,std::int64_t tick) {
                        return e.tick<tick;
                    });
                const auto finish=std::upper_bound(
                    begin,allEvents.end(),cutoff,
                    [](std::int64_t tick,const Event& e) {
                        return tick<e.tick;
                    });
                const std::span<const Event> batch=begin==finish
                    ?std::span<const Event>{}
                    :std::span<const Event>{
                        &*begin,static_cast<std::size_t>(finish-begin)};
                QVERIFY2(rolling->load(batch,firstM,lastM,
                    detectorCovered,true),
                    "Certified rolling window unexpectedly rejected");
                QCOMPARE(rolling->cached_events(),batch.size());
                QVERIFY(rolling->cached_events()<=Rolling::kMaxBatchEvents);

                // Gap must be rejected transactionally; keep the live
                // accepted cache and the expected clock unchanged.
                const auto beforeEvents=rolling->cached_events();
                const auto beforeTick=rolling->next_tick();
                const auto beforeBatches=rolling->batches();
                QVERIFY(!rolling->load(empty,firstM+1,lastM,
                    detectorCovered,true));
                QCOMPARE(rolling->cached_events(),beforeEvents);
                QCOMPARE(rolling->next_tick(),beforeTick);
                QCOMPARE(rolling->batches(),beforeBatches);

                if(startFrame==768) {
                    // Independent frozen controller restore at a batch
                    // boundary, not a DSP checkpoint serialization test.
                    const auto snap=controller.value()->snapshot();
                    auto restored=Controller::create(fs,kCeiling);
                    QVERIFY(restored);
                    QVERIFY(restored.value()->restore(snap));
                    QCOMPARE(restored.value()->snapshot().next_tick,
                        controller.value()->next_tick());
                    QCOMPARE(std::bit_cast<std::uint64_t>(
                        restored.value()->snapshot().density),
                        std::bit_cast<std::uint64_t>(snap.density));
                    controller=std::move(restored);
                    restoredState=true;
                }

                for(int n=startFrame;n<=lastFrame;++n) {
                    double applied=1.0;
                    for(std::int64_t m=n==0?-62:8*(n-1)+1;
                        m<=8*n;++m) {
                        while(eventCursor<allEvents.size()
                            && allEvents[eventCursor].tick<m)
                            ++eventCursor;
                        const bool currentEvent=eventCursor<allEvents.size()
                            && allEvents[eventCursor].tick==m;
                        const double current=currentEvent
                            ?allEvents[eventCursor].peak:+0.0;
                        auto choice=rolling->choose(m,current);
                        QVERIFY(choice);
                        const auto& winner=choice->future_event;
                        const auto selected=winner
                            ?std::span<const Event>{&*winner,1}
                            :std::span<const Event>{};
                        auto step=controller.value()->advance(current,selected);
                        QVERIFY(step);
                        QCOMPARE(step.value()->tick,m);
                        QCOMPARE(std::bit_cast<std::uint64_t>(
                            step.value()->attack_floor_db),
                            std::bit_cast<std::uint64_t>(
                                choice->attack_floor_db));
                        if(m==8*n)applied=step.value()->gain;
                    }
                    for(std::size_t ch=0;ch<fmt.channel_count();++ch) {
                        const auto src=source.view().channel(ch);
                        const auto dst=pcm->view().channel(ch);
                        QVERIFY(src&&dst);
                        const double input=(*src.value())[
                            static_cast<std::size_t>(n)];
                        const double expected=applied==1.0
                            ?input:input*applied;
                        QCOMPARE(std::bit_cast<std::uint64_t>(expected),
                            std::bit_cast<std::uint64_t>(
                                (*dst.value())[static_cast<std::size_t>(n)]));
                    }
                }
            }
            const auto candidateEnd=std::chrono::steady_clock::now();
            QCOMPARE(rolling->next_tick(),std::int64_t{8*(kFrames-1)+1});
            QCOMPARE(controller.value()->next_tick(),rolling->next_tick());
            QCOMPARE(rolling->batches(),
                std::size_t{kFrames/kBatchFrames});
            QVERIFY(restoredState);
            QVERIFY(rolling->high_water_events()<=Rolling::kMaxBatchEvents);

            const auto candidateNs=std::chrono::duration_cast<
                std::chrono::nanoseconds>(
                    candidateEnd-candidateStart).count();
            const auto originalNs=std::chrono::duration_cast<
                std::chrono::nanoseconds>(
                    referenceEnd-referenceStart).count();
            qInfo().nospace()<<"M17 rolling bounded PCM Fs="<<fs
                <<" channels="<<(stereo?2:1)<<" frames="<<kFrames
                <<" event_total="<<allEvents.size()
                <<" batches="<<rolling->batches()
                <<" high_water="<<rolling->high_water_events()
                <<" candidate_ns="<<candidateNs
                <<" original_imodule_ns="<<originalNs
                <<" pcm_bitwise=PASS controller_snapshot=PASS";
            if(!csvPath.empty()) {
                std::ofstream csv(csvPath,std::ios::app);
                QVERIFY2(csv.good(),"rolling test CSV append failed");
                csv<<fs<<','<<(stereo?2:1)<<','<<kCeiling<<','
                    <<kFrames<<','<<allEvents.size()<<','
                    <<rolling->batches()<<','<<rolling->high_water_events()
                    <<','<<Rolling::kMaxBatchEvents
                    <<",PASS,PASS,PASS,PASS,PASS,"
                    <<candidateNs<<','<<originalNs<<'\n';
            }
        }
    }
}


void TruePeakLimiterVerificationTest::experimentalLiveDetectorBoundedPcmOracle()
{
    using Controller=dsp::internal::TruePeakLimiterControllerState;
    using Rolling=dsp::internal::experimental::M17RollingBatchSelector;
    using Feeder=dsp::internal::experimental::M17BoundedDetectorFeed;
    constexpr int kFrames=4096;
    constexpr int kBatchFrames=64;
    constexpr double kCeiling=-6.0;
    constexpr double pi=std::numbers::pi_v<double>;
    const auto csvPath=qEnvironmentVariable(
        "RGS_M17_FEEDER_PROBE_FILE").toStdString();
    if(!csvPath.empty()) {
        std::ofstream csv(csvPath,std::ios::trunc);
        QVERIFY2(csv.good(),"bounded detector feed CSV creation failed");
        csv<<"sample_rate_hz,channels,ceiling_dbtp,frames,batches,"
              "source_frames_consumed,detector_active_events_total,"
              "detector_cache_high_water,selector_cache_high_water,"
              "detector_event_cap,selector_event_cap,"
              "rejected_nonfinite_without_mutation,rejected_gap,"
              "detector_drained,pcm_bitwise,checkpoint_state_bitwise,"
              "candidate_processing_ns,real_imodule_render_ns\n";
    }

    for(const int fs:{44100,48000,88200,96000,176400,192000}) {
        for(const bool stereo:{false,true}) {
            auto source=buffer(fs,stereo,kFrames);
            const auto fmt=source.view().format();
            for(std::size_t ch=0;ch<fmt.channel_count();++ch) {
                auto lane=source.mutable_view().channel(ch);
                QVERIFY(lane);
                for(int n=0;n<kFrames;++n) {
                    const double t=static_cast<double>(n)/fs;
                    double v=(ch==0?1.0:-1.0)*
                        (0.89*std::sin(2.0*pi*739.0*t)
                         +0.37*std::sin(2.0*pi*2939.0*t+0.15));
                    if(n%137==0)v+=ch==0?1.16:-1.13;
                    if(n>=897 && n<1409)v=+0.0;
                    if(n>=2901 && n<3264)v=+0.0;
                    (*lane.value())[static_cast<std::size_t>(n)]=v;
                }
            }

            const std::array<std::size_t,5> chunks{43,191,27,97,509};
            const auto referenceStart=std::chrono::steady_clock::now();
            const auto pcm=renderRealM17(source,kCeiling,chunks);
            const auto referenceEnd=std::chrono::steady_clock::now();
            QVERIFY2(pcm.has_value(),"frozen M17 PCM reference unavailable");

            auto feeder=Feeder::create(fs,kCeiling,stereo?2U:1U);
            auto scheduler=Rolling::create(fs,kCeiling);
            auto controller=Controller::create(fs,kCeiling);
            QVERIFY(feeder && scheduler && controller);
            const auto horizon=controller.value()->lookahead_ticks();
            QVERIFY(horizon>0);
            const auto startState=feeder->coverage_tick();
            const auto nan=std::numeric_limits<double>::quiet_NaN();
            QVERIFY(!feeder->push(nan,0.0));
            QCOMPARE(feeder->coverage_tick(),startState);
            QCOMPARE(feeder->frames(),std::uint64_t{0});
            QCOMPARE(feeder->retained_count(),std::size_t{0});
            QVERIFY(!feeder->drain());

            // No global detector-event vector exists in this test path.
            // Each batch is certified by an incrementally fed signed FIR
            // detector and only its retained future events are cached.
            std::size_t inputFrame=0;
            bool restoredState=false;
            const auto candidateStart=std::chrono::steady_clock::now();
            for(int startFrame=0;startFrame<kFrames;
                startFrame+=kBatchFrames) {
                const int lastFrame=std::min(kFrames,startFrame+kBatchFrames)-1;
                const std::int64_t firstM=startFrame==0
                    ?-62:8*(startFrame-1)+1;
                const std::int64_t lastM=8*lastFrame;
                const std::int64_t deadline=lastM+horizon;
                feeder->discard_before(firstM);

                while(!feeder->drained()
                    && feeder->coverage_tick()<deadline) {
                    if(inputFrame<kFrames) {
                        const auto l=source.view().channel(0);
                        const auto r=stereo?source.view().channel(1):l;
                        QVERIFY(l&&r);
                        const auto left=(*l.value())[
                            static_cast<std::size_t>(inputFrame)];
                        const auto right=stereo?(*r.value())[
                            static_cast<std::size_t>(inputFrame)]:+0.0;
                        QVERIFY2(feeder->push(left,right),
                            "bounded detector feed refused valid frame");
                        ++inputFrame;
                    } else {
                        QVERIFY2(feeder->drain(),
                            "bounded detector feed EOS drain refused");
                    }
                }
                QVERIFY(feeder->drained()
                    ||feeder->coverage_tick()>=deadline);
                QVERIFY(feeder->retained_count()<=Feeder::kMaxRetained);
                const auto events=feeder->batch(firstM,lastM,horizon);
                QVERIFY2(scheduler->load(events,firstM,lastM,
                    feeder->coverage_tick(),feeder->drained()),
                    "rolling scheduler rejected certified detector stream");
                QCOMPARE(scheduler->cached_events(),events.size());
                QVERIFY(scheduler->cached_events()<=Rolling::kMaxBatchEvents);

                // Non-contiguous origin must not advance scheduler state.
                const auto beforeCount=scheduler->cached_events();
                const auto beforeBatches=scheduler->batches();
                const auto beforeTick=scheduler->next_tick();
                QVERIFY(!scheduler->load({},firstM+1,lastM,
                    feeder->coverage_tick(),feeder->drained()));
                QCOMPARE(scheduler->cached_events(),beforeCount);
                QCOMPARE(scheduler->batches(),beforeBatches);
                QCOMPARE(scheduler->next_tick(),beforeTick);

                if(startFrame==kFrames/2) {
                    const auto state=controller.value()->snapshot();
                    auto restored=Controller::create(fs,kCeiling);
                    QVERIFY(restored);
                    QVERIFY(restored.value()->restore(state));
                    QCOMPARE(restored.value()->snapshot().next_tick,
                        controller.value()->next_tick());
                    QCOMPARE(std::bit_cast<std::uint64_t>(
                        restored.value()->snapshot().density),
                        std::bit_cast<std::uint64_t>(state.density));
                    QCOMPARE(std::bit_cast<std::uint64_t>(
                        restored.value()->snapshot().previous_reduction_db),
                        std::bit_cast<std::uint64_t>(
                            state.previous_reduction_db));
                    controller=std::move(restored);
                    restoredState=true;
                }

                for(int n=startFrame;n<=lastFrame;++n) {
                    double appliedGain=1.0;
                    for(std::int64_t m=n==0?-62:8*(n-1)+1;
                        m<=8*n;++m) {
                        const double current=feeder->peak_at(m);
                        auto choice=scheduler->choose(m,current);
                        QVERIFY(choice);
                        const auto& selected=choice->future_event;
                        const auto future=selected
                            ?std::span<const dsp::internal::LimiterDetectorEvent>{
                                &*selected,1}
                            :std::span<const dsp::internal::LimiterDetectorEvent>{};
                        auto step=controller.value()->advance(current,future);
                        QVERIFY(step);
                        QCOMPARE(step.value()->tick,m);
                        QCOMPARE(std::bit_cast<std::uint64_t>(
                            step.value()->attack_floor_db),
                            std::bit_cast<std::uint64_t>(
                                choice->attack_floor_db));
                        if(m==8*n)appliedGain=step.value()->gain;
                    }
                    for(std::size_t ch=0;ch<fmt.channel_count();++ch) {
                        const auto input=source.view().channel(ch);
                        const auto output=pcm->view().channel(ch);
                        QVERIFY(input&&output);
                        const double raw=(*input.value())[
                            static_cast<std::size_t>(n)];
                        const double predicted=appliedGain==1.0
                            ?raw:raw*appliedGain;
                        QCOMPARE(std::bit_cast<std::uint64_t>(predicted),
                            std::bit_cast<std::uint64_t>(
                                (*output.value())[static_cast<std::size_t>(n)]));
                    }
                }
            }
            const auto candidateEnd=std::chrono::steady_clock::now();
            QCOMPARE(inputFrame,std::size_t{kFrames});
            QCOMPARE(feeder->frames(),std::uint64_t{kFrames});
            QVERIFY(feeder->drained());
            QCOMPARE(scheduler->batches(),std::size_t{kFrames/kBatchFrames});
            QCOMPARE(scheduler->next_tick(),
                std::int64_t{8*(kFrames-1)+1});
            QCOMPARE(controller.value()->next_tick(),scheduler->next_tick());
            QVERIFY(restoredState);
            QVERIFY(feeder->high_water_events()<=Feeder::kMaxRetained);
            QVERIFY(scheduler->high_water_events()<=Rolling::kMaxBatchEvents);
            const auto candidateNs=std::chrono::duration_cast<
                std::chrono::nanoseconds>(
                    candidateEnd-candidateStart).count();
            const auto referenceNs=std::chrono::duration_cast<
                std::chrono::nanoseconds>(
                    referenceEnd-referenceStart).count();
            qInfo().nospace()<<"M17 bounded detector feed fs="<<fs
                <<" channels="<<(stereo?2:1)
                <<" consumed="<<feeder->frames()
                <<" batches="<<scheduler->batches()
                <<" total_active="<<feeder->total_active_events()
                <<" detector_high_water="<<feeder->high_water_events()
                <<" selector_high_water="<<scheduler->high_water_events()
                <<" candidate_ns="<<candidateNs
                <<" reference_ns="<<referenceNs
                <<" pcm_bitwise=PASS";
            if(!csvPath.empty()) {
                std::ofstream csv(csvPath,std::ios::app);
                QVERIFY2(csv.good(),"M17 bounded feeder CSV append failed");
                csv<<fs<<','<<(stereo?2:1)<<','<<kCeiling<<','
                    <<kFrames<<','<<scheduler->batches()<<','
                    <<feeder->frames()<<','<<feeder->total_active_events()<<','
                    <<feeder->high_water_events()<<','
                    <<scheduler->high_water_events()<<','
                    <<Feeder::kMaxRetained<<','<<Rolling::kMaxBatchEvents
                    <<",PASS,PASS,PASS,PASS,PASS,"
                    <<candidateNs<<','<<referenceNs<<'\n';
            }
        }
    }
}


void TruePeakLimiterVerificationTest::experimentalLongResourceRenderBenchmark()
{
    using Controller=dsp::internal::TruePeakLimiterControllerState;
    using Feeder=dsp::internal::experimental::M17BoundedDetectorFeed;
    using Rolling=dsp::internal::experimental::M17RollingBatchSelector;
    constexpr double pi=std::numbers::pi_v<double>;
    const auto csvPath=qEnvironmentVariable(
        "RGS_M17_LONG_RENDER_PROBE_FILE").toStdString();
    if(!csvPath.empty()) {
        std::ofstream csv(csvPath,std::ios::trunc);
        QVERIFY2(csv.good(),"M17 long render CSV creation failed");
        csv<<"sample_rate_hz,channels,ceiling_dbtp,frames,"
              "reference_imodule_render_ns,experimental_render_ns,"
              "ratio_reference_over_experimental,output_pcm_bitwise,"
              "detector_cache_high_water,selector_cache_high_water,"
              "detector_event_cap,selector_event_cap,batches,"
              "source_frames_consumed,final_detector_drained,"
              "checkpoint_snapshots_restored,experimental_final_m16_checked,"
              "m16_reference_ns,m16_experimental_ns,m16_outcome,"
              "m16_post_pcm_bitwise\n";
    }
    std::size_t matrixCaseCount=0;
    for(const int fs:{44100,48000,88200,96000,176400,192000}) {
        for(const bool stereo:{false,true}) {
            for(const double ceiling:{0.0,-1.0,-6.0,-12.0}) {
            // Keep legacy long 48k/-6 probe, bounded budget on others.
            const int nFrames=fs==48000 && ceiling==-6.0 ? 32768
                : (fs==192000 ? 8192 : 4096);
            auto source=buffer(fs,stereo,nFrames);
            const auto fmt=source.view().format();
            for(std::size_t ch=0;ch<fmt.channel_count();++ch) {
                auto dst=source.mutable_view().channel(ch);
                QVERIFY(dst);
                for(int n=0;n<nFrames;++n) {
                    const double t=static_cast<double>(n)/fs;
                    double sample=(ch==0?1.0:-1.0)*
                        (0.84*std::sin(2.0*pi*701.0*t)
                        +0.32*std::sin(2.0*pi*2803.0*t+0.37));
                    if(n%223==0)sample+=ch==0?1.21:-1.16;
                    if(n>=nFrames/4 && n<nFrames/3)sample=+0.0;
                    if(n>=3*nFrames/4 && n<7*nFrames/8)sample=+0.0;
                    (*dst.value())[static_cast<std::size_t>(n)]=sample;
                }
            }
            const std::array<std::size_t,4> realChunks{31,127,19,251};
            const auto originalStart=std::chrono::steady_clock::now();
            auto original=renderRealM17(source,ceiling,realChunks);
            const auto originalEnd=std::chrono::steady_clock::now();
            QVERIFY2(original.has_value(),
                "Long-form frozen M17 IModule render failed");

            struct Metrics final {
                std::size_t detectorPeak{0};
                std::size_t selectorPeak{0};
                std::size_t batches{0};
                std::size_t checkpointRestores{0};
                std::uint64_t consumed{0};
                bool drained{false};
            } metrics{};
            const auto candidateRender=[&]()
                ->std::optional<audio::AudioBuffer> {
                auto feeder=Feeder::create(fs,ceiling,stereo?2U:1U);
                auto rolling=Rolling::create(fs,ceiling);
                auto ctr=Controller::create(fs,ceiling);
                if(!feeder || !rolling || !ctr)return std::nullopt;
                auto prepared=buffer(fs,stereo,nFrames);
                const auto horizon=ctr.value()->lookahead_ticks();
                const std::array<int,4> partitions{64,31,113,257};
                std::size_t chunk=0;
                int sourceCursor=0;
                for(int begin=0;begin<nFrames;) {
                    const int stop=std::min(nFrames,
                        begin+partitions[chunk++%partitions.size()]);
                    const std::int64_t firstTick=begin==0
                        ?-62:8*(begin-1)+1;
                    const std::int64_t lastTick=8*(stop-1);
                    const std::int64_t deadline=lastTick+horizon;
                    feeder->discard_before(firstTick);
                    while(!feeder->drained()
                        && feeder->coverage_tick()<deadline) {
                        if(sourceCursor<nFrames) {
                            const auto first=source.view().channel(0);
                            const auto second=stereo
                                ?source.view().channel(1):first;
                            if(!first||!second)return std::nullopt;
                            const auto left=(*first.value())[
                                static_cast<std::size_t>(sourceCursor)];
                            const auto right=stereo?(*second.value())[
                                static_cast<std::size_t>(sourceCursor)]:+0.0;
                            if(!feeder->push(left,right))return std::nullopt;
                            ++sourceCursor;
                        } else if(!feeder->drain()) {
                            return std::nullopt;
                        }
                    }
                    const auto events=feeder->batch(firstTick,lastTick,horizon);
                    if(!rolling->load(events,firstTick,lastTick,
                        feeder->coverage_tick(),feeder->drained()))
                        return std::nullopt;
                    if((chunk%3U)==0U) {
                        // Resume at the current certified controller tick;
                        // this is not serialized M17 IModule checkpoint I/O.
                        auto restored=Controller::create(fs,ceiling);
                        if(!restored
                            || !restored.value()->restore(
                                ctr.value()->snapshot()))
                            return std::nullopt;
                        ctr=std::move(restored);
                        ++metrics.checkpointRestores;
                    }
                    for(int n=begin;n<stop;++n) {
                        double gain=1.0;
                        for(std::int64_t tick=n==0?-62:8*(n-1)+1;
                            tick<=8*n;++tick) {
                            const double current=feeder->peak_at(tick);
                            const auto choice=rolling->choose(tick,current);
                            if(!choice)return std::nullopt;
                            const auto& winningEvent=choice->future_event;
                            const auto future=winningEvent
                                ?std::span<const dsp::internal::LimiterDetectorEvent>{
                                    &*winningEvent,1}
                                :std::span<const dsp::internal::LimiterDetectorEvent>{};
                            auto step=ctr.value()->advance(current,future);
                            if(!step)return std::nullopt;
                            if(tick==8*n)gain=step.value()->gain;
                        }
                        for(std::size_t ch=0;ch<fmt.channel_count();++ch) {
                            const auto input=source.view().channel(ch);
                            const auto output=prepared.mutable_view().channel(ch);
                            if(!input||!output)return std::nullopt;
                            const double raw=(*input.value())[
                                static_cast<std::size_t>(n)];
                            (*output.value())[static_cast<std::size_t>(n)]=
                                gain==1.0?raw:raw*gain;
                        }
                    }
                    begin=stop;
                }
                if(sourceCursor!=nFrames || !feeder->drained()
                    || rolling->next_tick()!=8*(nFrames-1)+1
                    || ctr.value()->next_tick()!=rolling->next_tick()
                    || feeder->high_water_events()>Feeder::kMaxRetained
                    || rolling->high_water_events()>Rolling::kMaxBatchEvents)
                    return std::nullopt;
                metrics.detectorPeak=feeder->high_water_events();
                metrics.selectorPeak=rolling->high_water_events();
                metrics.batches=rolling->batches();
                metrics.consumed=feeder->frames();
                metrics.drained=feeder->drained();
                return prepared;
            };
            const auto candidateStart=std::chrono::steady_clock::now();
            auto accelerated=candidateRender();
            const auto candidateEnd=std::chrono::steady_clock::now();
            QVERIFY2(accelerated.has_value(),
                "Bounded candidate rendering failed");
            QVERIFY(metrics.drained);
            QVERIFY(metrics.checkpointRestores>0);
            QCOMPARE(metrics.consumed,std::uint64_t{static_cast<std::uint64_t>(
                nFrames)});
            for(std::size_t ch=0;ch<fmt.channel_count();++ch) {
                const auto baseline=original->view().channel(ch);
                const auto candidate=accelerated->view().channel(ch);
                QVERIFY(baseline && candidate);
                QCOMPARE(baseline.value()->size(),candidate.value()->size());
                for(int n=0;n<nFrames;++n) {
                    const auto pos=static_cast<std::size_t>(n);
                    QCOMPARE(std::bit_cast<std::uint64_t>(
                        (*baseline.value())[pos]),
                        std::bit_cast<std::uint64_t>(
                            (*candidate.value())[pos]));
                }
            }
            // Real frozen M16 authority and guarded global trim are
            // evaluated independently AFTER the untrimmed PCM oracle.
            const auto m16ReferenceStart=std::chrono::steady_clock::now();
            const auto refChecked=m17_verifier::verify_full_resource(
                *original,ceiling);
            const auto m16ReferenceEnd=std::chrono::steady_clock::now();
            const auto m16CandidateStart=std::chrono::steady_clock::now();
            const auto candidateChecked=m17_verifier::verify_full_resource(
                *accelerated,ceiling);
            const auto m16CandidateEnd=std::chrono::steady_clock::now();
            QCOMPARE(static_cast<bool>(refChecked),
                     static_cast<bool>(candidateChecked));
            if(refChecked) {
                QCOMPARE(refChecked.value()->trim_used,
                         candidateChecked.value()->trim_used);
                QCOMPARE(refChecked.value()->status,
                         candidateChecked.value()->status);
                for(const auto& compare:{
                    std::pair<double,double>{
                        refChecked.value()->before_peak,
                        candidateChecked.value()->before_peak},
                    {refChecked.value()->after_peak,
                     candidateChecked.value()->after_peak},
                    {refChecked.value()->global_trim_gain,
                     candidateChecked.value()->global_trim_gain}}) {
                    QCOMPARE(std::bit_cast<std::uint64_t>(compare.first),
                             std::bit_cast<std::uint64_t>(compare.second));
                }
            } else {
                QCOMPARE(category(*refChecked.error()),
                         category(*candidateChecked.error()));
            }
            // Also verify the final PCM AFTER M16: one common stereo
            // gain or a matching fail-closed unmodified candidate.
            for(std::size_t ch=0;ch<fmt.channel_count();++ch) {
                const auto baseline=original->view().channel(ch);
                const auto candidate=accelerated->view().channel(ch);
                QVERIFY(baseline && candidate);
                for(int n=0;n<nFrames;++n) {
                    const auto pos=static_cast<std::size_t>(n);
                    QCOMPARE(std::bit_cast<std::uint64_t>(
                        (*baseline.value())[pos]),
                        std::bit_cast<std::uint64_t>(
                            (*candidate.value())[pos]));
                }
            }
            const auto m16ReferenceNs=std::chrono::duration_cast<
                std::chrono::nanoseconds>(
                    m16ReferenceEnd-m16ReferenceStart).count();
            const auto m16CandidateNs=std::chrono::duration_cast<
                std::chrono::nanoseconds>(
                    m16CandidateEnd-m16CandidateStart).count();
            const auto originalNs=std::chrono::duration_cast<
                std::chrono::nanoseconds>(originalEnd-originalStart).count();
            const auto candidateNs=std::chrono::duration_cast<
                std::chrono::nanoseconds>(candidateEnd-candidateStart).count();
            const double ratio=candidateNs>0
                ?static_cast<double>(originalNs)/candidateNs:0.0;
            qInfo().nospace()<<"M17 longer isolated render Fs="<<fs
                <<" stereo="<<stereo<<" frames="<<nFrames
                <<" original_ns="<<originalNs
                <<" candidate_ns="<<candidateNs
                <<" ratio="<<ratio
                <<" detector_high_water="<<metrics.detectorPeak
                <<" selector_high_water="<<metrics.selectorPeak
                <<" batches="<<metrics.batches
                <<" m16_check="<<(refChecked?"PASS":"MATCHING_REJECTION")
                <<" m16_reference_ns="<<m16ReferenceNs
                <<" m16_experimental_ns="<<m16CandidateNs
                <<" pcm_before_after_m16_bitwise=PASS";
            if(!csvPath.empty()) {
                std::ofstream csv(csvPath,std::ios::app);
                QVERIFY2(csv.good(),"M17 long render CSV append failed");
                csv<<fs<<','<<(stereo?2:1)<<','<<ceiling<<','
                    <<nFrames<<','<<originalNs<<','<<candidateNs<<','
                    <<ratio<<",PASS,"
                    <<metrics.detectorPeak<<','<<metrics.selectorPeak<<','
                    <<Feeder::kMaxRetained<<','<<Rolling::kMaxBatchEvents
                    <<','<<metrics.batches<<','<<metrics.consumed
                    <<",PASS,"<<metrics.checkpointRestores<<",YES,"
                    <<m16ReferenceNs<<','<<m16CandidateNs<<','
                    <<(refChecked?"PASS":"MATCHING_REJECTION")
                    <<",PASS\n";
            }
                ++matrixCaseCount;
            }
        }
    }
    QCOMPARE(matrixCaseCount,std::size_t{48});
}


void TruePeakLimiterVerificationTest::experimental64FrameRealtimeQuantumProbe()
{
    using Controller=dsp::internal::TruePeakLimiterControllerState;
    using Feeder=dsp::internal::experimental::M17BoundedDetectorFeed;
    using Rolling=dsp::internal::experimental::M17RollingBatchSelector;
    constexpr int kQuantum=64;
    constexpr double pi=std::numbers::pi_v<double>;
    const auto csvPath=qEnvironmentVariable(
        "RGS_M17_QUANTUM_PROBE_FILE").toStdString();
    if(!csvPath.empty()) {
        std::ofstream csv(csvPath,std::ios::trunc);
        QVERIFY2(csv.good(),"M17 isolated quantum CSV creation failed");
        csv<<"sample_rate_hz,channels,ceiling_dbtp,frames,"
              "quantum_frames,quantum_count,quantum_budget_ns,"
              "mean_quantum_ns,p99_quantum_ns,max_quantum_ns,"
              "first_quantum_ns,over_budget_quanta,"
              "mean_steady_ns,p99_steady_ns,max_steady_ns,"
              "over_budget_steady,detector_event_high_water,"
              "selector_event_high_water,pcm_bitwise,detector_drained,"
              "frozen_reference_render_ns,real_time_qualified,"
              "mean_detector_ns,p99_detector_ns,max_detector_ns,"
              "mean_cache_ns,p99_cache_ns,max_cache_ns,"
              "mean_control_ns,p99_control_ns,max_control_ns,"
              "mean_pcm_store_ns,p99_pcm_store_ns,max_pcm_store_ns,"
              "hann_weight_build_count\n";
    }
    for(const int fs:{48000,192000}) {
        for(const bool stereo:{false,true}) {
            for(const double ceiling:{-6.0,-12.0}) {
                const int nFrames=fs==48000?4096:8192;
                auto source=buffer(fs,stereo,nFrames);
                const auto fmt=source.view().format();
                for(std::size_t ch=0;ch<fmt.channel_count();++ch) {
                    auto lane=source.mutable_view().channel(ch);
                    QVERIFY(lane);
                    for(int n=0;n<nFrames;++n) {
                        const double t=static_cast<double>(n)/fs;
                        double sample=(ch==0?1.0:-1.0)*
                            (0.95*std::sin(2.0*pi*941.0*t)
                             +0.43*std::sin(2.0*pi*2707.0*t+0.19));
                        if(n%91==0)sample+=ch==0?1.22:-1.15;
                        if(n>=nFrames/3 && n<nFrames/2)sample=+0.0;
                        (*lane.value())[static_cast<std::size_t>(n)]=sample;
                    }
                }
                const std::array<std::size_t,3> chunks{29,127,37};
                const auto refStart=std::chrono::steady_clock::now();
                const auto frozen=renderRealM17(source,ceiling,chunks);
                const auto refEnd=std::chrono::steady_clock::now();
                QVERIFY2(frozen.has_value(),"frozen M17 quantum oracle unavailable");

                auto feeder=Feeder::create(fs,ceiling,stereo?2U:1U);
                auto rolling=Rolling::create(fs,ceiling);
                auto controller=Controller::create(fs,ceiling);
                QVERIFY(feeder && rolling && controller);
                const auto horizon=controller.value()->lookahead_ticks();
                const auto budgetNs=std::int64_t{1'000'000'000}
                    *kQuantum/fs;
                std::size_t feedPosition=0;
                auto prepared=buffer(fs,stereo,nFrames);
                std::vector<std::int64_t> quantumNs;
                quantumNs.reserve(static_cast<std::size_t>(nFrames/kQuantum));
                std::vector<std::int64_t> detectorNs,cacheNs,controlNs,pcmNs;
                for(int start=0;start<nFrames;start+=kQuantum) {
                    const auto began=std::chrono::steady_clock::now();
                    const auto first=start==0?-62:8*(start-1)+1;
                    const auto last=8*(start+kQuantum-1);
                    const auto deadline=last+horizon;
                    feeder->discard_before(first);
                    while(!feeder->drained()
                          && feeder->coverage_tick()<deadline) {
                        if(feedPosition<static_cast<std::size_t>(nFrames)) {
                            const auto left=source.view().channel(0);
                            const auto right=stereo
                                ?source.view().channel(1):left;
                            QVERIFY(left && right);
                            const auto l=(*left.value())[feedPosition];
                            const auto r=stereo
                                ?(*right.value())[feedPosition]:+0.0;
                            QVERIFY2(feeder->push(l,r),
                                "Signed detector failed during quantum probe");
                            ++feedPosition;
                        } else {
                            QVERIFY(feeder->drain());
                        }
                    }
                    const auto afterDetector=std::chrono::steady_clock::now();
                    const auto events=feeder->batch(first,last,horizon);
                    QVERIFY2(rolling->load(events,first,last,
                        feeder->coverage_tick(),feeder->drained()),
                        "Unproven M17 quantum failed to load");
                    const auto afterCache=std::chrono::steady_clock::now();
                    std::int64_t blockControlNs=0,blockPcmNs=0;
                    for(int n=start;n<start+kQuantum;++n) {
                        const auto frameBegin=std::chrono::steady_clock::now();
                        double gain=1.0;
                        for(std::int64_t tick=n==0?-62:8*(n-1)+1;
                            tick<=8*n;++tick) {
                            const auto current=feeder->peak_at(tick);
                            auto chosen=rolling->choose(tick,current);
                            QVERIFY(chosen);
                            const auto& winner=chosen->future_event;
                            const auto future=winner
                                ?std::span<const dsp::internal::LimiterDetectorEvent>{
                                    &*winner,1}
                                :std::span<const dsp::internal::LimiterDetectorEvent>{};
                            auto step=controller.value()->advance(
                                current,future);
                            QVERIFY(step);
                            if(tick==8*n)gain=step.value()->gain;
                        }
                        const auto frameControlEnd=std::chrono::steady_clock::now();
                        blockControlNs+=std::chrono::duration_cast<
                            std::chrono::nanoseconds>(
                                frameControlEnd-frameBegin).count();
                        for(std::size_t ch=0;ch<fmt.channel_count();++ch) {
                            const auto input=source.view().channel(ch);
                            const auto output=prepared.mutable_view().channel(ch);
                            QVERIFY(input && output);
                            const auto raw=(*input.value())[
                                static_cast<std::size_t>(n)];
                            (*output.value())[static_cast<std::size_t>(n)]=
                                gain==1.0?raw:raw*gain;
                        }
                        blockPcmNs+=std::chrono::duration_cast<
                            std::chrono::nanoseconds>(
                                std::chrono::steady_clock::now()
                                -frameControlEnd).count();
                    }
                    const auto ended=std::chrono::steady_clock::now();
                    detectorNs.push_back(std::chrono::duration_cast<
                        std::chrono::nanoseconds>(afterDetector-began).count());
                    cacheNs.push_back(std::chrono::duration_cast<
                        std::chrono::nanoseconds>(afterCache-afterDetector).count());
                    controlNs.push_back(blockControlNs);
                    pcmNs.push_back(blockPcmNs);
                    quantumNs.push_back(std::chrono::duration_cast<
                        std::chrono::nanoseconds>(ended-began).count());
                }
                QCOMPARE(feedPosition,static_cast<std::size_t>(nFrames));
                QCOMPARE(controller.value()->next_tick(),
                         std::int64_t{8*(nFrames-1)+1});
                QCOMPARE(rolling->next_tick(),
                         controller.value()->next_tick());
                QVERIFY(feeder->drained());
                QVERIFY(feeder->high_water_events()<=Feeder::kMaxRetained);
                QVERIFY(rolling->high_water_events()<=Rolling::kMaxBatchEvents);
                // The immutable exact-Hann weights must be constructed
                // once and reused across all 64-frame batches.
                QCOMPARE(rolling->weight_build_count(),std::size_t{1});
                for(std::size_t ch=0;ch<fmt.channel_count();++ch) {
                    const auto input=frozen->view().channel(ch);
                    const auto output=prepared.view().channel(ch);
                    QVERIFY(input && output);
                    QCOMPARE(input.value()->size(),output.value()->size());
                    for(std::size_t n=0;n<input.value()->size();++n)
                        QCOMPARE(std::bit_cast<std::uint64_t>(
                                     (*input.value())[n]),
                                 std::bit_cast<std::uint64_t>(
                                     (*output.value())[n]));
                }
                QVERIFY(quantumNs.size()>=64U);
                const auto summary=[&](std::vector<std::int64_t> measured) {
                    std::sort(measured.begin(),measured.end());
                    const auto total=std::accumulate(
                        measured.begin(),measured.end(),std::int64_t{0});
                    const auto p99idx=static_cast<std::size_t>(
                        0.99*static_cast<double>(measured.size()-1));
                    const auto over=std::count_if(measured.begin(),measured.end(),
                        [&](std::int64_t v){return v>budgetNs;});
                    return std::array<std::int64_t,4>{
                        total/static_cast<std::int64_t>(measured.size()),
                        measured[p99idx],measured.back(),over};
                };
                const auto all=summary(quantumNs);
                const std::vector<std::int64_t> steady(
                    quantumNs.begin()+4,quantumNs.end()-4);
                const auto sustained=summary(steady);
                const auto detectorStats=summary(detectorNs);
                const auto cacheStats=summary(cacheNs);
                const auto controlStats=summary(controlNs);
                const auto pcmStats=summary(pcmNs);
                const auto baselineNs=std::chrono::duration_cast<
                    std::chrono::nanoseconds>(refEnd-refStart).count();
                qInfo().nospace()<<"M17 isolated 64-frame quantum fs="<<fs
                    <<" ch="<<(stereo?2:1)<<" ceiling="<<ceiling
                    <<" budget_ns="<<budgetNs<<" mean_ns="<<all[0]
                    <<" p99_ns="<<all[1]<<" worst_ns="<<all[2]
                    <<" over="<<all[3]<<"/"<<quantumNs.size()
                    <<" steady_p99_ns="<<sustained[1]
                    <<" steady_over="<<sustained[3]
                    <<" mean_detector_ns="<<detectorStats[0]
                    <<" mean_cache_ns="<<cacheStats[0]
                    <<" mean_controller_ns="<<controlStats[0]
                    <<" mean_pcm_ns="<<pcmStats[0]
                    <<" PCM_bitwise=PASS; real_time_qualified=NO";
                if(!csvPath.empty()) {
                    std::ofstream csv(csvPath,std::ios::app);
                    QVERIFY2(csv.good(),"M17 quantum diagnostic append failed");
                    csv<<fs<<','<<(stereo?2:1)<<','<<ceiling<<','
                       <<nFrames<<','<<kQuantum<<','<<quantumNs.size()<<','
                       <<budgetNs<<','<<all[0]<<','<<all[1]<<','<<all[2]<<','
                       <<quantumNs.front()<<','<<all[3]<<','
                       <<sustained[0]<<','<<sustained[1]<<','<<sustained[2]
                       <<','<<sustained[3]<<','
                       <<feeder->high_water_events()<<','
                       <<rolling->high_water_events()<<",PASS,PASS,"
                       <<baselineNs<<",NO,"
                       <<detectorStats[0]<<','<<detectorStats[1]<<','<<detectorStats[2]<<','
                       <<cacheStats[0]<<','<<cacheStats[1]<<','<<cacheStats[2]<<','
                       <<controlStats[0]<<','<<controlStats[1]<<','<<controlStats[2]<<','
                       <<pcmStats[0]<<','<<pcmStats[1]<<','<<pcmStats[2]<<','
                       <<rolling->weight_build_count()<<'\n';
                }
            }
        }
    }
}


void TruePeakLimiterVerificationTest::experimentalLazyControllerBitwiseOracle()
{
    using Event=dsp::internal::LimiterDetectorEvent;
    using Detector=dsp::internal::TruePeakLimiterDetectorState;
    using Reference=dsp::internal::TruePeakLimiterControllerState;
    using Lazy=dsp::internal::experimental::M17LazyController;
    using Selector=dsp::internal::experimental::M17LookaheadSelector;
    constexpr double pi=std::numbers::pi_v<double>;
    const auto csvPath=qEnvironmentVariable(
        "RGS_M17_LAZY_CONTROL_PROBE_FILE").toStdString();
    if(!csvPath.empty()) {
        std::ofstream csv(csvPath,std::ios::trunc);
        QVERIFY2(csv.good(),"M17 lazy controller CSV creation failed");
        csv<<"sample_rate_hz,channels,ceiling_dbtp,frames,"
              "controller_ticks,release_exp_skipped,"
              "release_exp_evaluated,gain_pow_reused,gain_pow_evaluated,"
              "reference_controller_ns,lazy_controller_ns,"
              "state_fields_bitwise,pcm_imodule_bitwise,checkpoint_resume_bitwise\n";
    }

    std::size_t scenarios=0;
    for(const int fs:{44100,48000,88200,96000,176400,192000}) {
        for(const bool stereo:{false,true}) {
            for(const double ceiling:{0.0,-1.0,-6.0,-12.0}) {
                const int nFrames=fs==192000 && stereo && ceiling==-6.0
                    ?2560:384;
                auto source=buffer(fs,stereo,nFrames);
                const auto fmt=source.view().format();
                for(std::size_t ch=0;ch<fmt.channel_count();++ch) {
                    auto samples=source.mutable_view().channel(ch);
                    QVERIFY(samples);
                    for(int n=0;n<nFrames;++n) {
                        const double t=static_cast<double>(n)/fs;
                        double v=+0.0;
                        if(n<48 || (n>=160 && n<184)
                            || (n>=nFrames-20 && n<nFrames-6)) {
                            v=(ch==0?1.0:-1.0)*
                                (0.88*std::sin(2.0*pi*929.0*t)
                                 +0.47*std::sin(2.0*pi*3137.0*t+0.31));
                            if(n%11==0)v+=ch==0?1.16:-1.13;
                        }
                        (*samples.value())[static_cast<std::size_t>(n)]=v;
                    }
                }
                const std::array<std::size_t,4> chunks{17,64,31,113};
                const auto truePcm=renderRealM17(source,ceiling,chunks);
                QVERIFY2(truePcm.has_value(),
                    "frozen M17 PCM unavailable for lazy controller oracle");

                auto real=Reference::create(fs,ceiling);
                auto lazy=Lazy::create(fs,ceiling);
                auto selector=Selector::create(fs,ceiling);
                QVERIFY(real && lazy && selector);
                Detector detector(stereo?2U:1U);
                const double linear=std::pow(10.0,ceiling/20.0);
                std::vector<Event> events;
                events.reserve(static_cast<std::size_t>(8*nFrames+118));
                std::int64_t covered=std::numeric_limits<std::int64_t>::min();
                const auto accept=[&](const Event& event) {
                    if(!event.valid)return true;
                    if(event.tick<=covered
                        || !std::isfinite(event.peak)
                        || event.peak<0.0)return false;
                    covered=event.tick;
                    if(event.peak>linear)events.push_back(event);
                    return true;
                };
                for(int n=0;n<nFrames;++n) {
                    const auto left=source.view().channel(0);
                    const auto right=stereo?source.view().channel(1):left;
                    QVERIFY(left && right);
                    const double l=(*left.value())[static_cast<std::size_t>(n)];
                    const double r=stereo?(*right.value())[
                        static_cast<std::size_t>(n)]:+0.0;
                    auto quarters=detector.push_programme_frame(l,r);
                    QVERIFY(quarters);
                    for(const auto& quarter:*quarters.value()) {
                        QVERIFY(accept(quarter.preceding_anchor));
                        QVERIFY(accept(quarter.midpoint));
                    }
                }
                const auto tail=detector.finalize_detector();
                QVERIFY(tail);
                for(const auto& quarter:*tail.value()) {
                    QVERIFY(accept(quarter.preceding_anchor));
                    QVERIFY(accept(quarter.midpoint));
                }
                QVERIFY(selector->prepare(events));
                std::size_t cursor=0;
                std::int64_t originalNs=0,lazyNs=0;
                Lazy::Counters priorCounters{};
                bool restored=false;
                for(int n=0;n<nFrames;++n) {
                    if(n==nFrames/2) {
                        const auto frozenState=real.value()->snapshot();
                        const auto lazyState=lazy->snapshot();
                        const auto partial=lazy->counters();
                        priorCounters.ticks+=partial.ticks;
                        priorCounters.release_exp_evaluated+=
                            partial.release_exp_evaluated;
                        priorCounters.release_exp_skipped+=
                            partial.release_exp_skipped;
                        priorCounters.gain_pow_evaluated+=
                            partial.gain_pow_evaluated;
                        priorCounters.gain_pow_reused+=
                            partial.gain_pow_reused;
                        QCOMPARE(frozenState.next_tick,lazyState.next_tick);
                        QCOMPARE(frozenState.hold_remaining,
                                 lazyState.hold_remaining);
                        QCOMPARE(std::bit_cast<std::uint64_t>(
                            frozenState.density),
                            std::bit_cast<std::uint64_t>(lazyState.density));
                        QCOMPARE(std::bit_cast<std::uint64_t>(
                            frozenState.previous_reduction_db),
                            std::bit_cast<std::uint64_t>(
                                lazyState.previous_reduction_db));
                        auto resumedReal=Reference::create(fs,ceiling);
                        auto resumedLazy=Lazy::create(fs,ceiling);
                        QVERIFY(resumedReal && resumedLazy);
                        QVERIFY(resumedReal.value()->restore(frozenState));
                        QVERIFY(resumedLazy->restore(lazyState));
                        real=std::move(resumedReal);
                        lazy=std::move(resumedLazy);
                        restored=true;
                    }
                    double frameGain=1.0;
                    for(std::int64_t m=n==0?-62:8*(n-1)+1;
                        m<=8*n;++m) {
                        while(cursor<events.size()&&events[cursor].tick<m)
                            ++cursor;
                        const bool currentEvent=cursor<events.size()
                            && events[cursor].tick==m;
                        const double current=currentEvent
                            ?events[cursor].peak:+0.0;
                        const auto selected=selector->choose(m,current);
                        QVERIFY(selected);
                        const auto& winner=selected->future_event;
                        const auto future=winner
                            ?std::span<const Event>{&*winner,1}
                            :std::span<const Event>{};
                        const auto t0=std::chrono::steady_clock::now();
                        const auto referenceStep=real.value()->advance(
                            current,future);
                        const auto t1=std::chrono::steady_clock::now();
                        const auto lazyStep=lazy->advance(current,future);
                        const auto t2=std::chrono::steady_clock::now();
                        QVERIFY(referenceStep && lazyStep);
                        originalNs+=std::chrono::duration_cast<
                            std::chrono::nanoseconds>(t1-t0).count();
                        lazyNs+=std::chrono::duration_cast<
                            std::chrono::nanoseconds>(t2-t1).count();
                        const auto& a=*referenceStep.value();
                        const auto& b=*lazyStep.value();
                        QCOMPARE(a.tick,b.tick);
                        QCOMPARE(a.hold_remaining,b.hold_remaining);
                        for(const auto& pair:{
                            std::pair<double,double>{a.required_db,b.required_db},
                            {a.attack_floor_db,b.attack_floor_db},
                            {a.reduction_db,b.reduction_db},
                            {a.gain,b.gain},
                            {a.density,b.density},
                            {a.release_ms,b.release_ms}}) {
                            QCOMPARE(std::bit_cast<std::uint64_t>(pair.first),
                                     std::bit_cast<std::uint64_t>(pair.second));
                        }
                        if(m==8*n)frameGain=b.gain;
                    }
                    for(std::size_t ch=0;ch<fmt.channel_count();++ch) {
                        const auto input=source.view().channel(ch);
                        const auto expected=truePcm->view().channel(ch);
                        QVERIFY(input && expected);
                        const auto value=(*input.value())[
                            static_cast<std::size_t>(n)];
                        const auto actual=frameGain==1.0
                            ?value:value*frameGain;
                        QCOMPARE(std::bit_cast<std::uint64_t>(actual),
                            std::bit_cast<std::uint64_t>(
                                (*expected.value())[static_cast<std::size_t>(n)]));
                    }
                }
                QCOMPARE(real.value()->next_tick(),lazy->next_tick());
                auto counters=lazy->counters();
                counters.ticks+=priorCounters.ticks;
                counters.release_exp_evaluated+=
                    priorCounters.release_exp_evaluated;
                counters.release_exp_skipped+=
                    priorCounters.release_exp_skipped;
                counters.gain_pow_evaluated+=priorCounters.gain_pow_evaluated;
                counters.gain_pow_reused+=priorCounters.gain_pow_reused;
                QCOMPARE(counters.ticks,
                    std::uint64_t{static_cast<std::uint64_t>(8*nFrames+55)});
                QVERIFY(counters.release_exp_skipped>0);
                QVERIFY(counters.gain_pow_reused>0);
                QVERIFY(restored);
                ++scenarios;
                qInfo().nospace()<<"M17 isolated lazy-controller fs="<<fs
                    <<" channels="<<(stereo?2:1)
                    <<" ceiling="<<ceiling<<" frames="<<nFrames
                    <<" frozen_ns="<<originalNs<<" lazy_ns="<<lazyNs
                    <<" release_exp_skipped="<<counters.release_exp_skipped
                    <<" release_exp_evaluated="<<counters.release_exp_evaluated
                    <<" pow_reused="<<counters.gain_pow_reused
                    <<" state_pcm_bitwise=PASS";
                if(!csvPath.empty()) {
                    std::ofstream csv(csvPath,std::ios::app);
                    QVERIFY2(csv.good(),"M17 lazy-controller CSV append failed");
                    csv<<fs<<','<<(stereo?2:1)<<','<<ceiling<<','
                        <<nFrames<<','<<counters.ticks<<','
                        <<counters.release_exp_skipped<<','
                        <<counters.release_exp_evaluated<<','
                        <<counters.gain_pow_reused<<','
                        <<counters.gain_pow_evaluated<<','
                        <<originalNs<<','<<lazyNs<<",PASS,PASS,PASS\n";
                }
            }
        }
    }
    QCOMPARE(scenarios,std::size_t{48});
}


void TruePeakLimiterVerificationTest::experimentalIntegratedLazyQuantumBenchmark()
{
    using Reference=dsp::internal::TruePeakLimiterControllerState;
    using Lazy=dsp::internal::experimental::M17LazyController;
    using Event=dsp::internal::LimiterDetectorEvent;
    using Feeder=dsp::internal::experimental::M17BoundedDetectorFeed;
    using Rolling=dsp::internal::experimental::M17RollingBatchSelector;
    constexpr int kQuantum=64;
    constexpr double pi=std::numbers::pi_v<double>;
    const auto csvPath=qEnvironmentVariable(
        "RGS_M17_INTEGRATED_LAZY_PROBE_FILE").toStdString();
    if(!csvPath.empty()) {
        std::ofstream csv(csvPath,std::ios::trunc);
        QVERIFY2(csv.good(),"M17 integrated lazy CSV creation failed");
        csv<<"sample_rate_hz,channels,ceiling_dbtp,frames,controller_type,"
              "quantum_frames,quantum_count,quantum_budget_ns,"
              "mean_quantum_ns,p99_quantum_ns,max_quantum_ns,"
              "missed_quantum_count,total_render_ns,source_frames_consumed,"
              "detector_cache_high_water,selector_cache_high_water,"
              "hann_weight_build_count,release_exp_skipped,"
              "release_exp_evaluated,gain_pow_reused,"
              "pcm_imodule_bitwise,detector_drained,qualified_realtime,"
              "mean_selector_ns,p99_selector_ns,max_selector_ns,"
              "mean_controller_advance_ns,p99_controller_advance_ns,"
              "max_controller_advance_ns,"
              "mean_other_ns,p99_other_ns,max_other_ns\n";
    }
    std::size_t scenarios=0;
    for(const int fs:{48000,192000}) {
        for(const bool stereo:{false,true}) {
            for(const double ceiling:{-6.0,-12.0}) {
                const int nFrames=fs==48000?4096:8192;
                auto source=buffer(fs,stereo,nFrames);
                const auto fmt=source.view().format();
                for(std::size_t ch=0;ch<fmt.channel_count();++ch) {
                    auto lane=source.mutable_view().channel(ch);
                    QVERIFY(lane);
                    for(int n=0;n<nFrames;++n) {
                        const double t=static_cast<double>(n)/fs;
                        double sample=(ch==0?1.0:-1.0)*
                            (0.95*std::sin(2.0*pi*941.0*t)
                             +0.43*std::sin(2.0*pi*2707.0*t+0.19));
                        if(n%91==0)sample+=ch==0?1.22:-1.15;
                        if(n>=nFrames/3 && n<nFrames/2)sample=+0.0;
                        (*lane.value())[static_cast<std::size_t>(n)]=sample;
                    }
                }
                const std::array<std::size_t,3> chunks{29,127,37};
                const auto frozen=renderRealM17(source,ceiling,chunks);
                QVERIFY2(frozen.has_value(),
                    "Frozen IModule full-PCM oracle unavailable");
                const auto budgetNs=std::int64_t{1'000'000'000}
                    *kQuantum/fs;
                // Alternate measurement order between sample rates.
                // No simultaneous tasks: compare the same input on one runner.
                for(int order=0;order<2;++order) {
                    const bool lazyMode=(order==((fs==192000)?0:1));
                    auto feeder=Feeder::create(fs,ceiling,stereo?2U:1U);
                    auto rolling=Rolling::create(fs,ceiling);
                    auto reference=Reference::create(fs,ceiling);
                    auto alternative=Lazy::create(fs,ceiling);
                    QVERIFY(feeder && rolling && reference && alternative);
                    const auto horizon=reference.value()->lookahead_ticks();
                    QCOMPARE(horizon,alternative->lookahead_ticks());
                    std::size_t feedPosition=0;
                    auto prepared=buffer(fs,stereo,nFrames);
                    std::vector<std::int64_t> quantumNs;
                    quantumNs.reserve(static_cast<std::size_t>(nFrames/kQuantum));
                    // Stage clocks are observational only: clock overhead
                    // increases quantum time. The original controller and
                    // detector mathematics remain identical.
                    std::vector<std::int64_t> selectorNs,advanceNs,otherNs;
                    selectorNs.reserve(quantumNs.capacity());
                    advanceNs.reserve(quantumNs.capacity());
                    otherNs.reserve(quantumNs.capacity());
                    const auto totalStart=std::chrono::steady_clock::now();
                    for(int start=0;start<nFrames;start+=kQuantum) {
                        const auto quantumStart=std::chrono::steady_clock::now();
                        std::int64_t chooseCostNs=0,advanceCostNs=0;
                        const auto first=start==0?-62:8*(start-1)+1;
                        const auto last=8*(start+kQuantum-1);
                        const auto deadline=last+horizon;
                        feeder->discard_before(first);
                        while(!feeder->drained()
                              && feeder->coverage_tick()<deadline) {
                            if(feedPosition<static_cast<std::size_t>(nFrames)) {
                                const auto left=source.view().channel(0);
                                const auto right=stereo
                                    ?source.view().channel(1):left;
                                QVERIFY(left && right);
                                const auto l=(*left.value())[feedPosition];
                                const auto r=stereo
                                    ?(*right.value())[feedPosition]:+0.0;
                                QVERIFY(feeder->push(l,r));
                                ++feedPosition;
                            } else {
                                QVERIFY(feeder->drain());
                            }
                        }
                        const auto events=feeder->batch(first,last,horizon);
                        QVERIFY2(rolling->load(events,first,last,
                            feeder->coverage_tick(),feeder->drained()),
                            "Lookahead batch was not physically certified");
                        for(int n=start;n<start+kQuantum;++n) {
                            double gain=1.0;
                            for(std::int64_t tick=n==0?-62:8*(n-1)+1;
                                tick<=8*n;++tick) {
                                const auto current=feeder->peak_at(tick);
                                const auto chooseStart=std::chrono::steady_clock::now();
                                const auto chosen=rolling->choose(tick,current);
                                const auto chooseStop=std::chrono::steady_clock::now();
                                QVERIFY(chosen);
                                chooseCostNs+=std::chrono::duration_cast<
                                    std::chrono::nanoseconds>(
                                        chooseStop-chooseStart).count();
                                const auto& winner=chosen->future_event;
                                const auto future=winner
                                    ?std::span<const Event>{&*winner,1}
                                    :std::span<const Event>{};
                                const auto advanceStart=std::chrono::steady_clock::now();
                                const auto step=lazyMode
                                    ?alternative->advance(current,future)
                                    :reference.value()->advance(current,future);
                                const auto advanceStop=std::chrono::steady_clock::now();
                                QVERIFY(step);
                                advanceCostNs+=std::chrono::duration_cast<
                                    std::chrono::nanoseconds>(
                                        advanceStop-advanceStart).count();
                                if(tick==8*n)gain=step.value()->gain;
                            }
                            for(std::size_t ch=0;ch<fmt.channel_count();++ch) {
                                const auto input=source.view().channel(ch);
                                const auto output=prepared.mutable_view().channel(ch);
                                QVERIFY(input && output);
                                const auto raw=(*input.value())[
                                    static_cast<std::size_t>(n)];
                                (*output.value())[static_cast<std::size_t>(n)]=
                                    gain==1.0?raw:raw*gain;
                            }
                        }
                        const auto quantumElapsed=std::chrono::duration_cast<
                            std::chrono::nanoseconds>(
                                std::chrono::steady_clock::now()-quantumStart)
                                    .count();
                        QVERIFY2(quantumElapsed>=chooseCostNs+advanceCostNs,
                            "M17 stage durations exceed outer quantum observation");
                        quantumNs.push_back(quantumElapsed);
                        selectorNs.push_back(chooseCostNs);
                        advanceNs.push_back(advanceCostNs);
                        otherNs.push_back(quantumElapsed-chooseCostNs-advanceCostNs);
                    }
                    const auto totalNs=std::chrono::duration_cast<
                        std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now()-totalStart)
                                .count();
                    QCOMPARE(feedPosition,static_cast<std::size_t>(nFrames));
                    const auto lastTick=std::int64_t{8*(nFrames-1)+1};
                    QCOMPARE(rolling->next_tick(),lastTick);
                    if(lazyMode)QCOMPARE(alternative->next_tick(),lastTick);
                    else QCOMPARE(reference.value()->next_tick(),lastTick);
                    QCOMPARE(rolling->weight_build_count(),std::size_t{1});
                    QVERIFY(feeder->drained());
                    QVERIFY(feeder->high_water_events()<=Feeder::kMaxRetained);
                    QVERIFY(rolling->high_water_events()<=Rolling::kMaxBatchEvents);
                    for(std::size_t ch=0;ch<fmt.channel_count();++ch) {
                        const auto actual=prepared.view().channel(ch);
                        const auto expected=frozen->view().channel(ch);
                        QVERIFY(actual && expected);
                        QCOMPARE(actual.value()->size(),expected.value()->size());
                        for(std::size_t n=0;n<actual.value()->size();++n)
                            QCOMPARE(std::bit_cast<std::uint64_t>(
                                         (*actual.value())[n]),
                                     std::bit_cast<std::uint64_t>(
                                         (*expected.value())[n]));
                    }
                    std::sort(quantumNs.begin(),quantumNs.end());
                    QCOMPARE(quantumNs.size(),
                        static_cast<std::size_t>(nFrames/kQuantum));
                    const auto p99Index=static_cast<std::size_t>(
                        0.99*static_cast<double>(quantumNs.size()-1));
                    const auto meanNs=std::accumulate(quantumNs.begin(),
                        quantumNs.end(),std::int64_t{0})/
                        static_cast<std::int64_t>(quantumNs.size());
                    const auto p99Ns=quantumNs[p99Index];
                    const auto worstNs=quantumNs.back();
                    const auto misses=std::count_if(
                        quantumNs.begin(),quantumNs.end(),
                        [&](std::int64_t ns){return ns>budgetNs;});
                    const auto stageStats=[&](std::vector<std::int64_t> points) {
                        std::sort(points.begin(),points.end());
                        const auto total=std::accumulate(points.begin(),
                            points.end(),std::int64_t{0});
                        return std::array<std::int64_t,3>{
                            total/static_cast<std::int64_t>(points.size()),
                            points[p99Index],points.back()};
                    };
                    const auto selectStats=stageStats(selectorNs);
                    const auto advanceStats=stageStats(advanceNs);
                    const auto otherStats=stageStats(otherNs);
                    const auto counters=alternative->counters();
                    qInfo().nospace()<<"M17 integrated lazy quantum fs="<<fs
                        <<" ch="<<(stereo?2:1)<<" ceiling="<<ceiling
                        <<" engine="<<(lazyMode?"LAZY":"FROZEN")
                        <<" mean_ns="<<meanNs<<" p99_ns="<<p99Ns
                        <<" worst_ns="<<worstNs<<" missed="<<misses
                        <<"/"<<quantumNs.size()
                        <<" mean_selector_ns="<<selectStats[0]
                        <<" mean_advance_ns="<<advanceStats[0]
                        <<" mean_other_ns="<<otherStats[0]
                        <<" pcm_bitwise=PASS realtime_qualified=NO";
                    if(!csvPath.empty()) {
                        std::ofstream csv(csvPath,std::ios::app);
                        QVERIFY2(csv.good(),"M17 integrated lazy CSV append failed");
                        csv<<fs<<','<<(stereo?2:1)<<','<<ceiling<<','
                            <<nFrames<<','<<(lazyMode?"LAZY":"FROZEN")<<','
                            <<kQuantum<<','<<quantumNs.size()<<','<<budgetNs<<','
                            <<meanNs<<','<<p99Ns<<','<<worstNs<<','
                            <<misses<<','<<totalNs<<','<<feedPosition<<','
                            <<feeder->high_water_events()<<','
                            <<rolling->high_water_events()<<','
                            <<rolling->weight_build_count()<<','
                            <<(lazyMode?counters.release_exp_skipped:0)<<','
                            <<(lazyMode?counters.release_exp_evaluated:0)<<','
                            <<(lazyMode?counters.gain_pow_reused:0)
                            <<",PASS,PASS,NO,"
                            <<selectStats[0]<<','<<selectStats[1]<<','<<selectStats[2]<<','
                            <<advanceStats[0]<<','<<advanceStats[1]<<','<<advanceStats[2]<<','
                            <<otherStats[0]<<','<<otherStats[1]<<','<<otherStats[2]
                            <<'\n';
                    }
                }
                ++scenarios;
            }
        }
    }
    QCOMPARE(scenarios,std::size_t{8});
}


void TruePeakLimiterVerificationTest::experimentalSelectorWorkProfileFast()
{
    using Event=dsp::internal::LimiterDetectorEvent;
    using Selector=dsp::internal::experimental::M17LookaheadSelector;
    using Frozen=dsp::internal::TruePeakLimiterControllerState;
    constexpr double pi=std::numbers::pi_v<double>;
    const auto path=qEnvironmentVariable(
        "RGS_M17_SELECTOR_WORK_PROBE_FILE").toStdString();
    if(!path.empty()) {
        std::ofstream csv(path,std::ios::trunc);
        QVERIFY2(csv.good(),"M17 selector FAST CSV creation failed");
        csv<<"sample_rate_hz,ceiling_dbtp,pattern,query_ticks,"
              "event_count,events_in_horizon,events_visited,events_skipped,"
              "visit_percent,superblocks_skipped,halfblocks_skipped,"
              "selector_weight_builds,group_mean_ns,group_p99_ns,group_max_ns,"
              "super_mean_ns,super_p99_ns,super_max_ns,"
              "half_mean_ns,half_p99_ns,half_max_ns,"
              "paired_speed_ratio,half_over_super_ratio,exact_paired,"
              "exact_attack_floor,"
              "exact_selected_event,exact_bounds,"
              "sparse_legacy_mean_ns,sparse_legacy_p99_ns,sparse_legacy_max_ns,"
              "sparse_cached_mean_ns,sparse_cached_p99_ns,sparse_cached_max_ns,"
              "sparse_legacy_over_cached_ratio,"
              "prepare_cold_ns,prepare_warm_ns,select_128ticks_ns,"
              "warm_prepare_over_select_ratio\n";
    }
    std::size_t cases=0;
    for(const int fs:{48000,192000}) {
        constexpr double ceiling=-6.0;
        const double threshold=std::pow(10.0,ceiling/20.0);
        const auto reference=Frozen::create(fs,ceiling);
        QVERIFY(reference);
        const auto horizon=reference.value()->lookahead_ticks();
        for(const bool dense:{false,true}) {
            // Two deterministic patterns: sparse transient clusters and
            // continuously hot signal, both with realistic active-event
            // density and the same mathematical upper-bound requirement.
            std::vector<Event> events;
            events.reserve(9000);
            for(std::int64_t tick=1;tick<=9000;++tick) {
                const double peak=dense
                    ?(tick%7==0?1.18:0.61+0.01*static_cast<double>(tick%11))
                    :(tick%257==0?1.37:0.17+0.01*static_cast<double>(tick%5));
                events.push_back(Event{tick,peak,true});
            }
            auto selector=Selector::create(fs,ceiling);
            QVERIFY(selector);
            QVERIFY(selector->prepare(events));
            QCOMPARE(selector->weight_build_count(),std::size_t{1});
            std::uint64_t visited=0,skipped=0,inWindow=0,superSkipped=0;
            std::uint64_t halfSkipped=0;
            std::vector<std::int64_t> groupTimes,superTimes,halfTimes;
            groupTimes.reserve(256);
            superTimes.reserve(256);
            halfTimes.reserve(256);
            for(std::int64_t m=0;m<256;++m) {
                const double current=(m%51==0)?0.93:0.0;
                // Three variants share immutable exact binary64 caches.
                // Rotate measurement order to distribute warm-cache bias.
                const auto timed=[&](int variant) {
                    const auto started=std::chrono::steady_clock::now();
                    const auto choice=variant==0
                        ?selector->choose_group_only_for_test(m,current)
                        :variant==1
                            ?selector->choose_superblock_only_for_test(m,current)
                            :selector->choose(m,current);
                    const auto ended=std::chrono::steady_clock::now();
                    const auto elapsed=std::chrono::duration_cast<
                        std::chrono::nanoseconds>(ended-started).count();
                    return std::pair{choice,static_cast<std::int64_t>(elapsed)};
                };
                std::array<std::pair<std::optional<Selector::Choice>,
                    std::int64_t>,3> measurements{};
                for(int i=0;i<3;++i) {
                    const int variant=(static_cast<int>(m%3)+i)%3;
                    measurements[static_cast<std::size_t>(variant)]=timed(variant);
                }
                const auto& baseline=measurements[0];
                const auto& former=measurements[1];
                const auto& candidate=measurements[2];
                const auto& result=candidate.first;
                QVERIFY(result);
                QVERIFY(baseline.first);
                QVERIFY(former.first);
                groupTimes.push_back(baseline.second);
                superTimes.push_back(former.second);
                halfTimes.push_back(candidate.second);
                QCOMPARE(std::bit_cast<std::uint64_t>(
                    result->attack_floor_db),
                    std::bit_cast<std::uint64_t>(
                        former.first->attack_floor_db));
                QCOMPARE(result->future_event.has_value(),
                         former.first->future_event.has_value());
                if(result->future_event) {
                    QCOMPARE(result->future_event->tick,
                             former.first->future_event->tick);
                    QCOMPARE(std::bit_cast<std::uint64_t>(
                        result->future_event->peak),
                        std::bit_cast<std::uint64_t>(
                            former.first->future_event->peak));
                }
                QCOMPARE(std::bit_cast<std::uint64_t>(
                    result->attack_floor_db),
                    std::bit_cast<std::uint64_t>(
                        baseline.first->attack_floor_db));
                QCOMPARE(result->future_event.has_value(),
                         baseline.first->future_event.has_value());
                if(result->future_event) {
                    QCOMPARE(result->future_event->tick,
                             baseline.first->future_event->tick);
                    QCOMPARE(std::bit_cast<std::uint64_t>(
                        result->future_event->peak),
                        std::bit_cast<std::uint64_t>(
                            baseline.first->future_event->peak));
                }
                QCOMPARE(baseline.first->skipped_superblocks,std::size_t{0});
                QCOMPARE(baseline.first->skipped_halfblocks,std::size_t{0});
                QCOMPARE(former.first->skipped_halfblocks,std::size_t{0});
                QCOMPARE(baseline.first->visited+baseline.first->skipped,
                         result->visited+result->skipped);
                QVERIFY(result->bounded);
                visited+=result->visited;
                skipped+=result->skipped;
                superSkipped+=result->skipped_superblocks;
                halfSkipped+=result->skipped_halfblocks;
                QVERIFY(result->skipped_superblocks<=
                    result->skipped/Selector::kSuperblock);
                QVERIFY(result->skipped_halfblocks<=
                    result->skipped/Selector::kHalfblock);
                const auto begin=std::upper_bound(events.begin(),events.end(),
                    m,[](std::int64_t t,const Event& e){return t<e.tick;});
                const auto end=std::upper_bound(begin,events.end(),
                    m+horizon,
                    [](std::int64_t t,const Event& e){return t<e.tick;});
                const auto n=static_cast<std::uint64_t>(end-begin);
                inWindow+=n;
                QCOMPARE(static_cast<std::uint64_t>(
                    result->visited+result->skipped),n);
                // Independent O(L) oracle: the frozen unweighted
                // demand and exact binary64 cosine are recomputed for
                // EVERY valid future event. The grouped selector may
                // skip only with a provably safe upper bound.
                double floor=current>threshold
                    ?20.0*std::log10(current/threshold):+0.0;
                std::optional<Event> expected;
                for(auto event=begin;event!=end;++event) {
                    const double demand=event->peak>threshold
                        ?20.0*std::log10(event->peak/threshold):+0.0;
                    if(demand>0.0) {
                        const double d=static_cast<double>(event->tick-m);
                        const double w=0.5*(1.0+std::cos(
                            pi*d/static_cast<double>(horizon)));
                        const double weighted=demand*w;
                        if(weighted>floor) {
                            floor=weighted;
                            expected=*event;
                        }
                    }
                }
                QCOMPARE(std::bit_cast<std::uint64_t>(
                    result->attack_floor_db),
                    std::bit_cast<std::uint64_t>(floor));
                QCOMPARE(result->future_event.has_value(),
                         expected.has_value());
                if(expected) {
                    QCOMPARE(result->future_event->tick,expected->tick);
                    QCOMPARE(std::bit_cast<std::uint64_t>(
                        result->future_event->peak),
                        std::bit_cast<std::uint64_t>(expected->peak));
                }
            }
            QVERIFY(visited>0);
            QCOMPARE(visited+skipped,inWindow);
            const double pct=100.0*static_cast<double>(visited)/
                static_cast<double>(inWindow);
            const auto timingStats=[](std::vector<std::int64_t> samples) {
                std::sort(samples.begin(),samples.end());
                const auto total=std::accumulate(samples.begin(),samples.end(),
                    std::int64_t{0});
                const auto p99=(samples.size()*99U+99U)/100U-1U;
                return std::array<std::int64_t,3>{
                    total/static_cast<std::int64_t>(samples.size()),
                    samples[p99],samples.back()};
            };
            const auto groupStats=timingStats(groupTimes);
            const auto superStats=timingStats(superTimes);
            const auto halfStats=timingStats(halfTimes);
            const auto groupSum=std::accumulate(groupTimes.begin(),
                groupTimes.end(),std::int64_t{0});
            const auto superSum=std::accumulate(superTimes.begin(),
                superTimes.end(),std::int64_t{0});
            const auto halfSum=std::accumulate(halfTimes.begin(),
                halfTimes.end(),std::int64_t{0});
            const double speedRatio=superSum>0
                ?static_cast<double>(groupSum)/static_cast<double>(superSum)
                :0.0;
            const double halfVsSuper=halfSum>0
                ?static_cast<double>(superSum)/static_cast<double>(halfSum)
                :0.0;
            qInfo().nospace()<<"M17 selector FAST work fs="<<fs
                <<" pattern="<<(dense?"dense":"sparse")
                <<" visited="<<visited<<" skipped="<<skipped
                <<" superblocks_skipped="<<superSkipped
                <<" halfblocks_skipped="<<halfSkipped
                <<" visit_percent="<<pct<<" paired_speed_ratio="<<speedRatio
                <<" half_over_super_ratio="<<halfVsSuper
                <<" group_mean_ns="<<groupStats[0]
                <<" super_mean_ns="<<superStats[0]
                <<" half_mean_ns="<<halfStats[0]
                <<" exact_paired=PASS exact_oracle=PASS";
            if(!path.empty()) {
                std::ofstream csv(path,std::ios::app);
                QVERIFY2(csv.good(),"M17 selector FAST CSV append failed");
                csv<<fs<<','<<ceiling<<','
                    <<(dense?"dense":"sparse")<<",256,"
                    <<events.size()<<','<<inWindow<<','<<visited<<','
                    <<skipped<<','<<pct<<','<<superSkipped<<','
                    <<halfSkipped<<','<<selector->weight_build_count()<<','
                    <<groupStats[0]<<','<<groupStats[1]<<','<<groupStats[2]<<','
                    <<superStats[0]<<','<<superStats[1]<<','<<superStats[2]<<','
                    <<halfStats[0]<<','<<halfStats[1]<<','<<halfStats[2]<<','
                    <<speedRatio<<','<<halfVsSuper
                    <<",PASS,PASS,PASS,PASS,0,0,0,0,0,0,0,0,0,0,0\n";
            }
            ++cases;
        }
    }
    QCOMPARE(cases,std::size_t{4});
}


void TruePeakLimiterVerificationTest::experimentalDetectorSourcedSelectorProfileFast()
{
    using Event=dsp::internal::LimiterDetectorEvent;
    using Selector=dsp::internal::experimental::M17LookaheadSelector;
    using Feeder=dsp::internal::experimental::M17BoundedDetectorFeed;
    using Frozen=dsp::internal::TruePeakLimiterControllerState;
    constexpr double pi=std::numbers::pi_v<double>;
    const auto outputPath=qEnvironmentVariable(
        "RGS_M17_SELECTOR_WORK_PROBE_FILE").toStdString();
    std::size_t cases=0;
    for(const int fs:{44100,48000,88200,96000,176400,192000}) {
        for(const std::uint32_t channels:{1U,2U}) {
            for(const double ceiling:{-6.0,-12.0}) {
                const double linear=std::pow(10.0,ceiling/20.0);
                auto reference=Frozen::create(fs,ceiling);
                QVERIFY(reference);
                const auto horizon=reference.value()->lookahead_ticks();
                for(const bool dense:{false,true}) {
                    auto feeder=Feeder::create(fs,ceiling,channels);
                    QVERIFY(feeder);
                    // The actual frozen signed FIR detector, not invented
                    // event ticks. Active events only, including EOS drain.
                    const int frames=fs>=176400?2048:1536;
                    for(int n=0;n<frames;++n) {
                        const double left=dense
                            ?0.92*std::sin(0.23*static_cast<double>(n))
                             +0.76*std::sin(0.049*static_cast<double>(n))
                            :(n%113==0?1.33:-0.03);
                        const double right=channels==2U
                            ?(dense
                                ?0.85*std::sin(0.19*static_cast<double>(n)+0.13)
                                 +0.77*std::sin(0.073*static_cast<double>(n))
                                :(n%151==0?-1.24:0.01))
                            :0.0;
                        QVERIFY(feeder->push(left,right));
                    }
                    QVERIFY(feeder->drain());
                    const std::int64_t firstM=-62;
                    const std::int64_t lastM=65;
                    const auto eventSpan=feeder->batch(
                        firstM,lastM,horizon);
                    QVERIFY(eventSpan.size()<=Selector::kMaxBatchEvents);
                    auto selector=Selector::create(fs,ceiling);
                    QVERIFY(selector);
                    const auto coldStarted=std::chrono::steady_clock::now();
                    const bool coldPrepared=selector->prepare(eventSpan);
                    const auto coldFinished=std::chrono::steady_clock::now();
                    QVERIFY(coldPrepared);
                    QCOMPARE(selector->cached_event_count(),eventSpan.size());
                    // Warm repeat on the SAME certified events, after Hann
                    // weights are initialized. The second prepare must not
                    // change observable selector output or weight count.
                    const auto warmStarted=std::chrono::steady_clock::now();
                    const bool warmPrepared=selector->prepare(eventSpan);
                    const auto warmFinished=std::chrono::steady_clock::now();
                    QVERIFY(warmPrepared);
                    QCOMPARE(selector->cached_event_count(),eventSpan.size());
                    const auto coldPrepNs=std::chrono::duration_cast<
                        std::chrono::nanoseconds>(
                            coldFinished-coldStarted).count();
                    const auto warmPrepNs=std::chrono::duration_cast<
                        std::chrono::nanoseconds>(
                            warmFinished-warmStarted).count();
                    std::vector<std::int64_t> grouped,super,half;
                    std::vector<std::int64_t> directCosine,cachedSparsePair;
                    grouped.reserve(128);
                    super.reserve(128);
                    half.reserve(128);
                    directCosine.reserve(128);
                    cachedSparsePair.reserve(128);
                    const bool sparseCache=eventSpan.size()<Selector::kDense
                        && !eventSpan.empty();
                    if(!eventSpan.empty())
                        QCOMPARE(selector->weight_build_count(),std::size_t{1});
                    std::uint64_t visit=0,skip=0,window=0;
                    std::uint64_t pruned512=0,pruned256=0;
                    for(std::int64_t m=firstM;m<=lastM;++m) {
                        const double current=feeder->peak_at(m);
                        const auto timed=[&](int variant) {
                            const auto start=std::chrono::steady_clock::now();
                            const auto choice=variant==0
                                ?selector->choose_group_only_for_test(m,current)
                                :variant==1
                                    ?selector->choose_superblock_only_for_test(m,current)
                                    :selector->choose(m,current);
                            const auto stop=std::chrono::steady_clock::now();
                            const auto elapsed=std::chrono::duration_cast<
                                std::chrono::nanoseconds>(stop-start).count();
                            return std::pair{choice,static_cast<std::int64_t>(elapsed)};
                        };
                        std::array<std::pair<std::optional<Selector::Choice>,
                            std::int64_t>,3> measures{};
                        for(int i=0;i<3;++i) {
                            const int which=(static_cast<int>(m-firstM)%3+i)%3;
                            measures[static_cast<std::size_t>(which)]=timed(which);
                        }
                        const auto& a=measures[0].first;
                        const auto& b=measures[1].first;
                        const auto& c=measures[2].first;
                        QVERIFY(a && b && c);
                        grouped.push_back(measures[0].second);
                        super.push_back(measures[1].second);
                        half.push_back(measures[2].second);
                        if(sparseCache) {
                            // Extra paired A/B avoids conflating earlier
                            // three-way group/superblock measurements.
                            // Flip execution order for adjacent ticks.
                            const auto sparseTimed=[&](bool oldPath) {
                                const auto start=std::chrono::steady_clock::now();
                                const auto choice=oldPath
                                    ?selector->
                                        choose_sparse_original_cosine_for_test(
                                            m,current)
                                    :selector->choose(m,current);
                                const auto stop=std::chrono::steady_clock::now();
                                return std::pair{choice,
                                    std::chrono::duration_cast<
                                        std::chrono::nanoseconds>(
                                            stop-start).count()};
                            };
                            const bool oldFirst=((m-firstM)%2==0);
                            const auto x=sparseTimed(oldFirst);
                            const auto y=sparseTimed(!oldFirst);
                            const auto& old=oldFirst?x:y;
                            const auto& cached=oldFirst?y:x;
                            QVERIFY(old.first);
                            QVERIFY(cached.first);
                            directCosine.push_back(old.second);
                            cachedSparsePair.push_back(cached.second);
                            for(const auto* legacyOrCached:{
                                &old.first.value(),&cached.first.value()}) {
                                QCOMPARE(std::bit_cast<std::uint64_t>(
                                    legacyOrCached->attack_floor_db),
                                    std::bit_cast<std::uint64_t>(
                                        c->attack_floor_db));
                                QCOMPARE(legacyOrCached->future_event.has_value(),
                                         c->future_event.has_value());
                                if(legacyOrCached->future_event) {
                                    QCOMPARE(legacyOrCached->future_event->tick,
                                             c->future_event->tick);
                                    QCOMPARE(std::bit_cast<std::uint64_t>(
                                        legacyOrCached->future_event->peak),
                                        std::bit_cast<std::uint64_t>(
                                            c->future_event->peak));
                                }
                            }
                        }
                        QCOMPARE(std::bit_cast<std::uint64_t>(a->attack_floor_db),
                                 std::bit_cast<std::uint64_t>(b->attack_floor_db));
                        QCOMPARE(std::bit_cast<std::uint64_t>(a->attack_floor_db),
                                 std::bit_cast<std::uint64_t>(c->attack_floor_db));
                        QCOMPARE(a->future_event.has_value(),b->future_event.has_value());
                        QCOMPARE(a->future_event.has_value(),c->future_event.has_value());
                        if(a->future_event) {
                            QCOMPARE(a->future_event->tick,b->future_event->tick);
                            QCOMPARE(a->future_event->tick,c->future_event->tick);
                            QCOMPARE(std::bit_cast<std::uint64_t>(a->future_event->peak),
                                     std::bit_cast<std::uint64_t>(b->future_event->peak));
                            QCOMPARE(std::bit_cast<std::uint64_t>(a->future_event->peak),
                                     std::bit_cast<std::uint64_t>(c->future_event->peak));
                        }
                        const auto begin=std::upper_bound(eventSpan.begin(),
                            eventSpan.end(),m,[](std::int64_t tick,const Event& e) {
                                return tick<e.tick;
                            });
                        const auto end=std::upper_bound(begin,eventSpan.end(),
                            m+horizon,[](std::int64_t tick,const Event& e) {
                                return tick<e.tick;
                            });
                        const auto n=static_cast<std::size_t>(end-begin);
                        QCOMPARE(c->visited+c->skipped,n);
                        QCOMPARE(a->visited+a->skipped,n);
                        QCOMPARE(b->visited+b->skipped,n);
                        double oracle=current>linear
                            ?20.0*std::log10(current/linear):+0.0;
                        std::optional<Event> expected;
                        for(auto it=begin;it!=end;++it) {
                            const double demand=it->peak>linear
                                ?20.0*std::log10(it->peak/linear):+0.0;
                            if(demand>0.0) {
                                const double lag=static_cast<double>(it->tick-m);
                                const double hann=0.5*(1.0+std::cos(
                                    pi*lag/static_cast<double>(horizon)));
                                const double weighted=demand*hann;
                                if(weighted>oracle) {
                                    oracle=weighted;
                                    expected=*it;
                                }
                            }
                        }
                        QCOMPARE(std::bit_cast<std::uint64_t>(c->attack_floor_db),
                                 std::bit_cast<std::uint64_t>(oracle));
                        QCOMPARE(c->future_event.has_value(),expected.has_value());
                        if(expected) {
                            QCOMPARE(c->future_event->tick,expected->tick);
                            QCOMPARE(std::bit_cast<std::uint64_t>(c->future_event->peak),
                                     std::bit_cast<std::uint64_t>(expected->peak));
                        }
                        window+=n;
                        visit+=c->visited;
                        skip+=c->skipped;
                        pruned512+=c->skipped_superblocks;
                        pruned256+=c->skipped_halfblocks;
                    }
                    const auto stats=[](std::vector<std::int64_t> v) {
                        std::sort(v.begin(),v.end());
                        const auto sum=std::accumulate(v.begin(),v.end(),
                            std::int64_t{0});
                        const auto p99=(v.size()*99U+99U)/100U-1U;
                        return std::array<std::int64_t,3>{
                            sum/static_cast<std::int64_t>(v.size()),
                            v[p99],v.back()};
                    };
                    const auto a=stats(grouped);
                    const auto b=stats(super);
                    const auto c=stats(half);
                    const auto aSum=std::accumulate(grouped.begin(),
                        grouped.end(),std::int64_t{0});
                    const auto bSum=std::accumulate(super.begin(),
                        super.end(),std::int64_t{0});
                    const auto cSum=std::accumulate(half.begin(),
                        half.end(),std::int64_t{0});
                    const double warmPrepareOverSelect=cSum>0
                        ?static_cast<double>(warmPrepNs)/
                            static_cast<double>(cSum):0.0;
                    const double baseRatio=bSum>0
                        ?static_cast<double>(aSum)/static_cast<double>(bSum):0.0;
                    const double halfRatio=cSum>0
                        ?static_cast<double>(bSum)/static_cast<double>(cSum):0.0;
                    const auto cosineSum=std::accumulate(directCosine.begin(),
                        directCosine.end(),std::int64_t{0});
                    const auto cachedPairSum=std::accumulate(
                        cachedSparsePair.begin(),cachedSparsePair.end(),
                        std::int64_t{0});
                    const double sparseCacheSpeed=cachedPairSum>0
                        ?static_cast<double>(cosineSum)/
                            static_cast<double>(cachedPairSum):0.0;
                    const auto sparseOldStats=directCosine.empty()
                        ?std::array<std::int64_t,3>{0,0,0}
                        :stats(directCosine);
                    const auto sparseNewStats=cachedSparsePair.empty()
                        ?std::array<std::int64_t,3>{0,0,0}
                        :stats(cachedSparsePair);
                    QVERIFY(feeder->drained());
                    QCOMPARE(visit+skip,window);
                    qInfo().nospace()<<"M17 detector selector fs="<<fs
                        <<" channels="<<channels<<" ceiling="<<ceiling
                        <<" dense="<<dense<<" retained="<<eventSpan.size()
                        <<" 512_over_group="<<baseRatio
                        <<" 256_over_512="<<halfRatio
                        <<" sparse_hann_cache_speedup="<<sparseCacheSpeed
                        <<" prep_cold_ns="<<coldPrepNs
                        <<" prep_warm_ns="<<warmPrepNs
                        <<" choose_128ticks_ns="<<cSum
                        <<" warm_prepare_over_choose="<<warmPrepareOverSelect
                        <<" exact_pcm_controller_unmodified=YES";
                    if(!outputPath.empty()) {
                        std::ofstream csv(outputPath,std::ios::app);
                        QVERIFY2(csv.good(),"M17 detector selector CSV append failed");
                        csv<<fs<<','<<ceiling<<','
                           <<"detector_"<<(dense?"dense":"sparse")
                           <<(channels==2?"_stereo":"_mono")<<",128,"
                           <<eventSpan.size()<<','<<window<<','<<visit<<','
                           <<skip<<','
                           <<(window?100.0*static_cast<double>(visit)/
                               static_cast<double>(window):0.0)<<','
                           <<pruned512<<','<<pruned256<<','
                           <<selector->weight_build_count()<<','
                           <<a[0]<<','<<a[1]<<','<<a[2]<<','
                           <<b[0]<<','<<b[1]<<','<<b[2]<<','
                           <<c[0]<<','<<c[1]<<','<<c[2]<<','
                           <<baseRatio<<','<<halfRatio
                           <<",PASS,PASS,PASS,PASS,"
                           <<sparseOldStats[0]<<','<<sparseOldStats[1]<<','
                           <<sparseOldStats[2]<<','<<sparseNewStats[0]<<','
                           <<sparseNewStats[1]<<','<<sparseNewStats[2]<<','
                           <<sparseCacheSpeed<<','
                           <<coldPrepNs<<','<<warmPrepNs<<','<<cSum<<','
                           <<warmPrepareOverSelect<<'\n';
                        if(sparseCache) {
                            qInfo().nospace()
                                <<"M17 original-cosine vs cached Hann, fs="<<fs
                                <<" channels="<<channels
                                <<" sparse_hann_cache_speedup="
                                <<sparseCacheSpeed
                                <<" exact_binary64=PASS";
                        }
                    }
                    // After the 128-tick oracle, load an actual adjacent
                    // 64-frame window. Compare reused and full recompute
                    // demands from the same detector-certified events.
                    const std::int64_t nextFirst=505;
                    const std::int64_t nextLast=1016;
                    const auto nextEvents=feeder->batch(
                        nextFirst,nextLast,horizon);
                    QVERIFY(nextEvents.size()<=Selector::kMaxBatchEvents);
                    auto full=Selector::create(fs,ceiling);
                    QVERIFY(full);
                    const auto incStart=std::chrono::steady_clock::now();
                    const bool incReady=selector->prepare(nextEvents);
                    const auto incStop=std::chrono::steady_clock::now();
                    const auto fullStart=std::chrono::steady_clock::now();
                    const bool fullReady=full->prepare_full_recompute_for_test(
                        nextEvents);
                    const auto fullStop=std::chrono::steady_clock::now();
                    QVERIFY(incReady);
                    QVERIFY(fullReady);
                    QCOMPARE(selector->reused_demands_last_prepare()
                             +selector->computed_demands_last_prepare(),
                             nextEvents.size());
                    QCOMPARE(full->reused_demands_last_prepare(),std::size_t{0});
                    QCOMPARE(full->computed_demands_last_prepare(),
                             nextEvents.size());
                    const auto incrementalNs=std::chrono::duration_cast<
                        std::chrono::nanoseconds>(incStop-incStart).count();
                    const auto recomputeNs=std::chrono::duration_cast<
                        std::chrono::nanoseconds>(fullStop-fullStart).count();
                    for(std::int64_t tick=nextFirst;tick<nextFirst+16;++tick) {
                        const auto current=feeder->peak_at(tick);
                        const auto incrementalChoice=selector->choose(tick,current);
                        const auto recomputedChoice=full->choose(tick,current);
                        QVERIFY(incrementalChoice && recomputedChoice);
                        QCOMPARE(std::bit_cast<std::uint64_t>(
                            incrementalChoice->attack_floor_db),
                            std::bit_cast<std::uint64_t>(
                                recomputedChoice->attack_floor_db));
                        QCOMPARE(incrementalChoice->future_event.has_value(),
                                 recomputedChoice->future_event.has_value());
                        if(incrementalChoice->future_event) {
                            QCOMPARE(incrementalChoice->future_event->tick,
                                     recomputedChoice->future_event->tick);
                            QCOMPARE(std::bit_cast<std::uint64_t>(
                                incrementalChoice->future_event->peak),
                                std::bit_cast<std::uint64_t>(
                                    recomputedChoice->future_event->peak));
                        }
                    }
                    qInfo().nospace()<<"M17 adjacent-batch demand fs="<<fs
                        <<" channels="<<channels<<" ceiling="<<ceiling
                        <<" dense="<<dense
                        <<" reused="<<selector->reused_demands_last_prepare()
                        <<" recomputed="<<selector->computed_demands_last_prepare()
                        <<" warm_incremental_ns="<<incrementalNs
                        <<" full_recompute_ns="<<recomputeNs
                        <<" exact_pair=PASS";
                    if(!outputPath.empty()) {
                        std::ofstream csv(outputPath,std::ios::app);
                        QVERIFY2(csv.good(),
                            "M17 incremental selector CSV append failed");
                        csv<<"incremental,"<<fs<<','<<channels<<','
                            <<ceiling<<','<<(dense?"dense":"sparse")<<','
                            <<nextEvents.size()<<','
                            <<selector->reused_demands_last_prepare()<<','
                            <<selector->computed_demands_last_prepare()<<','
                            <<incrementalNs<<','<<recomputeNs
                            <<",PASS\n";
                    }
                    ++cases;
                }
            }
        }
    }
    QCOMPARE(cases,std::size_t{48});
}

} // namespace
} // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::TruePeakLimiterVerificationTest)
#include "test_true_peak_limiter_verification.moc"
