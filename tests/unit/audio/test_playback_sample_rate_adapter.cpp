#include "wav_test_support.hpp"

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/playback_sample_rate_adapter.hpp>
#include <rgsml/audio/full_resource_output_rate_candidate.hpp>
#include <rgsml/audio/master_monitoring_rate_plan.hpp>
#include <rgsml/audio/monitoring_pcm_candidate.hpp>

#include <QTest>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace wav_support;

[[nodiscard]] core::SampleRate rate(std::int64_t value)
{
    auto result = core::SampleRate::create(value);
    Q_ASSERT(result);
    return *result.value();
}

[[nodiscard]] std::unique_ptr<audio::WavReader> open_f64_stereo(
    std::uint32_t sampleRate,
    std::span<const std::uint64_t> interleaved,
    const std::shared_ptr<ReaderControl>& control =
        std::make_shared<ReaderControl>())
{
    auto bytes = make_wav(
        3U, 64U, 2U, sampleRate, f64_payload(interleaved));
    auto reader = audio::WavReader::open(memory_reader(std::move(bytes), control));
    Q_ASSERT(reader);
    return std::move(*reader.value());
}

[[nodiscard]] audio::PlaybackSampleRateAdapter adapter(
    std::int64_t inputRate,
    std::int64_t outputRate,
    std::int64_t inputFrames)
{
    auto result = audio::PlaybackSampleRateAdapter::create(
        audio::PlaybackRateSpec{
            rate(inputRate),
            rate(outputRate),
            audio::ChannelLayout::STEREO_LR,
            frame_count(inputFrames),
        });
    Q_ASSERT(result);
    return std::move(*result.value());
}

[[nodiscard]] audio::AudioBuffer output_buffer(
    const audio::PlaybackSampleRateAdapter& converter,
    std::int64_t start,
    std::int64_t frames)
{
    auto format = audio::AudioFormat::create(
        converter.output_rate(), converter.channel_layout());
    Q_ASSERT(format);
    auto buffer = audio::AudioBuffer::create(
        *format.value(),
        audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{start},
        frame_count(frames));
    Q_ASSERT(buffer);
    return std::move(*buffer.value());
}

[[nodiscard]] std::vector<std::uint64_t> render_partitioned(
    audio::PlaybackSampleRateAdapter& converter,
    audio::WavReader& reader,
    std::span<const std::int64_t> partition,
    std::size_t channel)
{
    std::vector<std::uint64_t> result;
    std::int64_t start = 0;
    for (const auto frames : partition) {
        auto block = output_buffer(converter, start, frames);
        auto rendered = converter.read_frames(
            reader, core::FrameIndex{start}, block.mutable_view());
        Q_ASSERT(rendered);
        auto bits = channel_bits(block, channel);
        result.insert(result.end(), bits.begin(), bits.end());
        start += frames;
    }
    return result;
}

}  // namespace

class PlaybackSampleRateAdapterTest final : public QObject {
    Q_OBJECT

private slots:
    void policyIdentityAndFrameMap();
    void offlineCandidateMatchesFrozenPlaybackKernelBitwise();
    void experimentalFullResourceSixBySixRateMatrix();
    void finalMasterMonitoringCannotRetargetMasterRate();
    void isolatedOfflineMonitoringPcmSixBySixMatrix();
    void assetsAreFrozenAndSymmetric();
    void constantAndChunkInvariance();
    void reverseDirectionAndAbsoluteSeekInvariance();
    void oneFrameBoundaryAndStereoIndependence();
    void decodeWindowIsTrackDurationIndependent();
    void conservativeInputDependenciesMatchIndependentTapOracle();
    void conservativeInputDependenciesRejectInvalidRanges();
    void noFutureDependencyPrefixAgainstIndependentOracle();
};


