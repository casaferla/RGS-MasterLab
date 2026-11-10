#include <rgsml/analysis/streamed_reference_analyzer.hpp>
#include <rgsml/analysis/basic_level_analyzer.hpp>
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/wav_reader.hpp>
#include "../audio/wav_test_support.hpp"
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <span>
#include <vector>

namespace rgsml::tests {
class StreamedReferenceAnalyzerTest final : public QObject {
    Q_OBJECT
private slots:
    void sixRatesAndBothLayoutsMatchExistingEngines();
    void silentEmptyGateAndPartialBlock();
    void shortReadMustFail();
    void realSparseWavDecodedLargerThan128MiB();
};

namespace {
using namespace analysis;
[[nodiscard]] StreamedReferenceRead from_buffer(const audio::AudioBuffer& owner) {
    return [&owner](core::FrameIndex start,audio::MutableAudioBufferView dst)
        ->core::Result<core::FrameCount>
    {
        using R=core::Result<core::FrameCount>;
        auto src=owner.view().subview(start,dst.frame_count());
        if(!src)return R::failure(*src.error());
        for(std::size_t ch=0;ch<dst.format().channel_count();++ch){
            auto s=src.value()->channel(ch),d=dst.channel(ch);
            if(!s)return R::failure(*s.error());
            if(!d)return R::failure(*d.error());
            std::copy(s.value()->begin(),s.value()->end(),d.value()->begin());
        }
        return R::success(dst.frame_count());
    };
}
void compare_scalar(const BasicLevelValue& actual,const BasicLevelValue& expected) {
    QCOMPARE(actual.state,expected.state);
    QCOMPARE(actual.db.has_value(),expected.db.has_value());
    if(expected.db)QVERIFY(std::abs(*actual.db-*expected.db)<1e-11);
}
void compare_m20(const TonalStereoAnalysisResult& got,
                 const TonalStereoAnalysisResult& expected) {
    QCOMPARE(got.spectrum_state,expected.spectrum_state);
    QCOMPARE(got.width_ratio.state,expected.width_ratio.state);
    QCOMPARE(got.width_all_scope_fallback,expected.width_all_scope_fallback);
    QCOMPARE(got.complete_spectral_windows,expected.complete_spectral_windows);
    QCOMPARE(got.selected_spectral_windows,expected.selected_spectral_windows);
    QCOMPARE(got.mean_linear_psd.size(),expected.mean_linear_psd.size());
    for(std::size_t i=0;i<got.mean_linear_psd.size();++i)
        QVERIFY(std::abs(got.mean_linear_psd[i]-expected.mean_linear_psd[i])<1e-12);
    QCOMPARE(got.correlation_grid100.size(),expected.correlation_grid100.size());
    for(std::size_t i=0;i<got.correlation_grid100.size();++i){
        QCOMPARE(got.correlation_grid100[i].absolute_window_start,
                 expected.correlation_grid100[i].absolute_window_start);
        QCOMPARE(got.correlation_grid100[i].correlation.state,
                 expected.correlation_grid100[i].correlation.state);
        const auto a=got.correlation_grid100[i].correlation.value;
        const auto b=expected.correlation_grid100[i].correlation.value;
        QCOMPARE(a.has_value(),b.has_value());
        if(a)QVERIFY(std::abs(*a-*b)<1e-12);
    }
    const auto check=[](const TonalStereoValue& a,const TonalStereoValue& b) {
        QCOMPARE(a.state,b.state);
        QCOMPARE(a.value.has_value(),b.value.has_value());
        if(a.value)QVERIFY(std::abs(*a.value-*b.value)<1e-12);
    };
    check(got.width_ratio,expected.width_ratio);
    check(got.correlation_p10,expected.correlation_p10);
    check(got.correlation_p50,expected.correlation_p50);
    check(got.correlation_p90,expected.correlation_p90);
    QCOMPARE(got.primary_bands_1_12.size(),expected.primary_bands_1_12.size());
    for(std::size_t i=0;i<got.primary_bands_1_12.size();++i){
        const auto& a=got.primary_bands_1_12[i];
        const auto& b=expected.primary_bands_1_12[i];
        QCOMPARE(a.band_index,b.band_index);
        QVERIFY(std::abs(a.measured_linear_energy-b.measured_linear_energy)<1e-12);
        QVERIFY(!a.normalized_fraction.value);
    }
}
} // namespace

void StreamedReferenceAnalyzerTest::sixRatesAndBothLayoutsMatchExistingEngines()
{
    for(const int fs:{44100,48000,88200,96000,176400,192000}){
        for(const auto layout:{audio::ChannelLayout::MONO_C,
                               audio::ChannelLayout::STEREO_LR}){
            auto rate=core::SampleRate::create(fs);
            QVERIFY(rate);
            auto format=audio::AudioFormat::create(*rate.value(),layout);
            QVERIFY(format);
            auto frames=core::FrameCount::create(fs/2+137);
            QVERIFY(frames);
            auto owner=audio::AudioBuffer::create(
                *format.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
                core::FrameIndex{0},*frames.value());
            QVERIFY(owner);
            auto v=owner.value()->mutable_view();
            auto l=v.channel(0U);
            QVERIFY(l);
            std::span<double> r;
            if(layout==audio::ChannelLayout::STEREO_LR){
                auto right=v.channel(1U);
                QVERIFY(right);
                r=*right.value();
            }
            for(std::size_t i=0;i<l.value()->size();++i){
                const auto phase=2.0*std::numbers::pi*440.0*
                    static_cast<double>(i)/static_cast<double>(fs);
                (*l.value())[i]=0.2*std::sin(phase);
                if(!r.empty())r[i]=0.2*std::sin(phase+0.25);
            }
            auto got=measure_streamed_reference_scope(
                *format.value(),*frames.value(),from_buffer(*owner.value()));
            QVERIFY(got);
            auto level=measure_basic_level_scope(owner.value()->view(),
                core::FrameIndex{0},*frames.value(),
                audio::FrameDomainId::SOURCE_PROCESSING_RATE);
            QVERIFY(level);
            compare_scalar(got.value()->level.integrated_lufs,
                           level.value()->integrated_lufs);
            compare_scalar(got.value()->level.full_scope_rms_dbfs,
                           level.value()->full_scope_rms_dbfs);
            compare_scalar(got.value()->level.sample_peak_dbfs,
                           level.value()->sample_peak_dbfs);
            compare_scalar(got.value()->level.sample_peak_crest_db,
                           level.value()->sample_peak_crest_db);
            QCOMPARE(got.value()->level.true_peak_db_tp.state,
                BasicLevelValueState::M16_SCOPE_UNAVAILABLE);
            QCOMPARE(got.value()->level.retained_block_union.size(),
                     level.value()->retained_block_union.size());
            for(std::size_t i=0;i<level.value()->retained_block_union.size();++i){
                QCOMPARE(got.value()->level.retained_block_union[i].begin,
                         level.value()->retained_block_union[i].begin);
                QCOMPARE(got.value()->level.retained_block_union[i].end,
                         level.value()->retained_block_union[i].end);
            }
            std::vector<TonalStereoRetainedInterval> kept;
            for(const auto& part:level.value()->retained_block_union)
                kept.push_back({part.begin,part.end});
            const TonalStereoTrustedSupport support{
                false,{},level.value()->retained_block_disclosure_complete,kept,{}};
            auto expected=measure_tonal_stereo_scope(owner.value()->view(),
                core::FrameIndex{0},*frames.value(),
                audio::FrameDomainId::SOURCE_PROCESSING_RATE,support);
            QVERIFY(expected);
            compare_m20(got.value()->tonal,*expected.value());
            QVERIFY(got.value()->max_decoded_block_frames<=4096U);
            QVERIFY(got.value()->max_correlation_window_frames<=76800U);
        }
    }
}

void StreamedReferenceAnalyzerTest::silentEmptyGateAndPartialBlock()
{
    auto rate=core::SampleRate::create(48000);
    QVERIFY(rate);
    auto fmt=audio::AudioFormat::create(*rate.value(),audio::ChannelLayout::STEREO_LR);
    QVERIFY(fmt);
    auto frames=core::FrameCount::create(24001);
    QVERIFY(frames);
    auto owner=audio::AudioBuffer::create(*fmt.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*frames.value());
    QVERIFY(owner);
    auto decoded=measure_streamed_reference_scope(
        *fmt.value(),*frames.value(),from_buffer(*owner.value()));
    QVERIFY(decoded);
    QVERIFY(decoded.value()->level.retained_block_disclosure_complete);
    QVERIFY(decoded.value()->level.retained_block_union.empty());
    QVERIFY(!decoded.value()->tonal.width_all_scope_fallback);
    QCOMPARE(decoded.value()->tonal.width_ratio.state,
             TonalStereoMetricState::INSUFFICIENT_CONTENT);
    QVERIFY(!decoded.value()->tonal.width_ratio.value);
}

void StreamedReferenceAnalyzerTest::shortReadMustFail()
{
    auto rate=core::SampleRate::create(48000);
    QVERIFY(rate);
    auto fmt=audio::AudioFormat::create(*rate.value(),audio::ChannelLayout::MONO_C);
    QVERIFY(fmt);
    auto frames=core::FrameCount::create(8193);
    QVERIFY(frames);
    auto failure=measure_streamed_reference_scope(*fmt.value(),*frames.value(),
        [](core::FrameIndex,audio::MutableAudioBufferView view){
            return core::Result<core::FrameCount>::success(
                *core::FrameCount::create(view.frame_count().value()-1).value());
        });
    QVERIFY(!failure);
}

void StreamedReferenceAnalyzerTest::realSparseWavDecodedLargerThan128MiB()
{
    // A REAL valid RIFF/PCM16 header with logically sparse zero data:
    // WavReader parses and decodes >128 MiB of binary64 PCM without
    // storing a giant byte vector or accessing a source/render alias.
    using namespace wav_support;
    constexpr std::uint64_t frames=128ULL*1024ULL*1024ULL/sizeof(double)+257ULL;
    constexpr std::uint64_t dataBytes=frames*2ULL;
    Bytes header;
    append_fourcc(header,fourcc("RIFF"));
    append_u32(header,static_cast<std::uint32_t>(dataBytes+36ULL));
    append_fourcc(header,fourcc("WAVE"));
    auto fmt=make_fmt_payload(1U,1U,192000U,16U);
    append_chunk(header,fourcc("fmt "),fmt);
    append_fourcc(header,fourcc("data"));
    append_u32(header,static_cast<std::uint32_t>(dataBytes));
    QCOMPARE(header.size(),std::size_t{44});
    auto control=std::make_shared<ReaderControl>();
    std::vector<SparseSegment> segments;
    segments.push_back({0,std::move(header)});
    auto opened=audio::WavReader::open(
        std::make_unique<SparseReader>(44ULL+dataBytes,std::move(segments),control));
    QVERIFY(opened);
    auto& wav=**opened.value();
    QCOMPARE(wav.info().frame_count().value(),
             static_cast<std::int64_t>(frames));
    auto result=measure_streamed_reference_scope(
        wav.info().audio_format(),wav.info().frame_count(),
        [&wav](core::FrameIndex first,audio::MutableAudioBufferView dest){
            return wav.read_frames(first,dest);
        });
    QVERIFY(result);
    QVERIFY(result.value()->level.retained_block_disclosure_complete);
    QVERIFY(result.value()->level.retained_block_union.empty());
    QVERIFY(!result.value()->level.integrated_lufs.db);
    QCOMPARE(result.value()->tonal.width_ratio.state,
             TonalStereoMetricState::NOT_APPLICABLE);
    QVERIFY(result.value()->max_decoded_block_frames<=4096U);
    QVERIFY(control->largestReadRequest<=4096U*2U);
    auto closed=wav.close();
    QVERIFY(closed);
}
} // namespace rgsml::tests

QTEST_GUILESS_MAIN(rgsml::tests::StreamedReferenceAnalyzerTest)
#include "test_streamed_reference_analyzer.moc"
