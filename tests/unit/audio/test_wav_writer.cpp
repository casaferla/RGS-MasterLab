#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/wav_writer.hpp>
#include <rgsml/core/resource_io.hpp>

#include <QtTest/QTest>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace rgsml::audio;
using namespace rgsml::core;
using Bytes = std::vector<std::byte>;

struct WriterControl final {
    Bytes bytes;
    std::uint64_t logical_size{0};
    std::size_t max_transfer{std::numeric_limits<std::size_t>::max()};
    bool zero_progress{false};
    bool allow_seek{true};
    bool allow_resize{true};
    bool allow_flush{true};
    std::size_t write_calls{0};
    std::size_t flush_calls{0};
    std::size_t close_calls{0};
};

[[nodiscard]] ResourceReference memory_reference()
{
    auto value = ResourceReference::create(
        "test.memory", "wav-writer", false, true, "WAV writer test");
    Q_ASSERT(value.value() != nullptr);
    return *value.value();
}

class MemoryWriter final : public IResourceWriter {
public:
    explicit MemoryWriter(std::shared_ptr<WriterControl> control)
        : reference_(memory_reference())
        , control_(std::move(control))
    {
    }

    [[nodiscard]] const ResourceReference& reference() const noexcept override
    {
        return reference_;
    }

    [[nodiscard]] ResourceCapabilities capabilities() const noexcept override
    {
        return ResourceCapabilities::create(
            control_->allow_seek, true, control_->allow_resize, control_->allow_flush);
    }

    [[nodiscard]] Result<std::uint64_t> position_bytes() const override
    {
        if (closed_) {
            return Result<std::uint64_t>::failure(
                Error{ErrorCode::InvalidState, "closed"});
        }
        return Result<std::uint64_t>::success(position_);
    }

    [[nodiscard]] Result<std::size_t> write(std::span<const std::byte> source) override
    {
        ++control_->write_calls;
        if (closed_) {
            return Result<std::size_t>::failure(
                Error{ErrorCode::InvalidState, "closed"});
        }
        if (source.empty()) {
            return Result<std::size_t>::success(0U);
        }
        if (control_->zero_progress) {
            return Result<std::size_t>::success(0U);
        }
        const auto transfer = std::min(source.size(), control_->max_transfer);
        const auto end = position_ + static_cast<std::uint64_t>(transfer);
        if (end <= 1024U * 1024U) {
            if (control_->bytes.size() < static_cast<std::size_t>(end)) {
                control_->bytes.resize(static_cast<std::size_t>(end));
            }
            std::copy_n(
                source.begin(),
                static_cast<std::ptrdiff_t>(transfer),
                control_->bytes.begin() + static_cast<std::ptrdiff_t>(position_));
        }
        position_ = end;
        control_->logical_size = std::max(control_->logical_size, position_);
        return Result<std::size_t>::success(transfer);
    }

    [[nodiscard]] Status seek_bytes(std::uint64_t absoluteOffset) override
    {
        if (closed_ || !control_->allow_seek) {
            return Status::failure(Error{ErrorCode::UnsupportedOperation, "seek"});
        }
        position_ = absoluteOffset;
        return Status::success();
    }

    [[nodiscard]] Status resize_bytes(std::uint64_t sizeBytes) override
    {
        if (closed_ || !control_->allow_resize) {
            return Status::failure(Error{ErrorCode::UnsupportedOperation, "resize"});
        }
        control_->logical_size = sizeBytes;
        if (sizeBytes <= 1024U * 1024U) {
            control_->bytes.resize(static_cast<std::size_t>(sizeBytes));
        }
        if (position_ > sizeBytes) {
            position_ = sizeBytes;
        }
        return Status::success();
    }

    [[nodiscard]] Status flush() override
    {
        ++control_->flush_calls;
        if (closed_ || !control_->allow_flush) {
            return Status::failure(Error{ErrorCode::UnsupportedOperation, "flush"});
        }
        return Status::success();
    }

    [[nodiscard]] Status close() override
    {
        ++control_->close_calls;
        closed_ = true;
        return Status::success();
    }

private:
    ResourceReference reference_;
    std::shared_ptr<WriterControl> control_;
    std::uint64_t position_{0};
    bool closed_{false};
};

