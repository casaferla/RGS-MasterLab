// M18 candidate readback through REAL canonical WavReader, bounded RAM.
// This test does not authorize M18 publication or certify final Windows FQ.
#include "internal/m18_candidate_delivery_validation.hpp"
#include "wav_test_support.hpp"
#include "m18_streaming_wav.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <QtTest/QTest>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace rgsml::tests {
namespace {
class ByteCollector final : public audio::internal::M18ByteSink {
public:
    [[nodiscard]] bool write_all(
        std::span<const std::uint8_t> data) noexcept override
    {
        try{
            bytes.insert(bytes.end(),data.begin(),data.end());
            return true;
        }catch(...){return false;}
    }
    std::vector<std::uint8_t> bytes;
};

[[nodiscard]] std::shared_ptr<audio::AudioBuffer> master(
    audio::ChannelLayout layout,std::int64_t frames)
{
    const auto hz=core::SampleRate::create(48000);
    if(!hz)return {};
    const auto format=audio::AudioFormat::create(*hz.value(),layout);
    const auto count=core::FrameCount::create(frames);
    if(!format||!count)return {};
    auto buffer=audio::AudioBuffer::create(
        *format.value(),audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{0},*count.value());
    if(!buffer)return {};
    auto result=std::make_shared<audio::AudioBuffer>(
        std::move(*buffer.value()));
    auto view=result->mutable_view();
    for(std::size_t ch=0;ch<view.format().channel_count();++ch){
        auto plane=view.channel(ch);
        Q_ASSERT(plane);
        for(std::size_t i=0;i<plane.value()->size();++i){
            (*plane.value())[i]=(i%2U==0U?0.4:-0.4)
                *(ch==0U?1.0:0.75);
        }
    }
    return result;
}

[[nodiscard]] wav_support::Bytes render(
    audio::AudioBufferView original,
    audio::WavSampleFormat encoding,
    std::uint64_t seed)
{
    using namespace audio::internal;
    ByteCollector sink;
    const auto channels=static_cast<std::uint32_t>(
        original.format().channel_count());
    const auto frames=static_cast<std::uint64_t>(
        original.frame_count().value());
    const M18WavSpec spec{
        encoding==audio::WavSampleFormat::PCM_S24
            ?M18WavEncoding::PCM24:M18WavEncoding::IEEE_FLOAT32,
        static_cast<std::uint32_t>(
            original.format().sample_rate().value()),
        channels,frames,seed};
    M18StreamingWavWriter writer(sink,spec);
    if(writer.start()!=M18StreamStatus::OK)return {};
    std::vector<double> interleaved(
        static_cast<std::size_t>(frames)*channels);
    for(std::size_t ch=0;ch<channels;++ch){
        const auto p=original.channel(ch);
        Q_ASSERT(p);
        for(std::size_t i=0;i<frames;++i)
            interleaved[i*channels+ch]=(*p.value())[i];
    }
    if(writer.append(interleaved)!=M18StreamStatus::OK)return {};
    const auto finished=writer.finish();
    if(finished.status!=M18StreamStatus::OK ||!finished.receipt)return {};
    wav_support::Bytes bytes;
    bytes.reserve(sink.bytes.size());
    for(const auto b:sink.bytes)bytes.push_back(static_cast<std::byte>(b));
    return bytes;
}

[[nodiscard]] core::Result<std::unique_ptr<audio::WavReader>>
open(wav_support::Bytes bytes,
     const std::shared_ptr<wav_support::ReaderControl>& control)
{
    return audio::WavReader::open(
        wav_support::memory_reader(std::move(bytes),control));
}
} // namespace

class M18CandidateDeliveryValidationTest final : public QObject{
    Q_OBJECT
private slots:
    void exactPcm24ReadbackAndCeilingEvidence();
    void float32AndWrongSeedFailClosed();
    void tamperedPayloadAndWrongMasterRejected();
};

