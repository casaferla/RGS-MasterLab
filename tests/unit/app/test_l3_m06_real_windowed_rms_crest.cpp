#include "internal/l3_m06_real_windowed_rms_crest.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <QtTest/QTest>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace rgsml::tests {
namespace {
namespace real=rgsml::app::l3_automatch::m05m06_real;
[[nodiscard]] core::Result<audio::AudioBuffer> make_audio(
    std::size_t frames,bool stereo=true) {
    auto fs=core::SampleRate::create(48000);
    auto fmt=audio::AudioFormat::create(
        *fs.value(),stereo?audio::ChannelLayout::STEREO_LR:
                           audio::ChannelLayout::MONO);
    const auto fc=core::FrameCount::create(
        static_cast<std::int64_t>(frames));
    return audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*fc.value());
}
class RealWindowedRmsCrestTest final:public QObject {
    Q_OBJECT
private slots:
    void actualStereoPcmSameWindowRmsAndCrest();
    void belowFloorAndNonfiniteKeepCompleteNativeDenominator();
    void fourBlockOverlapStartsAndTrailingFrameExcluded();
    void constantGainMovesRmsNotCrestAndDoesNotClaimM10();
};
void RealWindowedRmsCrestTest::
actualStereoPcmSameWindowRmsAndCrest() {
    auto src=make_audio(48000);
    QVERIFY(src);
    auto l=src.value()->mutable_view().channel(0);
    auto r=src.value()->mutable_view().channel(1);
    QVERIFY(l&&r);
    for(std::size_t i=0;i<l.value()->size();++i) {
        (*l.value())[i]=0.5;
        (*r.value())[i]=-0.5;
    }
    const auto observed=real::observe_d03_d04_real_pcm(src.value()->view());
    QCOMPARE(observed.problem,real::NativeWindowProblem::none);
    QCOMPARE(observed.complete_native_windows.size(),std::size_t{7});
    QCOMPARE(observed.valid_rms_windows,std::uint64_t{7});
    QCOMPARE(observed.valid_crest_windows,std::uint64_t{7});
    const double expected=20.0*std::log10(0.5);
    for(const auto& window:observed.complete_native_windows) {
        QVERIFY(window.rms_dbfs);
        QVERIFY(window.crest_db);
        QVERIFY(std::abs(*window.rms_dbfs-expected)<1e-11);
        QVERIFY(std::abs(*window.crest_db)<1e-11);
    }
    const auto values=real::extract_d03_d04_type7(observed);
    QVERIFY(values.d03_type7_rms_p95_minus_p10_db);
    QVERIFY(std::abs(*values.d03_type7_rms_p95_minus_p10_db)<1e-11);
    QCOMPARE(*values.rms_native_support,1.0);
    QCOMPARE(*values.crest_native_support,1.0);
    QVERIFY(!values.authorizes_m10());
}
void RealWindowedRmsCrestTest::
belowFloorAndNonfiniteKeepCompleteNativeDenominator() {
    auto source=make_audio(48000);
    QVERIFY(source);
    auto l=source.value()->mutable_view().channel(0);
    auto r=source.value()->mutable_view().channel(1);
    for(std::size_t i=0;i<48000;++i) {
        (*l.value())[i]=i<19200?0.0:0.2;
        (*r.value())[i]=i<19200?0.0:0.2;
    }
    (*l.value())[43200]=std::numeric_limits<double>::quiet_NaN();
    const auto observed=real::observe_d03_d04_real_pcm(
        source.value()->view());
    QCOMPARE(observed.complete_native_windows.size(),std::size_t{7});
    QCOMPARE(observed.valid_rms_windows,std::uint64_t{4});
    QCOMPARE(observed.valid_crest_windows,std::uint64_t{4});
    QVERIFY(!observed.complete_native_windows[0].rms_dbfs);
    QVERIFY(!observed.complete_native_windows[5].rms_dbfs);
    QVERIFY(!observed.complete_native_windows[6].rms_dbfs);
    const auto selectors=real::extract_d03_d04_type7(observed);
    QCOMPARE(*selectors.rms_native_support,4.0/7.0);
    QCOMPARE(*selectors.crest_native_support,4.0/7.0);
    QVERIFY(selectors.d03_type7_rms_p95_minus_p10_db);
}
void RealWindowedRmsCrestTest::
fourBlockOverlapStartsAndTrailingFrameExcluded() {
    auto s=make_audio(48000+4799);
    QVERIFY(s);
    auto l=s.value()->mutable_view().channel(0);
    auto r=s.value()->mutable_view().channel(1);
    for(std::size_t i=0;i<l.value()->size();++i) {
        (*l.value())[i]=0.2;(*r.value())[i]=0.2;
    }
    const auto got=real::observe_d03_d04_real_pcm(s.value()->view());
    QCOMPARE(got.complete_native_windows.size(),std::size_t{7});
    for(std::size_t i=0;i<got.complete_native_windows.size();++i)
        QCOMPARE(got.complete_native_windows[i].absolute_start_frame,
                 static_cast<std::int64_t>(4800*i));
}
void RealWindowedRmsCrestTest::
constantGainMovesRmsNotCrestAndDoesNotClaimM10() {
    auto s=make_audio(48000,false);
    QVERIFY(s);
    auto l=s.value()->mutable_view().channel(0);
    for(std::size_t i=0;i<l.value()->size();++i)
        (*l.value())[i]=(i%2==0)?0.2:-0.2;
    const auto first=real::observe_d03_d04_real_pcm(s.value()->view());
    for(auto& sample:*l.value())sample*=2.0;
    const auto second=real::observe_d03_d04_real_pcm(s.value()->view());
    QCOMPARE(first.complete_native_windows.size(),
             second.complete_native_windows.size());
    for(std::size_t i=0;i<first.complete_native_windows.size();++i) {
        QVERIFY(std::abs(*second.complete_native_windows[i].rms_dbfs-
                         *first.complete_native_windows[i].rms_dbfs-
                         20.0*std::log10(2.0))<1e-10);
        QVERIFY(std::abs(*second.complete_native_windows[i].crest_db-
                         *first.complete_native_windows[i].crest_db)<1e-10);
    }
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::RealWindowedRmsCrestTest)
#include "test_l3_m06_real_windowed_rms_crest.moc"