[[nodiscard]] AudioFormat make_format(ChannelLayout layout, std::int64_t rate = 48000)
{
    auto sampleRate = SampleRate::create(rate);
    Q_ASSERT(sampleRate.value() != nullptr);
    auto format = AudioFormat::create(*sampleRate.value(), layout);
    Q_ASSERT(format.value() != nullptr);
    return *format.value();
}

[[nodiscard]] AudioBuffer make_buffer(
    AudioFormat format,
    std::span<const double> first,
    std::span<const double> second = {})
{
    auto count = FrameCount::create(static_cast<std::int64_t>(first.size()));
    Q_ASSERT(count.value() != nullptr);
    auto buffer = AudioBuffer::create(
        format, FrameDomainId::SOURCE_PROCESSING_RATE, FrameIndex{0}, *count.value());
    Q_ASSERT(buffer.value() != nullptr);
    auto mutableView = buffer.value()->mutable_view();
    auto channel0 = mutableView.channel(0U);
    Q_ASSERT(channel0.value() != nullptr);
    std::copy(first.begin(), first.end(), channel0.value()->begin());
    if (format.channel_count() == 2U) {
        Q_ASSERT(second.size() == first.size());
        auto channel1 = mutableView.channel(1U);
        Q_ASSERT(channel1.value() != nullptr);
        std::copy(second.begin(), second.end(), channel1.value()->begin());
    }
    return std::move(*buffer.value());
}

[[nodiscard]] std::uint16_t read_u16(const Bytes& bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1U]) << 8U);
}

[[nodiscard]] std::uint32_t read_u32(const Bytes& bytes, std::size_t offset)
{
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < 4U; ++index) {
        value |= static_cast<std::uint32_t>(bytes[offset + index]) << (index * 8U);
    }
    return value;
}

[[nodiscard]] std::uint64_t read_u64(const Bytes& bytes, std::size_t offset)
{
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < 8U; ++index) {
        value |= static_cast<std::uint64_t>(bytes[offset + index]) << (index * 8U);
    }
    return value;
}

[[nodiscard]] std::string fourcc(const Bytes& bytes, std::size_t offset)
{
    std::string value;
    for (std::size_t index = 0; index < 4U; ++index) {
        value.push_back(static_cast<char>(bytes[offset + index]));
    }
    return value;
}

struct WriteOutcome final {
    Bytes bytes;
    std::size_t write_calls;
};

[[nodiscard]] WriteOutcome write_complete(
    AudioBufferView view,
    std::span<const std::int64_t> partitions = {},
    std::size_t maxTransfer = std::numeric_limits<std::size_t>::max())
{
    auto control = std::make_shared<WriterControl>();
    control->max_transfer = maxTransfer;
    WavWriteSpec spec{view.format(), view.frame_count(), WavSampleFormat::IEEE_F64};
    auto writer = WavWriter::open(std::make_unique<MemoryWriter>(control), spec);
    Q_ASSERT(writer.value() != nullptr);
    if (partitions.empty()) {
        const auto write = (*writer.value())->write_frames(view);
        Q_ASSERT(write.has_value());
    } else {
        std::int64_t offset = 0;
        for (const auto part : partitions) {
            auto count = FrameCount::create(part);
            Q_ASSERT(count.value() != nullptr);
            auto subview = view.subview(
                FrameIndex{view.absolute_start_frame().value() + offset}, *count.value());
            Q_ASSERT(subview.value() != nullptr);
            const auto write = (*writer.value())->write_frames(*subview.value());
            Q_ASSERT(write.has_value());
            offset += part;
        }
        Q_ASSERT(offset == view.frame_count().value());
    }
    const auto finalize = (*writer.value())->finalize();
    Q_ASSERT(finalize.has_value());
    const auto close = (*writer.value())->close();
    Q_ASSERT(close.has_value());
    return WriteOutcome{control->bytes, control->write_calls};
}

class WavWriterTest final : public QObject {
    Q_OBJECT

private slots:
    void exactRiffMonoAndStereo();
    void payloadBitPreservationAndIndependentOracle();
    void nonFiniteRejectedAndFrameAccounting();
    void partitionAndShortWriteSemantics();
    void formatCapabilityAndArithmeticPreflight();
    void riffRf64VirtualBoundaries();
    void exactRf64Ds64AndNoMetadata();
};

