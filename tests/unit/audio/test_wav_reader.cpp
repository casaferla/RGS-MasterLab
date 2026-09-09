#include "wav_test_support.hpp"

#include "../../audio_golden/wav/expected_bits.hpp"
#include "../../audio_golden/wav/golden_vectors.hpp"

#include <QtTest/QTest>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace rgsml::audio;
using namespace rgsml::core;
using namespace rgsml::tests::wav_support;

static_assert(std::is_move_constructible_v<WavReader>);
static_assert(std::is_move_assignable_v<WavReader>);
static_assert(!std::is_copy_constructible_v<WavReader>);
static_assert(!std::is_copy_assignable_v<WavReader>);

[[nodiscard]] std::vector<std::uint64_t> decode_mono_bits(const Bytes& bytes)
{
    auto control = std::make_shared<ReaderControl>();
    auto opened = WavReader::open(memory_reader(bytes, control));
    Q_ASSERT(opened.value() != nullptr);
    auto& reader = **opened.value();
    auto destination = make_destination(
        reader.info(), 0, reader.info().frame_count().value());
    Q_ASSERT(destination.value() != nullptr);
    auto decoded = reader.read_frames(FrameIndex{0}, destination.value()->mutable_view());
    Q_ASSERT(decoded.value() != nullptr);
    return channel_bits(*destination.value(), 0U);
}

void expect_open_error(Bytes bytes, ErrorCode expected)
{
    auto control = std::make_shared<ReaderControl>();
    auto opened = WavReader::open(memory_reader(std::move(bytes), control));
    QVERIFY(opened.error() != nullptr);
    QCOMPARE(opened.error()->code(), expected);
}

[[nodiscard]] Bytes standard_chunks(
    const Bytes& format,
    const Bytes& payload,
    int formatCopies = 1,
    int dataCopies = 1)
{
    Bytes chunks;
    for (int index = 0; index < formatCopies; ++index) {
        append_chunk(chunks, fourcc("fmt "), format);
    }
    for (int index = 0; index < dataCopies; ++index) {
        append_chunk(chunks, fourcc("data"), payload);
    }
    return chunks;
}

class WavReaderTest final : public QObject {
    Q_OBJECT

private slots:
    void goldenAnchorsAndContracts();
    void supportedContainerMatrix();
    void pcmGoldenVectors();
    void ieeeFloatIntegrity();
    void chunkRandomAccessAndFailureAtomicity();
    void malformedAndUnsupportedMatrix();
    void boundedResourceAndLifecycleContract();
};

void WavReaderTest::goldenAnchorsAndContracts()
{
    const auto riffBytes = from_u8_array(audio_golden::kRiffPcm16Mono);
    auto riffControl = std::make_shared<ReaderControl>();
    auto riffOpened = WavReader::open(memory_reader(riffBytes, riffControl));
    QVERIFY(riffOpened.value() != nullptr);
    auto& riff = **riffOpened.value();
    QCOMPARE(riff.info().container_kind(), WavContainerKind::RIFF);
    QCOMPARE(riff.info().encoded_sample_format(), WavSampleFormat::PCM_S16);
    QCOMPARE(riff.info().audio_format().channel_layout(), ChannelLayout::MONO_C);
    QCOMPARE(riff.info().audio_format().sample_rate().value(), std::int64_t{44100});
    QCOMPARE(riff.info().frame_count().value(), std::int64_t{3});
    QCOMPARE(riff.info().block_align_bytes(), std::uint16_t{2});
    QCOMPARE(riff.info().data_offset_bytes(), std::uint64_t{44});
    QCOMPARE(riff.info().data_size_bytes(), std::uint64_t{6});
    QVERIFY(!riff.info().is_extensible());
    QVERIFY(riff.info().decoder_contract_version() == kWavDecoderContractVersion);
    QVERIFY(riff.info().source_conversion_version() == kSourcePcmConversionVersion);

    auto riffDestination = make_destination(riff.info(), 0, 3);
    QVERIFY(riffDestination.value() != nullptr);
    auto riffDecoded = riff.read_frames(
        FrameIndex{0}, riffDestination.value()->mutable_view());
    QVERIFY(riffDecoded.value() != nullptr);
    QCOMPARE(riffDecoded.value()->value(), std::int64_t{3});
    QCOMPARE(
        channel_bits(*riffDestination.value(), 0U),
        std::vector<std::uint64_t>(
            audio_golden::kRiffPcm16MonoExpectedBits.begin(),
            audio_golden::kRiffPcm16MonoExpectedBits.end()));

    const auto rf64Bytes = from_u8_array(audio_golden::kRf64F64NegativeZero);
    auto rf64Control = std::make_shared<ReaderControl>();
    auto rf64Opened = WavReader::open(memory_reader(rf64Bytes, rf64Control));
    QVERIFY(rf64Opened.value() != nullptr);
    auto& rf64 = **rf64Opened.value();
    QCOMPARE(rf64.info().container_kind(), WavContainerKind::RF64);
    QCOMPARE(rf64.info().encoded_sample_format(), WavSampleFormat::IEEE_F64);
    QCOMPARE(rf64.info().frame_count().value(), std::int64_t{1});
    auto rf64Destination = make_destination(rf64.info(), 0, 1);
    QVERIFY(rf64Destination.value() != nullptr);
    auto rf64Decoded = rf64.read_frames(
        FrameIndex{0}, rf64Destination.value()->mutable_view());
    QVERIFY(rf64Decoded.value() != nullptr);
    QCOMPARE(
        channel_bits(*rf64Destination.value(), 0U),
        std::vector<std::uint64_t>(
            audio_golden::kRf64F64ExpectedBits.begin(),
            audio_golden::kRf64F64ExpectedBits.end()));

    constexpr std::array<std::pair<ErrorCode, std::string_view>, 5> additions{{
        {ErrorCode::UnsupportedAudioEncoding, "unsupported_audio_encoding"},
        {ErrorCode::UnsupportedAudioLayout, "unsupported_audio_layout"},
        {ErrorCode::InvalidAudioSample, "invalid_audio_sample"},
        {ErrorCode::MalformedAudioContainer, "malformed_audio_container"},
        {ErrorCode::TruncatedAudioData, "truncated_audio_data"},
    }};
    for (const auto& [code, token] : additions) {
        QCOMPARE(error_code_token(code), token);
        auto parsed = parse_error_code(token);
        QVERIFY(parsed.value() != nullptr);
        QCOMPARE(*parsed.value(), code);
    }
}