void PlaybackSampleRateAdapterTest::finalMasterMonitoringCannotRetargetMasterRate()
{
    // This is a non-audible CONTRACT test, not a 192->48 SRC claim.
    // M17/M18 master identity is selected separately from any physical
    // output device. A mismatched device must NOT trigger direct install.
    constexpr std::array<std::int64_t, 6> masters{
        44100, 48000, 88200, 96000, 176400, 192000
    };
    constexpr std::array<std::int64_t, 8> devices{
        32000, 44100, 48000, 88200, 96000, 176400, 192000, 384000
    };
    std::size_t direct=0,needsSrc=0;
    for(const auto masterHz:masters) {
        for(const auto deviceHz:devices) {
            const auto master=rate(masterHz);
            const auto device=rate(deviceHz);
            auto routed=audio::plan_master_monitoring_rate(master,device);
            QVERIFY(routed);
            QCOMPARE(routed.value()->final_master_rate,master);
            QCOMPARE(routed.value()->physical_device_rate,device);
            const auto exact=masterHz==deviceHz;
            QCOMPARE(routed.value()->can_install_master_pcm_directly,exact);
            QCOMPARE(routed.value()->monitoring_src_required,!exact);
            if(exact)++direct;
            else ++needsSrc;
        }
    }
    QCOMPARE(direct,std::size_t{6});
    QCOMPARE(needsSrc,std::size_t{42});
    // In particular a 192 kHz master + 48 kHz soundcard remains a
    // 192 kHz MASTER. Realtime monitoring SRC must be implemented
    // separately before it can be played on that device.
    const auto at192=audio::plan_master_monitoring_rate(
        rate(192000),rate(48000));
    QVERIFY(at192);
    QCOMPARE(at192.value()->final_master_rate.value(),std::int64_t{192000});
    QVERIFY(at192.value()->monitoring_src_required);
    QVERIFY(!at192.value()->can_install_master_pcm_directly);

    // A physical interface may support rates outside the six exported
    // master rates; this does not expand M17/M18 master-rate eligibility.
    auto invalidMaster=audio::plan_master_monitoring_rate(
        rate(32000),rate(32000));
    QVERIFY(!invalidMaster);
    QVERIFY(invalidMaster.error());
    QCOMPARE(invalidMaster.error()->code(),
             core::ErrorCode::UnsupportedOperation);
}

void PlaybackSampleRateAdapterTest::isolatedOfflineMonitoringPcmSixBySixMatrix()
{
    // Integration seam only: explicitly NOT a real DAC or M16 LIVE test.
    constexpr std::array<std::int64_t,6> supported{
        44100,48000,88200,96000,176400,192000};
    constexpr std::int64_t N=113;
    for(const auto masterHz:supported) {
        auto format=audio::AudioFormat::create(
            rate(masterHz),audio::ChannelLayout::STEREO_LR);
        QVERIFY(format);
        auto master=audio::AudioBuffer::create(*format.value(),
            audio::FrameDomainId::OUTPUT_RATE,core::FrameIndex{0},frame_count(N));
        QVERIFY(master);
        for(std::size_t ch=0;ch<2;++ch) {
            auto channel=master.value()->mutable_view().channel(ch);
            QVERIFY(channel);
            for(std::int64_t i=0;i<N;++i)
                (*channel.value())[static_cast<std::size_t>(i)] =
                    (i%23==0?-0.34:0.2)*(ch==0?1.0:-0.6);
        }
        const auto beforeL=channel_bits(*master.value(),0);
        const auto beforeR=channel_bits(*master.value(),1);
        for(const auto deviceHz:supported) {
            auto monitor=audio::materialize_monitoring_pcm_candidate(
                master.value()->view(),rate(deviceHz),frame_count(31));
            QVERIFY2(monitor,monitor?"":monitor.error()->message().c_str());
            QCOMPARE(monitor.value()->immutable_master_rate.value(),masterHz);
            QCOMPARE(monitor.value()->immutable_master_frames.value(),N);
            QCOMPARE(monitor.value()->device_rate.value(),deviceHz);
            QCOMPARE(monitor.value()->device_src_applied,masterHz!=deviceHz);
            QVERIFY(audio::validate_monitoring_pcm_candidate(*monitor.value()));
            const auto pcm=monitor.value()->device_pcm.view();
            QCOMPARE(pcm.timebase().frame_domain_id(),
                audio::FrameDomainId::OUTPUT_RATE);
            QCOMPARE(pcm.format().sample_rate().value(),deviceHz);
            const auto expected=(N/masterHz)*deviceHz+
                ((N%masterHz)*deviceHz+masterHz-1)/masterHz;
            QCOMPARE(pcm.frame_count().value(),expected);
            for(std::size_t ch=0;ch<2;++ch) {
                auto lane=pcm.channel(ch);
                QVERIFY(lane);
                for(const double x:*lane.value()) QVERIFY(std::isfinite(x));
            }
            if(masterHz==deviceHz) {
                QCOMPARE(channel_bits(monitor.value()->device_pcm,0),beforeL);
                QCOMPARE(channel_bits(monitor.value()->device_pcm,1),beforeR);
            }
            QCOMPARE(channel_bits(*master.value(),0),beforeL);
            QCOMPARE(channel_bits(*master.value(),1),beforeR);
        }
        auto wrongDomain=audio::AudioBuffer::create(*format.value(),
            audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{0},frame_count(N));
        QVERIFY(wrongDomain);
        QVERIFY(!audio::materialize_monitoring_pcm_candidate(
            wrongDomain.value()->view(),rate(48000),frame_count(31)));
    }
    // An arbitrary OS device rate does not silently expand the experimental
    // monitor converter's six-rate matrix or the immutable master contract.
    auto fmt=audio::AudioFormat::create(
        rate(192000),audio::ChannelLayout::MONO_C);
    QVERIFY(fmt);
    auto master=audio::AudioBuffer::create(*fmt.value(),
        audio::FrameDomainId::OUTPUT_RATE,core::FrameIndex{0},frame_count(1));
    QVERIFY(master);
    QVERIFY(!audio::materialize_monitoring_pcm_candidate(
        master.value()->view(),rate(32000),frame_count(31)));
    // Forged and stale device ownership never reaches Qt for installation.
    auto stereoFormat=audio::AudioFormat::create(
        rate(192000),audio::ChannelLayout::STEREO_LR);
    QVERIFY(stereoFormat);
    auto owner=audio::AudioBuffer::create(
        *stereoFormat.value(),audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{0},frame_count(193));
    QVERIFY(owner);
    auto valid=audio::materialize_monitoring_pcm_candidate(
        owner.value()->view(),rate(48000),frame_count(31));
    QVERIFY(valid);
    auto& forged=*valid.value();
    QVERIFY(audio::validate_monitoring_pcm_candidate(forged));
    forged.device_src_applied=false;
    QVERIFY(!audio::validate_monitoring_pcm_candidate(forged));
    forged.device_src_applied=true;
    forged.immutable_master_frames=frame_count(192);
    QVERIFY(!audio::validate_monitoring_pcm_candidate(forged));
    forged.immutable_master_frames=frame_count(193);
    forged.device_rate=rate(96000);
    QVERIFY(!audio::validate_monitoring_pcm_candidate(forged));
}

