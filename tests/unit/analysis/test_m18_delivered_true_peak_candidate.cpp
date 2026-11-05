// M18: decoded-delivery streaming TP matches the frozen M16 full-resource
// 4x/12-tap/11-EOS arithmetic; this is NOT an M18 product/FQ certification.
#include <rgsml/analysis/m18_delivered_true_peak_candidate.hpp>
#include "internal/true_peak_full_resource.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/core/error.hpp>
#include <QtTest/QTest>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

namespace rgsml::tests {
namespace {
[[nodiscard]] std::shared_ptr<audio::AudioBuffer> make_decoded(
    std::int64_t hz,audio::ChannelLayout layout,std::int64_t count)
{
    auto rate=core::SampleRate::create(hz);
    if(!rate)return {};
    auto format=audio::AudioFormat::create(*rate.value(),layout);
    auto frames=core::FrameCount::create(count);
    if(!format ||!frames)return {};
    auto buffer=audio::AudioBuffer::create(
        *format.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*frames.value());
    if(!buffer)return {};
    auto result=std::make_shared<audio::AudioBuffer>(
        std::move(*buffer.value()));
    const auto view=result->mutable_view();
    for(std::size_t ch=0;ch<view.format().channel_count();++ch){
        auto p=result->mutable_view().channel(ch);
        Q_ASSERT(p);
        for(std::size_t i=0;i<p.value()->size();++i){
            const std::int64_t n=static_cast<std::int64_t>(i%23U)-11;
            (*p.value())[i]=static_cast<double>(n)/64.0;
        }
        if(!p.value()->empty()){
            (*p.value())[0]=ch==0U?-0.0:0.0;
        }
    }
    return result;
}
} // namespace

class M18DeliveredTruePeakCandidateTest final : public QObject {
    Q_OBJECT
private slots:
    void sixRatesAndBothLayoutsMatchFrozenFullResource();
    void rejectWrongDomainPrematureFinalAndSecondFinal();
    void channelAtomicRollbackAfterNonfinite();
};

void M18DeliveredTruePeakCandidateTest::
sixRatesAndBothLayoutsMatchFrozenFullResource()
{
    using analysis::M18DeliveredTruePeakCandidate;
    constexpr std::array<std::int64_t,4> parts{{1,5,4096,1}};
    for(const auto hz:{44100,48000,88200,96000,176400,192000}){
        for(const auto layout:{audio::ChannelLayout::MONO_C,
                              audio::ChannelLayout::STEREO_LR}){
            auto source=make_decoded(hz,layout,4103);
            QVERIFY(source);
            const auto reference=analysis::internal::measure_unqualified_true_peak_full_resource(
                *source);
            QVERIFY(reference);
            auto pending=M18DeliveredTruePeakCandidate::create(
                source->view().format(),source->view().frame_count());
            QVERIFY(pending);
            std::int64_t offset=0;
            for(const auto n:parts){
                auto frames=core::FrameCount::create(n);
                QVERIFY(frames);
                auto chunk=source->view().subview(
                    core::FrameIndex{offset},*frames.value());
                QVERIFY(chunk);
                QVERIFY(pending.value()->get()->push_decoded(*chunk.value()));
                offset+=n;
            }
            QCOMPARE(offset,std::int64_t{4103});
            const auto measured=pending.value()->get()->finalize();
            QVERIFY(measured);
            const auto& m=*measured.value();
            const auto& r=*reference.value();
            QCOMPARE(m.sample_rate_hz,hz);
            QCOMPARE(m.channel_count,r.channel_count);
            QCOMPARE(m.programme_frames,std::uint64_t{4103});
            QCOMPARE(m.global_linear_peak,r.global_linear_peak);
            QVERIFY(m.global_db_tp==r.global_db_tp);
            // M16 internal evidence has no channel-tie DTO; linear peaks are checked.
            QCOMPARE(m.is_exact_silence,r.is_exact_silence);
            QCOMPARE(m.qualification_status,
                analysis::TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED);
            for(std::size_t ch=0;ch<m.channel_count;++ch)
                QCOMPARE(m.per_channel_linear_peak[ch],
                    r.per_channel_linear_peak[ch]);
        }
    }
}

void M18DeliveredTruePeakCandidateTest::
rejectWrongDomainPrematureFinalAndSecondFinal()
{
    using analysis::M18DeliveredTruePeakCandidate;
    auto source=make_decoded(48000,audio::ChannelLayout::MONO_C,3);
    QVERIFY(source);
    auto accum=M18DeliveredTruePeakCandidate::create(
        source->view().format(),source->view().frame_count());
    QVERIFY(accum);
    QVERIFY(!accum.value()->get()->finalize());
    auto frames=core::FrameCount::create(1);
    QVERIFY(frames);
    auto second=source->view().subview(core::FrameIndex{1},*frames.value());
    QVERIFY(second);
    QVERIFY(!accum.value()->get()->push_decoded(*second.value()));
    const auto full=source->view();
    QVERIFY(accum.value()->get()->push_decoded(full));
    QVERIFY(accum.value()->get()->finalize());
    QVERIFY(!accum.value()->get()->finalize());
    QVERIFY(!accum.value()->get()->push_decoded(full));
    const auto unsupported=make_decoded(32000,
        audio::ChannelLayout::MONO_C,3);
    QVERIFY(unsupported);
    QVERIFY(!M18DeliveredTruePeakCandidate::create(
        unsupported->view().format(),unsupported->view().frame_count()));
}

void M18DeliveredTruePeakCandidateTest::
channelAtomicRollbackAfterNonfinite()
{
    using analysis::M18DeliveredTruePeakCandidate;
    auto source=make_decoded(96000,audio::ChannelLayout::STEREO_LR,4);
    QVERIFY(source);
    const auto format=source->view().format();
    auto accum=M18DeliveredTruePeakCandidate::create(
        format,source->view().frame_count());
    QVERIFY(accum);
    auto right=source->mutable_view().channel(1U);
    QVERIFY(right);
    (*right.value())[3]=std::numeric_limits<double>::quiet_NaN();
    const auto fail=accum.value()->get()->push_decoded(source->view());
    QVERIFY(!fail);
    QCOMPARE(fail.error()->code(),core::ErrorCode::InvalidAudioSample);
    (*right.value())[3]=0.25;
    QVERIFY(accum.value()->get()->push_decoded(source->view()));
    const auto m=accum.value()->get()->finalize();
    const auto r=analysis::internal::measure_unqualified_true_peak_full_resource(*source);
    QVERIFY(m &&r);
    QCOMPARE(m.value()->global_linear_peak,r.value()->global_linear_peak());
}
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M18DeliveredTruePeakCandidateTest)
#include "test_m18_delivered_true_peak_candidate.moc"
