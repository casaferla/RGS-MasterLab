#include "../../oracles/true_peak_limiter/core_test_support.hpp"
#include "internal/true_peak_limiter_state.hpp"

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
        for(const double ceiling:{-1.0,-6.0,-12.0}) {
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
        csv<<"sample_rate_hz,ceiling_dbtp,controller_ticks,"
              "evaluated_future_events,original_us,memoized_us,"
              "ratio_original_over_memoized,attack_floor_bitwise\n";
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
                csv<<fs<<','<<ceilingDb<<','<<ticksToProbe<<','
                    <<evaluations<<','<<originalUs<<','<<cachedUs
                    <<','<<ratio<<",PASS\n";
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

} // namespace
} // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::TruePeakLimiterVerificationTest)
#include "test_true_peak_limiter_verification.moc"