void PlaybackSampleRateAdapterTest::offlineCandidateMatchesFrozenPlaybackKernelBitwise()
{
    for(const auto hz: {44100,48000}) {
        const auto target=hz==44100?48000:44100;
        constexpr std::int64_t N=441;
        std::vector<std::uint64_t> interleaved;
        interleaved.reserve(static_cast<std::size_t>(N*2));
        auto format=audio::AudioFormat::create(
            rate(hz),audio::ChannelLayout::STEREO_LR);
        QVERIFY(format);
        auto pcm=audio::AudioBuffer::create(*format.value(),
            audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{0},frame_count(N));
        QVERIFY(pcm);
        auto left=pcm.value()->mutable_view().channel(0);
        auto right=pcm.value()->mutable_view().channel(1);
        QVERIFY(left&&right);
        for(std::int64_t i=0;i<N;++i) {
            const double l=i==0?4.0:(i%17==0?-0.75:0.125);
            const double rr=i==N-1?-2.0:(i%13==0?0.8125:-0.0625);
            (*left.value())[static_cast<std::size_t>(i)]=l;
            (*right.value())[static_cast<std::size_t>(i)]=rr;
            interleaved.push_back(std::bit_cast<std::uint64_t>(l));
            interleaved.push_back(std::bit_cast<std::uint64_t>(rr));
        }
        auto reader=open_f64_stereo(static_cast<std::uint32_t>(hz),interleaved);
        auto playback=adapter(hz,target,N);
        auto golden=output_buffer(playback,0,
            playback.output_frame_count().value());
        QVERIFY(playback.read_frames(*reader,core::FrameIndex{0},
            golden.mutable_view()));

        for(const auto block: {1,17,257,1024}) {
            auto candidate=audio::materialize_full_resource_output_rate_candidate(
                pcm.value()->view(),rate(target),frame_count(block));
            QVERIFY2(candidate,candidate?"":candidate.error()->message().c_str());
            QVERIFY(candidate.value()->used_src);
            QCOMPARE(candidate.value()->source_frames.value(),N);
            QCOMPARE(candidate.value()->pcm.view().format().sample_rate().value(),
                std::int64_t{target});
            QCOMPARE(candidate.value()->pcm.view().frame_count(),
                playback.output_frame_count());
            QCOMPARE(candidate.value()->pcm.view().timebase().frame_domain_id(),
                audio::FrameDomainId::OUTPUT_RATE);
            QCOMPARE(candidate.value()->kernel_sha256,playback.kernel_sha256());
            for(std::size_t channel=0;channel<2;++channel)
                QCOMPARE(channel_bits(candidate.value()->pcm,channel),
                         channel_bits(golden,channel));
        }
        auto srcCandidate=audio::materialize_full_resource_output_rate_candidate(
            pcm.value()->view(),rate(hz),frame_count(17));
        QVERIFY(srcCandidate);
        QVERIFY(!srcCandidate.value()->used_src);
        for(std::size_t channel=0;channel<2;++channel)
            QCOMPARE(channel_bits(srcCandidate.value()->pcm,channel),
                     channel_bits(*pcm.value(),channel));

        auto candidate96=audio::materialize_full_resource_output_rate_candidate(
            pcm.value()->view(),rate(96000),frame_count(17));
        QVERIFY(candidate96);
        QVERIFY(candidate96.value()->used_src);
        auto overBlock=audio::materialize_full_resource_output_rate_candidate(
            pcm.value()->view(),rate(target),frame_count(1025));
        QVERIFY(!overBlock);
        auto segment=pcm.value()->view().subview(
            core::FrameIndex{1},frame_count(N-1));
        QVERIFY(segment);
        auto partial=audio::materialize_full_resource_output_rate_candidate(
            *segment.value(),rate(target),frame_count(17));
        QVERIFY(!partial);
        QCOMPARE(channel_bits(*pcm.value(),0).size(),std::size_t{N});
    }
}

