// L1-M18 opt-in canonical WavWriter: frozen Float64 path remains independent.
// M18 still lacks Windows transaction activation and full delivered TP/FQ.
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/wav_writer.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/resource_io.hpp>
#include "internal/m18_wav_candidate.hpp"
#include <QtTest/QTest>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::tests {
namespace {
using Bytes=std::vector<std::byte>;
using rgsml::audio::WavWriteSpec;
using rgsml::audio::WavWriter;
using rgsml::audio::WavSampleFormat;
using rgsml::audio::FrameDomainId;
using rgsml::audio::ChannelLayout;

struct State {
    Bytes bytes;
    std::uint64_t pos{0};
    std::size_t calls{0};
    std::size_t max_transfer{std::numeric_limits<std::size_t>::max()};
    bool zero_progress{false};
    bool closed{false};
    bool flushed{false};
};

[[nodiscard]] core::ResourceReference make_reference()
{
    auto r=core::ResourceReference::create(
        "test.memory","m18-canonical-wav",false,true,
        "M18 canonical writer in-memory test");
    Q_ASSERT(r);
    return *r.value();
}

class MemoryWriter final : public core::IResourceWriter {
public:
    explicit MemoryWriter(std::shared_ptr<State> state)
        : reference_(make_reference()),s_(std::move(state)) {}
    [[nodiscard]] const core::ResourceReference& reference() const noexcept override
    {return reference_;}
    [[nodiscard]] core::ResourceCapabilities capabilities() const noexcept override
    {return core::ResourceCapabilities::create(true,true,true,true);}
    [[nodiscard]] core::Result<std::uint64_t> position_bytes() const override
    {return core::Result<std::uint64_t>::success(s_->pos);}
    [[nodiscard]] core::Result<std::size_t> write(
        std::span<const std::byte> bytes) override
    {
        ++s_->calls;
        if(s_->zero_progress &&!bytes.empty())
            return core::Result<std::size_t>::success(0U);
        const auto count=std::min(bytes.size(),s_->max_transfer);
        if(s_->pos>static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())-count)
            return core::Result<std::size_t>::failure(
                core::Error{core::ErrorCode::IntegerOverflow,"size overflow"});
        const auto end=static_cast<std::size_t>(s_->pos)+count;
        if(end>1000000U)
            return core::Result<std::size_t>::failure(
                core::Error{core::ErrorCode::IoFailure,"test memory limit"});
        if(s_->bytes.size()<end)s_->bytes.resize(end);
        std::copy_n(bytes.begin(),count,
            s_->bytes.begin()+static_cast<std::size_t>(s_->pos));
        s_->pos=end;
        return core::Result<std::size_t>::success(count);
    }
    [[nodiscard]] core::Status seek_bytes(std::uint64_t n) override
    {s_->pos=n;return core::Status::success();}
    [[nodiscard]] core::Status resize_bytes(std::uint64_t n) override
    {
        if(n<=1000000U)s_->bytes.resize(static_cast<std::size_t>(n));
        if(s_->pos>n)s_->pos=n;
        return core::Status::success();
    }
    [[nodiscard]] core::Status flush() override
    {s_->flushed=true;return core::Status::success();}
    [[nodiscard]] core::Status close() override
    {s_->closed=true;return core::Status::success();}
private:
    core::ResourceReference reference_;
    std::shared_ptr<State> s_;
};

[[nodiscard]] std::shared_ptr<audio::AudioBuffer> make_master(
    std::int64_t rate,ChannelLayout layout,std::int64_t frames,
    FrameDomainId domain=FrameDomainId::OUTPUT_RATE,
    std::int64_t first=0)
{
    const auto fs=core::SampleRate::create(rate);
    if(!fs)return {};
    const auto format=audio::AudioFormat::create(*fs.value(),layout);
    const auto count=core::FrameCount::create(frames);
    if(!format ||!count)return {};
    auto buffer=audio::AudioBuffer::create(
        *format.value(),domain,core::FrameIndex{first},*count.value());
    if(!buffer)return {};
    return std::make_shared<audio::AudioBuffer>(
        std::move(*buffer.value()));
}