void WavReaderTest::supportedContainerMatrix()
{
    struct MatrixCase final {
        std::uint16_t tag;
        std::uint16_t bits;
        std::uint16_t channels;
        std::uint32_t rate;
        WavSampleFormat encoded;
        ChannelLayout layout;
        bool extensible;
        bool rf64;
        bool unknownChunks;
    };
    constexpr std::array<MatrixCase, 10> cases{{
        {1U, 16U, 1U, 44100U, WavSampleFormat::PCM_S16, ChannelLayout::MONO_C, false, false, false},
        {1U, 24U, 2U, 48000U, WavSampleFormat::PCM_S24, ChannelLayout::STEREO_LR, false, false, false},
        {1U, 32U, 2U, 96000U, WavSampleFormat::PCM_S32, ChannelLayout::STEREO_LR, false, false, false},
        {3U, 32U, 1U, 192000U, WavSampleFormat::IEEE_F32, ChannelLayout::MONO_C, false, false, false},
        {3U, 32U, 2U, 48000U, WavSampleFormat::IEEE_F32, ChannelLayout::STEREO_LR, false, false, false},
        {3U, 64U, 1U, 96000U, WavSampleFormat::IEEE_F64, ChannelLayout::MONO_C, false, false, false},
        {3U, 64U, 2U, 192000U, WavSampleFormat::IEEE_F64, ChannelLayout::STEREO_LR, false, false, false},
        {1U, 24U, 2U, 44100U, WavSampleFormat::PCM_S24, ChannelLayout::STEREO_LR, true, false, false},
        {3U, 32U, 1U, 48000U, WavSampleFormat::IEEE_F32, ChannelLayout::MONO_C, true, false, false},
        {3U, 64U, 1U, 48000U, WavSampleFormat::IEEE_F64, ChannelLayout::MONO_C, false, true, true},
    }};
    const Bytes evenUnknown{std::byte{1}, std::byte{2}};
    const Bytes oddUnknown{std::byte{3}, std::byte{4}, std::byte{5}};

    for (const auto& item : cases) {
        const auto bytesPerSample = static_cast<std::size_t>(item.bits / 8U);
        const Bytes payload(bytesPerSample * item.channels, std::byte{0});
        const auto bytes = make_wav(
            item.tag,
            item.bits,
            item.channels,
            item.rate,
            payload,
            item.extensible,
            item.rf64,
            item.unknownChunks ? std::span<const std::byte>{evenUnknown} : std::span<const std::byte>{},
            item.unknownChunks ? std::span<const std::byte>{oddUnknown} : std::span<const std::byte>{});
        auto control = std::make_shared<ReaderControl>();
        auto opened = WavReader::open(memory_reader(bytes, control));
        QVERIFY(opened.value() != nullptr);
        const auto& info = (*opened.value())->info();
        QCOMPARE(info.container_kind(), item.rf64 ? WavContainerKind::RF64 : WavContainerKind::RIFF);
        QCOMPARE(info.encoded_sample_format(), item.encoded);
        QCOMPARE(info.audio_format().sample_rate().value(), static_cast<std::int64_t>(item.rate));
        QCOMPARE(info.audio_format().channel_layout(), item.layout);
        QCOMPARE(info.audio_format().channel_count(), static_cast<std::size_t>(item.channels));
        QCOMPARE(info.frame_count().value(), std::int64_t{1});
        QCOMPARE(
            info.block_align_bytes(),
            static_cast<std::uint16_t>(bytesPerSample * item.channels));
        QCOMPARE(info.data_size_bytes(), static_cast<std::uint64_t>(payload.size()));
        QCOMPARE(info.is_extensible(), item.extensible);
        QCOMPARE(info.valid_bits_per_sample(), item.bits);
        QCOMPARE(
            info.channel_mask(),
            item.channels == 1U ? std::uint32_t{0x4U} : std::uint32_t{0x3U});

        auto destination = make_destination(info, 0, 1);
        QVERIFY(destination.value() != nullptr);
        auto decoded = (*opened.value())->read_frames(
            FrameIndex{0}, destination.value()->mutable_view());
        QVERIFY(decoded.value() != nullptr);
        for (std::size_t channel = 0; channel < item.channels; ++channel) {
            QCOMPARE(channel_bits(*destination.value(), channel).front(), std::uint64_t{0});
        }
    }
}

