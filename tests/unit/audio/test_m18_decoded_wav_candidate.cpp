// L1-M18: actual canonical writer -> canonical WavReader -> exact
// decoded postquant evidence in bounded chunks. No Windows file publication.
#include "internal/m18_decoded_wav_candidate.hpp"
#include "wav_test_support.hpp"
#include <rgsml/audio/wav_writer.hpp>
#include <rgsml/core/resource_io.hpp>
#include <QtTest/QTest>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::tests {
namespace {
struct Storage {
    wav_support::Bytes bytes;
    std::uint64_t position{0};
    bool flushed{false};
    bool closed{false};
};

[[nodiscard]] core::ResourceReference writable_reference()
{
    const auto r=core::ResourceReference::create(
        "test.memory","m18-real-wav-roundtrip",false,true,
        "M18 WAV readback fixture");
    Q_ASSERT(r);
    return *r.value();
}

class InMemoryWriter final : public core::IResourceWriter {
public:
    explicit InMemoryWriter(std::shared_ptr<Storage> storage)
        : ref_(writable_reference()),s_(std::move(storage)) {}
    [[nodiscard]] const core::ResourceReference& reference() const noexcept override
    {return ref_;}
    [[nodiscard]] core::ResourceCapabilities capabilities() const noexcept override
    {return core::ResourceCapabilities::create(true,true,true,true);}
    [[nodiscard]] core::Result<std::uint64_t> position_bytes() const override
    {return core::Result<std::uint64_t>::success(s_->position);}
    [[nodiscard]] core::Result<std::size_t> write(
        std::span<const std::byte> bytes) override
    {
        if(s_->position>std::numeric_limits<std::size_t>::max()-bytes.size())
            return core::Result<std::size_t>::failure(
                core::Error{core::ErrorCode::IntegerOverflow,"offset"});
        const auto end=static_cast<std::size_t>(s_->position)+bytes.size();
        if(end>100000U)
            return core::Result<std::size_t>::failure(
                core::Error{core::ErrorCode::OutOfRange,"fixture max"});
        if(s_->bytes.size()<end)s_->bytes.resize(end);
        std::copy(bytes.begin(),bytes.end(),
            s_->bytes.begin()+static_cast<std::size_t>(s_->position));
        s_->position=end;
        return core::Result<std::size_t>::success(bytes.size());
    }
    [[nodiscard]] core::Status seek_bytes(std::uint64_t n) override
    {s_->position=n;return core::Status::success();}
    [[nodiscard]] core::Status resize_bytes(std::uint64_t n) override
    {
        if(n>100000U)return core::Status::failure(
            core::Error{core::ErrorCode::OutOfRange,"fixture max"});
        s_->bytes.resize(static_cast<std::size_t>(n));
        if(s_->position>n)s_->position=n;
        return core::Status::success();
    }
    [[nodiscard]] core::Status flush() override
    {s_->flushed=true;return core::Status::success();}
    [[nodiscard]] core::Status close() override
    {s_->closed=true;return core::Status::success();}
private:
    core::ResourceReference ref_;
    std::shared_ptr<Storage> s_;
};

[[nodiscard]] std::shared_ptr<audio::AudioBuffer> make_master(
    std::int64_t fs,audio::ChannelLayout layout,std::int64_t frames)
{
    const auto rate=core::SampleRate::create(fs);
    if(!rate)return {};
    const auto format=audio::AudioFormat::create(*rate.value(),layout);
    const auto count=core::FrameCount::create(frames);
    if(!format ||!count)return {};
    auto owned=audio::AudioBuffer::create(
        *format.value(),audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{0},*count.value());
    if(!owned)return {};
    auto ptr=std::make_shared<audio::AudioBuffer>(std::move(*owned.value()));
    auto view=ptr->mutable_view();
    for(std::size_t ch=0;ch<view.format().channel_count();++ch){
        auto data=view.channel(ch);
        Q_ASSERT(data);
        for(std::size_t i=0;i<data.value()->size();++i){
            const auto v=static_cast<std::int64_t>(i%17U)-8;
            (*data.value())[i]=static_cast<double>(v)/32.0;
        }
        (*data.value())[0]=-0.0;
        (*data.value())[1]=std::numeric_limits<double>::denorm_min();
    }
    return ptr;
}

[[nodiscard]] std::shared_ptr<Storage> write_m18_wav(
    audio::AudioBufferView master,audio::WavSampleFormat encoding,
    std::optional<std::uint64_t> seed)
{
    const auto state=std::make_shared<Storage>();
    auto opened=audio::WavWriter::open_m18(
        std::make_unique<InMemoryWriter>(state),
        audio::WavWriteSpec{master.format(),master.frame_count(),encoding},seed);
    Q_ASSERT(opened);
    auto status=opened.value()->get()->write_frames(master);
    Q_ASSERT(status);
    auto final=opened.value()->get()->finalize();
    Q_ASSERT(final);
    auto closed=opened.value()->get()->close();
    Q_ASSERT(closed &&state->flushed &&state->closed);
    return state;
}
} // namespace

class M18DecodedWavCandidateTest final : public QObject {
    Q_OBJECT
private slots:
    void realPcm24Over4096FramesAndTamper();
    void pcm24MonoOddAndEvenPayloadReaderRoundtrip();
    void realFloat32StereoSignedZeroAndSeedRejection();
};