void populate(audio::AudioBuffer& buffer)
{
    auto view=buffer.mutable_view();
    for(std::size_t c=0;c<view.format().channel_count();++c){
        auto channel=view.channel(c);
        Q_ASSERT(channel);
        for(std::size_t i=0;i<channel.value()->size();++i){
            const auto n=static_cast<std::int64_t>(i%7U)-3;
            (*channel.value())[i]=static_cast<double>(n)/8.0;
        }
        if(channel.value()->size()>=2U){
            (*channel.value())[0]=-0.0;
            (*channel.value())[1]=std::numeric_limits<double>::denorm_min();
        }
    }
}

[[nodiscard]] Bytes render(
    audio::AudioBufferView view,WavSampleFormat encoding,
    std::optional<std::uint64_t> seed,std::span<const std::int64_t> parts={},
    std::size_t maxWrite=std::numeric_limits<std::size_t>::max())
{
    auto s=std::make_shared<State>();
    s->max_transfer=maxWrite;
    const WavWriteSpec spec{view.format(),view.frame_count(),encoding};
    auto opened=WavWriter::open_m18(
        std::make_unique<MemoryWriter>(s),spec,seed);
    Q_ASSERT(opened);
    auto& writer=*opened.value()->get();
    if(parts.empty()){
        auto status=writer.write_frames(view);
        Q_ASSERT(status);
    } else {
        std::int64_t offset=0;
        for(const auto n:parts){
            auto count=core::FrameCount::create(n);
            Q_ASSERT(count);
            auto part=view.subview(core::FrameIndex{offset},*count.value());
            Q_ASSERT(part);
            auto written=writer.write_frames(*part.value());
            Q_ASSERT(written);
            offset+=n;
        }
        Q_ASSERT(offset==view.frame_count().value());
    }
    auto done=writer.finalize();Q_ASSERT(done);
    auto closed=writer.close();Q_ASSERT(closed);
    Q_ASSERT(s->flushed &&s->closed);
    return s->bytes;
}
} // namespace

class M18CanonicalWavWriterTest final : public QObject {
    Q_OBJECT
private slots:
    void allSixRatesBothLayoutsAndFormats();
    void partitionIndependentAbsoluteFrameDither();
    void optInAndSeedContractsFailClosed();
    void wrongDomainAndOverloadsNeverFinalize();
    void exactRf64HeaderPlanWithoutMultiGigabyteAllocation();
};

void M18CanonicalWavWriterTest::allSixRatesBothLayoutsAndFormats()
{
    for(const auto rate:{44100,48000,88200,96000,176400,192000}){
        for(const auto layout:{ChannelLayout::MONO_C,ChannelLayout::STEREO_LR}){
            auto pcm=make_master(rate,layout,7);
            QVERIFY(pcm);
            populate(*pcm);
            const auto view=pcm->view();
            const auto channels=static_cast<std::uint32_t>(
                view.format().channel_count());
            for(const auto encoding:
                {WavSampleFormat::PCM_S24,WavSampleFormat::IEEE_F32}){
                const bool isPcm=encoding==WavSampleFormat::PCM_S24;
                const auto seed=isPcm
                    ?std::optional<std::uint64_t>{UINT64_C(0xFEDCBA9876543210)}
                    :std::nullopt;
                const auto data=render(view,encoding,seed,{},2U);
                const audio::internal::M18WavSpec spec{
                    isPcm?audio::internal::M18WavEncoding::PCM24
                         :audio::internal::M18WavEncoding::IEEE_FLOAT32,
                    static_cast<std::uint32_t>(rate),
                    channels,7U,seed.value_or(0U)};
                const auto plan=audio::internal::m18_plan_wav(spec);
                QCOMPARE(plan.status,audio::internal::M18WavStatus::OK);
                QCOMPARE(data.size(),
                    static_cast<std::size_t>(plan.plan.file_bytes));
                const auto header=audio::internal::m18_wav_header(plan.plan);
                QCOMPARE(header.size(),
                    static_cast<std::size_t>(plan.plan.header_bytes));
                for(std::size_t i=0;i<header.size();++i)
                    QCOMPARE(std::to_integer<std::uint8_t>(data[i]),header[i]);
                std::vector<double> interleaved(7U*channels);
                for(std::size_t c=0;c<channels;++c){
                    const auto plane=view.channel(c);
                    QVERIFY(plane);
                    for(std::size_t i=0;i<7U;++i)
                        interleaved[i*channels+c]=(*plane.value())[i];
                }
                const auto bps=isPcm?3U:4U;
                std::vector<std::uint8_t> expected(interleaved.size()*bps);
                const auto status=isPcm
                    ?audio::internal::m18_pcm24_block(
                        interleaved,channels,0U,*seed,expected)
                    :audio::internal::m18_float32_block(
                        interleaved,channels,0U,expected);
                QCOMPARE(status,audio::internal::M18CodecStatus::OK);
                for(std::size_t i=0;i<expected.size();++i)
                    QCOMPARE(std::to_integer<std::uint8_t>(
                        data[header.size()+i]),expected[i]);
                // RIFF data-chunk length is the logical PCM byte count;
                // an ODD mono PCM24 payload requires one physical ZERO
                // after the chunk and that pad is NOT an audio sample.
                const auto padding=static_cast<std::size_t>(
                    plan.plan.data_bytes&UINT64_C(1));
                QCOMPARE(data.size(),header.size()+expected.size()+padding);
                if(padding!=0U)
                    QCOMPARE(std::to_integer<std::uint8_t>(data.back()),
                             std::uint8_t{0});
            }
        }
    }
}

