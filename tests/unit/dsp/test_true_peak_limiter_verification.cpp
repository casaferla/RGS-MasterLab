#include "../../oracles/true_peak_limiter/core_test_support.hpp"

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/dsp/module_descriptor.hpp>
#include <rgsml/core/frame_time.hpp>

#include <QtTest/QTest>

#include <bit>
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
    std::span<const std::size_t> chunks)
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
    auto& limiter=**module.value();
    const auto view=source.view();
    const auto fmt=view.format();
    const auto n=view.frame_count().value();
    const auto origin=view.absolute_range().begin().value();
    if(n<=0)return std::nullopt;
    const dsp::DspProcessSpec spec{fmt,audio::FrameDomainId::OUTPUT_RATE,
        *core::FrameCount::create(4096).value()};
    const auto requirements=limiter.runtime_requirements(spec);
    if(!requirements || !limiter.prepare(spec))return std::nullopt;
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
        const auto count=std::min<std::int64_t>(n-pos,
            static_cast<std::int64_t>(chunks[chunkIndex++%chunks.size()]));
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
        if(!limiter.process(in.value()->view(),out.value()->mutable_view(),
            dsp::DspProcessContext{*range.value(),pos==0,false})
            || !append(*out.value()))return std::nullopt;
        pos+=count;
    }
    for(std::int64_t drain=0;drain<d;) {
        const auto count=std::min<std::int64_t>(d-drain,127);
        const auto first=origin+n+drain;
        auto out=audio::AudioBuffer::create(fmt,audio::FrameDomainId::OUTPUT_RATE,
            core::FrameIndex{first},*core::FrameCount::create(count).value());
        auto range=core::FrameRange::create(core::FrameIndex{first},
            core::FrameIndex{first+count});
        if(!out || !range || !limiter.finalize(out.value()->mutable_view(),
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

} // namespace
} // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::TruePeakLimiterVerificationTest)
#include "test_true_peak_limiter_verification.moc"