void M18CandidateDeliveryValidationTest::
exactPcm24ReadbackAndCeilingEvidence()
{
    using platform::windows::internal::validate_m18_candidate_decoded_delivery;
    const auto original=master(audio::ChannelLayout::STEREO_LR,4105);
    QVERIFY(original);
    const auto bytes=render(original->view(),audio::WavSampleFormat::PCM_S24,
        UINT64_C(0xFEDCBA9876543210));
    auto control=std::make_shared<wav_support::ReaderControl>();
    auto wav=open(bytes,control);
    QVERIFY(wav);
    const auto evidence=validate_m18_candidate_decoded_delivery(
        *wav.value()->get(),original->view(),
        audio::WavSampleFormat::PCM_S24,
        UINT64_C(0xFEDCBA9876543210),0.0);
    QVERIFY(evidence);
    QCOMPARE(evidence.value()->exact_frames,std::uint64_t{4105});
    QCOMPARE(evidence.value()->encoded_format,audio::WavSampleFormat::PCM_S24);
    QCOMPARE(evidence.value()->canonical_decoded_sha256.size(),std::size_t{64});
    QCOMPARE(evidence.value()->decoded_true_peak.programme_frames,
        std::uint64_t{4105});
    QCOMPARE(evidence.value()->decoded_true_peak.qualification_status,
        analysis::TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED);
    QVERIFY(evidence.value()->meets_ceiling);
    QVERIFY(control->readCalls>1U);
    auto again=open(bytes,std::make_shared<wav_support::ReaderControl>());
    QVERIFY(again);
    const auto nonconforming=validate_m18_candidate_decoded_delivery(
        *again.value()->get(),original->view(),
        audio::WavSampleFormat::PCM_S24,
        UINT64_C(0xFEDCBA9876543210),-12.0);
    QVERIFY(nonconforming);
    QVERIFY(!nonconforming.value()->meets_ceiling);
    QVERIFY(nonconforming.value()->decoded_true_peak.global_db_tp.has_value());
}

void M18CandidateDeliveryValidationTest::float32AndWrongSeedFailClosed()
{
    using platform::windows::internal::validate_m18_candidate_decoded_delivery;
    const auto original=master(audio::ChannelLayout::MONO_C,85);
    QVERIFY(original);
    auto bytes=render(original->view(),audio::WavSampleFormat::IEEE_F32,0U);
    auto control=std::make_shared<wav_support::ReaderControl>();
    auto wav=open(bytes,control);
    QVERIFY(wav);
    const auto valid=validate_m18_candidate_decoded_delivery(
        *wav.value()->get(),original->view(),
        audio::WavSampleFormat::IEEE_F32,std::nullopt,0.0);
    QVERIFY(valid);
    QVERIFY(valid.value()->meets_ceiling);
    auto other=open(bytes,std::make_shared<wav_support::ReaderControl>());
    QVERIFY(other);
    QVERIFY(!validate_m18_candidate_decoded_delivery(
        *other.value()->get(),original->view(),
        audio::WavSampleFormat::IEEE_F32,17U,0.0));
    QVERIFY(!validate_m18_candidate_decoded_delivery(
        *other.value()->get(),original->view(),
        audio::WavSampleFormat::IEEE_F32,std::nullopt,-13.0));
}

void M18CandidateDeliveryValidationTest::
tamperedPayloadAndWrongMasterRejected()
{
    using platform::windows::internal::validate_m18_candidate_decoded_delivery;
    const auto original=master(audio::ChannelLayout::STEREO_LR,4105);
    QVERIFY(original);
    constexpr auto seed=UINT64_C(0x123456789ABCDEF0);
    auto bytes=render(original->view(),audio::WavSampleFormat::PCM_S24,seed);
    const auto header=audio::internal::m18_plan_wav({
        audio::internal::M18WavEncoding::PCM24,48000U,2U,4105U,seed});
    QCOMPARE(header.status,audio::internal::M18WavStatus::OK);
    const auto tamper=static_cast<std::size_t>(header.plan.header_bytes)
        +(4101U*2U+1U)*3U;
    QVERIFY(tamper<bytes.size());
    bytes[tamper]^=std::byte{0x01};
    auto wav=open(bytes,std::make_shared<wav_support::ReaderControl>());
    QVERIFY(wav);
    const auto checked=validate_m18_candidate_decoded_delivery(
        *wav.value()->get(),original->view(),
        audio::WavSampleFormat::PCM_S24,seed,0.0);
    QVERIFY(!checked);
    QCOMPARE(checked.error()->code(),core::ErrorCode::IoFailure);
    const auto clean=render(original->view(),
        audio::WavSampleFormat::PCM_S24,seed);
    auto again=open(clean,std::make_shared<wav_support::ReaderControl>());
    QVERIFY(again);
    const auto wrong=master(audio::ChannelLayout::MONO_C,4105);
    QVERIFY(wrong);
    const auto rejected=validate_m18_candidate_decoded_delivery(
        *again.value()->get(),wrong->view(),
        audio::WavSampleFormat::PCM_S24,seed,0.0);
    QVERIFY(!rejected);
    QCOMPARE(rejected.error()->code(),
        core::ErrorCode::MalformedAudioContainer);
}
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M18CandidateDeliveryValidationTest)
#include "test_m18_candidate_delivery_validation.moc"