void WavReaderTest::pcmGoldenVectors()
{
    constexpr std::array<std::uint16_t, 3> widths{16U, 24U, 32U};
    for (const auto bits : widths) {
        const auto scale = std::int64_t{1} << (bits - 1U);
        const std::array<std::int64_t, 9> codes{
            -scale,
            -scale + 1,
            -scale / 2,
            -1,
            0,
            1,
            scale / 2,
            scale - 2,
            scale - 1,
        };
        const auto payload = pcm_payload(codes, bits);
        const auto bytes = make_wav(1U, bits, 1U, 48000U, payload);
        const auto actual = decode_mono_bits(bytes);
        QCOMPARE(actual.size(), codes.size());
        for (std::size_t index = 0; index < codes.size(); ++index) {
            const auto expected = std::ldexp(
                static_cast<double>(codes[index]),
                -static_cast<int>(bits - 1U));
            QCOMPARE(actual[index], std::bit_cast<std::uint64_t>(expected));
        }
        QCOMPARE(actual.front(), std::bit_cast<std::uint64_t>(-1.0));
        QVERIFY(std::bit_cast<double>(actual.back()) < 1.0);
    }
}

void WavReaderTest::ieeeFloatIntegrity()
{
    constexpr std::array<std::uint32_t, 14> f32Patterns{
        0x00000000U,
        0x80000000U,
        0x00000001U,
        0x80000001U,
        0x00800000U,
        0x80800000U,
        0x3f000000U,
        0xbf000000U,
        0x3f800000U,
        0xbf800000U,
        0x40200000U,
        0xc0200000U,
        0x7f7fffffU,
        0xff7fffffU,
    };
    const auto f32Bytes = make_wav(3U, 32U, 1U, 48000U, f32_payload(f32Patterns));
    const auto actualF32 = decode_mono_bits(f32Bytes);
    QCOMPARE(actualF32.size(), f32Patterns.size());
    for (std::size_t index = 0; index < f32Patterns.size(); ++index) {
        const auto expected = static_cast<double>(
            std::bit_cast<float>(f32Patterns[index]));
        QCOMPARE(actualF32[index], std::bit_cast<std::uint64_t>(expected));
    }
    QCOMPARE(actualF32[1], std::uint64_t{0x8000000000000000ULL});

    constexpr std::array<std::uint64_t, 14> f64Patterns{
        0x0000000000000000ULL,
        0x8000000000000000ULL,
        0x0000000000000001ULL,
        0x8000000000000001ULL,
        0x0010000000000000ULL,
        0x8010000000000000ULL,
        0x3fe0000000000000ULL,
        0xbfe0000000000000ULL,
        0x3ff0000000000000ULL,
        0xbff0000000000000ULL,
        0x4004000000000000ULL,
        0xc004000000000000ULL,
        0x7fefffffffffffffULL,
        0xffefffffffffffffULL,
    };
    const auto f64Bytes = make_wav(3U, 64U, 1U, 96000U, f64_payload(f64Patterns));
    QCOMPARE(decode_mono_bits(f64Bytes), std::vector<std::uint64_t>(
        f64Patterns.begin(), f64Patterns.end()));

    constexpr std::array<std::uint32_t, 4> badF32{
        0x7fc00001U, 0x7f800001U, 0x7f800000U, 0xff800000U};
    for (const auto pattern : badF32) {
        const std::array<std::uint32_t, 2> samples{0x3e800000U, pattern};
        const auto bytes = make_wav(3U, 32U, 1U, 48000U, f32_payload(samples));
        auto control = std::make_shared<ReaderControl>();
        auto opened = WavReader::open(memory_reader(bytes, control));
        QVERIFY(opened.value() != nullptr);
        auto destination = make_destination((*opened.value())->info(), 0, 2);
        QVERIFY(destination.value() != nullptr);
        auto plane = destination.value()->mutable_view().channel(0U);
        QVERIFY(plane.value() != nullptr);
        std::fill(plane.value()->begin(), plane.value()->end(), -17.25);
        const auto sentinel = std::bit_cast<std::uint64_t>(-17.25);
        auto decoded = (*opened.value())->read_frames(
            FrameIndex{0}, destination.value()->mutable_view());
        QVERIFY(decoded.error() != nullptr);
        QCOMPARE(decoded.error()->code(), ErrorCode::InvalidAudioSample);
        QCOMPARE(channel_bits(*destination.value(), 0U), std::vector<std::uint64_t>(2U, sentinel));
    }

    constexpr std::array<std::uint64_t, 4> badF64{
        0x7ff8000000000001ULL,
        0x7ff0000000000001ULL,
        0x7ff0000000000000ULL,
        0xfff0000000000000ULL,
    };
    for (const auto pattern : badF64) {
        const std::array<std::uint64_t, 2> samples{
            0x3fd0000000000000ULL, pattern};
        const auto bytes = make_wav(3U, 64U, 1U, 48000U, f64_payload(samples));
        auto control = std::make_shared<ReaderControl>();
        auto opened = WavReader::open(memory_reader(bytes, control));
        QVERIFY(opened.value() != nullptr);
        auto destination = make_destination((*opened.value())->info(), 0, 2);
        QVERIFY(destination.value() != nullptr);
        auto plane = destination.value()->mutable_view().channel(0U);
        QVERIFY(plane.value() != nullptr);
        std::fill(plane.value()->begin(), plane.value()->end(), 19.5);
        const auto sentinel = std::bit_cast<std::uint64_t>(19.5);
        auto decoded = (*opened.value())->read_frames(
            FrameIndex{0}, destination.value()->mutable_view());
        QVERIFY(decoded.error() != nullptr);
        QCOMPARE(decoded.error()->code(), ErrorCode::InvalidAudioSample);
        QCOMPARE(channel_bits(*destination.value(), 0U), std::vector<std::uint64_t>(2U, sentinel));
    }
}

