#include "internal/waveform_geometry.hpp"

#include <rgsml/audio/wav_reader.hpp>

#include "../../unit/audio/wav_test_support.hpp"

#include <QTest>

#include <array>
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
    void finestBoundedLevelIsSelected();
    void adjacentBucketSpansCoverViewportWithoutGaps();
};

void WaveformGeometryTest::physicalWidthIsClamped()
{
    QCOMPARE(ui::internal::waveform_target_range_count(0.0, 1.0), std::size_t{1});
    QCOMPARE(ui::internal::waveform_target_range_count(0.5, 2.0), std::size_t{1});
    QCOMPARE(ui::internal::waveform_target_range_count(800.0, 1.25), std::size_t{1'000});
    QCOMPARE(ui::internal::waveform_target_range_count(9'000.0, 2.0), std::size_t{4'096});
    QCOMPARE(ui::internal::waveform_target_range_count(200.0, 0.0), std::size_t{1});
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
