// M18 post-M17 owner bridge: genuine OUTPUT_RATE AudioBuffer, no mock DSP.
// Does not establish WAV export, session revalidation or Windows product FQ.
#include "internal/m18_owned_master_frame_provider.hpp"
#include <QtTest/QTest>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::tests {
namespace {
[[nodiscard]] std::shared_ptr<audio::AudioBuffer> make_master(
    std::int64_t hz,audio::ChannelLayout layout,
    audio::FrameDomainId domain,std::int64_t start,std::int64_t frames)
{
    const auto rate=core::SampleRate::create(hz);
    if (!rate)return {};
    const auto format=audio::AudioFormat::create(*rate.value(),layout);
    const auto length=core::FrameCount::create(frames);
    if (!format ||!length)return {};
    auto buffer=audio::AudioBuffer::create(
        *format.value(),domain,core::FrameIndex{start},*length.value());
    if (!buffer)return {};
    return std::make_shared<audio::AudioBuffer>(std::move(*buffer.value()));
}
void fill_master(audio::AudioBuffer& buffer)
{
    auto view=buffer.mutable_view();
    const auto n=static_cast<std::size_t>(view.frame_count().value());
    for (std::size_t c=0;c<view.format().channel_count();++c) {
        auto p=view.channel(c);
        Q_ASSERT(p &&p.value()!=nullptr);
        for (std::size_t i=0;i<n;++i) {
            (*p.value())[i]=(static_cast<double>(i%113U)-56.0)
                /128.0 +static_cast<double>(c)/16.0;
        }
        if (n>=3) {
            (*p.value())[0]=-0.0;
            (*p.value())[1]=std::numeric_limits<double>::denorm_min();
            (*p.value())[2]=-std::numeric_limits<double>::denorm_min();
        }
    }
}
} // namespace

class M18OwnedMasterFrameProviderTest final : public QObject {
    Q_OBJECT
private slots:
    void sixRatesBothLayoutsAndBits();
    void contiguousVsRaggedAndLifetime();
    void invalidDomainOriginRateAndEmpty();
    void rejectedReadNeverTouchesDestination();
};

void M18OwnedMasterFrameProviderTest::sixRatesBothLayoutsAndBits()
{
    using audio::internal::M18OwnedMasterFrameProvider;
    for (const auto hz : {44100,48000,88200,96000,176400,192000}) {
        const auto rate=core::SampleRate::create(hz);
        QVERIFY(rate);
        for (const auto layout :
             {audio::ChannelLayout::MONO_C,audio::ChannelLayout::STEREO_LR}) {
            auto owner=make_master(hz,layout,
                                  audio::FrameDomainId::OUTPUT_RATE,0,17);
            QVERIFY(owner);
            fill_master(*owner);
            const auto source=owner->view();
            const auto channels=static_cast<std::uint32_t>(
                source.format().channel_count());
            auto captured=M18OwnedMasterFrameProvider::capture(
                owner,*rate.value());
            QVERIFY(captured);
            QCOMPARE(captured->frame_count(),std::uint64_t{17});
            QCOMPARE(captured->channel_count(),channels);
            QCOMPARE(captured->master_rate(),*rate.value());
            std::vector<double> interleaved(17U*channels,42.0);
            QVERIFY(captured->read_exact(0U,17U,channels,interleaved));
            for (std::size_t c=0;c<channels;++c) {
                const auto plane=source.channel(c);
                QVERIFY(plane &&plane.value());
                for (std::size_t i=0;i<17U;++i) {
                    QCOMPARE(std::bit_cast<std::uint64_t>(
                                 interleaved[i*channels+c]),
                             std::bit_cast<std::uint64_t>(
                                 (*plane.value())[i]));
                }
            }
        }
    }
}