void WavReaderTest::chunkRandomAccessAndFailureAtomicity()
{
    std::vector<std::int64_t> interleaved;
    for (std::int64_t frame = 0; frame < 17; ++frame) {
        interleaved.push_back(-4000000 + frame * 123457);
        interleaved.push_back(3000000 - frame * 76543);
    }
    const auto bytes = make_wav(
        1U, 24U, 2U, 48000U, pcm_payload(interleaved, 24U));

    auto fullControl = std::make_shared<ReaderControl>();
    auto fullOpened = WavReader::open(memory_reader(bytes, fullControl));
    QVERIFY(fullOpened.value() != nullptr);
    auto fullBuffer = make_destination((*fullOpened.value())->info(), 0, 17);
    QVERIFY(fullBuffer.value() != nullptr);
    auto fullResult = (*fullOpened.value())->read_frames(
        FrameIndex{0}, fullBuffer.value()->mutable_view());
    QVERIFY(fullResult.value() != nullptr);
    const auto fullLeft = channel_bits(*fullBuffer.value(), 0U);
    const auto fullRight = channel_bits(*fullBuffer.value(), 1U);
    for (std::size_t frame = 0; frame < 17U; ++frame) {
        const auto expectedLeft = std::ldexp(
            static_cast<double>(interleaved[frame * 2U]), -23);
        const auto expectedRight = std::ldexp(
            static_cast<double>(interleaved[frame * 2U + 1U]), -23);
        QCOMPARE(fullLeft[frame], std::bit_cast<std::uint64_t>(expectedLeft));
        QCOMPARE(fullRight[frame], std::bit_cast<std::uint64_t>(expectedRight));
    }

    auto chunkControl = std::make_shared<ReaderControl>();
    auto chunkOpened = WavReader::open(memory_reader(bytes, chunkControl));
    QVERIFY(chunkOpened.value() != nullptr);
    const std::array<std::int64_t, 5> partitions{1, 2, 3, 7, 4};
    std::vector<std::uint64_t> chunkLeft;
    std::vector<std::uint64_t> chunkRight;
    std::int64_t start = 0;
    for (const auto partition : partitions) {
        auto buffer = make_destination((*chunkOpened.value())->info(), start, partition);
        QVERIFY(buffer.value() != nullptr);
        auto decoded = (*chunkOpened.value())->read_frames(
            FrameIndex{start}, buffer.value()->mutable_view());
        QVERIFY(decoded.value() != nullptr);
        QCOMPARE(decoded.value()->value(), partition);
        const auto left = channel_bits(*buffer.value(), 0U);
        const auto right = channel_bits(*buffer.value(), 1U);
        chunkLeft.insert(chunkLeft.end(), left.begin(), left.end());
        chunkRight.insert(chunkRight.end(), right.begin(), right.end());
        start += partition;
    }
    QCOMPARE(chunkLeft, fullLeft);
    QCOMPARE(chunkRight, fullRight);

    for (const auto randomStart : {9, 0, 14, 3}) {
        auto buffer = make_destination((*chunkOpened.value())->info(), randomStart, 2);
        QVERIFY(buffer.value() != nullptr);
        auto decoded = (*chunkOpened.value())->read_frames(
            FrameIndex{randomStart}, buffer.value()->mutable_view());
        QVERIFY(decoded.value() != nullptr);
        const auto left = channel_bits(*buffer.value(), 0U);
        QCOMPARE(
            left,
            std::vector<std::uint64_t>(
                fullLeft.begin() + randomStart,
                fullLeft.begin() + randomStart + 2));
    }

    for (const auto maximum : {std::size_t{1}, std::size_t{3}, std::size_t{5}}) {
        auto control = std::make_shared<ReaderControl>();
        control->maxTransfer = maximum;
        auto opened = WavReader::open(memory_reader(bytes, control));
        QVERIFY(opened.value() != nullptr);
        auto buffer = make_destination((*opened.value())->info(), 0, 17);
        QVERIFY(buffer.value() != nullptr);
        auto decoded = (*opened.value())->read_frames(
            FrameIndex{0}, buffer.value()->mutable_view());
        QVERIFY(decoded.value() != nullptr);
        QCOMPARE(channel_bits(*buffer.value(), 0U), fullLeft);
        QCOMPARE(channel_bits(*buffer.value(), 1U), fullRight);
    }

    constexpr auto sentinelValue = 123.25;
    const auto sentinelBits = std::bit_cast<std::uint64_t>(sentinelValue);
    auto tail = make_destination((*chunkOpened.value())->info(), 15, 5);
    QVERIFY(tail.value() != nullptr);
    for (std::size_t channel = 0; channel < 2U; ++channel) {
        auto plane = tail.value()->mutable_view().channel(channel);
        QVERIFY(plane.value() != nullptr);
        std::fill(plane.value()->begin(), plane.value()->end(), sentinelValue);
    }
    auto tailResult = (*chunkOpened.value())->read_frames(
        FrameIndex{15}, tail.value()->mutable_view());
    QVERIFY(tailResult.value() != nullptr);
    QCOMPARE(tailResult.value()->value(), std::int64_t{2});
    for (std::size_t channel = 0; channel < 2U; ++channel) {
        const auto bits = channel_bits(*tail.value(), channel);
        QCOMPARE(bits[2], sentinelBits);
        QCOMPARE(bits[3], sentinelBits);
        QCOMPARE(bits[4], sentinelBits);
    }

    auto atEnd = make_destination((*chunkOpened.value())->info(), 17, 1);
    QVERIFY(atEnd.value() != nullptr);
    auto atEndPlane = atEnd.value()->mutable_view().channel(0U);
    QVERIFY(atEndPlane.value() != nullptr);
    (*atEndPlane.value())[0] = sentinelValue;
    auto atEndResult = (*chunkOpened.value())->read_frames(
        FrameIndex{17}, atEnd.value()->mutable_view());
    QVERIFY(atEndResult.value() != nullptr);
    QCOMPARE(atEndResult.value()->value(), std::int64_t{0});
    QCOMPARE(channel_bits(*atEnd.value(), 0U).front(), sentinelBits);

    auto beyondEnd = make_destination((*chunkOpened.value())->info(), 18, 1);
    QVERIFY(beyondEnd.value() != nullptr);
    auto beyondResult = (*chunkOpened.value())->read_frames(
        FrameIndex{18}, beyondEnd.value()->mutable_view());
    QVERIFY(beyondResult.error() != nullptr);
    QCOMPARE(beyondResult.error()->code(), ErrorCode::OutOfRange);

    auto wrongDomain = AudioBuffer::create(
        (*chunkOpened.value())->info().audio_format(),
        FrameDomainId::OUTPUT_RATE,
        FrameIndex{0},
        frame_count(1));
    QVERIFY(wrongDomain.value() != nullptr);
    auto wrongDomainResult = (*chunkOpened.value())->read_frames(
        FrameIndex{0}, wrongDomain.value()->mutable_view());
    QVERIFY(wrongDomainResult.error() != nullptr);
    QCOMPARE(wrongDomainResult.error()->code(), ErrorCode::InvalidArgument);

    auto failureControl = std::make_shared<ReaderControl>();
    failureControl->maxTransfer = 3U;
    auto failureOpened = WavReader::open(memory_reader(bytes, failureControl));
    QVERIFY(failureOpened.value() != nullptr);
    failureControl->failAtOffset = (*failureOpened.value())->info().data_offset_bytes() + 3U;
    auto failureBuffer = make_destination((*failureOpened.value())->info(), 0, 4);
    QVERIFY(failureBuffer.value() != nullptr);
    for (std::size_t channel = 0; channel < 2U; ++channel) {
        auto plane = failureBuffer.value()->mutable_view().channel(channel);
        QVERIFY(plane.value() != nullptr);
        std::fill(plane.value()->begin(), plane.value()->end(), sentinelValue);
    }
    auto failureResult = (*failureOpened.value())->read_frames(
        FrameIndex{0}, failureBuffer.value()->mutable_view());
    QVERIFY(failureResult.error() != nullptr);
    QCOMPARE(failureResult.error()->code(), ErrorCode::IoFailure);
    QCOMPARE(channel_bits(*failureBuffer.value(), 0U), std::vector<std::uint64_t>(4U, sentinelBits));
    QCOMPARE(channel_bits(*failureBuffer.value(), 1U), std::vector<std::uint64_t>(4U, sentinelBits));
}

