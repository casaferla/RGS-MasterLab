#include <rgsml/audio/audio_buffer.hpp>

#include <QtTest/QTest>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace rgsml::tests {
namespace {

using namespace rgsml::audio;
using namespace rgsml::core;

static_assert(std::is_move_constructible_v<AudioBuffer>);
static_assert(std::is_move_assignable_v<AudioBuffer>);
static_assert(!std::is_copy_constructible_v<AudioBuffer>);
static_assert(!std::is_copy_assignable_v<AudioBuffer>);

[[nodiscard]] AudioFormat make_format(ChannelLayout layout)
{
    auto rate = SampleRate::create(48000);
    Q_ASSERT(rate.value() != nullptr);
    auto format = AudioFormat::create(*rate.value(), layout);
    Q_ASSERT(format.value() != nullptr);
    return *format.value();
}

[[nodiscard]] FrameCount frame_count(std::int64_t value)
{
    auto count = FrameCount::create(value);
    Q_ASSERT(count.value() != nullptr);
    return *count.value();
}

class AudioBufferTest final : public QObject {
    Q_OBJECT

private slots:
    void shapesAlignmentAndInitialization();
    void absoluteViewsAndSubviews();
    void bitPreservingMutationAndMove();
    void invalidRangesAndOverflow();
};

void AudioBufferTest::shapesAlignmentAndInitialization()
{
    for (const auto layout : {ChannelLayout::MONO_C, ChannelLayout::STEREO_LR}) {
        for (const std::int64_t frames : {0, 1, 3, 16}) {
            auto buffer = AudioBuffer::create(
                make_format(layout),
                FrameDomainId::SOURCE_PROCESSING_RATE,
                FrameIndex{7},
                frame_count(frames));
            QVERIFY(buffer.value() != nullptr);
            auto view = buffer.value()->view();
            QCOMPARE(view.frame_count().value(), frames);
            QCOMPARE(view.absolute_start_frame(), FrameIndex{7});
            QCOMPARE(view.absolute_end_frame(), FrameIndex{7 + frames});
            QCOMPARE(
                view.timebase().frame_domain_id(),
                FrameDomainId::SOURCE_PROCESSING_RATE);

            std::array<const double*, 2> pointers{};
            for (std::size_t channel = 0;
                 channel < view.format().channel_count();
                 ++channel) {
                auto plane = view.channel(channel);
                QVERIFY(plane.value() != nullptr);
                QCOMPARE(plane.value()->size(), static_cast<std::size_t>(frames));
                pointers[channel] = plane.value()->data();
                if (frames > 0) {
                    QCOMPARE(
                        reinterpret_cast<std::uintptr_t>(pointers[channel]) % 64U,
                        std::uintptr_t{0});
                    for (const double sample : *plane.value()) {
                        QCOMPARE(
                            std::bit_cast<std::uint64_t>(sample),
                            std::uint64_t{0});
                    }
                }
            }
            if (frames > 0 && view.format().channel_count() == 2U) {
                QVERIFY(pointers[0] != pointers[1]);
            }
        }
    }
}

void AudioBufferTest::absoluteViewsAndSubviews()
{
    auto buffer = AudioBuffer::create(
        make_format(ChannelLayout::STEREO_LR),
        FrameDomainId::SOURCE_PROCESSING_RATE,
        FrameIndex{100},
        frame_count(7));
    QVERIFY(buffer.value() != nullptr);
    auto mutableView = buffer.value()->mutable_view();
    QCOMPARE(*mutableView.absolute_frame(0).value(), FrameIndex{100});
    QCOMPARE(*mutableView.absolute_frame(6).value(), FrameIndex{106});
    QVERIFY(mutableView.absolute_frame(7).error() != nullptr);

    auto subview = mutableView.subview(FrameIndex{102}, frame_count(3));
    QVERIFY(subview.value() != nullptr);
    QCOMPARE(subview.value()->absolute_start_frame(), FrameIndex{102});
    QCOMPARE(subview.value()->absolute_end_frame(), FrameIndex{105});
    QCOMPARE(subview.value()->frame_count().value(), std::int64_t{3});
    QCOMPARE(subview.value()->format(), mutableView.format());
    QCOMPARE(subview.value()->timebase(), mutableView.timebase());
    QCOMPARE(subview.value()->channel(0).value()->size(), std::size_t{3});

    auto emptyTail = mutableView.subview(FrameIndex{107}, frame_count(0));
    QVERIFY(emptyTail.value() != nullptr);
    QCOMPARE(emptyTail.value()->absolute_start_frame(), FrameIndex{107});
    QCOMPARE(emptyTail.value()->absolute_end_frame(), FrameIndex{107});
    QVERIFY(mutableView.subview(FrameIndex{99}, frame_count(1)).error() != nullptr);
    QVERIFY(mutableView.subview(FrameIndex{106}, frame_count(2)).error() != nullptr);
    QVERIFY(mutableView.channel(2).error() != nullptr);
}

void AudioBufferTest::bitPreservingMutationAndMove()
{
    auto buffer = AudioBuffer::create(
        make_format(ChannelLayout::MONO_C),
        FrameDomainId::SOURCE_PROCESSING_RATE,
        FrameIndex{0},
        frame_count(4));
    QVERIFY(buffer.value() != nullptr);
    auto plane = buffer.value()->mutable_view().channel(0);
    QVERIFY(plane.value() != nullptr);
    const std::array<std::uint64_t, 4> patterns{
        0x8000000000000000ULL,
        0x0000000000000001ULL,
        0x8000000000000001ULL,
        0x4004000000000000ULL,
    };
    for (std::size_t index = 0; index < patterns.size(); ++index) {
        (*plane.value())[index] = std::bit_cast<double>(patterns[index]);
    }

    auto subview = buffer.value()->view().subview(FrameIndex{0}, frame_count(4));
    QVERIFY(subview.value() != nullptr);
    auto constPlane = subview.value()->channel(0);
    QVERIFY(constPlane.value() != nullptr);
    for (std::size_t index = 0; index < patterns.size(); ++index) {
        QCOMPARE(
            std::bit_cast<std::uint64_t>((*constPlane.value())[index]),
            patterns[index]);
    }

    auto copied = AudioBuffer::create(
        make_format(ChannelLayout::MONO_C),
        FrameDomainId::SOURCE_PROCESSING_RATE,
        FrameIndex{0},
        frame_count(4));
    QVERIFY(copied.value() != nullptr);
    auto copiedPlane = copied.value()->mutable_view().channel(0);
    QVERIFY(copiedPlane.value() != nullptr);
    std::copy(
        constPlane.value()->begin(),
        constPlane.value()->end(),
        copiedPlane.value()->begin());
    for (std::size_t index = 0; index < patterns.size(); ++index) {
        QCOMPARE(
            std::bit_cast<std::uint64_t>((*copiedPlane.value())[index]),
            patterns[index]);
    }

    AudioBuffer moved = std::move(*buffer.value());
    auto movedPlane = moved.view().channel(0);
    QVERIFY(movedPlane.value() != nullptr);
    for (std::size_t index = 0; index < patterns.size(); ++index) {
        QCOMPARE(
            std::bit_cast<std::uint64_t>((*movedPlane.value())[index]),
            patterns[index]);
    }
}

void AudioBufferTest::invalidRangesAndOverflow()
{
    auto negativeStart = AudioBuffer::create(
        make_format(ChannelLayout::MONO_C),
        FrameDomainId::SOURCE_PROCESSING_RATE,
        FrameIndex{-1},
        frame_count(1));
    QVERIFY(negativeStart.error() != nullptr);
    QCOMPARE(negativeStart.error()->code(), ErrorCode::OutOfRange);

    auto endOverflow = AudioBuffer::create(
        make_format(ChannelLayout::MONO_C),
        FrameDomainId::SOURCE_PROCESSING_RATE,
        FrameIndex{std::numeric_limits<std::int64_t>::max()},
        frame_count(1));
    QVERIFY(endOverflow.error() != nullptr);
    QCOMPARE(endOverflow.error()->code(), ErrorCode::IntegerOverflow);

    auto hugeCount = FrameCount::create(std::numeric_limits<std::int64_t>::max());
    QVERIFY(hugeCount.value() != nullptr);
    auto sizeOverflow = AudioBuffer::create(
        make_format(ChannelLayout::STEREO_LR),
        FrameDomainId::SOURCE_PROCESSING_RATE,
        FrameIndex{0},
        *hugeCount.value());
    QVERIFY(sizeOverflow.error() != nullptr);
    QCOMPARE(sizeOverflow.error()->code(), ErrorCode::IntegerOverflow);
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::AudioBufferTest)

#include "test_audio_buffer.moc"