void PlaybackSampleRateAdapterTest::experimentalFullResourceSixBySixRateMatrix()
{
    // NONQUALIFYING functional test; spectral and physical gates pending.
    constexpr std::array<std::int64_t,6> rates{
        44100,48000,88200,96000,176400,192000};
    constexpr std::int64_t N=97;
    std::size_t verified=0;
    for(const auto srcHz:rates) {
        auto format=audio::AudioFormat::create(
            rate(srcHz),audio::ChannelLayout::STEREO_LR);
        QVERIFY(format);
        auto pcm=audio::AudioBuffer::create(*format.value(),
            audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{0},frame_count(N));
        QVERIFY(pcm);
        auto l=pcm.value()->mutable_view().channel(0);
        auto r=pcm.value()->mutable_view().channel(1);
        QVERIFY(l&&r);
        for(std::int64_t i=0;i<N;++i) {
            (*l.value())[static_cast<std::size_t>(i)]=0.125;
            (*r.value())[static_cast<std::size_t>(i)]=-0.25;
        }
        for(const auto dstHz:rates) {
            auto a=audio::materialize_full_resource_output_rate_candidate(
                pcm.value()->view(),rate(dstHz),frame_count(1));
            auto b=audio::materialize_full_resource_output_rate_candidate(
                pcm.value()->view(),rate(dstHz),frame_count(257));
            QVERIFY2(a,a?"":a.error()->message().c_str());
            QVERIFY2(b,b?"":b.error()->message().c_str());
            const auto frames=(N*dstHz+srcHz-1)/srcHz;
            QCOMPARE(a.value()->pcm.view().frame_count().value(),frames);
            QCOMPARE(b.value()->pcm.view().frame_count().value(),frames);
            QCOMPARE(a.value()->used_src,srcHz!=dstHz);
            for(std::size_t ch=0;ch<2;++ch) {
                QCOMPARE(channel_bits(a.value()->pcm,ch),
                         channel_bits(b.value()->pcm,ch));
                auto samples=a.value()->pcm.view().channel(ch);
                QVERIFY(samples);
                const double expected=ch==0?0.125:-0.25;
                // The two historical 44.1/48 kernels have their own
                // frozen bitwise oracle and must not be requalified by a
                // stricter NEW-kernel DC tolerance in this matrix.
                const bool frozenPair=(srcHz==44100&&dstHz==48000)
                    || (srcHz==48000&&dstHz==44100);
                for(const double v:*samples.value()) {
                    QVERIFY2(std::isfinite(v),
                        "All 36 SRC pairs must generate finite output.");
                    if(!frozenPair)
                        QVERIFY2(std::abs(v-expected)<2e-12,
                            "New rational SRC must preserve constant PCM.");
                }
            }
            ++verified;
        }
    }
    QCOMPARE(verified,std::size_t{36});
}