void M18CanonicalWavWriterTest::partitionIndependentAbsoluteFrameDither()
{
    auto pcm=make_master(48000,ChannelLayout::STEREO_LR,4101);
    QVERIFY(pcm);
    populate(*pcm);
    const auto view=pcm->view();
    constexpr std::array<std::int64_t,4> parts{{1,4000,96,4}};
    const auto one=render(view,WavSampleFormat::PCM_S24,123456789U);
    const auto many=render(view,WavSampleFormat::PCM_S24,123456789U,parts,5U);
    QVERIFY(one==many);
    const auto different=render(view,WavSampleFormat::PCM_S24,987654321U);
    QVERIFY(different!=one);
    const auto floating=render(view,WavSampleFormat::IEEE_F32,std::nullopt,
                               parts,13U);
    const auto floatingOne=render(view,WavSampleFormat::IEEE_F32,std::nullopt);
    QVERIFY(floatingOne==floating);
}

void M18CanonicalWavWriterTest::optInAndSeedContractsFailClosed()
{
    const auto pcm=make_master(48000,ChannelLayout::MONO_C,3);
    QVERIFY(pcm);
    const WavWriteSpec pcmSpec{
        pcm->view().format(),pcm->view().frame_count(),WavSampleFormat::PCM_S24};
    const WavWriteSpec floatSpec{
        pcm->view().format(),pcm->view().frame_count(),WavSampleFormat::IEEE_F32};
    QVERIFY(!WavWriter::open(std::make_unique<MemoryWriter>(
        std::make_shared<State>()),pcmSpec));
    QVERIFY(!WavWriter::open(std::make_unique<MemoryWriter>(
        std::make_shared<State>()),floatSpec));
    const auto absent=WavWriter::open_m18(
        std::make_unique<MemoryWriter>(std::make_shared<State>()),
        pcmSpec,std::nullopt);
    QVERIFY(!absent);
    QCOMPARE(absent.error()->code(),core::ErrorCode::InvalidArgument);
    const auto wrong=WavWriter::open_m18(
        std::make_unique<MemoryWriter>(std::make_shared<State>()),
        floatSpec,123U);
    QVERIFY(!wrong);
    QCOMPARE(wrong.error()->code(),core::ErrorCode::InvalidArgument);
    const auto unsupported=WavWriter::open_m18(
        std::make_unique<MemoryWriter>(std::make_shared<State>()),
        WavWriteSpec{pcm->view().format(),pcm->view().frame_count(),
                     WavSampleFormat::IEEE_F64},std::nullopt);
    QVERIFY(!unsupported);
    QCOMPARE(unsupported.error()->code(),
             core::ErrorCode::UnsupportedAudioEncoding);
}

