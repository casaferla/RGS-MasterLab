#include "internal/waveform_viewport.hpp"

#include <QTest>

#include <array>
#include <cstdlib>
#include <cstdint>
#include <limits>

namespace rgsml::tests {

class WaveformViewportTest final : public QObject {
    Q_OBJECT

private slots:
    void tiesToEvenAndOverflowFreeProduct();
    void sourceSizesAndMinimumSpan();
    void mappingEndpointsAndSmallExhaustiveMonotonicity();
    void zoomPanFitAndDragSnapshotAreDeterministic();
    void invalidSourceIsDisabled();
};

void WaveformViewportTest::tiesToEvenAndOverflowFreeProduct()
{
    const auto positiveHalfEven = ui::internal::round_div_ties_even(1, 2);
    const auto positiveHalfOdd = ui::internal::round_div_ties_even(3, 2);
    const auto negativeHalfOdd = ui::internal::round_div_ties_even(-1, 2);
    const auto negativeHalfEven = ui::internal::round_div_ties_even(-3, 2);
    QVERIFY(positiveHalfEven && positiveHalfOdd && negativeHalfOdd && negativeHalfEven);
    QCOMPARE(*positiveHalfEven.value(), std::int64_t{0});
    QCOMPARE(*positiveHalfOdd.value(), std::int64_t{2});
    QCOMPARE(*negativeHalfOdd.value(), std::int64_t{0});
    QCOMPARE(*negativeHalfEven.value(), std::int64_t{-2});

    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    const auto identity = ui::internal::mul_div_ties_even(maximum, maximum, maximum);
    const auto nearIdentity = ui::internal::mul_div_ties_even(
        maximum - 1, maximum, maximum);
    QVERIFY(identity && nearIdentity);
    QCOMPARE(*identity.value(), maximum);
    QCOMPARE(*nearIdentity.value(), maximum - 1);

    const auto invalid = ui::internal::mul_div_ties_even(1, 2, 0);
    QVERIFY(!invalid);

    for (std::int64_t left = 0; left <= 31; ++left) {
        for (std::int64_t right = 0; right <= 31; ++right) {
            for (std::int64_t divisor = 1; divisor <= 31; ++divisor) {
                const auto product = left * right;
                auto expected = product / divisor;
                const auto remainder = product % divisor;
                if (remainder > divisor - remainder
                    || (remainder == divisor - remainder && expected % 2 != 0)) {
                    ++expected;
                }
                const auto actual = ui::internal::mul_div_ties_even(
                    left, right, divisor);
                QVERIFY(actual);
                QCOMPARE(*actual.value(), expected);
            }
        }
    }
}

void WaveformViewportTest::sourceSizesAndMinimumSpan()
{
    struct Case final {
        std::int64_t frames;
        std::int64_t base;
        std::int64_t expectedMinimum;
    };
    constexpr std::array cases{
        Case{1, 1, 1},
        Case{63, 1, 63},
        Case{64, 1, 64},
        Case{65, 1, 64},
        Case{480'000, 8, 512},
        Case{std::numeric_limits<std::int64_t>::max(),
             std::numeric_limits<std::int64_t>::max() / 63,
             std::numeric_limits<std::int64_t>::max()},
    };
    for (const auto& testCase : cases) {
        ui::internal::WaveformViewport viewport;
        const auto frames = core::FrameCount::create(testCase.frames);
        const auto base = core::FrameCount::create(testCase.base);
        QVERIFY(frames && base);
        QVERIFY(viewport.reset(*frames.value(), *base.value()));
        QCOMPARE(viewport.visible_range().begin().value(), std::int64_t{0});
        QCOMPARE(viewport.visible_range().end().value(), testCase.frames);
        QCOMPARE(viewport.minimum_span().value(), testCase.expectedMinimum);
    }
}

void WaveformViewportTest::mappingEndpointsAndSmallExhaustiveMonotonicity()
{
    for (std::int64_t frames = 1; frames <= 33; ++frames) {
        ui::internal::WaveformViewport viewport;
        QVERIFY(viewport.reset(
            *core::FrameCount::create(frames).value(),
            *core::FrameCount::create(1).value()));
        for (std::int64_t width = 1; width <= 37; ++width) {
            QCOMPARE(viewport.frame_boundary(0, width).value(), std::int64_t{0});
            QCOMPARE(viewport.frame_boundary(width, width).value(), frames);
            QCOMPARE(viewport.pixel_boundary(core::FrameIndex{0}, width), std::int64_t{0});
            QCOMPARE(viewport.pixel_boundary(core::FrameIndex{frames}, width), width);
            auto previous = std::int64_t{0};
            for (std::int64_t pixel = 0; pixel <= width; ++pixel) {
                const auto mapped = viewport.frame_boundary(pixel, width).value();
                QVERIFY(mapped >= previous);
                QVERIFY(mapped >= 0 && mapped <= frames);
                previous = mapped;
            }
            QCOMPARE(viewport.seek_frame(width, width).value(), frames - 1);
        }
    }
}

void WaveformViewportTest::zoomPanFitAndDragSnapshotAreDeterministic()
{
    const auto frames = *core::FrameCount::create(480'000).value();
    const auto base = *core::FrameCount::create(8).value();
    ui::internal::WaveformViewport viewport;
    QVERIFY(viewport.reset(frames, base));
    for (const auto anchor : std::array<std::int64_t, 4>{0, 317, 500, 1'001}) {
        ui::internal::WaveformViewport anchored;
        QVERIFY(anchored.reset(frames, base));
        const auto before = anchored.frame_boundary(anchor, 1'001).value();
        static_cast<void>(anchored.zoom(true, anchor, 1'001));
        const auto after = anchored.frame_boundary(anchor, 1'001).value();
        QVERIFY(std::abs(after - before) <= 1);
    }
    for (int index = 0; index < 100; ++index) {
        static_cast<void>(viewport.zoom(true, 317, 1'001));
    }
    QCOMPARE(viewport.visible_span().value(), viewport.minimum_span().value());
    const auto maximumDetail = viewport.visible_range();
    QVERIFY(maximumDetail.begin().value() >= 0);
    QVERIFY(maximumDetail.end().value() <= frames.value());

    const auto dragStart = viewport.visible_range();
    ui::internal::WaveformViewport direct = viewport;
    ui::internal::WaveformViewport coalesced = viewport;
    static_cast<void>(coalesced.pan_from_snapshot(500, 499, 1'001, dragStart));
    static_cast<void>(coalesced.pan_from_snapshot(500, 700, 1'001, dragStart));
    static_cast<void>(direct.pan_from_snapshot(500, 700, 1'001, dragStart));
    QCOMPARE(coalesced.visible_range(), direct.visible_range());

    for (int index = 0; index < 100; ++index) {
        static_cast<void>(viewport.zoom(false, 0, 1'001));
    }
    QVERIFY(viewport.is_full_fit());
    QVERIFY(!viewport.pan_step(true, false, 1'001));

    QVERIFY(viewport.zoom(true, 500, 1'001));
    const auto beforeRegion = *core::FrameRange::create(
        core::FrameIndex{0}, core::FrameIndex{100}).value();
    QVERIFY(viewport.fit_region(beforeRegion));
    QCOMPARE(viewport.visible_span().value(), viewport.minimum_span().value());
    QCOMPARE(viewport.visible_range().begin().value(), std::int64_t{0});
    QVERIFY(viewport.fit_source());
    const auto exactMinimum = *core::FrameRange::create(
        core::FrameIndex{10'000},
        core::FrameIndex{10'000 + viewport.minimum_span().value()}).value();
    QVERIFY(viewport.fit_region(exactMinimum));
    QCOMPARE(viewport.visible_range(), exactMinimum);
    const auto larger = *core::FrameRange::create(
        core::FrameIndex{470'000}, core::FrameIndex{480'000}).value();
    QVERIFY(viewport.fit_region(larger));
    QCOMPARE(viewport.visible_range(), larger);
    const auto beforeResize = viewport.visible_range();
    QCOMPARE(ui::internal::waveform_physical_width(853.0, 1.25), std::int64_t{1'066});
    QCOMPARE(ui::internal::waveform_physical_width(853.0, 2.0), std::int64_t{1'706});
    QCOMPARE(viewport.visible_range(), beforeResize);

    ui::internal::WaveformViewport first;
    ui::internal::WaveformViewport second;
    QVERIFY(first.reset(frames, base));
    QVERIFY(second.reset(frames, base));
    for (int index = 0; index < 1'000; ++index) {
        const bool zoomIn = index % 3 != 0;
        static_cast<void>(first.zoom(zoomIn, (index * 37) % 1'001, 1'001));
        static_cast<void>(second.zoom(zoomIn, (index * 37) % 1'001, 1'001));
        static_cast<void>(first.pan_step(index % 2 == 0, index % 5 == 0, 1'001));
        static_cast<void>(second.pan_step(index % 2 == 0, index % 5 == 0, 1'001));
    }
    QCOMPARE(first.visible_range(), second.visible_range());
    QVERIFY(first.visible_span().value() >= first.minimum_span().value());
    QVERIFY(first.visible_range().begin().value() >= 0);
    QVERIFY(first.visible_range().end().value() <= frames.value());
}

void WaveformViewportTest::invalidSourceIsDisabled()
{
    ui::internal::WaveformViewport viewport;
    QVERIFY(!viewport.reset(
        *core::FrameCount::create(0).value(),
        *core::FrameCount::create(1).value()));
    QVERIFY(!viewport.enabled());
    QCOMPARE(viewport.frame_boundary(1, 0).value(), std::int64_t{0});
    QVERIFY(!viewport.zoom(true, 0, 1));
}

}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::WaveformViewportTest)

#include "test_waveform_viewport.moc"
