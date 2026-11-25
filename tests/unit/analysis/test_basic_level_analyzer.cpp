#include <rgsml/analysis/basic_level_analyzer.hpp>
#include <rgsml/analysis/true_peak_measurement.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/frame_time.hpp>
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>

namespace rgsml::tests {

class BasicLevelAnalyzerTest final : public QObject {
    Q_OBJECT
private slots:
    void fullOwningResourceUsesActualM16OnlyAcrossSixRates();
    void scopedSubviewNeverReportsFakeRegionalTruePeak();
    void silenceAndInsufficientScopeRemainTyped();
    void silentLoudnessGateIsAuthoritativelyEmpty();
    void loudnessUnavailableIsNotAnInventedRetainedUnion();
    void retainedLoudnessUnionIsAbsoluteAndFullAtSixRates();
    void l3M05L2RealSameKPassGrid100AndType7P50();
    void l3M05L2QuietUngatedWindowsRemainBelowMinus70();
};

void BasicLevelAnalyzerTest::
fullOwningResourceUsesActualM16OnlyAcrossSixRates()
{
    using namespace analysis;
    for(const auto hz:{44100,48000,88200,96000,176400,192000}) {
        const auto sampleRate=core::SampleRate::create(hz);
        QVERIFY(sampleRate);
        const auto format=audio::AudioFormat::create(
            *sampleRate.value(),audio::ChannelLayout::STEREO_LR);
        QVERIFY(format);
        const auto frames=core::FrameCount::create((hz*2)/5);
        QVERIFY(frames);
        auto buffer=audio::AudioBuffer::create(*format.value(),
            audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{317},*frames.value());
        QVERIFY(buffer);
        auto v=buffer.value()->mutable_view();
        for(std::size_t channel=0;channel<2U;++channel) {
            auto samples=v.channel(channel);
            QVERIFY(samples);
            std::fill(samples.value()->begin(),samples.value()->end(),
                      channel==0U?0.5:0.25);
        }
        const auto canonicalM16=measure_true_peak_full_resource(
            *buffer.value());
        QVERIFY(canonicalM16);
        const auto product=measure_basic_level_full_resource(
            *buffer.value());
        QVERIFY(product);
        const auto combined=measure_basic_level_and_true_peak_full_resource(*buffer.value());
        QVERIFY(combined);
        const auto& shared=combined.value()->true_peak;
        QVERIFY(shared.global_linear_peak()==canonicalM16.value()->global_linear_peak());
        QCOMPARE(shared.global_db_tp(),canonicalM16.value()->global_db_tp());
        QCOMPARE(shared.global_peak_channel(),canonicalM16.value()->global_peak_channel());
        QCOMPARE(shared.qualification_status(),canonicalM16.value()->qualification_status());
        for(std::size_t c=0;c<shared.channel_count();++c)
            QVERIFY(shared.per_channel_linear_peak()[c]==canonicalM16.value()->per_channel_linear_peak()[c]);
        QCOMPARE(combined.value()->levels.true_peak_db_tp.db,product.value()->true_peak_db_tp.db);
        QCOMPARE(combined.value()->levels.integrated_lufs.db,product.value()->integrated_lufs.db);
        const auto& snapshot=*product.value();
        QCOMPARE(snapshot.scope_begin_frame,std::int64_t{317});
        QCOMPARE(snapshot.scope_frame_count,frames.value()->value());
        QCOMPARE(snapshot.sample_rate_hz,std::int64_t{hz});
        QCOMPARE(snapshot.channel_count,std::size_t{2});
        QVERIFY(snapshot.sample_peak_dbfs.db.has_value());
        QVERIFY(snapshot.full_scope_rms_dbfs.db.has_value());
        QVERIFY(snapshot.sample_peak_crest_db.db.has_value());
        QVERIFY(snapshot.integrated_lufs.db.has_value());
        QVERIFY(snapshot.true_peak_db_tp.db.has_value());
        QCOMPARE(snapshot.sample_peak_dbfs.state,
                 BasicLevelValueState::AVAILABLE);
        // Not a second oversampler or a different TP dB conversion.
        QCOMPARE(snapshot.true_peak_db_tp.db,
                 canonicalM16.value()->global_db_tp());
        QCOMPARE(snapshot.true_peak_db_tp.state,
                 BasicLevelValueState::M16_SUPPORTED_UNQUALIFIED);
        QVERIFY(!snapshot.k_weighting_bank_member_id.empty());
        QCOMPARE(snapshot.k_weighting_bank_sha256.size(),
                 std::size_t{64});
        QVERIFY(std::abs(*snapshot.sample_peak_dbfs.db
                        -20.0*std::log10(0.5))<1e-9);
    }
}

void BasicLevelAnalyzerTest::
scopedSubviewNeverReportsFakeRegionalTruePeak()
{
    using namespace analysis;
    const auto rate=core::SampleRate::create(48000);
    QVERIFY(rate);
    const auto format=audio::AudioFormat::create(
        *rate.value(),audio::ChannelLayout::MONO_C);
    QVERIFY(format);
    const auto count=core::FrameCount::create(40000);
    const auto selectedCount=core::FrameCount::create(19200);
    QVERIFY(count&&selectedCount);
    auto owner=audio::AudioBuffer::create(*format.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*count.value());
    QVERIFY(owner);
    auto plane=owner.value()->mutable_view().channel(0);
    QVERIFY(plane);
    std::fill(plane.value()->begin(),plane.value()->end(),0.7);
    // Poison both sides, including the immediately adjacent sample.
    (*plane.value())[4999]=(std::numeric_limits<double>::quiet_NaN)();
    (*plane.value())[5000+19200]=(std::numeric_limits<double>::infinity)();
    const auto scope=measure_basic_level_scope(owner.value()->view(),
        core::FrameIndex{5000},*selectedCount.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(scope);
    QCOMPARE(scope.value()->scope_begin_frame,std::int64_t{5000});
    QVERIFY(scope.value()->integrated_lufs.db.has_value());
    QVERIFY(scope.value()->sample_peak_dbfs.db.has_value());
    QVERIFY(!scope.value()->true_peak_db_tp.db);
    QCOMPARE(scope.value()->true_peak_db_tp.state,
             BasicLevelValueState::M16_SCOPE_UNAVAILABLE);
    QVERIFY(!measure_basic_level_scope(owner.value()->view(),
        core::FrameIndex{5000},*selectedCount.value(),
        audio::FrameDomainId::OUTPUT_RATE));
    QVERIFY(!measure_basic_level_full_resource(*owner.value()));
}

void BasicLevelAnalyzerTest::
silenceAndInsufficientScopeRemainTyped()
{
    using namespace analysis;
    const auto rate=core::SampleRate::create(48000);
    QVERIFY(rate);
    const auto format=audio::AudioFormat::create(
        *rate.value(),audio::ChannelLayout::MONO_C);
    QVERIFY(format);
    const auto count=core::FrameCount::create(2400);
    QVERIFY(count);
    auto buffer=audio::AudioBuffer::create(*format.value(),
        audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{4321},*count.value());
    QVERIFY(buffer);
    auto plane=buffer.value()->mutable_view().channel(0);
    QVERIFY(plane);
    std::fill(plane.value()->begin(),plane.value()->end(),0.0);
    const auto result=measure_basic_level_full_resource(*buffer.value());
    QVERIFY(result);
    QVERIFY(!result.value()->integrated_lufs.db);
    QCOMPARE(result.value()->integrated_lufs.state,
             BasicLevelValueState::INSUFFICIENT_DURATION);
    QVERIFY(!result.value()->full_scope_rms_dbfs.db);
    QCOMPARE(result.value()->full_scope_rms_dbfs.state,
             BasicLevelValueState::SILENCE);
    QVERIFY(!result.value()->sample_peak_dbfs.db);
    QCOMPARE(result.value()->sample_peak_dbfs.state,
             BasicLevelValueState::SILENCE);
    QVERIFY(!result.value()->sample_peak_crest_db.db);
    QVERIFY(!result.value()->true_peak_db_tp.db);
    QCOMPARE(result.value()->true_peak_db_tp.state,
             BasicLevelValueState::M16_SUPPORTED_UNQUALIFIED);
}

void BasicLevelAnalyzerTest::
l3M05L2RealSameKPassGrid100AndType7P50()
{
    using namespace analysis;
    const auto rate=core::SampleRate::create(48000);
    const auto fmt=audio::AudioFormat::create(
        *rate.value(),audio::ChannelLayout::MONO_C);
    const auto frames=core::FrameCount::create(480000); // exactly 10s
    QVERIFY(rate&&fmt&&frames);
    auto first=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{103},*frames.value());
    auto twice=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{103},*frames.value());
    QVERIFY(first&&twice);
    auto a=first.value()->mutable_view().channel(0);
    auto b=twice.value()->mutable_view().channel(0);
    QVERIFY(a&&b);
    for(std::size_t i=0;i<a.value()->size();++i) {
        const double tone=std::sin(
            2.0*std::numbers::pi*997.0*static_cast<double>(i)/48000.0);
        (*a.value())[i]=0.20*tone;
        (*b.value())[i]=0.40*tone;
    }
    // Selected-scope M19 uses the same original K bank and zero state;
    // no M16 call or second K pass.
    const auto low=measure_basic_level_scope(
        first.value()->view(),core::FrameIndex{103},*frames.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    const auto high=measure_basic_level_scope(
        twice.value()->view(),core::FrameIndex{103},*frames.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(low&&high);
    QVERIFY(low.value()->short_term_3s_observation_complete);
    QVERIFY(high.value()->short_term_3s_observation_complete);
    QCOMPARE(low.value()->short_term_3s_complete_windows,
             std::uint64_t{71});
    QCOMPARE(high.value()->short_term_3s_complete_windows,
             std::uint64_t{71});
    QCOMPARE(low.value()->short_term_3s_grid100.size(),std::size_t{71});
    QCOMPARE(high.value()->short_term_3s_grid100.size(),std::size_t{71});
    QVERIFY(low.value()->short_term_3s_p50_lufs.db);
    QVERIFY(high.value()->short_term_3s_p50_lufs.db);
    QCOMPARE(low.value()->short_term_3s_p50_lufs.state,
             BasicLevelValueState::AVAILABLE);
    for(std::size_t j=0;j<71;++j) {
        const auto& x=low.value()->short_term_3s_grid100[j];
        const auto& y=high.value()->short_term_3s_grid100[j];
        QCOMPARE(x.absolute_start_frame,
                 static_cast<std::int64_t>(103+j*4800));
        QCOMPARE(y.absolute_start_frame,x.absolute_start_frame);
        QVERIFY(x.lufs&&y.lufs);
        QVERIFY(std::abs((*y.lufs-*x.lufs)-
                         20.0*std::log10(2.0))<1e-9);
    }
    QVERIFY(std::abs((*high.value()->short_term_3s_p50_lufs.db-
                      *low.value()->short_term_3s_p50_lufs.db)-
                     20.0*std::log10(2.0))<1e-9);
    // The original M19 integrated result has the SAME expected
    // amplitude scaling and the original one-pass strict gate.
    QVERIFY(low.value()->integrated_lufs.db);
    QVERIFY(high.value()->integrated_lufs.db);
    QVERIFY(std::abs((*high.value()->integrated_lufs.db-
                      *low.value()->integrated_lufs.db)-
                     20.0*std::log10(2.0))<1e-9);
    QVERIFY(!low.value()->true_peak_db_tp.db);
    QCOMPARE(low.value()->true_peak_db_tp.state,
             BasicLevelValueState::M16_SCOPE_UNAVAILABLE);
}

void BasicLevelAnalyzerTest::
l3M05L2QuietUngatedWindowsRemainBelowMinus70()
{
    using namespace analysis;
    const auto rate=core::SampleRate::create(48000);
    QVERIFY(rate);
    const auto fmt=audio::AudioFormat::create(
        *rate.value(),audio::ChannelLayout::MONO_C);
    QVERIFY(fmt);
    const auto length=core::FrameCount::create(48000*4);
    QVERIFY(length);
    auto quiet=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*length.value());
    QVERIFY(quiet);
    auto v=quiet.value()->mutable_view().channel(0);
    QVERIFY(v);
    for(std::size_t i=0;i<v.value()->size();++i)
        (*v.value())[i]=1e-6*std::sin(
            2.0*std::numbers::pi*997.0*static_cast<double>(i)/48000.0);
    const auto result=measure_basic_level_scope(
        quiet.value()->view(),core::FrameIndex{0},*length.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(result);
    QCOMPARE(result.value()->short_term_3s_complete_windows,
             std::uint64_t{11});
    QVERIFY(result.value()->short_term_3s_p50_lufs.db);
    QVERIFY(*result.value()->short_term_3s_p50_lufs.db< -70.0);
    for(const auto& window:result.value()->short_term_3s_grid100) {
        QVERIFY(window.lufs);
        QVERIFY(*window.lufs< -70.0);
    }
    QVERIFY(!result.value()->integrated_lufs.db);
    QCOMPARE(result.value()->integrated_lufs.state,
             BasicLevelValueState::BELOW_LOUDNESS_GATE);
    const auto shortLength=core::FrameCount::create(48000*2);
    QVERIFY(shortLength);
    const auto shortOnly=measure_basic_level_scope(
        quiet.value()->view(),core::FrameIndex{0},*shortLength.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(shortOnly);
    QVERIFY(shortOnly.value()->short_term_3s_observation_complete);
    QCOMPARE(shortOnly.value()->short_term_3s_complete_windows,
             std::uint64_t{0});
    QVERIFY(!shortOnly.value()->short_term_3s_p50_lufs.db);
    QCOMPARE(shortOnly.value()->short_term_3s_p50_lufs.state,
             BasicLevelValueState::INSUFFICIENT_DURATION);
}

void BasicLevelAnalyzerTest::
retainedLoudnessUnionIsAbsoluteAndFullAtSixRates()
{
    using namespace analysis;
    for(const int hz:{44100,48000,88200,96000,176400,192000}) {
        auto rate=core::SampleRate::create(hz);
        QVERIFY(rate);
        auto format=audio::AudioFormat::create(
            *rate.value(),audio::ChannelLayout::STEREO_LR);
        auto count=core::FrameCount::create(hz/2);
        QVERIFY(format&&count);
        auto pcm=audio::AudioBuffer::create(*format.value(),
            audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{317},*count.value());
        QVERIFY(pcm);
        auto frame=pcm.value()->mutable_view();
        for(std::size_t ch=0;ch<2;++ch) {
            auto plane=frame.channel(ch);
            QVERIFY(plane);
            for(std::size_t i=0;i<plane.value()->size();++i) {
                (*plane.value())[i]=0.5*std::sin(
                    2.0*std::numbers::pi*440.0*static_cast<double>(i)/hz);
            }
        }
        auto out=measure_basic_level_scope(
            pcm.value()->view(),core::FrameIndex{317},*count.value(),
            audio::FrameDomainId::SOURCE_PROCESSING_RATE);
        QVERIFY(out);
        QVERIFY(out.value()->integrated_lufs.db);
        QVERIFY(out.value()->retained_block_disclosure_complete);
        QCOMPARE(out.value()->retained_block_union.size(),std::size_t{1});
        QCOMPARE(out.value()->retained_block_union[0].begin,
                 std::uint64_t{317});
        QCOMPARE(out.value()->retained_block_union[0].end,
                 static_cast<std::uint64_t>(317+hz/2));
    }
}

void BasicLevelAnalyzerTest::
loudnessUnavailableIsNotAnInventedRetainedUnion()
{
    using namespace analysis;
    auto rate=core::SampleRate::create(48000);
    auto format=audio::AudioFormat::create(
        *rate.value(),audio::ChannelLayout::MONO_C);
    auto frames=core::FrameCount::create(4800);
    QVERIFY(rate&&format&&frames);
    auto pcm=audio::AudioBuffer::create(*format.value(),
        audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{1001},*frames.value());
    QVERIFY(pcm);
    auto samples=pcm.value()->mutable_view().channel(0);
    QVERIFY(samples);
    std::fill(samples.value()->begin(),samples.value()->end(),0.5);
    const auto out=measure_basic_level_scope(
        pcm.value()->view(),core::FrameIndex{1001},*frames.value(),
        audio::FrameDomainId::OUTPUT_RATE);
    QVERIFY(out);
    QVERIFY(!out.value()->retained_block_disclosure_complete);
    QVERIFY(out.value()->retained_block_union.empty());
    QVERIFY(out.value()->integrated_lufs.state==
            BasicLevelValueState::INSUFFICIENT_DURATION);
}

void BasicLevelAnalyzerTest::
silentLoudnessGateIsAuthoritativelyEmpty()
{
    using namespace analysis;
    auto rate=core::SampleRate::create(48000);
    auto format=audio::AudioFormat::create(
        *rate.value(),audio::ChannelLayout::STEREO_LR);
    auto frames=core::FrameCount::create(48000);
    QVERIFY(rate&&format&&frames);
    auto pcm=audio::AudioBuffer::create(*format.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{2002},*frames.value());
    QVERIFY(pcm);
    auto out=measure_basic_level_scope(
        pcm.value()->view(),core::FrameIndex{2002},*frames.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(out);
    QVERIFY(out.value()->retained_block_disclosure_complete);
    QVERIFY(out.value()->retained_block_union.empty());
    QVERIFY(!out.value()->integrated_lufs.db);
    QVERIFY(out.value()->integrated_lufs.state==
            BasicLevelValueState::BELOW_LOUDNESS_GATE);
}

} // namespace rgsml::tests

QTEST_GUILESS_MAIN(rgsml::tests::BasicLevelAnalyzerTest)
#include "test_basic_level_analyzer.moc"