void PlaybackSampleRateAdapterTest::policyIdentityAndFrameMap()
{
    auto identity = audio::PlaybackSampleRateAdapter::create(
        audio::PlaybackRateSpec{
            rate(44'100),
            rate(44'100),
            audio::ChannelLayout::STEREO_LR,
            frame_count(441),
        });
    QVERIFY(!identity);
    QCOMPARE(identity.error()->code(), core::ErrorCode::UnsupportedOperation);
    QCOMPARE(
        QString::fromStdString(identity.error()->message()),
        QStringLiteral("PLAYBACK_RATE_PAIR_UNSUPPORTED_V1"));

    auto unsupported = audio::PlaybackSampleRateAdapter::create(
        audio::PlaybackRateSpec{
            rate(96'000),
            rate(48'000),
            audio::ChannelLayout::STEREO_LR,
            frame_count(960),
        });
    QVERIFY(!unsupported);
    QCOMPARE(unsupported.error()->code(), core::ErrorCode::UnsupportedOperation);

    auto forward = adapter(44'100, 48'000, 441);
    QCOMPARE(forward.interpolation_factor(), std::int64_t{160});
    QCOMPARE(forward.decimation_factor(), std::int64_t{147});
    QCOMPARE(forward.output_frame_count().value(), std::int64_t{480});
    QCOMPARE(
        forward.map_input_frame_to_output(core::FrameIndex{1}).value()->value(),
        std::int64_t{2});
    QCOMPARE(
        forward.map_output_frame_to_input_cursor(core::FrameIndex{160})
            .value()
            ->value(),
        std::int64_t{147});

    auto interval = core::FrameRange::create(
        core::FrameIndex{10}, core::FrameIndex{20});
    QVERIFY(interval);
    auto mapped = forward.map_input_range_to_output(*interval.value());
    QVERIFY(mapped);
    QCOMPARE(mapped.value()->begin().value(), std::int64_t{11});
    QCOMPARE(mapped.value()->end().value(), std::int64_t{22});

    auto reverse = adapter(48'000, 44'100, 480);
    QCOMPARE(reverse.interpolation_factor(), std::int64_t{147});
    QCOMPARE(reverse.decimation_factor(), std::int64_t{160});
    QCOMPARE(reverse.output_frame_count().value(), std::int64_t{441});
}

void PlaybackSampleRateAdapterTest::assetsAreFrozenAndSymmetric()
{
    auto forward = adapter(44'100, 48'000, 10);
    QCOMPARE(forward.kernel_size(), std::size_t{30'721});
    QCOMPARE(forward.group_delay_high_rate_frames(), std::int64_t{15'360});
    QCOMPARE(
        QString::fromUtf8(forward.kernel_sha256()),
        QStringLiteral(
            "ec8c4d554eb4ede15f58eead1fede923e74c4692c57832bda716e3fc826926b4"));
    for (std::size_t index = 0; index < forward.kernel_size(); ++index) {
        QCOMPARE(
            forward.coefficient_bits(index),
            forward.coefficient_bits(forward.kernel_size() - 1U - index));
    }

    auto reverse = adapter(48'000, 44'100, 10);
    QCOMPARE(reverse.kernel_size(), std::size_t{28'225});
    QCOMPARE(reverse.group_delay_high_rate_frames(), std::int64_t{14'112});
    QCOMPARE(
        QString::fromUtf8(reverse.kernel_sha256()),
        QStringLiteral(
            "37cfda8e885e2d00646ec60d02bdf5b7dcb85348fca416e065bab71a824efb22"));
    for (std::size_t index = 0; index < reverse.kernel_size(); ++index) {
        QCOMPARE(
            reverse.coefficient_bits(index),
            reverse.coefficient_bits(reverse.kernel_size() - 1U - index));
    }
}

void PlaybackSampleRateAdapterTest::constantAndChunkInvariance()
{
    constexpr std::int64_t kInputFrames = 900;
    std::vector<std::uint64_t> interleaved;
    interleaved.reserve(static_cast<std::size_t>(kInputFrames) * 2U);
    for (std::int64_t frame = 0; frame < kInputFrames; ++frame) {
        interleaved.push_back(std::bit_cast<std::uint64_t>(0.25));
        interleaved.push_back(std::bit_cast<std::uint64_t>(-0.5));
    }

    auto control = std::make_shared<ReaderControl>();
    auto wholeReader = open_f64_stereo(44'100, interleaved, control);
    auto converter = adapter(44'100, 48'000, kInputFrames);
    const auto outputFrames = converter.output_frame_count().value();
    QVERIFY(outputFrames <= audio::PlaybackSampleRateAdapter::kMaximumOutputBlockFrames);
    auto whole = output_buffer(converter, 0, outputFrames);
    QVERIFY(converter.read_frames(
        *wholeReader, core::FrameIndex{0}, whole.mutable_view()));
    const auto wholeLeft = channel_bits(whole, 0);
    const auto wholeRight = channel_bits(whole, 1);

    const std::vector<std::int64_t> partition{
        1, 17, 257, outputFrames - 275};
    auto partitionReader = open_f64_stereo(44'100, interleaved);
    const auto partitionLeft = render_partitioned(
        converter, *partitionReader, partition, 0);
    auto secondPartitionReader = open_f64_stereo(44'100, interleaved);
    const auto partitionRight = render_partitioned(
        converter, *secondPartitionReader, partition, 1);
    QCOMPARE(partitionLeft, wholeLeft);
    QCOMPARE(partitionRight, wholeRight);
    QVERIFY(control->largestReadRequest <= 4096U);

    auto left = whole.view().channel(0);
    auto right = whole.view().channel(1);
    QVERIFY(left);
    QVERIFY(right);
    for (std::size_t frame = 0; frame < left.value()->size(); ++frame) {
        QVERIFY(std::abs((*left.value())[frame] - 0.25) < 2.0e-6);
        QVERIFY(std::abs((*right.value())[frame] + 0.5) < 4.0e-6);
    }
}

void PlaybackSampleRateAdapterTest::oneFrameBoundaryAndStereoIndependence()
{
    const std::vector<std::uint64_t> oneFrame{
        std::bit_cast<std::uint64_t>(0.125),
        std::bit_cast<std::uint64_t>(-0.25),
    };
    auto oneReader = open_f64_stereo(48'000, oneFrame);
    auto reverse = adapter(48'000, 44'100, 1);
    QCOMPARE(reverse.output_frame_count().value(), std::int64_t{1});
    auto oneOutput = output_buffer(reverse, 0, 1);
    QVERIFY(reverse.read_frames(
        *oneReader, core::FrameIndex{0}, oneOutput.mutable_view()));
    auto left = oneOutput.view().channel(0);
    auto right = oneOutput.view().channel(1);
    QVERIFY(left);
    QVERIFY(right);
    QVERIFY(std::abs((*left.value())[0] - 0.125) < 2.0e-6);
    QVERIFY(std::abs((*right.value())[0] + 0.25) < 2.0e-6);

    std::vector<std::uint64_t> impulse(64U * 2U, UINT64_C(0));
    impulse[20U] = std::bit_cast<std::uint64_t>(1.0);
    auto impulseReader = open_f64_stereo(44'100, impulse);
    auto forward = adapter(44'100, 48'000, 64);
    auto impulseOutput = output_buffer(
        forward, 0, forward.output_frame_count().value());
    QVERIFY(forward.read_frames(
        *impulseReader, core::FrameIndex{0}, impulseOutput.mutable_view()));
    auto silentRight = impulseOutput.view().channel(1);
    QVERIFY(silentRight);
    for (const auto sample : *silentRight.value()) {
        QCOMPARE(std::bit_cast<std::uint64_t>(sample), UINT64_C(0));
    }
}

void PlaybackSampleRateAdapterTest::reverseDirectionAndAbsoluteSeekInvariance()
{
    constexpr std::int64_t kInputFrames = 1'000;
    std::vector<std::uint64_t> interleaved;
    interleaved.reserve(static_cast<std::size_t>(kInputFrames) * 2U);
    for (std::int64_t frame = 0; frame < kInputFrames; ++frame) {
        const double left = static_cast<double>((frame % 101) - 50) / 100.0;
        const double right = static_cast<double>((frame % 79) - 39) / 100.0;
        interleaved.push_back(std::bit_cast<std::uint64_t>(left));
        interleaved.push_back(std::bit_cast<std::uint64_t>(right));
    }
    auto converter = adapter(48'000, 44'100, kInputFrames);
    const auto outputFrames = converter.output_frame_count().value();
    auto wholeReader = open_f64_stereo(48'000, interleaved);
    auto whole = output_buffer(converter, 0, outputFrames);
    QVERIFY(converter.read_frames(
        *wholeReader, core::FrameIndex{0}, whole.mutable_view()));
    const auto wholeLeft = channel_bits(whole, 0);
    const auto wholeRight = channel_bits(whole, 1);

    const std::vector<std::int64_t> partition{
        3, 127, 509, outputFrames - 639};
    auto leftReader = open_f64_stereo(48'000, interleaved);
    auto rightReader = open_f64_stereo(48'000, interleaved);
    QCOMPARE(
        render_partitioned(converter, *leftReader, partition, 0),
        wholeLeft);
    QCOMPARE(
        render_partitioned(converter, *rightReader, partition, 1),
        wholeRight);

    constexpr std::int64_t kSeekStart = 317;
    constexpr std::int64_t kSeekFrames = 91;
    auto seekReader = open_f64_stereo(48'000, interleaved);
    auto seekBlock = output_buffer(converter, kSeekStart, kSeekFrames);
    QVERIFY(converter.read_frames(
        *seekReader,
        core::FrameIndex{kSeekStart},
        seekBlock.mutable_view()));
    const auto seekLeft = channel_bits(seekBlock, 0);
    const auto seekRight = channel_bits(seekBlock, 1);
    QCOMPARE(
        seekLeft,
        std::vector<std::uint64_t>(
            wholeLeft.begin() + kSeekStart,
            wholeLeft.begin() + kSeekStart + kSeekFrames));
    QCOMPARE(
        seekRight,
        std::vector<std::uint64_t>(
            wholeRight.begin() + kSeekStart,
            wholeRight.begin() + kSeekStart + kSeekFrames));
}

void PlaybackSampleRateAdapterTest::decodeWindowIsTrackDurationIndependent()
{
    constexpr std::int64_t kInputFrames = 100'000;
    std::vector<std::uint64_t> interleaved(
        static_cast<std::size_t>(kInputFrames) * 2U,
        std::bit_cast<std::uint64_t>(0.0));
    auto control = std::make_shared<ReaderControl>();
    auto reader = open_f64_stereo(44'100, interleaved, control);
    auto converter = adapter(44'100, 48'000, kInputFrames);
    constexpr std::int64_t kOutputStart = 50'000;
    auto block = output_buffer(
        converter,
        kOutputStart,
        audio::PlaybackSampleRateAdapter::kMaximumOutputBlockFrames);
    QVERIFY(converter.read_frames(
        *reader, core::FrameIndex{kOutputStart}, block.mutable_view()));
    QVERIFY(control->totalBytesRead < 50'000U);
    QVERIFY(control->largestReadRequest <= 4096U);

    auto oversized = output_buffer(
        converter,
        0,
        audio::PlaybackSampleRateAdapter::kMaximumOutputBlockFrames + 1);
    auto rejected = converter.read_frames(
        *reader, core::FrameIndex{0}, oversized.mutable_view());
    QVERIFY(!rejected);
    QCOMPARE(rejected.error()->code(), core::ErrorCode::InvalidArgument);
}

void PlaybackSampleRateAdapterTest::conservativeInputDependenciesMatchIndependentTapOracle()
{
    // Independent tap oracle: enumerate rational convolution coefficients
    // by interpolation phase; reflect the logical SOURCE frame separately.
    // Do not call production convolution_window(), cursor or enclosure.
    const auto independentlyReflected = [](std::int64_t logical,
                                          std::int64_t sourceFrames) {
        if (sourceFrames == 1) return std::int64_t{0};
        const auto period = sourceFrames * 2;
        auto index = logical % period;
        if (index < 0) index += period;
        return index < sourceFrames ? index : period - 1 - index;
    };
    for (const auto& [inputRate, outputRate] :
         std::array<std::pair<std::int64_t, std::int64_t>, 2>{{
             {44'100, 48'000}, {48'000, 44'100}}}) {
        for (const auto sourceFrames :
             std::array<std::int64_t, 5>{{1, 2, 19, 257, 2'000}}) {
            auto converter = adapter(inputRate, outputRate, sourceFrames);
            const auto outputFrames = converter.output_frame_count().value();
            const auto blockLength = std::min<std::int64_t>(128, outputFrames);
            const std::array<std::pair<std::int64_t, std::int64_t>, 4> ranges{{
                {0, 1},
                {outputFrames / 2, outputFrames / 2 + 1},
                {outputFrames - 1, outputFrames},
                {0, blockLength},
            }};
            for (const auto& [begin, end] : ranges) {
                auto interval = core::FrameRange::create(
                    core::FrameIndex{begin}, core::FrameIndex{end});
                QVERIFY(interval);
                auto enclosure = converter.conservative_input_enclosure_for_output_range(
                    *interval.value());
                QVERIFY(enclosure);
                std::int64_t minimum = sourceFrames;
                std::int64_t maximum = -1;
                for (auto outputFrame = begin; outputFrame < end; ++outputFrame) {
                    const auto q = outputFrame * converter.decimation_factor()
                        + converter.group_delay_high_rate_frames();
                    const auto phase = q % converter.interpolation_factor();
                    for (auto coefficient = phase;
                         coefficient < static_cast<std::int64_t>(converter.kernel_size());
                         coefficient += converter.interpolation_factor()) {
                        const auto logical =
                            (q - coefficient) / converter.interpolation_factor();
                        const auto sourceIndex = independentlyReflected(
                            logical, sourceFrames);
                        minimum = std::min(minimum, sourceIndex);
                        maximum = std::max(maximum, sourceIndex);
                    }
                }
                QCOMPARE(enclosure.value()->begin().value(), minimum);
                QCOMPARE(enclosure.value()->end().value(), maximum + 1);
            }
        }
    }
    // Negative oracle: output cursor zero DOES NOT prove that only
    // SOURCE frame zero contributes to the already-consumed output frame.
    auto converter = adapter(44'100, 48'000, 2'000);
    auto firstOutput = core::FrameRange::create(
        core::FrameIndex{0}, core::FrameIndex{1});
    QVERIFY(firstOutput);
    auto dependencies = converter.conservative_input_enclosure_for_output_range(
        *firstOutput.value());
    QVERIFY(dependencies);
    auto cursor = converter.map_output_frame_to_input_cursor(core::FrameIndex{1});
    QVERIFY(cursor);
    QVERIFY(dependencies.value()->end().value() > cursor.value()->value());
}

void PlaybackSampleRateAdapterTest::conservativeInputDependenciesRejectInvalidRanges()
{
    auto converter = adapter(44'100, 48'000, 2'000);
    const auto outputFrames = converter.output_frame_count().value();
    const std::array<std::pair<std::int64_t, std::int64_t>, 4> invalid{{
        {-1, 1}, {0, 0}, {outputFrames - 1, outputFrames + 1},
        {0, audio::PlaybackSampleRateAdapter::kMaximumOutputBlockFrames + 1},
    }};
    for (const auto& [begin, end] : invalid) {
        auto interval = core::FrameRange::create(
            core::FrameIndex{begin}, core::FrameIndex{end});
        // FrameRange may reject impossible ranges earlier than the adapter.
        if (!interval) continue;
        auto rejected = converter.conservative_input_enclosure_for_output_range(
            *interval.value());
        QVERIFY(!rejected);
        QCOMPARE(rejected.error()->code(), core::ErrorCode::InvalidArgument);
    }
}

void PlaybackSampleRateAdapterTest::noFutureDependencyPrefixAgainstIndependentOracle()
{
    // Independent chronological oracle: enumerate every FIR coefficient
    // contribution of every NOT-YET-CONSUMED output frame. It deliberately
    // does not use the new production source-frontier formula.
    const auto reflectIndependent = [](std::int64_t logical,
                                      std::int64_t inputFrames) {
        if (inputFrames == 1) return std::int64_t{0};
        const auto period = inputFrames * 2;
        auto phase = logical % period;
        if (phase < 0) phase += period;
        return phase < inputFrames ? phase : period - 1 - phase;
    };
    for (const auto& [inputRate, outputRate] :
         std::array<std::pair<std::int64_t, std::int64_t>, 2>{{
             {44'100, 48'000}, {48'000, 44'100}}}) {
        for (const auto inputFrames :
             std::array<std::int64_t, 4>{{1, 2, 257, 2'000}}) {
            auto converter = adapter(inputRate, outputRate, inputFrames);
            const auto outputFrames = converter.output_frame_count().value();
            const std::array<std::int64_t, 5> consumedPositions{{
                0, outputFrames / 4, outputFrames / 2,
                outputFrames - 1, outputFrames}};
            std::int64_t lastRetired = 0;
            for (const auto consumed : consumedPositions) {
                auto boundary = converter.no_future_dependency_source_prefix(
                    core::FrameIndex{consumed});
                QVERIFY(boundary);
                const auto retired = boundary.value()->value();
                QVERIFY(retired >= lastRetired);
                QVERIFY(retired <= inputFrames);
                lastRetired = retired;
                if (consumed == outputFrames) {
                    QCOMPARE(retired, inputFrames);
                    continue;
                }
                std::int64_t earliestFuture = inputFrames;
                for (auto outputFrame = consumed;
                     outputFrame < outputFrames; ++outputFrame) {
                    const auto q = outputFrame * converter.decimation_factor()
                        + converter.group_delay_high_rate_frames();
                    const auto phase = q % converter.interpolation_factor();
                    for (auto tap = phase;
                         tap < static_cast<std::int64_t>(converter.kernel_size());
                         tap += converter.interpolation_factor()) {
                        const auto logical =
                            (q - tap) / converter.interpolation_factor();
                        earliestFuture = std::min(earliestFuture,
                            reflectIndependent(logical, inputFrames));
                    }
                }
                // Any source sample 'retired' by the helper must be absent
                // from all future SRC output frames. A lower bound may be
                // conservative, never above the exact independent oracle.
                QVERIFY(retired <= earliestFuture);
            }
            auto beforeZero = converter.no_future_dependency_source_prefix(
                core::FrameIndex{-1});
            QVERIFY(!beforeZero);
            QCOMPARE(beforeZero.error()->code(),
                     core::ErrorCode::InvalidArgument);
            auto pastEnd = converter.no_future_dependency_source_prefix(
                core::FrameIndex{outputFrames + 1});
            QVERIFY(!pastEnd);
            QCOMPARE(pastEnd.error()->code(),
                     core::ErrorCode::InvalidArgument);
        }
    }
}

}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::PlaybackSampleRateAdapterTest)

#include "test_playback_sample_rate_adapter.moc"
