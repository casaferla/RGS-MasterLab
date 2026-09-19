#include "internal/waveform_geometry.hpp"
#include "internal/waveform_viewport.hpp"

#include <rgsml/audio/wav_reader.hpp>

#include "../../unit/audio/wav_test_support.hpp"

#include <QTest>

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace rgsml::tests {

class WaveformGeometryTest final : public QObject {
    Q_OBJECT

private slots:
    void physicalWidthIsClamped();
    void frozenOverlayGeometryIsExact();
    void finestBoundedLevelIsSelected();
    void adjacentBucketSpansCoverViewportWithoutGaps();
    void visibleWindowIsBoundedAndSummaryRemainsImmutable();
};

void WaveformGeometryTest::frozenOverlayGeometryIsExact()
{
    QCOMPARE(ui::internal::kWaveformPlayheadWidth, 2.0F);
    QCOMPARE(ui::internal::kWaveformPlayheadMarkerSize, 8.0F);
    QCOMPARE(ui::internal::kWaveformRegionBoundaryWidth, 2.0F);
    QCOMPARE(ui::internal::kWaveformRegionHandleWidth, 6.0F);
    QCOMPARE(ui::internal::kWaveformRegionHandleHeight, 16.0F);
}

void WaveformGeometryTest::physicalWidthIsClamped()
{
    QCOMPARE(ui::internal::waveform_target_range_count(0.0, 1.0), std::size_t{1});
    QCOMPARE(ui::internal::waveform_target_range_count(0.5, 2.0), std::size_t{1});
    QCOMPARE(ui::internal::waveform_target_range_count(800.0, 1.25), std::size_t{1'000});
    QCOMPARE(ui::internal::waveform_target_range_count(9'000.0, 2.0), std::size_t{4'096});
    QCOMPARE(ui::internal::waveform_target_range_count(200.0, 0.0), std::size_t{1});
}

void WaveformGeometryTest::visibleWindowIsBoundedAndSummaryRemainsImmutable()
{
    using namespace wav_support;
    std::vector<std::int64_t> codes(70'001U);
    for (std::size_t index = 0; index < codes.size(); ++index) {
        codes[index] = static_cast<std::int64_t>((index * 131U) % 65'536U) - 32'768;
    }
    auto reader = audio::WavReader::open(memory_reader(
        make_wav(1U, 16U, 1U, 48'000U, pcm_payload(codes, 16U)),
        std::make_shared<ReaderControl>()));
    QVERIFY(reader);
    auto built = audio::build_waveform_summary(**reader.value());
    QVERIFY(built);
    const auto& summary = *built.value();
    const auto payloadBefore = summary.payload_bytes();
    const auto levelsBefore = summary.level_count();
    const auto baseBefore = summary.level(0U);
    QVERIFY(baseBefore);
    const auto peakBefore = baseBefore.value()->peak(0U, core::FrameIndex{17});
    QVERIFY(peakBefore);
    const auto minimumBits = std::bit_cast<std::uint64_t>(peakBefore.value()->minimum);
    const auto maximumBits = std::bit_cast<std::uint64_t>(peakBefore.value()->maximum);

    ui::internal::WaveformViewport viewport;
    QVERIFY(viewport.reset(
        summary.source_frame_count(),
        baseBefore.value()->frames_per_bucket()));
    constexpr std::array widths{640, 997, 1'337, 2'048, 4'801};
    for (const auto width : widths) {
        static_cast<void>(viewport.zoom(true, width / 3, width));
        static_cast<void>(viewport.pan_step(true, false, width));
        const auto selected = ui::internal::select_visible_waveform_window(
            summary, viewport.visible_range());
        QVERIFY(selected.has_value());
        QVERIFY(selected->count() > 0);
        QVERIFY(selected->count() <= static_cast<std::int64_t>(
            audio::WaveformSummary::kMaximumUiRangesPerChannel));
        const auto level = summary.level(selected->levelIndex);
        QVERIFY(level);
        const auto bucketSize = level.value()->frames_per_bucket().value();
        QCOMPARE(
            selected->firstBucket,
            viewport.visible_range().begin().value() / bucketSize);
        const auto expectedEnd = std::min(
            level.value()->bucket_count().value(),
            viewport.visible_range().end().value() / bucketSize
                + (viewport.visible_range().end().value() % bucketSize != 0 ? 1 : 0));
        QCOMPARE(selected->endBucket, expectedEnd);

        for (auto bucket = selected->firstBucket;
             bucket + 1 < selected->endBucket;
             ++bucket) {
            const auto boundary = std::min(
                summary.source_frame_count().value(),
                (bucket + 1) * bucketSize);
            const auto right = viewport.pixel_boundary(core::FrameIndex{boundary}, width);
            const auto nextLeft = viewport.pixel_boundary(core::FrameIndex{boundary}, width);
            QCOMPARE(right, nextLeft);
        }
    }

    QCOMPARE(summary.payload_bytes(), payloadBefore);
    QCOMPARE(summary.level_count(), levelsBefore);
    const auto baseAfter = summary.level(0U);
    QVERIFY(baseAfter);
    const auto peakAfter = baseAfter.value()->peak(0U, core::FrameIndex{17});
    QVERIFY(peakAfter);
    QCOMPARE(std::bit_cast<std::uint64_t>(peakAfter.value()->minimum), minimumBits);
    QCOMPARE(std::bit_cast<std::uint64_t>(peakAfter.value()->maximum), maximumBits);
}

void WaveformGeometryTest::finestBoundedLevelIsSelected()
{
    using namespace wav_support;
    std::vector<std::int64_t> codes(10'001U);
    for (std::size_t index = 0; index < codes.size(); ++index) {
        codes[index] = static_cast<std::int64_t>(index % 65'536U) - 32'768;
    }
    const auto bytes = make_wav(
        1U, 16U, 1U, 48'000U, pcm_payload(codes, 16U));
    auto reader = audio::WavReader::open(
        memory_reader(bytes, std::make_shared<ReaderControl>()));
    QVERIFY(reader);
    auto result = audio::build_waveform_summary(**reader.value());
    QVERIFY(result);
    const auto& summary = *result.value();

    const auto selected = ui::internal::select_waveform_level(summary, 4'096U);
    const auto level = summary.level(selected);
    QVERIFY(level);
    QVERIFY(level.value()->bucket_count().value() <= 4'096);
    if (selected > 0U) {
        const auto finer = summary.level(selected - 1U);
        QVERIFY(finer);
        QVERIFY(finer.value()->bucket_count().value() > 4'096);
    }

    const auto oneRange = ui::internal::select_waveform_level(summary, 1U);
    const auto coarsest = summary.level(oneRange);
    QVERIFY(coarsest);
    QCOMPARE(coarsest.value()->bucket_count().value(), std::int64_t{1});
}

void WaveformGeometryTest::adjacentBucketSpansCoverViewportWithoutGaps()
{
    struct Case final {
        double width;
        double devicePixelRatio;
        std::size_t bucketCount;
    };
    constexpr std::array cases{
        Case{640.0, 1.0, 640U},
        Case{853.0, 1.25, 997U},
        Case{1'024.0, 1.5, 1'337U},
        Case{1'279.5, 2.0, 2'048U},
        Case{1'920.0, 2.5, 4'096U},
    };

    for (const auto& testCase : cases) {
        double previousRight = 0.0;
        for (std::size_t bucket = 0U; bucket < testCase.bucketCount; ++bucket) {
            const auto span = ui::internal::waveform_bucket_span(
                testCase.width,
                testCase.devicePixelRatio,
                testCase.bucketCount,
                bucket);
            QCOMPARE(span.left, previousRight);
            QVERIFY2(span.right > span.left, "Every selected bucket must cover at least one physical column");
            QVERIFY(span.right <= testCase.width);

            if (bucket + 1U < testCase.bucketCount) {
                const double physicalBoundary = span.right * testCase.devicePixelRatio;
                QVERIFY(std::abs(physicalBoundary - std::floor(physicalBoundary)) < 1.0e-9);
            }
            previousRight = span.right;
        }
        QCOMPARE(previousRight, testCase.width);
    }

    const auto invalid = ui::internal::waveform_bucket_span(640.0, 1.0, 0U, 0U);
    QCOMPARE(invalid.left, 0.0);
    QCOMPARE(invalid.right, 0.0);
}

}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::WaveformGeometryTest)

#include "test_waveform_geometry.moc"
