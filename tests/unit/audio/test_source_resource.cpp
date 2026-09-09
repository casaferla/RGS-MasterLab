#include <rgsml/audio/source_resource.hpp>

#include "wav_test_support.hpp"
#include "../../audio_golden/wav/golden_vectors.hpp"

#include <QTest>

#include <memory>
#include <span>
#include <utility>

namespace rgsml::tests {
namespace {

using namespace wav_support;

class TrackingReader final : public core::IResourceReader {
public:
    TrackingReader(
        Bytes bytes,
        std::shared_ptr<ReaderControl> control,
        std::shared_ptr<bool> released,
        bool canSeek = true,
        bool canRead = true)
        : inner_(std::make_unique<MemoryReader>(
              std::move(bytes), control, canSeek, true, canRead))
        , released_(std::move(released))
    {
    }

    ~TrackingReader() noexcept override
    {
        *released_ = true;
    }

    const core::ResourceReference& reference() const noexcept override
    {
        return inner_->reference();
    }
    core::ResourceCapabilities capabilities() const noexcept override
    {
        return inner_->capabilities();
    }
    core::Result<std::uint64_t> size_bytes() const override
    {
        return inner_->size_bytes();
    }
    core::Result<std::uint64_t> position_bytes() const override
    {
        return inner_->position_bytes();
    }
    core::Result<std::size_t> read(std::span<std::byte> destination) override
    {
        return inner_->read(destination);
    }
    core::Status seek_bytes(std::uint64_t offset) override
    {
        return inner_->seek_bytes(offset);
    }
    core::Status close() override
    {
        return inner_->close();
    }

private:
    std::unique_ptr<MemoryReader> inner_;
    std::shared_ptr<bool> released_;
};

[[nodiscard]] std::unique_ptr<TrackingReader> tracking_reader(
    Bytes bytes,
    const std::shared_ptr<ReaderControl>& control,
    const std::shared_ptr<bool>& released,
    bool canSeek = true,
    bool canRead = true)
{
    return std::make_unique<TrackingReader>(
        std::move(bytes), control, released, canSeek, canRead);
}

}  // namespace

class SourceResourceTest final : public QObject {
    Q_OBJECT

private slots:
    void acceptedGoldenMetadata();
    void representativeAcceptedEncodings();
    void failureCategoriesAndRelease();
};

void SourceResourceTest::acceptedGoldenMetadata()
{
    const auto riffBytes = from_u8_array(audio_golden::kRiffPcm16Mono);
    auto riffControl = std::make_shared<ReaderControl>();
    auto riffReleased = std::make_shared<bool>(false);
    auto riff = audio::SourceResource::probe(tracking_reader(
        riffBytes, riffControl, riffReleased));
    QVERIFY(riff);
    QVERIFY(*riffReleased);
    QCOMPARE(riffControl->closeCalls, std::size_t{1});
    QVERIFY(riffControl->totalBytesRead < riffBytes.size());
    QCOMPARE(riff.value()->reference().provider_id(), std::string{"test.memory"});
    QCOMPARE(riff.value()->wav_info().container_kind(), audio::WavContainerKind::RIFF);
    QCOMPARE(
        riff.value()->wav_info().encoded_sample_format(),
        audio::WavSampleFormat::PCM_S16);
    QCOMPARE(riff.value()->wav_info().audio_format().sample_rate().value(), 44100);
    QCOMPARE(riff.value()->wav_info().frame_count().value(), 3);
    QCOMPARE(riff.value()->wav_info().data_size_bytes(), std::uint64_t{6});

    const auto rf64Bytes = from_u8_array(audio_golden::kRf64F64NegativeZero);
    auto rf64Control = std::make_shared<ReaderControl>();
    auto rf64Released = std::make_shared<bool>(false);
    auto rf64 = audio::SourceResource::probe(tracking_reader(
        rf64Bytes, rf64Control, rf64Released));
    QVERIFY(rf64);
    QVERIFY(*rf64Released);
    QCOMPARE(rf64Control->closeCalls, std::size_t{1});
    QVERIFY(rf64Control->totalBytesRead < rf64Bytes.size());
    QCOMPARE(rf64.value()->wav_info().container_kind(), audio::WavContainerKind::RF64);
    QCOMPARE(
        rf64.value()->wav_info().encoded_sample_format(),
        audio::WavSampleFormat::IEEE_F64);
    QCOMPARE(rf64.value()->wav_info().audio_format().sample_rate().value(), 48000);
    QCOMPARE(rf64.value()->wav_info().frame_count().value(), 1);
}

void SourceResourceTest::representativeAcceptedEncodings()
{
    const auto pcm24Payload = pcm_payload(
        std::array<std::int64_t, 4>{-8388608, 0, 1, 8388607}, 24);
    const auto pcm24 = make_wav(1, 24, 2, 48000, pcm24Payload);
    auto pcmControl = std::make_shared<ReaderControl>();
    auto pcmReleased = std::make_shared<bool>(false);
    auto pcm = audio::SourceResource::probe(tracking_reader(
        pcm24, pcmControl, pcmReleased));
    QVERIFY(pcm);
    QCOMPARE(
        pcm.value()->wav_info().encoded_sample_format(),
        audio::WavSampleFormat::PCM_S24);
    QCOMPARE(
        pcm.value()->wav_info().audio_format().channel_layout(),
        audio::ChannelLayout::STEREO_LR);
    QCOMPARE(pcm.value()->wav_info().frame_count().value(), 2);

    const auto floatPayload = f32_payload(
        std::array<std::uint32_t, 2>{0x00000000U, 0x3f000000U});
    const auto ieee = make_wav(3, 32, 1, 96000, floatPayload);
    auto floatControl = std::make_shared<ReaderControl>();
    auto floatReleased = std::make_shared<bool>(false);
    auto floating = audio::SourceResource::probe(tracking_reader(
        ieee, floatControl, floatReleased));
    QVERIFY(floating);
    QCOMPARE(
        floating.value()->wav_info().encoded_sample_format(),
        audio::WavSampleFormat::IEEE_F32);
    QCOMPARE(floating.value()->wav_info().audio_format().sample_rate().value(), 96000);
    QCOMPARE(floating.value()->wav_info().frame_count().value(), 2);
}

void SourceResourceTest::failureCategoriesAndRelease()
{
    std::unique_ptr<core::IResourceReader> nullReader;
    auto nullResult = audio::SourceResource::probe(std::move(nullReader));
    QVERIFY(!nullResult);
    QCOMPARE(nullResult.error()->code(), core::ErrorCode::InvalidArgument);

    const auto valid = from_u8_array(audio_golden::kRiffPcm16Mono);
    auto noReadControl = std::make_shared<ReaderControl>();
    auto noReadReleased = std::make_shared<bool>(false);
    auto noRead = audio::SourceResource::probe(tracking_reader(
        valid, noReadControl, noReadReleased, true, false));
    QVERIFY(!noRead);
    QCOMPARE(noRead.error()->code(), core::ErrorCode::AccessDenied);
    QVERIFY(*noReadReleased);

    auto noSeekControl = std::make_shared<ReaderControl>();
    auto noSeekReleased = std::make_shared<bool>(false);
    auto noSeek = audio::SourceResource::probe(tracking_reader(
        valid, noSeekControl, noSeekReleased, false, true));
    QVERIFY(!noSeek);
    QCOMPARE(noSeek.error()->code(), core::ErrorCode::UnsupportedOperation);
    QVERIFY(*noSeekReleased);

    Bytes truncated{
        std::byte{0x52}, std::byte{0x49}, std::byte{0x46}, std::byte{0x46}};
    auto truncatedControl = std::make_shared<ReaderControl>();
    auto truncatedReleased = std::make_shared<bool>(false);
    auto truncatedResult = audio::SourceResource::probe(tracking_reader(
        std::move(truncated), truncatedControl, truncatedReleased));
    QVERIFY(!truncatedResult);
    QCOMPARE(truncatedResult.error()->code(), core::ErrorCode::TruncatedAudioData);
    QVERIFY(*truncatedReleased);
}

}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::SourceResourceTest)

#include "test_source_resource.moc"