void M18CanonicalWavWriterTest::wrongDomainAndOverloadsNeverFinalize()
{
    auto wrongDomain=make_master(48000,ChannelLayout::MONO_C,2,
        FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(wrongDomain);
    const WavWriteSpec spec{wrongDomain->view().format(),
        wrongDomain->view().frame_count(),WavSampleFormat::PCM_S24};
    auto s=std::make_shared<State>();
    auto writer=WavWriter::open_m18(
        std::make_unique<MemoryWriter>(s),spec,0U);
    QVERIFY(writer);
    auto bad=writer.value()->get()->write_frames(wrongDomain->view());
    QVERIFY(!bad);
    QCOMPARE(bad.error()->code(),core::ErrorCode::InvalidFrameRange);
    QVERIFY(!writer.value()->get()->finalize());
    auto wrongStart=make_master(48000,ChannelLayout::MONO_C,2,
        FrameDomainId::OUTPUT_RATE,1);
    QVERIFY(wrongStart);
    auto another=WavWriter::open_m18(
        std::make_unique<MemoryWriter>(std::make_shared<State>()),
        spec,0U);
    QVERIFY(another);
    QVERIFY(!another.value()->get()->write_frames(wrongStart->view()));
    auto overload=make_master(48000,ChannelLayout::MONO_C,2);
    QVERIFY(overload);
    auto plane=overload->mutable_view().channel(0);
    QVERIFY(plane);
    (*plane.value())[0]=0.0;
    (*plane.value())[1]=2.0;
    auto bytes=std::make_shared<State>();
    auto rejecting=WavWriter::open_m18(
        std::make_unique<MemoryWriter>(bytes),spec,42U);
    QVERIFY(rejecting);
    auto status=rejecting.value()->get()->write_frames(overload->view());
    QVERIFY(!status);
    QCOMPARE(status.error()->code(),core::ErrorCode::InvalidAudioSample);
    QVERIFY(!rejecting.value()->get()->finalize());
    auto unsupported=make_master(32000,ChannelLayout::MONO_C,2);
    QVERIFY(unsupported);
    const WavWriteSpec invalidRate{
        unsupported->view().format(),unsupported->view().frame_count(),
        WavSampleFormat::PCM_S24};
    auto noRate=WavWriter::open_m18(
        std::make_unique<MemoryWriter>(std::make_shared<State>()),
        invalidRate,23U);
    QVERIFY(!noRate);
}

void M18CanonicalWavWriterTest::exactRf64HeaderPlanWithoutMultiGigabyteAllocation()
{
    const auto fs=core::SampleRate::create(192000);
    QVERIFY(fs);
    const auto fmt=audio::AudioFormat::create(
        *fs.value(),ChannelLayout::STEREO_LR);
    QVERIFY(fmt);
    for(const auto encoding:
        {WavSampleFormat::PCM_S24,WavSampleFormat::IEEE_F32}){
        const bool pcm=encoding==WavSampleFormat::PCM_S24;
        const std::uint64_t block=pcm?6U:8U;
        const std::uint64_t overhead=pcm?44U:58U;
        const std::uint64_t maxRiffFrames=
            (UINT64_C(0xFFFFFFFE)-(overhead-8U))/block;
        const auto count=core::FrameCount::create(
            static_cast<std::int64_t>(maxRiffFrames+1U));
        QVERIFY(count);
        auto control=std::make_shared<State>();
        auto opened=WavWriter::open_m18(
            std::make_unique<MemoryWriter>(control),
            WavWriteSpec{*fmt.value(),*count.value(),encoding},
            pcm?std::optional<std::uint64_t>{42U}:std::nullopt);
        QVERIFY(opened);
        QCOMPARE(opened.value()->get()->container_kind(),
            audio::WavContainerKind::RF64);
        const audio::internal::M18WavSpec spec{
            pcm?audio::internal::M18WavEncoding::PCM24
               :audio::internal::M18WavEncoding::IEEE_FLOAT32,
            192000U,2U,maxRiffFrames+1U,42U};
        const auto plan=audio::internal::m18_plan_wav(spec);
        QCOMPARE(plan.status,audio::internal::M18WavStatus::OK);
        QCOMPARE(opened.value()->get()->expected_file_size_bytes(),
                 plan.plan.file_bytes);
        QCOMPARE(control->bytes.size(),
                 static_cast<std::size_t>(plan.plan.header_bytes));
        QCOMPARE(std::to_integer<char>(control->bytes[0]),'R');
        QCOMPARE(std::to_integer<char>(control->bytes[1]),'F');
        // Header-only test; no giant file is written.
    }
}
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M18CanonicalWavWriterTest)
#include "test_m18_canonical_wav_writer.moc"
