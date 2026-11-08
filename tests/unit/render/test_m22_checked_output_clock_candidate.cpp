#include "m22_checked_output_clock_candidate.hpp"
#include "m22_checked_output_pcm_candidate.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/frame_time.hpp>
#include <QtTest/QTest>
#include <array>
#include <cstdint>
#include <numeric>
#include <string>

namespace rgsml::tests {
class M22CheckedOutputClockCandidateTest final : public QObject {
    Q_OBJECT
private slots:
    void exactSameRateAndAbsoluteDither() {
        using namespace rgsml::m22_private;
        constexpr std::array<std::uint32_t,6> rates{
            44100,48000,88200,96000,176400,192000};
        for(const auto hz:rates) {
            OutputClockFailure reason{};
            const OutputClockDeclaration d{hz,hz,10001,10001,
                "exact-real-M17-master",{}};
            const auto plan=CheckedOutputClockCandidate::freeze(d,reason);
            QVERIFY(plan);
            QCOMPARE(reason,OutputClockFailure::NONE);
            QVERIFY(!plan->authorizes_release());
            QVERIFY(!plan->uses_final_src());
            const auto ratio=plan->reduced_rate_ratio();
            QCOMPARE(ratio.first,std::uint32_t{1});
            QCOMPARE(ratio.second,std::uint32_t{1});
            for(const Window slice:{
                Window{0,1},Window{1,10000},
                Window{10000,10001},Window{0,10001}}) {
                const auto crop=plan->plan_window(
                    Mode::PREVIEW,slice,reason);
                QVERIFY(crop);
                QCOMPARE(crop->absolute_output_first,slice.first);
                QCOMPARE(crop->absolute_output_end,slice.end);
                QCOMPARE(crop->output_frame_count(),
                         slice.end-slice.first);
                QCOMPARE(crop->absolute_dither_first,slice.first);
                QCOMPARE(crop->replay_source_first,std::uint64_t{0});
                QCOMPARE(crop->required_source_true_eos,std::uint64_t{10001});
                QCOMPARE(crop->required_output_true_eos,std::uint64_t{10001});
                QVERIFY(!plan->authorizes_release());
            }
            QVERIFY(!plan->plan_window(Mode::FULL,{1,10001},reason));
            QCOMPARE(reason,OutputClockFailure::INVALID_WINDOW);
            const auto full=plan->plan_window(
                Mode::FULL,{0,10001},reason);
            QVERIFY(full);
            QCOMPARE(full->output_frame_count(),std::uint64_t{10001});
        }
    }
    void everyCrossRateRatioAndNoInferredLength() {
        using namespace rgsml::m22_private;
        constexpr std::array<std::uint32_t,6> rates{
            44100,48000,88200,96000,176400,192000};
        for(const auto from:rates)for(const auto to:rates) {
            if(from==to)continue;
            OutputClockFailure reason{};
            // 9011 output frames is supplied by an EXTERNAL genuine
            // full-M17 programme; this metadata test does NOT infer or
            // qualify SRC frame counts, kernels or quality.
            const OutputClockDeclaration d{from,to,8003,9011,
                "current-full-M17","existing-Q0-asset-id-not-verified"};
            const auto frozen=CheckedOutputClockCandidate::freeze(d,reason);
            QVERIFY(frozen);
            QVERIFY(frozen->uses_final_src());
            QVERIFY(!frozen->authorizes_release());
            const auto ratio=frozen->reduced_rate_ratio();
            const auto g=std::gcd(from,to);
            QCOMPARE(ratio.first,to/g);
            QCOMPARE(ratio.second,from/g);
            const auto crop=frozen->plan_window(
                Mode::PREVIEW,{307,8999},reason);
            QVERIFY(crop);
            QCOMPARE(crop->absolute_output_first,std::uint64_t{307});
            QCOMPARE(crop->absolute_output_end,std::uint64_t{8999});
            QCOMPARE(crop->output_frame_count(),std::uint64_t{8692});
            QCOMPARE(crop->absolute_dither_first,std::uint64_t{307});
            QCOMPARE(crop->required_source_true_eos,std::uint64_t{8003});
            QCOMPARE(crop->required_output_true_eos,std::uint64_t{9011});
            QVERIFY(!frozen->plan_window(Mode::FULL,{307,8999},reason));
            QCOMPARE(reason,OutputClockFailure::INVALID_WINDOW);
            QVERIFY(frozen->plan_window(Mode::FULL,{0,9011},reason));
        }
    }
    void genuineOutputBufferMetadataIsNotAnExternalClaim() {
        using namespace rgsml;
        using namespace rgsml::m22_private;
        for(const auto hz:{44100,48000,88200,96000,176400,192000}) {
            OutputClockFailure reason{};
            const auto expected=CheckedOutputClockCandidate::freeze(
                {48000,static_cast<std::uint32_t>(hz),101,103,
                 "checked-current-M17-owner",
                 hz==48000 ? "" : "unqualified-Q0-asset-id"},reason);
            // Same-rate needs equal source/output frame counts.
            if(hz==48000) {
                QVERIFY(!expected);
                QCOMPARE(reason,OutputClockFailure::WRONG_BYPASS_LENGTH);
                continue;
            }
            QVERIFY(expected);
            QVERIFY(!expected->authorizes_release());
            const auto rate=core::SampleRate::create(hz);
            const auto count=core::FrameCount::create(103);
            QVERIFY(rate&&count);
            for(const auto layout:{audio::ChannelLayout::MONO_C,
                                  audio::ChannelLayout::STEREO_LR}) {
                const auto fmt=audio::AudioFormat::create(*rate.value(),layout);
                QVERIFY(fmt);
                auto actual=audio::AudioBuffer::create(*fmt.value(),
                    audio::FrameDomainId::OUTPUT_RATE,
                    core::FrameIndex{0},*count.value());
                QVERIFY(actual);
                QCOMPARE(check_real_full_output_pcm_candidate(
                    *expected,actual.value()->view()),PcmClockFailure::NONE);
                QVERIFY(!expected->authorizes_release());
                auto sourceDomain=audio::AudioBuffer::create(*fmt.value(),
                    audio::FrameDomainId::SOURCE_PROCESSING_RATE,
                    core::FrameIndex{0},*count.value());
                QVERIFY(sourceDomain);
                QCOMPARE(check_real_full_output_pcm_candidate(
                    *expected,sourceDomain.value()->view()),
                    PcmClockFailure::INVALID_OUTPUT_DOMAIN);
                auto nonZeroOrigin=audio::AudioBuffer::create(*fmt.value(),
                    audio::FrameDomainId::OUTPUT_RATE,
                    core::FrameIndex{7},*count.value());
                QVERIFY(nonZeroOrigin);
                QCOMPARE(check_real_full_output_pcm_candidate(
                    *expected,nonZeroOrigin.value()->view()),
                    PcmClockFailure::NOT_WHOLE_PROGRAMME);
                const auto shortCount=core::FrameCount::create(102);
                QVERIFY(shortCount);
                auto shortened=audio::AudioBuffer::create(*fmt.value(),
                    audio::FrameDomainId::OUTPUT_RATE,
                    core::FrameIndex{0},*shortCount.value());
                QVERIFY(shortened);
                QCOMPARE(check_real_full_output_pcm_candidate(
                    *expected,shortened.value()->view()),
                    PcmClockFailure::NOT_WHOLE_PROGRAMME);
                const auto wrongRate=core::SampleRate::create(32000);
                QVERIFY(wrongRate);
                const auto wrongFormat=audio::AudioFormat::create(
                    *wrongRate.value(),layout);
                QVERIFY(wrongFormat);
                auto wrongClock=audio::AudioBuffer::create(
                    *wrongFormat.value(),audio::FrameDomainId::OUTPUT_RATE,
                    core::FrameIndex{0},*count.value());
                QVERIFY(wrongClock);
                QCOMPARE(check_real_full_output_pcm_candidate(
                    *expected,wrongClock.value()->view()),
                    PcmClockFailure::WRONG_OUTPUT_RATE);
            }
        }
    }
    void missingAndInvalidNeverFallbackToBypass() {
        using namespace rgsml::m22_private;
        OutputClockFailure reason{};
        auto d=OutputClockDeclaration{44100,48000,8003,9011,
            "M17-current","Q0-identity"};
        d.selected_final_src_asset_id.clear();
        QVERIFY(!CheckedOutputClockCandidate::freeze(d,reason));
        QCOMPARE(reason,OutputClockFailure::UNATTESTED_FINAL_SRC);
        d.selected_final_src_asset_id="unverified-asset-name";
        d.exact_checked_full_master_identity.clear();
        QVERIFY(!CheckedOutputClockCandidate::freeze(d,reason));
        QCOMPARE(reason,OutputClockFailure::MISSING_IDENTITY);
        d.exact_checked_full_master_identity="M17-current";
        d.output_hz=12345;
        QVERIFY(!CheckedOutputClockCandidate::freeze(d,reason));
        QCOMPARE(reason,OutputClockFailure::INVALID_CLOCK);
        d.output_hz=48000;
        d.declared_full_output_frames=0;
        QVERIFY(!CheckedOutputClockCandidate::freeze(d,reason));
        QCOMPARE(reason,OutputClockFailure::INVALID_LENGTH);
        d.declared_full_output_frames=9011;
        const auto valid=CheckedOutputClockCandidate::freeze(d,reason);
        QVERIFY(valid);
        for(const Window invalid:{
            Window{0,0},Window{9010,9010},
            Window{9011,9012},Window{9000,8999},
            Window{0,9012}}) {
            QVERIFY(!valid->plan_window(Mode::PREVIEW,invalid,reason));
            QCOMPARE(reason,OutputClockFailure::INVALID_WINDOW);
        }
        d.output_hz=d.source_processing_hz;
        d.declared_full_output_frames=8003;
        QVERIFY(!CheckedOutputClockCandidate::freeze(d,reason));
        QCOMPARE(reason,OutputClockFailure::UNATTESTED_FINAL_SRC);
        d.selected_final_src_asset_id.clear();
        d.declared_full_output_frames=8004;
        QVERIFY(!CheckedOutputClockCandidate::freeze(d,reason));
        QCOMPARE(reason,OutputClockFailure::WRONG_BYPASS_LENGTH);
    }
};
} // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::M22CheckedOutputClockCandidateTest)
#include "test_m22_checked_output_clock_candidate.moc"
