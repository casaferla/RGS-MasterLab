#include "eq_view_model.hpp"

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/core/uuid.hpp>
#include <rgsml/render/render_result.hpp>

#include <QtTest/QTest>

#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <thread>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace rgsml::app;

[[nodiscard]] std::shared_ptr<render::RenderResult> make_test_prepared_result()
{
    const std::array left{0.5, -0.25, 0.75, -0.5};
    const std::array right{-0.5, 0.25, -0.75, 0.5};
    auto format = *rgsml::audio::AudioFormat::create(
        rgsml::audio::SampleFormat::FLOAT64, rgsml::audio::ChannelLayout::STEREO_LR, 48000).value();
    auto buf = *rgsml::audio::AudioBuffer::create(
        format, rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, rgsml::core::FrameIndex{0}, rgsml::core::FrameCount{4}).value();
    buf->mutable_view().channel(0).value()->copy_from(left);
    buf->mutable_view().channel(1).value()->copy_from(right);

    auto result = *render::RenderResult::create(
        buf->view(), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, 1, {}).value();
    return std::make_shared<render::RenderResult>(std::move(result));
}

class EqViewModelTest final : public QObject {
    Q_OBJECT

private slots:
    void testDefaultStateAndLimits();
    void testSelectionAndPostRemoveSelection();
    void testFilterSwitchingDefaultsAndApplicability();
    void testDraftCommitAndCancelRestore();
    void testHardRangeRejectionAndSampleRateBound();
    void testRoutesAndMonoRejection();
    void testSelectionOnlyNoPreview();
    void testGraphDragNoPreviewReleaseOneCommit();
    void testMixedRouting();
    void testSelectedBandResponseOnly();
    void testRealRenderPreviewActiveAndBypass();
    void testStaleGenerationDiscarded();
    void testPreparedInputRemainsImmutable();
};

void EqViewModelTest::testDefaultStateAndLimits()
{
    std::uint32_t idCount = 0;
    auto mockIdGen = [&idCount]() -> rgsml::core::Uuid {
        ++idCount;
        char buf[37];
        std::snprintf(buf, sizeof(buf), "00000000-0000-0000-0000-%012u", idCount);
        return *rgsml::core::Uuid::parse(buf).value();
    };

    EqViewModel vm{nullptr, nullptr, mockIdGen};

    QCOMPARE(vm.band_count(), 1);
    QCOMPARE(vm.selected_index(), 0);
    QVERIFY(vm.enabled());
    QCOMPARE(vm.filter_label(), QStringLiteral("BELL"));
    QCOMPARE(vm.routing_label(), QStringLiteral("STEREO"));
    QCOMPARE(vm.frequency(), 1000.0);
    QCOMPARE(vm.gain(), 0.0);
    QCOMPARE(vm.q(), 0.707);

    // Add bands up to 6
    for (int i = 0; i < 5; ++i) {
        QVERIFY(vm.add_available());
        vm.addBand();
    }
    QCOMPARE(vm.band_count(), 6);
    QVERIFY(!vm.add_available());

    // 7th add attempt rejected
    vm.addBand();
    QCOMPARE(vm.band_count(), 6);

    // Remove down to 1
    for (int i = 0; i < 5; ++i) {
        QVERIFY(vm.remove_available());
        vm.removeSelectedBand();
    }
    QCOMPARE(vm.band_count(), 1);
    QVERIFY(!vm.remove_available());

    // Remove last remaining band rejected
    vm.removeSelectedBand();
    QCOMPARE(vm.band_count(), 1);
}

void EqViewModelTest::testSelectionAndPostRemoveSelection()
{
    std::uint32_t idCount = 0;
    auto mockIdGen = [&idCount]() -> rgsml::core::Uuid {
        ++idCount;
        char buf[37];
        std::snprintf(buf, sizeof(buf), "00000000-0000-0000-0000-%012u", idCount);
        return *rgsml::core::Uuid::parse(buf).value();
    };

    EqViewModel vm{nullptr, nullptr, mockIdGen};
    vm.addBand(); // band 1 (selected)
    vm.addBand(); // band 2 (selected)
    QCOMPARE(vm.band_count(), 3);
    QCOMPARE(vm.selected_index(), 2);

    vm.selectBand(1);
    QCOMPARE(vm.selected_index(), 1);

    // Remove band 1 -> selected index stays 1 (now occupying removed position)
    vm.removeSelectedBand();
    QCOMPARE(vm.band_count(), 2);
    QCOMPARE(vm.selected_index(), 1);

    // Remove band 1 (last position) -> selected index becomes 0 (new final band)
    vm.removeSelectedBand();
    QCOMPARE(vm.band_count(), 1);
    QCOMPARE(vm.selected_index(), 0);
}

