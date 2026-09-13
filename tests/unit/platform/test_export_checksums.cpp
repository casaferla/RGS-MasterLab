#include "internal/sha256.hpp"

#include <rgsml/audio/audio_buffer.hpp>

#include <QtTest/QTest>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>

namespace rgsml::tests {
namespace {

using namespace rgsml;
using platform::windows::internal::canonical_decoded_audio_sha256;
using platform::windows::internal::Sha256;
using platform::windows::internal::sha256_hex;

[[nodiscard]] audio::AudioBuffer make_buffer(
    audio::ChannelLayout layout,
    std::int64_t rate,
    std::span<const double> first,
    std::span<const double> second = {})
{
    auto sampleRate = core::SampleRate::create(rate);
    Q_ASSERT(sampleRate);
    auto format = audio::AudioFormat::create(*sampleRate.value(), layout);
    Q_ASSERT(format);
    auto count = core::FrameCount::create(static_cast<std::int64_t>(first.size()));
    Q_ASSERT(count);
    auto buffer = audio::AudioBuffer::create(
        *format.value(), audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0}, *count.value());
    Q_ASSERT(buffer);
    auto firstPlane = buffer.value()->mutable_view().channel(0U);
    Q_ASSERT(firstPlane);
    std::copy(first.begin(), first.end(), firstPlane.value()->begin());
    if (layout == audio::ChannelLayout::STEREO_LR) {
        Q_ASSERT(second.size() == first.size());
        auto secondPlane = buffer.value()->mutable_view().channel(1U);
        Q_ASSERT(secondPlane);
        std::copy(second.begin(), second.end(), secondPlane.value()->begin());
    }
    return std::move(*buffer.value());
}

[[nodiscard]] std::array<std::byte, 8> little_endian(double value)
{
    const auto bits = std::bit_cast<std::uint64_t>(value);
    std::array<std::byte, 8> bytes{};
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::byte>((bits >> (index * 8U)) & 0xffU);
    }
    return bytes;
}

class ExportChecksumTest final : public QObject {
    Q_OBJECT

private slots:
    void sha256KnownAnswers();
    void canonicalMonoAndStereoKnownAnswers();
    void signedZeroPoliciesDiffer();
    void subnormalPreservedAndNonFiniteRejected();
};

void ExportChecksumTest::sha256KnownAnswers()
{
    const std::span<const std::byte> empty{};
    QCOMPARE(
        sha256_hex(empty),
        std::string{"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"});
    constexpr std::array<std::byte, 3> abc{
        std::byte{'a'}, std::byte{'b'}, std::byte{'c'}};
    QCOMPARE(
        sha256_hex(abc),
        std::string{"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"});
    constexpr std::array<std::byte, 48> fortyEightZeros{};
    QCOMPARE(
        sha256_hex(fortyEightZeros),
        std::string{"17b0761f87b081d5cf10757ccc89f12be355c70e2e29df288b65b30710dcbcd1"});
    Sha256 incremental;
    incremental.update(std::span<const std::byte>{fortyEightZeros}.first(40));
    incremental.update(std::span<const std::byte>{fortyEightZeros}.subspan(40));
    QCOMPARE(
        sha256_hex(incremental.finalize()),
        std::string{"17b0761f87b081d5cf10757ccc89f12be355c70e2e29df288b65b30710dcbcd1"});
}

void ExportChecksumTest::canonicalMonoAndStereoKnownAnswers()
{
    constexpr std::array<double, 1> mono{0.0};
    auto monoBuffer = make_buffer(audio::ChannelLayout::MONO_C, 48000, mono);
    auto monoHash = canonical_decoded_audio_sha256(monoBuffer.view());
    QVERIFY(monoHash);
    QCOMPARE(
        *monoHash.value(),
        std::string{"8fe558df3b0761b8f549b63c7659a2d25db155f3de5c2d75c2f1ae41213722ee"});

    constexpr std::array<double, 2> left{0.5, 1.0};
    constexpr std::array<double, 2> right{-0.5, -1.0};
    auto stereoBuffer = make_buffer(
        audio::ChannelLayout::STEREO_LR, 44100, left, right);
    auto stereoHash = canonical_decoded_audio_sha256(stereoBuffer.view());
    QVERIFY(stereoHash);
    QCOMPARE(
        *stereoHash.value(),
        std::string{"6f9fa864eb5a59aded978b0394d0e5d9cc0ff018533f88b6e19036d57e836ef3"});
}

void ExportChecksumTest::signedZeroPoliciesDiffer()
{
    const auto positiveBytes = little_endian(0.0);
    const auto negativeBytes = little_endian(-0.0);
    QVERIFY(sha256_hex(positiveBytes) != sha256_hex(negativeBytes));

    const std::array<double, 1> positive{0.0};
    const std::array<double, 1> negative{-0.0};
    auto positiveBuffer = make_buffer(audio::ChannelLayout::MONO_C, 48000, positive);
    auto negativeBuffer = make_buffer(audio::ChannelLayout::MONO_C, 48000, negative);
    auto positiveHash = canonical_decoded_audio_sha256(positiveBuffer.view());
    auto negativeHash = canonical_decoded_audio_sha256(negativeBuffer.view());
    QVERIFY(positiveHash);
    QVERIFY(negativeHash);
    QCOMPARE(*positiveHash.value(), *negativeHash.value());
}

void ExportChecksumTest::subnormalPreservedAndNonFiniteRejected()
{
    const std::array<double, 1> subnormal{
        std::bit_cast<double>(std::uint64_t{1})};
    auto subnormalBuffer = make_buffer(
        audio::ChannelLayout::MONO_C, 48000, subnormal);
    auto subnormalHash = canonical_decoded_audio_sha256(subnormalBuffer.view());
    QVERIFY(subnormalHash);
    QCOMPARE(
        *subnormalHash.value(),
        std::string{"149851f5ce3acd3d0e9332fa0bad0bb25a8d71f4f05982e88dbe991563705173"});

    for (const double invalid : {
             std::numeric_limits<double>::quiet_NaN(),
             std::numeric_limits<double>::infinity(),
             -std::numeric_limits<double>::infinity()}) {
        const std::array<double, 1> values{invalid};
        auto invalidBuffer = make_buffer(
            audio::ChannelLayout::MONO_C, 48000, values);
        auto hash = canonical_decoded_audio_sha256(invalidBuffer.view());
        QVERIFY(hash.error() != nullptr);
        QCOMPARE(hash.error()->code(), core::ErrorCode::InvalidAudioSample);
    }
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::ExportChecksumTest)
#include "test_export_checksums.moc"