void WavReaderTest::malformedAndUnsupportedMatrix()
{
    const auto fmt16 = make_fmt_payload(1U, 1U, 48000U, 16U);
    const auto pcm16 = pcm_payload(std::array<std::int64_t, 1>{0}, 16U);
    const auto valid = make_wav(1U, 16U, 1U, 48000U, pcm16);

    expect_open_error(Bytes(4U, std::byte{0}), ErrorCode::TruncatedAudioData);

    auto rifx = valid;
    rifx[0] = std::byte{'R'};
    rifx[1] = std::byte{'I'};
    rifx[2] = std::byte{'F'};
    rifx[3] = std::byte{'X'};
    expect_open_error(std::move(rifx), ErrorCode::UnsupportedAudioEncoding);

    auto invalidForm = valid;
    invalidForm[8] = std::byte{'N'};
    expect_open_error(std::move(invalidForm), ErrorCode::MalformedAudioContainer);

    expect_open_error(
        wrap_riff(standard_chunks(fmt16, pcm16, 0, 1)),
        ErrorCode::MalformedAudioContainer);
    expect_open_error(
        wrap_riff(standard_chunks(fmt16, pcm16, 2, 1)),
        ErrorCode::MalformedAudioContainer);
    expect_open_error(
        wrap_riff(standard_chunks(fmt16, pcm16, 1, 0)),
        ErrorCode::MalformedAudioContainer);
    expect_open_error(
        wrap_riff(standard_chunks(fmt16, pcm16, 1, 2)),
        ErrorCode::MalformedAudioContainer);

    Bytes missingPadChunks;
    append_chunk(missingPadChunks, fourcc("fmt "), fmt16);
    const Bytes odd{std::byte{1}, std::byte{2}, std::byte{3}};
    append_chunk(missingPadChunks, fourcc("JUNK"), odd, false);
    append_chunk(missingPadChunks, fourcc("data"), pcm16);
    expect_open_error(
        wrap_riff(missingPadChunks), ErrorCode::MalformedAudioContainer);

    auto crossingForm = valid;
    write_u32(crossingForm, 16U, 1000U);
    expect_open_error(std::move(crossingForm), ErrorCode::MalformedAudioContainer);

    auto truncatedResource = valid;
    truncatedResource.pop_back();
    expect_open_error(std::move(truncatedResource), ErrorCode::TruncatedAudioData);

    Bytes overflowRf64;
    append_fourcc(overflowRf64, fourcc("RF64"));
    append_u32(overflowRf64, std::numeric_limits<std::uint32_t>::max());
    append_fourcc(overflowRf64, fourcc("WAVE"));
    Bytes overflowDs64;
    append_u64(overflowDs64, std::numeric_limits<std::uint64_t>::max());
    append_u64(overflowDs64, 0U);
    append_u64(overflowDs64, 0U);
    append_u32(overflowDs64, 0U);
    append_chunk(overflowRf64, fourcc("ds64"), overflowDs64);
    expect_open_error(std::move(overflowRf64), ErrorCode::IntegerOverflow);

    auto invalidBlockAlign = valid;
    write_u16(invalidBlockAlign, 32U, 3U);
    expect_open_error(
        std::move(invalidBlockAlign), ErrorCode::MalformedAudioContainer);

    auto invalidByteRate = valid;
    write_u32(invalidByteRate, 28U, 1U);
    expect_open_error(
        std::move(invalidByteRate), ErrorCode::MalformedAudioContainer);

    const Bytes partialStereoPayload{std::byte{0}, std::byte{0}, std::byte{0}};
    expect_open_error(
        make_wav(1U, 16U, 2U, 48000U, partialStereoPayload),
        ErrorCode::MalformedAudioContainer);
    expect_open_error(
        make_wav(1U, 8U, 1U, 48000U, Bytes{std::byte{0}}),
        ErrorCode::UnsupportedAudioEncoding);
    expect_open_error(
        make_wav(6U, 16U, 1U, 48000U, pcm16),
        ErrorCode::UnsupportedAudioEncoding);
    expect_open_error(
        make_wav(1U, 16U, 3U, 48000U, Bytes(6U, std::byte{0})),
        ErrorCode::UnsupportedAudioLayout);

    auto packedBits = make_wav(1U, 24U, 1U, 48000U, Bytes(3U, std::byte{0}), true);
    write_u16(packedBits, 38U, 20U);
    expect_open_error(std::move(packedBits), ErrorCode::UnsupportedAudioEncoding);

    auto badMask = make_wav(1U, 16U, 2U, 48000U, Bytes(4U, std::byte{0}), true);
    write_u32(badMask, 40U, 0x4U);
    expect_open_error(std::move(badMask), ErrorCode::UnsupportedAudioLayout);

    auto badGuid = make_wav(1U, 16U, 1U, 48000U, Bytes(2U, std::byte{0}), true);
    badGuid[44U] = std::byte{0x7f};
    expect_open_error(std::move(badGuid), ErrorCode::UnsupportedAudioEncoding);

    auto badCbSize = make_wav(1U, 16U, 1U, 48000U, Bytes(2U, std::byte{0}), true);
    write_u16(badCbSize, 36U, 21U);
    expect_open_error(std::move(badCbSize), ErrorCode::MalformedAudioContainer);

    auto badWaveFormatExSize = make_wav(
        3U, 32U, 1U, 48000U, f32_payload(std::array<std::uint32_t, 1>{0U}));
    write_u16(badWaveFormatExSize, 36U, 1U);
    expect_open_error(
        std::move(badWaveFormatExSize), ErrorCode::MalformedAudioContainer);

    const auto validRf64 = make_wav(1U, 16U, 1U, 48000U, pcm16, false, true);
    auto missingDs64 = valid;
    missingDs64[0U] = std::byte{'R'};
    missingDs64[1U] = std::byte{'F'};
    missingDs64[2U] = std::byte{'6'};
    missingDs64[3U] = std::byte{'4'};
    write_u32(missingDs64, 4U, std::numeric_limits<std::uint32_t>::max());
    expect_open_error(std::move(missingDs64), ErrorCode::MalformedAudioContainer);

    auto badDs64Length = validRf64;
    write_u32(badDs64Length, 16U, 27U);
    expect_open_error(std::move(badDs64Length), ErrorCode::MalformedAudioContainer);

    auto badRf64Sentinel = validRf64;
    write_u32(badRf64Sentinel, 4U, 80U);
    expect_open_error(std::move(badRf64Sentinel), ErrorCode::MalformedAudioContainer);

    auto badDataSentinel = validRf64;
    write_u32(badDataSentinel, 76U, 2U);
    expect_open_error(std::move(badDataSentinel), ErrorCode::MalformedAudioContainer);

    auto inconsistentSampleCount = validRf64;
    write_u64(inconsistentSampleCount, 36U, 2U);
    expect_open_error(
        std::move(inconsistentSampleCount), ErrorCode::MalformedAudioContainer);

    Bytes duplicateDs64Chunk;
    append_chunk(duplicateDs64Chunk, fourcc("ds64"), Bytes(28U, std::byte{0}));
    auto duplicateDs64 = validRf64;
    duplicateDs64.insert(
        duplicateDs64.begin() + 48,
        duplicateDs64Chunk.begin(),
        duplicateDs64Chunk.end());
    write_u64(duplicateDs64, 20U, static_cast<std::uint64_t>(duplicateDs64.size() - 8U));
    expect_open_error(std::move(duplicateDs64), ErrorCode::MalformedAudioContainer);
}