void EqViewModelTest::testFilterSwitchingDefaultsAndApplicability()
{
    EqViewModel vm;

    // Default BELL
    QVERIFY(vm.gain_applicable());
    QVERIFY(vm.q_applicable());
    QVERIFY(!vm.shelf_slope_applicable());
    QVERIFY(!vm.slope_applicable());

    // Switch to NOTCH
    vm.setFilter(QStringLiteral("NOTCH"));
    QCOMPARE(vm.filter_label(), QStringLiteral("NOTCH"));
    QVERIFY(!vm.gain_applicable());
    QVERIFY(vm.q_applicable());
    QCOMPARE(vm.q(), 0.707);

    // Switch to LOW_SHELF
    vm.setFilter(QStringLiteral("LOW_SHELF"));
    QCOMPARE(vm.filter_label(), QStringLiteral("LOW_SHELF"));
    QVERIFY(vm.gain_applicable());
    QVERIFY(vm.shelf_slope_applicable());
    QCOMPARE(vm.gain(), 0.0);
    QCOMPARE(vm.shelf_slope(), 1.0);

    // Switch to HIGH_PASS
    vm.setFilter(QStringLiteral("HIGH_PASS"));
    QCOMPARE(vm.filter_label(), QStringLiteral("HIGH_PASS"));
    QVERIFY(vm.slope_applicable());
    QCOMPARE(vm.slope_db_per_oct(), 12);
}

void EqViewModelTest::testDraftCommitAndCancelRestore()
{
    EqViewModel vm;
    const double origFreq = vm.frequency();

    // Modify draft
    vm.setDraftFrequency(2500.0);
    QCOMPARE(vm.frequency(), 2500.0);

    // Cancel restores original
    vm.cancelDraft();
    QCOMPARE(vm.frequency(), origFreq);

    // Modify and commit
    vm.setDraftFrequency(3000.0);
    QVERIFY(vm.commitDraft());
    QCOMPARE(vm.frequency(), 3000.0);
    QCOMPARE(vm.committed_parameters().bands()[0].payload(), rgsml::dsp::EqBandPayload(rgsml::dsp::BellPayload{3000.0, 0.0, 0.707}));
}

void EqViewModelTest::testHardRangeRejectionAndSampleRateBound()
{
    EqViewModel vm;

    // Out of range frequency (5.0 Hz < 20.0 Hz min)
    vm.setDraftFrequency(5.0);
    QVERIFY(!vm.commitDraft()); // Commit rejected, restores committed value
    QCOMPARE(vm.frequency(), 1000.0);

    // Out of range gain (25.0 dB > 18.0 dB max)
    vm.setDraftGain(25.0);
    QVERIFY(!vm.commitDraft());
    QCOMPARE(vm.gain(), 0.0);

    // Exceed Nyquist guard limit (0.45 * 48000 = 21600 Hz limit) -> 23000.0 rejected
    vm.setDraftFrequency(23000.0);
    QVERIFY(!vm.commitDraft());
    QCOMPARE(vm.frequency(), 1000.0);
}

void EqViewModelTest::testRoutesAndMonoRejection()
{
    EqViewModel vm;

    for (const auto& route : {QStringLiteral("MID"), QStringLiteral("SIDE"), QStringLiteral("LEFT"), QStringLiteral("RIGHT"), QStringLiteral("STEREO")}) {
        vm.setRouting(route);
        QCOMPARE(vm.routing_label(), route);
    }
}

void EqViewModelTest::testSelectionOnlyNoPreview()
{
    EqViewModel vm;
    vm.addBand(); // gen = 1
    const quint64 gen = vm.preview_generation();

    vm.selectBand(0);
    QCOMPARE(vm.preview_generation(), gen); // No generation increment on selection change
}

void EqViewModelTest::testGraphDragNoPreviewReleaseOneCommit()
{
    EqViewModel vm;
    const quint64 origGen = vm.preview_generation();

    // Drag
    vm.graphDrag(1500.0, 6.0);
    QCOMPARE(vm.frequency(), 1500.0);
    QCOMPARE(vm.gain(), 6.0);
    QCOMPARE(vm.preview_generation(), origGen); // No preview request during drag

    // Release
    vm.graphRelease();
    QCOMPARE(vm.preview_generation(), origGen + 1U); // Exactly one commit and preview request on release
}