void M18OwnedMasterFrameProviderTest::contiguousVsRaggedAndLifetime()
{
    using audio::internal::M18OwnedMasterFrameProvider;
    auto owner=make_master(48000,audio::ChannelLayout::STEREO_LR,
                           audio::FrameDomainId::OUTPUT_RATE,0,4105);
    QVERIFY(owner);
    fill_master(*owner);
    const auto rate=core::SampleRate::create(48000);
    QVERIFY(rate);
    auto captured=M18OwnedMasterFrameProvider::capture(owner,*rate.value());
    QVERIFY(captured);
    owner.reset();
    std::vector<double> whole(4105U*2U);
    QVERIFY(captured->read_exact(0U,4105U,2U,whole));
    std::vector<double> ragged(4105U*2U);
    QVERIFY(captured->read_exact(0U,1U,2U,std::span{ragged}.subspan(0,2)));
    QVERIFY(captured->read_exact(
        1U,4096U,2U,std::span{ragged}.subspan(2,8192)));
    QVERIFY(captured->read_exact(
        4097U,8U,2U,std::span{ragged}.subspan(8194,16)));
    for (std::size_t i=0;i<whole.size();++i)
        QCOMPARE(std::bit_cast<std::uint64_t>(whole[i]),
                 std::bit_cast<std::uint64_t>(ragged[i]));
}

void M18OwnedMasterFrameProviderTest::invalidDomainOriginRateAndEmpty()
{
    using audio::internal::M18OwnedMasterFrameProvider;
    const auto rate=core::SampleRate::create(48000);
    const auto wrongRate=core::SampleRate::create(44100);
    const auto unsupportedRate=core::SampleRate::create(32000);
    QVERIFY(rate &&wrongRate &&unsupportedRate);
    auto source=make_master(48000,audio::ChannelLayout::MONO_C,
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,0,4);
    QVERIFY(source);
    QVERIFY(!M18OwnedMasterFrameProvider::capture(source,*rate.value()));
    auto wrongOrigin=make_master(48000,audio::ChannelLayout::MONO_C,
        audio::FrameDomainId::OUTPUT_RATE,1,4);
    QVERIFY(wrongOrigin);
    QVERIFY(!M18OwnedMasterFrameProvider::capture(wrongOrigin,*rate.value()));
    auto empty=make_master(48000,audio::ChannelLayout::MONO_C,
        audio::FrameDomainId::OUTPUT_RATE,0,0);
    QVERIFY(empty);
    QVERIFY(!M18OwnedMasterFrameProvider::capture(empty,*rate.value()));
    auto valid=make_master(48000,audio::ChannelLayout::MONO_C,
        audio::FrameDomainId::OUTPUT_RATE,0,4);
    QVERIFY(valid);
    QVERIFY(!M18OwnedMasterFrameProvider::capture(valid,*wrongRate.value()));
    QVERIFY(!M18OwnedMasterFrameProvider::capture(valid,*unsupportedRate.value()));
    QVERIFY(!M18OwnedMasterFrameProvider::capture({},*rate.value()));
}

void M18OwnedMasterFrameProviderTest::rejectedReadNeverTouchesDestination()
{
    using audio::internal::M18OwnedMasterFrameProvider;
    const auto rate=core::SampleRate::create(48000);
    QVERIFY(rate);
    auto owner=make_master(48000,audio::ChannelLayout::STEREO_LR,
        audio::FrameDomainId::OUTPUT_RATE,0,4);
    QVERIFY(owner);
    auto captured=M18OwnedMasterFrameProvider::capture(owner,*rate.value());
    QVERIFY(captured);
    std::array<double,8> destination{};
    destination.fill(1234.0);
    QVERIFY(!captured->read_exact(0U,0U,2U,destination));
    QVERIFY(!captured->read_exact(0U,4U,1U,destination));
    QVERIFY(!captured->read_exact(0U,3U,2U,destination));
    QVERIFY(!captured->read_exact(3U,2U,2U,destination));
    QVERIFY(!captured->read_exact(4U,1U,2U,destination));
    QVERIFY(!captured->read_exact(
        std::numeric_limits<std::uint64_t>::max(),1U,2U,destination));
    for (const auto sample : destination)QCOMPARE(sample,1234.0);
}

} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M18OwnedMasterFrameProviderTest)
#include "test_m18_owned_master_frame_provider.moc"
