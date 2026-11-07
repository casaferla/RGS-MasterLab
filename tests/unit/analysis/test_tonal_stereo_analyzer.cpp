#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <vector>

namespace rgsml::tests {

class TonalStereoAnalyzerTest final : public QObject {
    Q_OBJECT
private slots:
    void boundedRealPsdAndStereoSixRates();
    void explicitGateAndNormalizedSupportAreNeverInferred();
    void monoScopeAndOutOfScopePoison();
};

void TonalStereoAnalyzerTest::boundedRealPsdAndStereoSixRates()
{
    using namespace analysis;
    for(const int hz:{44100,48000,88200,96000,176400,192000}) {
        const auto rate=core::SampleRate::create(hz);
        QVERIFY(rate);
        const auto format=audio::AudioFormat::create(
            *rate.value(),audio::ChannelLayout::STEREO_LR);
        QVERIFY(format);
        const auto count=core::FrameCount::create(hz/2);
        QVERIFY(count);
        auto owner=audio::AudioBuffer::create(*format.value(),
            audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{1536},*count.value());
        QVERIFY(owner);
        auto v=owner.value()->mutable_view();
        auto left=v.channel(0U),right=v.channel(1U);
        QVERIFY(left&&right);
        for(std::size_t i=0;i<left.value()->size();++i) {
            const double sample=0.4*std::sin(
                2.0*std::numbers::pi*1000.0*static_cast<double>(i)
                /static_cast<double>(hz));
            (*left.value())[i]=sample;
            (*right.value())[i]=sample;
        }
        const auto result=measure_tonal_stereo_scope(
            owner.value()->view(),core::FrameIndex{1536},
            *count.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE);
        QVERIFY(result);
        QCOMPARE(result.value()->sample_rate_hz,std::int64_t{hz});
        QCOMPARE(result.value()->scope_begin_frame,std::int64_t{1536});
        QVERIFY(result.value()->complete_spectral_windows>0);
        QVERIFY(result.value()->selected_spectral_windows>0);
        QVERIFY(result.value()->spectral_all_frames_fallback);
        QVERIFY(result.value()->width_all_scope_fallback);
        QCOMPARE(result.value()->spectrum_state,TonalStereoMetricState::AVAILABLE);
        QVERIFY(!result.value()->mean_linear_psd.empty());
        QVERIFY(!result.value()->primary_bands_1_12.empty());
        QCOMPARE(result.value()->width_ratio.state,TonalStereoMetricState::AVAILABLE);
        QVERIFY(result.value()->width_ratio.value);
        QVERIFY(std::abs(*result.value()->width_ratio.value)<1e-12);
        QVERIFY(result.value()->correlation_p50.value);
        QVERIFY(*result.value()->correlation_p50.value>0.99999);
        QVERIFY(result.value()->correlation_p10.value);
        QVERIFY(result.value()->correlation_p90.value);
        for(const auto& band:result.value()->primary_bands_1_12) {
            QVERIFY(band.measured_linear_energy>=0);
            QVERIFY(!band.normalized_fraction.value);
            QCOMPARE(band.normalized_fraction.state,
                     TonalStereoMetricState::UNAVAILABLE_AUTHORITATIVE_SUPPORT);
        }
    }
}

void TonalStereoAnalyzerTest::
explicitGateAndNormalizedSupportAreNeverInferred()
{
    using namespace analysis;
    const auto rate=core::SampleRate::create(48000);
    QVERIFY(rate);
    const auto format=audio::AudioFormat::create(
        *rate.value(),audio::ChannelLayout::STEREO_LR);
    QVERIFY(format);
    const auto count=core::FrameCount::create(24000);
    QVERIFY(count);
    auto owner=audio::AudioBuffer::create(*format.value(),
        audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{0},*count.value());
    QVERIFY(owner);
    auto v=owner.value()->mutable_view();
    auto l=v.channel(0),r=v.channel(1);
    QVERIFY(l&&r);
    for(std::size_t i=0;i<l.value()->size();++i) {
        const auto y=0.3*std::sin(
            2.0*std::numbers::pi*440.0*static_cast<double>(i)/48000.0);
        (*l.value())[i]=y;(*r.value())[i]=-y;
    }
    const auto baseline=measure_tonal_stereo_scope(
        owner.value()->view(),core::FrameIndex{0},*count.value(),
        audio::FrameDomainId::OUTPUT_RATE);
    QVERIFY(baseline);
    const auto windows=baseline.value()->complete_spectral_windows;
    QVERIFY(windows>0);
    const auto bands=baseline.value()->primary_bands_1_12.size();
    QVERIFY(bands>0);

    std::vector<std::uint8_t> zeroMask(
        static_cast<std::size_t>(windows),0U);
    const auto emptyGate=measure_tonal_stereo_scope(
        owner.value()->view(),core::FrameIndex{0},*count.value(),
        audio::FrameDomainId::OUTPUT_RATE,
        TonalStereoTrustedSupport{true,zeroMask,true,{},{}});
    QVERIFY(emptyGate);
    QCOMPARE(emptyGate.value()->spectrum_state,
             TonalStereoMetricState::INSUFFICIENT_CONTENT);
    QVERIFY(!emptyGate.value()->spectral_all_frames_fallback);
    QVERIFY(!emptyGate.value()->width_all_scope_fallback);
    QCOMPARE(emptyGate.value()->width_ratio.state,
             TonalStereoMetricState::INSUFFICIENT_CONTENT);

    std::vector<std::uint8_t> eligible(bands,1U);
    const auto normalized=measure_tonal_stereo_scope(
        owner.value()->view(),core::FrameIndex{0},*count.value(),
        audio::FrameDomainId::OUTPUT_RATE,
        TonalStereoTrustedSupport{false,{},false,{},eligible});
    QVERIFY(normalized);
    double sum=0.0;
    for(const auto& band:normalized.value()->primary_bands_1_12) {
        QVERIFY(band.normalized_fraction.value);
        sum+=*band.normalized_fraction.value;
    }
    QVERIFY(std::abs(sum-1.0)<1e-8);
    std::vector<std::uint8_t> malformed(bands-1,1U);
    QVERIFY(!measure_tonal_stereo_scope(
        owner.value()->view(),core::FrameIndex{0},*count.value(),
        audio::FrameDomainId::OUTPUT_RATE,
        TonalStereoTrustedSupport{false,{},false,{},malformed}));
}

void TonalStereoAnalyzerTest::monoScopeAndOutOfScopePoison()
{
    using namespace analysis;
    const auto rate=core::SampleRate::create(48000);
    QVERIFY(rate);
    const auto format=audio::AudioFormat::create(
        *rate.value(),audio::ChannelLayout::MONO_C);
    QVERIFY(format);
    const auto total=core::FrameCount::create(25000);
    const auto selected=core::FrameCount::create(19200);
    QVERIFY(total&&selected);
    auto owner=audio::AudioBuffer::create(*format.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*total.value());
    QVERIFY(owner);
    auto data=owner.value()->mutable_view().channel(0U);
    QVERIFY(data);
    for(std::size_t i=0;i<data.value()->size();++i)
        (*data.value())[i]=0.5*std::sin(
            2.0*std::numbers::pi*220.0*static_cast<double>(i)/48000.0);
    (*data.value())[4999]=(std::numeric_limits<double>::quiet_NaN)();
    (*data.value())[24200]=(std::numeric_limits<double>::infinity)();
    const auto scoped=measure_tonal_stereo_scope(
        owner.value()->view(),core::FrameIndex{5000},
        *selected.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(scoped);
    QCOMPARE(scoped.value()->scope_begin_frame,std::int64_t{5000});
    QVERIFY(scoped.value()->selected_spectral_windows>0);
    QCOMPARE(scoped.value()->width_ratio.state,
             TonalStereoMetricState::NOT_APPLICABLE);
    QCOMPARE(scoped.value()->correlation_p50.state,
             TonalStereoMetricState::NOT_APPLICABLE);
    QVERIFY(!measure_tonal_stereo_scope(
        owner.value()->view(),core::FrameIndex{5000},
        *selected.value(),audio::FrameDomainId::OUTPUT_RATE));
}

} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::TonalStereoAnalyzerTest)
#include "test_tonal_stereo_analyzer.moc"