void WavWriterTest::exactRiffMonoAndStereo()
{
    const std::array<double, 2> mono{0.5, -1.0};
    auto monoBuffer = make_buffer(make_format(ChannelLayout::MONO_C, 44100), mono);
    const auto monoResult = write_complete(monoBuffer.view());
    QCOMPARE(monoResult.bytes.size(), std::size_t{74});
    QCOMPARE(fourcc(monoResult.bytes, 0), std::string{"RIFF"});
    QCOMPARE(read_u32(monoResult.bytes, 4), std::uint32_t{66});
    QCOMPARE(fourcc(monoResult.bytes, 8), std::string{"WAVE"});
    QCOMPARE(fourcc(monoResult.bytes, 12), std::string{"fmt "});
    QCOMPARE(read_u32(monoResult.bytes, 16), std::uint32_t{18});
    QCOMPARE(read_u16(monoResult.bytes, 20), std::uint16_t{3});
    QCOMPARE(read_u16(monoResult.bytes, 22), std::uint16_t{1});
    QCOMPARE(read_u32(monoResult.bytes, 24), std::uint32_t{44100});
    QCOMPARE(read_u32(monoResult.bytes, 28), std::uint32_t{352800});
    QCOMPARE(read_u16(monoResult.bytes, 32), std::uint16_t{8});
    QCOMPARE(read_u16(monoResult.bytes, 34), std::uint16_t{64});
    QCOMPARE(read_u16(monoResult.bytes, 36), std::uint16_t{0});
    QCOMPARE(fourcc(monoResult.bytes, 38), std::string{"fact"});
    QCOMPARE(read_u32(monoResult.bytes, 42), std::uint32_t{4});
    QCOMPARE(read_u32(monoResult.bytes, 46), std::uint32_t{2});
    QCOMPARE(fourcc(monoResult.bytes, 50), std::string{"data"});
    QCOMPARE(read_u32(monoResult.bytes, 54), std::uint32_t{16});

    const std::array<double, 1> left{0.25};
    const std::array<double, 1> right{-0.75};
    auto stereoBuffer = make_buffer(make_format(ChannelLayout::STEREO_LR), left, right);
    const auto stereoResult = write_complete(stereoBuffer.view());
    QCOMPARE(stereoResult.bytes.size(), std::size_t{74});
    QCOMPARE(read_u16(stereoResult.bytes, 22), std::uint16_t{2});
    QCOMPARE(read_u32(stereoResult.bytes, 28), std::uint32_t{768000});
    QCOMPARE(read_u16(stereoResult.bytes, 32), std::uint16_t{16});
    QCOMPARE(read_u64(stereoResult.bytes, 58), std::bit_cast<std::uint64_t>(left[0]));
    QCOMPARE(read_u64(stereoResult.bytes, 66), std::bit_cast<std::uint64_t>(right[0]));
}

void WavWriterTest::payloadBitPreservationAndIndependentOracle()
{
    const auto positiveSubnormal = std::bit_cast<double>(std::uint64_t{1});
    const auto negativeSubnormal = std::bit_cast<double>(0x8000000000000001ULL);
    const std::array<double, 9> values{
        0.0, -0.0, positiveSubnormal, negativeSubnormal,
        0.5, -0.5, 1.0, -1.0, 2.5};
    auto buffer = make_buffer(make_format(ChannelLayout::MONO_C), values);
    const auto outcome = write_complete(buffer.view());
    for (std::size_t index = 0; index < values.size(); ++index) {
        QCOMPARE(
            read_u64(outcome.bytes, 58U + index * 8U),
            std::bit_cast<std::uint64_t>(values[index]));
    }
}

