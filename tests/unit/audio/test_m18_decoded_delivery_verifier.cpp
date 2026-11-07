// Exact M18 post-quant WAV decode comparison; no file transaction/FQ.
#include "internal/m18_decoded_delivery_verifier.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <QtTest/QTest>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>

namespace rgsml::tests {
namespace {
[[nodiscard]] std::shared_ptr<audio::AudioBuffer> make_buffer(
    std::int64_t hz,audio::ChannelLayout layout,
    audio::FrameDomainId domain,std::int64_t first,std::int64_t frames)
{
    const auto rate=core::SampleRate::create(hz);
    if(!rate)return {};
    const auto fmt=audio::AudioFormat::create(*rate.value(),layout);
    const auto count=core::FrameCount::create(frames);
    if(!fmt ||!count)return {};
    auto owned=audio::AudioBuffer::create(
        *fmt.value(),domain,core::FrameIndex{first},*count.value());
    if(!owned)return {};
    return std::make_shared<audio::AudioBuffer>(std::move(*owned.value()));
}
} // namespace
class M18DecodedDeliveryVerifierTest final : public QObject {
    Q_OBJECT
private slots:
    void pcm24ExactDecodedCodesAndCorruption();
    void float32RneSignedZeroAndSubnormal();
    void rejectWrongDomainsShapesSeedsAndFormats();
};

void M18DecodedDeliveryVerifierTest::pcm24ExactDecodedCodesAndCorruption()
{
    using namespace audio::internal;
    for(const auto layout:{audio::ChannelLayout::MONO_C,
                           audio::ChannelLayout::STEREO_LR}){
        auto master=make_buffer(48000,layout,
            audio::FrameDomainId::OUTPUT_RATE,3,257);
        auto delivered=make_buffer(48000,layout,
            audio::FrameDomainId::SOURCE_PROCESSING_RATE,3,257);
        QVERIFY(master &&delivered);
        const auto channels=master->view().format().channel_count();
        for(std::size_t ch=0;ch<channels;++ch){
            auto original=master->mutable_view().channel(ch);
            auto decoded=delivered->mutable_view().channel(ch);
            QVERIFY(original &&decoded);
            for(std::size_t frame=0;frame<257U;++frame){
                const auto n=static_cast<std::int64_t>(frame%13U)-6;
                const double sample=static_cast<double>(n)/32.0;
                (*original.value())[frame]=sample;
                const auto coded=m18_quantize_pcm24(
                    sample,UINT64_C(0x12345678ABCDEF90),3U+frame,
                    static_cast<std::uint32_t>(ch));
                QCOMPARE(coded.status,M18CodecStatus::OK);
                (*decoded.value())[frame]=static_cast<double>(coded.code)/8388608.0;
            }
        }
        const auto good=m18_verify_decoded_delivery(
            master->view(),delivered->view(),
            audio::WavSampleFormat::PCM_S24,
            UINT64_C(0x12345678ABCDEF90));
        QVERIFY(good.matched());
        auto damaged=delivered->mutable_view().channel(channels-1U);
        QVERIFY(damaged);
        (*damaged.value())[23]=0.987654321;
        const auto bad=m18_verify_decoded_delivery(
            master->view(),delivered->view(),
            audio::WavSampleFormat::PCM_S24,
            UINT64_C(0x12345678ABCDEF90));
        QCOMPARE(bad.status,M18DecodedVerifyStatus::SAMPLE_MISMATCH);
        QCOMPARE(bad.absolute_frame,std::uint64_t{26});
        QCOMPARE(bad.channel,static_cast<std::uint32_t>(channels-1U));
    }
}

void M18DecodedDeliveryVerifierTest::float32RneSignedZeroAndSubnormal()
{
    using namespace audio::internal;
    static_assert(m18_binary64_bits_from_f32_bits(0U)==0U);
    static_assert(m18_binary64_bits_from_f32_bits(0x80000000U)
        ==UINT64_C(0x8000000000000000));
    static_assert(m18_binary64_bits_from_f32_bits(0x3f800000U)
        ==UINT64_C(0x3ff0000000000000));
    static_assert(m18_binary64_bits_from_f32_bits(0x00000001U)
        ==(UINT64_C(874)<<52)); // exact 2^-149
    auto master=make_buffer(96000,audio::ChannelLayout::MONO_C,
        audio::FrameDomainId::OUTPUT_RATE,0,6);
    auto delivered=make_buffer(96000,audio::ChannelLayout::MONO_C,
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,0,6);
    QVERIFY(master &&delivered);
    auto src=master->mutable_view().channel(0);
    auto dst=delivered->mutable_view().channel(0);
    QVERIFY(src &&dst);
    const std::uint32_t reference[6]={
        0U,0x80000000U,0x1U,0x3F800000U,0xBF000000U,0x7F7FFFFFU};
    for(std::size_t i=0;i<6U;++i){
        const auto bits=m18_binary64_bits_from_f32_bits(reference[i]);
        (*src.value())[i]=std::bit_cast<double>(bits);
        (*dst.value())[i]=std::bit_cast<double>(bits);
    }
    const auto good=m18_verify_decoded_delivery(
        master->view(),delivered->view(),
        audio::WavSampleFormat::IEEE_F32,std::nullopt);
    QVERIFY(good.matched());
    (*dst.value())[1]=0.0; // change ONLY signed-zero bit
    const auto wrong=m18_verify_decoded_delivery(
        master->view(),delivered->view(),
        audio::WavSampleFormat::IEEE_F32,std::nullopt);
    QCOMPARE(wrong.status,M18DecodedVerifyStatus::SAMPLE_MISMATCH);
    QCOMPARE(wrong.absolute_frame,std::uint64_t{1});
}

void M18DecodedDeliveryVerifierTest::rejectWrongDomainsShapesSeedsAndFormats()
{
    using namespace audio::internal;
    auto master=make_buffer(44100,audio::ChannelLayout::MONO_C,
        audio::FrameDomainId::OUTPUT_RATE,0,2);
    auto decoded=make_buffer(44100,audio::ChannelLayout::MONO_C,
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,0,2);
    QVERIFY(master &&decoded);
    QCOMPARE(m18_verify_decoded_delivery(master->view(),decoded->view(),
        audio::WavSampleFormat::PCM_S24,std::nullopt).status,
        M18DecodedVerifyStatus::INVALID_SEED);
    QCOMPARE(m18_verify_decoded_delivery(master->view(),decoded->view(),
        audio::WavSampleFormat::IEEE_F32,42U).status,
        M18DecodedVerifyStatus::INVALID_SEED);
    QCOMPARE(m18_verify_decoded_delivery(master->view(),decoded->view(),
        audio::WavSampleFormat::IEEE_F64,std::nullopt).status,
        M18DecodedVerifyStatus::INVALID_ENCODING);
    auto wrongDomain=make_buffer(44100,audio::ChannelLayout::MONO_C,
        audio::FrameDomainId::OUTPUT_RATE,0,2);
    QVERIFY(wrongDomain);
    QCOMPARE(m18_verify_decoded_delivery(master->view(),wrongDomain->view(),
        audio::WavSampleFormat::IEEE_F32,std::nullopt).status,
        M18DecodedVerifyStatus::INVALID_DOMAIN);
    auto wrongStart=make_buffer(44100,audio::ChannelLayout::MONO_C,
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,1,2);
    QVERIFY(wrongStart);
    QCOMPARE(m18_verify_decoded_delivery(master->view(),wrongStart->view(),
        audio::WavSampleFormat::IEEE_F32,std::nullopt).status,
        M18DecodedVerifyStatus::INVALID_SHAPE);
    auto wrongRate=make_buffer(48000,audio::ChannelLayout::MONO_C,
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,0,2);
    QVERIFY(wrongRate);
    QCOMPARE(m18_verify_decoded_delivery(master->view(),wrongRate->view(),
        audio::WavSampleFormat::IEEE_F32,std::nullopt).status,
        M18DecodedVerifyStatus::INVALID_SHAPE);
    auto invalid=master->mutable_view().channel(0);
    QVERIFY(invalid);
    (*invalid.value())[0]=std::numeric_limits<double>::infinity();
    QCOMPARE(m18_verify_decoded_delivery(master->view(),decoded->view(),
        audio::WavSampleFormat::PCM_S24,1U).status,
        M18DecodedVerifyStatus::CODEC_REJECTED);
}
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M18DecodedDeliveryVerifierTest)
#include "test_m18_decoded_delivery_verifier.moc"