void M18DecodedWavCandidateTest::realPcm24Over4096FramesAndTamper()
{
    using namespace audio::internal;
    const auto master=make_master(48000,audio::ChannelLayout::STEREO_LR,4103);
    QVERIFY(master);
    constexpr std::uint64_t seed=UINT64_C(0xFEDCBA9876543210);
    const auto data=write_m18_wav(master->view(),
        audio::WavSampleFormat::PCM_S24,seed);
    QVERIFY(data &&data->bytes.size()>24600U);
    {
        auto control=std::make_shared<wav_support::ReaderControl>();
        auto wav=audio::WavReader::open(
            wav_support::memory_reader(data->bytes,control));
        QVERIFY(wav);
        const auto verified=m18_verify_decoded_wav_candidate(
            *wav.value()->get(),master->view(),
            audio::WavSampleFormat::PCM_S24,seed);
        QVERIFY(verified);
        QCOMPARE(verified.value()->frame_count,std::uint64_t{4103});
        QCOMPARE(verified.value()->encoding,audio::WavSampleFormat::PCM_S24);
        QVERIFY(control->readCalls>1U);
    }
    {
        auto corrupt=data->bytes;
        // A payload mutation beyond the 4096 boundary must be detected,
        // even though metadata remains unchanged.
        const std::size_t byteOffset=44U+(4099U*2U+1U)*3U;
        QVERIFY(corrupt.size()>byteOffset);
        corrupt[byteOffset]^=std::byte{0x01};
        auto control=std::make_shared<wav_support::ReaderControl>();
        auto wav=audio::WavReader::open(
            wav_support::memory_reader(std::move(corrupt),control));
        QVERIFY(wav);
        const auto checked=m18_verify_decoded_wav_candidate(
            *wav.value()->get(),master->view(),
            audio::WavSampleFormat::PCM_S24,seed);
        QVERIFY(!checked);
        QCOMPARE(checked.error()->code(),core::ErrorCode::IoFailure);
    }
}

void M18DecodedWavCandidateTest::realFloat32StereoSignedZeroAndSeedRejection()
{
    using namespace audio::internal;
    const auto master=make_master(192000,audio::ChannelLayout::STEREO_LR,65);
    QVERIFY(master);
    const auto data=write_m18_wav(master->view(),
        audio::WavSampleFormat::IEEE_F32,std::nullopt);
    QVERIFY(data);
    const auto control=std::make_shared<wav_support::ReaderControl>();
    auto wav=audio::WavReader::open(
        wav_support::memory_reader(data->bytes,control));
    QVERIFY(wav);
    const auto accepted=m18_verify_decoded_wav_candidate(
        *wav.value()->get(),master->view(),
        audio::WavSampleFormat::IEEE_F32,std::nullopt);
    QVERIFY(accepted);
    QCOMPARE(accepted.value()->frame_count,std::uint64_t{65});
    const auto invalid=m18_verify_decoded_wav_candidate(
        *wav.value()->get(),master->view(),
        audio::WavSampleFormat::IEEE_F32,UINT64_C(42));
    QVERIFY(!invalid);
    QCOMPARE(invalid.error()->code(),core::ErrorCode::InvalidArgument);
}

void M18DecodedWavCandidateTest::pcm24MonoOddAndEvenPayloadReaderRoundtrip()
{
    using namespace audio::internal;
    for(const std::int64_t frames:{4104,4105}){
        auto original=make_master(48000,audio::ChannelLayout::MONO_C,frames);
        QVERIFY(original);
        constexpr std::uint64_t seed=UINT64_C(0x123456789ABCDEF0);
        const auto wavBytes=write_m18_wav(original->view(),
            audio::WavSampleFormat::PCM_S24,seed);
        QVERIFY(wavBytes);
        const auto plan=m18_plan_wav({
            M18WavEncoding::PCM24,48000U,1U,
            static_cast<std::uint64_t>(frames),seed});
        QCOMPARE(plan.status,M18WavStatus::OK);
        QCOMPARE(plan.plan.data_bytes,static_cast<std::uint64_t>(frames)*3U);
        const std::uint64_t pad=static_cast<std::uint64_t>(frames)&1U;
        QCOMPARE(plan.plan.file_bytes,44U+plan.plan.data_bytes+pad);
        QCOMPARE(wavBytes->bytes.size(),
            static_cast<std::size_t>(plan.plan.file_bytes));
        if(pad!=0U)
            QCOMPARE(std::to_integer<std::uint8_t>(wavBytes->bytes.back()),
                std::uint8_t{0});
        auto readControl=std::make_shared<wav_support::ReaderControl>();
        auto decoded=audio::WavReader::open(
            wav_support::memory_reader(wavBytes->bytes,readControl));
        QVERIFY(decoded);
        const auto check=m18_verify_decoded_wav_candidate(
            *decoded.value()->get(),original->view(),
            audio::WavSampleFormat::PCM_S24,seed);
        QVERIFY(check);
        QCOMPARE(check.value()->frame_count,
            static_cast<std::uint64_t>(frames));
        QCOMPARE(check.value()->data_bytes,plan.plan.data_bytes);
    }
}

} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M18DecodedWavCandidateTest)
#include "test_m18_decoded_wav_candidate.moc"
