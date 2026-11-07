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

namespace rgsml::tests {

class BasicLevelAnalyzerTest final : public QObject {
    Q_OBJECT
private slots:
    void fullOwningResourceUsesActualM16OnlyAcrossSixRates();
    void scopedSubviewNeverReportsFakeRegionalTruePeak();
    void silenceAndInsufficientScopeRemainTyped();
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

} // namespace rgsml::tests

QTEST_GUILESS_MAIN(rgsml::tests::BasicLevelAnalyzerTest)
#include "test_basic_level_analyzer.moc"