void WavWriterTest::nonFiniteRejectedAndFrameAccounting()
{
    for (const double invalid : {
             std::numeric_limits<double>::quiet_NaN(),
             std::numeric_limits<double>::infinity(),
             -std::numeric_limits<double>::infinity()}) {
        const std::array<double, 1> values{invalid};
        auto buffer = make_buffer(make_format(ChannelLayout::MONO_C), values);
        auto control = std::make_shared<WriterControl>();
        WavWriteSpec spec{buffer.view().format(), buffer.view().frame_count(), WavSampleFormat::IEEE_F64};
        auto writer = WavWriter::open(std::make_unique<MemoryWriter>(control), spec);
        QVERIFY(writer.value() != nullptr);
        auto status = (*writer.value())->write_frames(buffer.view());
        QVERIFY(status.error() != nullptr);
        QCOMPARE(status.error()->code(), ErrorCode::InvalidAudioSample);
        QCOMPARE(control->bytes.size(), std::size_t{58});
    }

    const std::array<double, 2> values{0.0, 1.0};
    auto buffer = make_buffer(make_format(ChannelLayout::MONO_C), values);
    auto countOne = FrameCount::create(1);
    QVERIFY(countOne.value() != nullptr);
    auto control = std::make_shared<WriterControl>();
    WavWriteSpec tooSmall{buffer.view().format(), *countOne.value(), WavSampleFormat::IEEE_F64};
    auto writer = WavWriter::open(std::make_unique<MemoryWriter>(control), tooSmall);
    QVERIFY(writer.value() != nullptr);
    auto tooMany = (*writer.value())->write_frames(buffer.view());
    QVERIFY(tooMany.error() != nullptr);
    QCOMPARE(tooMany.error()->code(), ErrorCode::OutOfRange);

    auto control2 = std::make_shared<WriterControl>();
    WavWriteSpec exact{buffer.view().format(), buffer.view().frame_count(), WavSampleFormat::IEEE_F64};
    auto writer2 = WavWriter::open(std::make_unique<MemoryWriter>(control2), exact);
    QVERIFY(writer2.value() != nullptr);
    auto incomplete = (*writer2.value())->finalize();
    QVERIFY(incomplete.error() != nullptr);
    QCOMPARE(incomplete.error()->code(), ErrorCode::InvalidState);
}

void WavWriterTest::partitionAndShortWriteSemantics()
{
    const std::array<double, 7> left{0.0, -0.0, 0.25, -0.5, 1.0, -2.5, 0.75};
    const std::array<double, 7> right{-1.0, 0.5, -0.25, 2.5, -0.0, 0.0, 0.125};
    auto buffer = make_buffer(make_format(ChannelLayout::STEREO_LR), left, right);
    const auto single = write_complete(buffer.view());
    constexpr std::array<std::int64_t, 4> partitions{1, 2, 1, 3};
    const auto partitioned = write_complete(buffer.view(), partitions);
    QCOMPARE(partitioned.bytes, single.bytes);
    const auto shortWrites = write_complete(buffer.view(), {}, 3U);
    QCOMPARE(shortWrites.bytes, single.bytes);
    QVERIFY(shortWrites.write_calls > single.write_calls);

    auto control = std::make_shared<WriterControl>();
    control->zero_progress = true;
    WavWriteSpec spec{buffer.view().format(), buffer.view().frame_count(), WavSampleFormat::IEEE_F64};
    auto failed = WavWriter::open(std::make_unique<MemoryWriter>(control), spec);
    QVERIFY(failed.error() != nullptr);
    QCOMPARE(failed.error()->code(), ErrorCode::IoFailure);
}

void WavWriterTest::formatCapabilityAndArithmeticPreflight()
{
    auto zeroCount = FrameCount::create(0);
    QVERIFY(zeroCount.value() != nullptr);
    auto format = make_format(ChannelLayout::STEREO_LR);
    auto unsupported = WavWriter::open(
        std::make_unique<MemoryWriter>(std::make_shared<WriterControl>()),
        WavWriteSpec{format, *zeroCount.value(), WavSampleFormat::IEEE_F32});
    QVERIFY(unsupported.error() != nullptr);
    QCOMPARE(unsupported.error()->code(), ErrorCode::UnsupportedAudioEncoding);

    auto weak = std::make_shared<WriterControl>();
    weak->allow_flush = false;
    auto capabilities = WavWriter::open(
        std::make_unique<MemoryWriter>(weak),
        WavWriteSpec{format, *zeroCount.value(), WavSampleFormat::IEEE_F64});
    QVERIFY(capabilities.error() != nullptr);
    QCOMPARE(capabilities.error()->code(), ErrorCode::UnsupportedOperation);

    auto hugeRate = SampleRate::create(std::numeric_limits<std::uint32_t>::max());
    QVERIFY(hugeRate.value() != nullptr);
    auto hugeFormat = AudioFormat::create(*hugeRate.value(), ChannelLayout::STEREO_LR);
    QVERIFY(hugeFormat.value() != nullptr);
    auto byteRateOverflow = WavWriter::open(
        std::make_unique<MemoryWriter>(std::make_shared<WriterControl>()),
        WavWriteSpec{*hugeFormat.value(), *zeroCount.value(), WavSampleFormat::IEEE_F64});
    QVERIFY(byteRateOverflow.error() != nullptr);
    QCOMPARE(byteRateOverflow.error()->code(), ErrorCode::IntegerOverflow);

    auto maximumFrames = FrameCount::create(std::numeric_limits<std::int64_t>::max());
    QVERIFY(maximumFrames.value() != nullptr);
    auto sizeOverflow = WavWriter::open(
        std::make_unique<MemoryWriter>(std::make_shared<WriterControl>()),
        WavWriteSpec{format, *maximumFrames.value(), WavSampleFormat::IEEE_F64});
    QVERIFY(sizeOverflow.error() != nullptr);
    QCOMPARE(sizeOverflow.error()->code(), ErrorCode::IntegerOverflow);
}