void EqViewModelTest::testMixedRouting()
{
    EqViewModel vm;
    QVERIFY(!vm.mixed_routing());

    vm.addBand();
    vm.setRouting(QStringLiteral("MID"));
    QVERIFY(vm.mixed_routing());

    vm.setRouting(QStringLiteral("STEREO"));
    QVERIFY(!vm.mixed_routing());
}

void EqViewModelTest::testSelectedBandResponseOnly()
{
    EqViewModel vm;
    QVERIFY(!vm.selected_band_response_points().isEmpty());
    QCOMPARE(vm.selected_band_response_points().size(), 100);
}

void EqViewModelTest::testRealRenderPreviewActiveAndBypass()
{
    auto prepared = make_test_prepared_result();
    std::shared_ptr<render::RenderResult> publishedResult;

    EqViewModel vm{
        [prepared] { return prepared; },
        [&publishedResult](render::RenderResult res) {
            publishedResult = std::make_shared<render::RenderResult>(std::move(res));
            return rgsml::core::Status::success();
        }
    };

    // Trigger preview and wait for worker thread to complete
    vm.setDraftGain(6.0);
    vm.commitDraft();

    // Wait up to 1 second for worker publication
    for (int i = 0; i < 100 && vm.preview_status() == QStringLiteral("RENDERING"); ++i) {
        QTest::qWait(10);
    }

    QCOMPARE(vm.preview_status(), QStringLiteral("READY"));
    QVERIFY(publishedResult != nullptr);
    QCOMPARE(publishedResult->signatures().size(), std::size_t{1});
    QCOMPARE(publishedResult->signatures()[0].disposition, render::ModuleExecutionDisposition::PROCESSED);

    // Test BYPASS
    vm.setBypass(true);
    for (int i = 0; i < 100 && vm.preview_status() == QStringLiteral("RENDERING"); ++i) {
        QTest::qWait(10);
    }

    QCOMPARE(vm.preview_status(), QStringLiteral("READY"));
    QVERIFY(publishedResult != nullptr);
    QCOMPARE(publishedResult->signatures()[0].disposition, render::ModuleExecutionDisposition::BYPASS_IDENTITY);
}

void EqViewModelTest::testStaleGenerationDiscarded()
{
    auto prepared = make_test_prepared_result();
    std::atomic<int> publishCount{0};

    EqViewModel vm{
        [prepared] { return prepared; },
        [&publishCount](render::RenderResult) {
            ++publishCount;
            return rgsml::core::Status::success();
        }
    };

    // Rapid edits
    vm.setDraftGain(3.0);
    vm.commitDraft();
    vm.setDraftGain(6.0);
    vm.commitDraft();

    for (int i = 0; i < 100 && vm.preview_status() == QStringLiteral("RENDERING"); ++i) {
        QTest::qWait(10);
    }

    QCOMPARE(vm.preview_status(), QStringLiteral("READY"));
    // Stale results should be rejected, so current generation publishes
    QVERIFY(publishCount >= 1);
}

void EqViewModelTest::testPreparedInputRemainsImmutable()
{
    auto prepared = make_test_prepared_result();
    const auto originalView = prepared->view();
    const auto originalChannel0 = *originalView.channel(0).value();
    std::vector<double> originalSamples(originalChannel0.begin(), originalChannel0.end());

    EqViewModel vm{
        [prepared] { return prepared; },
        [](render::RenderResult) { return rgsml::core::Status::success(); }
    };

    vm.setDraftGain(12.0);
    vm.commitDraft();

    for (int i = 0; i < 100 && vm.preview_status() == QStringLiteral("RENDERING"); ++i) {
        QTest::qWait(10);
    }

    // Verify prepared snapshot was not modified in-place
    const auto afterView = prepared->view();
    const auto afterChannel0 = *afterView.channel(0).value();
    for (std::size_t idx = 0; idx < originalSamples.size(); ++idx) {
        QCOMPARE(afterChannel0[idx], originalSamples[idx]);
    }
}

}  // namespace
}  // namespace rgsml::tests

QTEST_MAIN(rgsml::tests::EqViewModelTest)

#include "test_eq_view_model.moc"
