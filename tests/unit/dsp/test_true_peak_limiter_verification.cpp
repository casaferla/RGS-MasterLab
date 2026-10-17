#include "../../oracles/true_peak_limiter/core_test_support.hpp"

#include <rgsml/audio/audio_buffer.hpp>
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

namespace rgsml::tests {
namespace {
[[nodiscard]] audio::AudioBuffer buffer(int fs, bool stereo, int frames)
{
    auto fmt=audio::AudioFormat::create(
        *core::SampleRate::create(fs).value(),
        stereo?audio::ChannelLayout::STEREO_LR:audio::ChannelLayout::MONO_C);
    return std::move(*audio::AudioBuffer::create(*fmt.value(),
        audio::FrameDomainId::OUTPUT_RATE,core::FrameIndex{0},
        *core::FrameCount::create(frames).value()).value());
}
[[nodiscard]] std::string_view category(const core::Error& e)
{
    for(const auto& d:e.details())if(d.key=="category")return d.value;
    return {};
}
class TruePeakLimiterVerificationTest final : public QObject {
    Q_OBJECT
private slots:
    void exactBinary64TrimThreshold();
    void genuineM16WholeResourceQuietAndFalsifiableHot();
    void rejectWrongCeilingAndPropagateM16Failures();
    void actualM16ReverificationUsesOneGlobalStereoScalar();
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


} // namespace
} // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::TruePeakLimiterVerificationTest)
#include "test_true_peak_limiter_verification.moc"