void WavReaderTest::boundedResourceAndLifecycleContract()
{
    const auto payload = pcm_payload(
        std::vector<std::int64_t>(2048U, 7), 16U);
    const auto bytes = make_wav(1U, 16U, 1U, 48000U, payload);
    auto boundedControl = std::make_shared<ReaderControl>();
    auto boundedOpened = WavReader::open(memory_reader(bytes, boundedControl));
    QVERIFY(boundedOpened.value() != nullptr);
    QVERIFY(boundedControl->totalBytesRead < 128U);
    QVERIFY(boundedControl->largestReadRequest <= 16U);
    QVERIFY(boundedControl->seekCalls >= 3U);

    auto noSeekControl = std::make_shared<ReaderControl>();
    auto noSeek = WavReader::open(memory_reader(bytes, noSeekControl, false));
    QVERIFY(noSeek.error() != nullptr);
    QCOMPARE(noSeek.error()->code(), ErrorCode::UnsupportedOperation);

    auto unknownSizeControl = std::make_shared<ReaderControl>();
    auto unknownSize = WavReader::open(
        memory_reader(bytes, unknownSizeControl, true, false));
    QVERIFY(unknownSize.value() != nullptr);

    auto noReadControl = std::make_shared<ReaderControl>();
    auto noRead = WavReader::open(
        memory_reader(bytes, noReadControl, true, true, false));
    QVERIFY(noRead.error() != nullptr);
    QCOMPARE(noRead.error()->code(), ErrorCode::AccessDenied);
    QCOMPARE(noReadControl->totalBytesRead, std::size_t{0});

    std::unique_ptr<IResourceReader> nullReader;
    auto nullOpen = WavReader::open(std::move(nullReader));
    QVERIFY(nullOpen.error() != nullptr);
    QCOMPARE(nullOpen.error()->code(), ErrorCode::InvalidArgument);

    auto lifecycleControl = std::make_shared<ReaderControl>();
    auto lifecycle = WavReader::open(memory_reader(bytes, lifecycleControl));
    QVERIFY(lifecycle.value() != nullptr);
    QVERIFY((*lifecycle.value())->close());
    QVERIFY((*lifecycle.value())->close());
    QCOMPARE(lifecycleControl->closeCalls, std::size_t{1});
    auto afterCloseBuffer = make_destination((*lifecycle.value())->info(), 0, 1);
    QVERIFY(afterCloseBuffer.value() != nullptr);
    auto afterClose = (*lifecycle.value())->read_frames(
        FrameIndex{0}, afterCloseBuffer.value()->mutable_view());
    QVERIFY(afterClose.error() != nullptr);
    QCOMPARE(afterClose.error()->code(), ErrorCode::InvalidState);

    constexpr std::uint32_t largeUnknownSize = 16U * 1024U * 1024U;
    Bytes prefix;
    append_fourcc(prefix, fourcc("RIFF"));
    append_u32(prefix, 0U);
    append_fourcc(prefix, fourcc("WAVE"));
    append_fourcc(prefix, fourcc("JUNK"));
    append_u32(prefix, largeUnknownSize);
    Bytes suffix;
    const auto fmt = make_fmt_payload(1U, 1U, 48000U, 16U);
    append_chunk(suffix, fourcc("fmt "), fmt);
    append_chunk(suffix, fourcc("data"), pcm_payload(std::array<std::int64_t, 1>{1}, 16U));
    const auto suffixOffset = static_cast<std::uint64_t>(prefix.size()) + largeUnknownSize;
    const auto logicalSize = suffixOffset + suffix.size();
    write_u32(prefix, 4U, static_cast<std::uint32_t>(logicalSize - 8U));
    auto sparseControl = std::make_shared<ReaderControl>();
    std::vector<SparseSegment> segments;
    segments.push_back(SparseSegment{0U, prefix});
    segments.push_back(SparseSegment{suffixOffset, suffix});
    auto sparse = WavReader::open(std::make_unique<SparseReader>(
        logicalSize, std::move(segments), sparseControl));
    QVERIFY(sparse.value() != nullptr);
    QCOMPARE((*sparse.value())->info().frame_count().value(), std::int64_t{1});
    QVERIFY(sparseControl->totalBytesRead < 128U);
    QVERIFY(sparseControl->seekCalls >= 4U);
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::WavReaderTest)

#include "test_wav_reader.moc"