void WavWriterTest::riffRf64VirtualBoundaries()
{
    struct Boundary final {
        ChannelLayout layout;
        std::int64_t largest_riff;
        std::int64_t first_rf64;
    };
    constexpr std::array<Boundary, 2> boundaries{{
        {ChannelLayout::MONO_C, 536870905, 536870906},
        {ChannelLayout::STEREO_LR, 268435452, 268435453},
    }};
    for (const auto& item : boundaries) {
        auto riffCount = FrameCount::create(item.largest_riff);
        auto rf64Count = FrameCount::create(item.first_rf64);
        QVERIFY(riffCount.value() != nullptr);
        QVERIFY(rf64Count.value() != nullptr);
        auto riffControl = std::make_shared<WriterControl>();
        auto riff = WavWriter::open(
            std::make_unique<MemoryWriter>(riffControl),
            WavWriteSpec{make_format(item.layout), *riffCount.value(), WavSampleFormat::IEEE_F64});
        QVERIFY(riff.value() != nullptr);
        QCOMPARE((*riff.value())->container_kind(), WavContainerKind::RIFF);
        QCOMPARE(fourcc(riffControl->bytes, 0), std::string{"RIFF"});

        auto rf64Control = std::make_shared<WriterControl>();
        auto rf64 = WavWriter::open(
            std::make_unique<MemoryWriter>(rf64Control),
            WavWriteSpec{make_format(item.layout), *rf64Count.value(), WavSampleFormat::IEEE_F64});
        QVERIFY(rf64.value() != nullptr);
        QCOMPARE((*rf64.value())->container_kind(), WavContainerKind::RF64);
        QCOMPARE(fourcc(rf64Control->bytes, 0), std::string{"RF64"});
    }
}

void WavWriterTest::exactRf64Ds64AndNoMetadata()
{
    auto count = FrameCount::create(536870906);
    QVERIFY(count.value() != nullptr);
    auto control = std::make_shared<WriterControl>();
    auto writer = WavWriter::open(
        std::make_unique<MemoryWriter>(control),
        WavWriteSpec{make_format(ChannelLayout::MONO_C), *count.value(), WavSampleFormat::IEEE_F64});
    QVERIFY(writer.value() != nullptr);
    QCOMPARE(control->bytes.size(), std::size_t{94});
    QCOMPARE(read_u32(control->bytes, 4), std::uint32_t{0xffffffffU});
    QCOMPARE(fourcc(control->bytes, 12), std::string{"ds64"});
    QCOMPARE(read_u32(control->bytes, 16), std::uint32_t{28});
    QCOMPARE(read_u64(control->bytes, 20), (*writer.value())->expected_file_size_bytes() - 8U);
    QCOMPARE(read_u64(control->bytes, 28), std::uint64_t{536870906} * 8U);
    QCOMPARE(read_u64(control->bytes, 36), std::uint64_t{536870906});
    QCOMPARE(read_u32(control->bytes, 44), std::uint32_t{0});
    QCOMPARE(fourcc(control->bytes, 48), std::string{"fmt "});
    QCOMPARE(fourcc(control->bytes, 74), std::string{"fact"});
    QCOMPARE(fourcc(control->bytes, 86), std::string{"data"});
    QCOMPARE(read_u32(control->bytes, 90), std::uint32_t{0xffffffffU});
    const std::string all{
        reinterpret_cast<const char*>(control->bytes.data()), control->bytes.size()};
    QVERIFY(all.find("LIST") == std::string::npos);
    QVERIFY(all.find("INFO") == std::string::npos);
    QVERIFY(all.find("RGS") == std::string::npos);
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::WavWriterTest)
#include "test_wav_writer.moc"
