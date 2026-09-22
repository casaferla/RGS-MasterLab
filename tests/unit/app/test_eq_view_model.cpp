#include "eq_view_model.hpp"

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>
#include <rgsml/render/render_result.hpp>

#include <QUuid>
#include <QVariant>
#include <QtTest/QTest>

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace rgsml::app;

[[nodiscard]] std::shared_ptr<render::RenderResult> make_test_prepared_result(
    core::SampleRate rate = *core::SampleRate::create(48000).value(),
    audio::ChannelLayout layout = audio::ChannelLayout::STEREO_LR)
{
    auto format = audio::AudioFormat::create(rate, layout);
    auto count = core::FrameCount::create(100);
    auto buffer = audio::AudioBuffer::create(
        *format.value(), audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0}, *count.value());
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = dsp::ProcessingChain::create(
        *registry.value(),
        dsp::ProcessingChainContext{
            dsp::ProcessingStage::MASTER, dsp::ChainSegment::MANUAL});
    auto request = render::RenderRequest::create(
        buffer.value()->view(), buffer.value()->view().absolute_range(),
        *chain.value(), {}, *core::FrameCount::create(7).value());
    auto result = render::render_preview(*request.value(), *registry.value());
    Q_ASSERT(result);
    return std::make_shared<render::RenderResult>(std::move(*result.value()));
}

class EqViewModelTest final : public QObject {
    Q_OBJECT

private slots:
    void testInvalidTextDraftAndCommitRejection();
    void testMonoPreparedRoutingRejection();
    void testAllSixFilterTypesAndApplicability();
    void testStableBandIds();
    void testBandCardinalityAndRemoveSelection();
    void testEnabledDisabledSonicCommit();
    void testHardRangeFamilyRejection();
    void testSampleRateUpperFrequencyBound();
    void testDeterministicStaleCompletionRejection();
    void testCrossSourceStalePreviewInvalidation();
    void testRealProductionPathActiveAndBypass();
    void testCurrentFailurePreservesLastGoodAudio();
    void testPreparedInputRemainsImmutable();
    void testSelectionOnlyNoPreview();
    void testGraphDragNoPreviewReleaseOneCommit();
    void testMixedRouting();
    void testSelectedBandResponseOnly();
};

void EqViewModelTest::testInvalidTextDraftAndCommitRejection()
{
    EqViewModel vm;
    const quint64 origGen = vm.preview_generation();
    const double origFreq = vm.frequency();

    for (const auto& invalidStr : {QStringLiteral(""), QStringLiteral("-"), QStringLiteral("."), QStringLiteral("abc"), QStringLiteral("1000.abc")}) {
        vm.setDraftFrequencyText(invalidStr);
        QCOMPARE(vm.frequency_text(), invalidStr);
        QCOMPARE(vm.preview_generation(), origGen);

        QVERIFY(!vm.commitDraft());

        QCOMPARE(vm.frequency_text(), invalidStr);
        QCOMPARE(vm.frequency(), origFreq);
        QCOMPARE(vm.preview_generation(), origGen);
    }

    vm.cancelDraft();
    QCOMPARE(vm.frequency_text(), QString::number(origFreq));
    QCOMPARE(vm.frequency(), origFreq);

    vm.setDraftFrequencyText(QStringLiteral("2500"));
    QVERIFY(vm.commitDraft());
    QCOMPARE(vm.frequency(), 2500.0);
    QCOMPARE(vm.frequency_text(), QStringLiteral("2500"));
    QCOMPARE(vm.preview_generation(), origGen + 1U);
}

void EqViewModelTest::testMonoPreparedRoutingRejection()
{
    auto monoPrepared = make_test_prepared_result(
        *core::SampleRate::create(48000).value(), audio::ChannelLayout::MONO_C);

    EqViewModel vm{[monoPrepared] { return monoPrepared; }};

    const quint64 origGen = vm.preview_generation();
    QVERIFY(!vm.route_available());

    vm.setRouting(QStringLiteral("STEREO"));
    QCOMPARE(vm.routing_label(), QStringLiteral("STEREO"));

    for (const auto& invalidRoute : {QStringLiteral("MID"), QStringLiteral("SIDE"), QStringLiteral("LEFT"), QStringLiteral("RIGHT")}) {
        vm.setRouting(invalidRoute);
        QCOMPARE(vm.routing_label(), QStringLiteral("STEREO"));
        QCOMPARE(vm.preview_generation(), origGen);
    }
}

void EqViewModelTest::testAllSixFilterTypesAndApplicability()
{
    EqViewModel vm;

    vm.setFilter(QStringLiteral("BELL"));
    QCOMPARE(vm.filter_label(), QStringLiteral("BELL"));
    QVERIFY(vm.gain_applicable());
    QVERIFY(vm.q_applicable());
    QVERIFY(!vm.shelf_slope_applicable());
    QVERIFY(!vm.slope_applicable());
    QCOMPARE(vm.gain(), 0.0);
    QCOMPARE(vm.q(), 0.707);

    vm.setFilter(QStringLiteral("NOTCH"));
    QCOMPARE(vm.filter_label(), QStringLiteral("NOTCH"));
    QVERIFY(!vm.gain_applicable());
    QVERIFY(vm.q_applicable());
    QCOMPARE(vm.q(), 0.707);

    vm.setFilter(QStringLiteral("LOW_SHELF"));
    QCOMPARE(vm.filter_label(), QStringLiteral("LOW_SHELF"));
    QVERIFY(vm.gain_applicable());
    QVERIFY(vm.shelf_slope_applicable());
    QCOMPARE(vm.gain(), 0.0);
    QCOMPARE(vm.shelf_slope(), 1.0);

    vm.setFilter(QStringLiteral("HIGH_SHELF"));
    QCOMPARE(vm.filter_label(), QStringLiteral("HIGH_SHELF"));
    QVERIFY(vm.gain_applicable());
    QVERIFY(vm.shelf_slope_applicable());

    vm.setFilter(QStringLiteral("HIGH_PASS"));
    QCOMPARE(vm.filter_label(), QStringLiteral("HIGH_PASS"));
    QVERIFY(vm.slope_applicable());
    QCOMPARE(vm.slope_db_per_oct(), 12);

    vm.setFilter(QStringLiteral("LOW_PASS"));
    QCOMPARE(vm.filter_label(), QStringLiteral("LOW_PASS"));
    QVERIFY(vm.slope_applicable());
    QCOMPARE(vm.slope_db_per_oct(), 12);
}

void EqViewModelTest::testStableBandIds()
{
    std::uint32_t idCounter = 0;
    auto mockIdGen = [&idCounter]() -> rgsml::core::Uuid {
        ++idCounter;
        char buf[37];
        std::snprintf(buf, sizeof(buf), "00000000-0000-0000-0000-%012u", idCounter);
        return *rgsml::core::Uuid::parse(buf).value();
    };

    EqViewModel vm{nullptr, nullptr, mockIdGen};

    const QString band0Id = vm.selected_band_id();
    QVERIFY(!band0Id.isEmpty());

    vm.addBand();
    const QString band1Id = vm.selected_band_id();
    QVERIFY(!band1Id.isEmpty());
    QVERIFY(band0Id != band1Id);

    vm.selectBand(0);
    QCOMPARE(vm.selected_band_id(), band0Id);

    vm.setFilter(QStringLiteral("NOTCH"));
    QCOMPARE(vm.selected_band_id(), band0Id);

    vm.setRouting(QStringLiteral("MID"));
    QCOMPARE(vm.selected_band_id(), band0Id);

    vm.setDraftFrequency(3000.0);
    vm.commitDraft();
    QCOMPARE(vm.selected_band_id(), band0Id);
}

void EqViewModelTest::testBandCardinalityAndRemoveSelection()
{
    std::uint32_t idCounter = 0;
    auto mockIdGen = [&idCounter]() -> rgsml::core::Uuid {
        ++idCounter;
        char buf[37];
        std::snprintf(buf, sizeof(buf), "00000000-0000-0000-0000-%012u", idCounter);
        return *rgsml::core::Uuid::parse(buf).value();
    };

    EqViewModel vm{nullptr, nullptr, mockIdGen};

    QCOMPARE(vm.band_count(), 1);

    for (int i = 0; i < 5; ++i) {
        QVERIFY(vm.add_available());
        vm.addBand();
    }
    QCOMPARE(vm.band_count(), 6);
    QVERIFY(!vm.add_available());

    // 7th add attempt rejected
    vm.addBand();
    QCOMPARE(vm.band_count(), 6);

    // Remove middle band (select index 2, record next band ID at index 3, remove -> selectedBandId equals nextBandId)
    vm.selectBand(3);
    const QString nextBandId = vm.selected_band_id();
    vm.selectBand(2);
    vm.removeSelectedBand();
    QCOMPARE(vm.band_count(), 5);
    QCOMPARE(vm.selected_index(), 2);
    QCOMPARE(vm.selected_band_id(), nextBandId);

    // Remove final band (select index 4, record previous band ID at index 3, remove -> selectedBandId equals prevBandId)
    vm.selectBand(3);
    const QString prevBandId = vm.selected_band_id();
    vm.selectBand(4);
    vm.removeSelectedBand();
    QCOMPARE(vm.band_count(), 4);
    QCOMPARE(vm.selected_index(), 3);
    QCOMPARE(vm.selected_band_id(), prevBandId);

    for (int i = 0; i < 3; ++i) {
        vm.removeSelectedBand();
    }
    QCOMPARE(vm.band_count(), 1);
    QVERIFY(!vm.remove_available());

    // Reject removal of final remaining band
    vm.removeSelectedBand();
    QCOMPARE(vm.band_count(), 1);
}

void EqViewModelTest::testEnabledDisabledSonicCommit()
{
    EqViewModel vm;
    const quint64 origGen = vm.preview_generation();

    // Disable selected band
    vm.setEnabled(false);
    QCOMPARE(vm.preview_generation(), origGen + 1U);
    QVERIFY(!vm.enabled());
    QVERIFY(!vm.committed_parameters().bands()[0].enabled());

    // Re-enable selected band
    vm.setEnabled(true);
    QCOMPARE(vm.preview_generation(), origGen + 2U);
    QVERIFY(vm.enabled());
    QVERIFY(vm.committed_parameters().bands()[0].enabled());
}

void EqViewModelTest::testHardRangeFamilyRejection()
{
    EqViewModel vm;
    const quint64 origGen = vm.preview_generation();
    const auto origParams = vm.committed_parameters();

    // Gain out of range
    vm.setDraftGain(-19.0);
    QVERIFY(!vm.commitDraft());
    QCOMPARE(vm.preview_generation(), origGen);
    QCOMPARE(vm.committed_parameters(), origParams);

    vm.setDraftGain(+19.0);
    QVERIFY(!vm.commitDraft());
    QCOMPARE(vm.preview_generation(), origGen);
    QCOMPARE(vm.committed_parameters(), origParams);

    // Q out of range
    vm.setDraftQ(0.05);
    QVERIFY(!vm.commitDraft());
    QCOMPARE(vm.preview_generation(), origGen);
    QCOMPARE(vm.committed_parameters(), origParams);

    vm.setDraftQ(13.0);
    QVERIFY(!vm.commitDraft());
    QCOMPARE(vm.preview_generation(), origGen);
    QCOMPARE(vm.committed_parameters(), origParams);

    // Shelf slope out of range
    vm.setFilter(QStringLiteral("LOW_SHELF"));
    const quint64 shelfGen = vm.preview_generation();
    const auto shelfParams = vm.committed_parameters();

    vm.setDraftShelfSlope(0.05);
    QVERIFY(!vm.commitDraft());
    QCOMPARE(vm.preview_generation(), shelfGen);
    QCOMPARE(vm.committed_parameters(), shelfParams);

    vm.setDraftShelfSlope(1.5);
    QVERIFY(!vm.commitDraft());
    QCOMPARE(vm.preview_generation(), shelfGen);
    QCOMPARE(vm.committed_parameters(), shelfParams);
}

void EqViewModelTest::testSampleRateUpperFrequencyBound()
{
    auto rate32k = make_test_prepared_result(*core::SampleRate::create(32000).value());

    EqViewModel vm{[rate32k] { return rate32k; }};

    vm.setDraftFrequency(15000.0);
    QVERIFY(!vm.commitDraft());

    vm.setDraftFrequency(14000.0);
    QVERIFY(vm.commitDraft());
    QCOMPARE(vm.frequency(), 14000.0);
}

void EqViewModelTest::testDeterministicStaleCompletionRejection()
{
    auto prepared = make_test_prepared_result();
    std::vector<std::uint64_t> publishedGenerations;

    EqViewModel vm{
        [prepared] { return prepared; },
        [&publishedGenerations](render::RenderResult res) {
            publishedGenerations.push_back(res.chain_revision());
            return rgsml::core::Status::success();
        }
    };

    std::atomic<bool> blockJob1{true};
    std::mutex cvMutex;
    std::condition_variable cv;
    std::atomic<int> executorCalls{0};

    vm.set_preview_executor([&](const EqViewModel::PreviewJob&) {
        const int callNum = ++executorCalls;
        if (callNum == 1) { // Job 1 (gen 1) blocks
            std::unique_lock lock{cvMutex};
            cv.wait(lock, [&] { return !blockJob1.load(); });
        }
        auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
        auto chain = dsp::ProcessingChain::create(
            *registry.value(),
            dsp::ProcessingChainContext{
                dsp::ProcessingStage::MASTER, dsp::ChainSegment::MANUAL});
        auto request = render::RenderRequest::create(
            prepared->view(), prepared->view().absolute_range(),
            *chain.value(), {}, *core::FrameCount::create(7).value());
        return render::render_preview(*request.value(), *registry.value());
    });

    // 1. Commit 1 -> starts Job 1 and BLOCKS
    vm.setDraftGain(3.0);
    vm.commitDraft();

    while (executorCalls.load() < 1) {
        QTest::qWait(5);
    }

    // 2. Commit 2 -> advances generation to 2
    vm.setDraftGain(6.0);
    vm.commitDraft();

    // 3. Unblock Job 1 (generation 1 is now stale)
    {
        const std::scoped_lock lock{cvMutex};
        blockJob1 = false;
    }
    cv.notify_all();

    for (int i = 0; i < 100 && vm.preview_status() == QStringLiteral("RENDERING"); ++i) {
        QTest::qWait(10);
    }

    QCOMPARE(vm.preview_status(), QStringLiteral("READY"));
    QCOMPARE(vm.preview_generation(), 2U);
    QCOMPARE(vm.stale_results_discarded(), 1U);
    QCOMPARE(publishedGenerations.size(), std::size_t{1});
}

void EqViewModelTest::testCrossSourceStalePreviewInvalidation()
{
    auto preparedA = make_test_prepared_result(*core::SampleRate::create(48000).value());
    auto preparedB = make_test_prepared_result(*core::SampleRate::create(44100).value());

    std::shared_ptr<const render::RenderResult> currentPrepared = preparedA;
    std::vector<std::shared_ptr<const render::RenderResult>> publishedResults;

    EqViewModel vm{
        [&currentPrepared] { return currentPrepared; },
        [&publishedResults](render::RenderResult res) {
            publishedResults.push_back(std::make_shared<render::RenderResult>(std::move(res)));
            return rgsml::core::Status::success();
        }
    };

    std::atomic<bool> blockA{true};
    std::mutex cvMutex;
    std::condition_variable cv;
    std::atomic<int> executorCalls{0};

    vm.set_preview_executor([&](const EqViewModel::PreviewJob& job) {
        const int callNum = ++executorCalls;
        if (callNum == 1) { // Source A preview blocks
            std::unique_lock lock{cvMutex};
            cv.wait(lock, [&] { return !blockA.load(); });
        }
        auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
        auto chain = dsp::ProcessingChain::create(
            *registry.value(),
            dsp::ProcessingChainContext{
                dsp::ProcessingStage::MASTER, dsp::ChainSegment::MANUAL});
        auto request = render::RenderRequest::create(
            job.preparedSnapshot->view(), job.preparedSnapshot->view().absolute_range(),
            *chain.value(), {}, *core::FrameCount::create(7).value());
        return render::render_preview(*request.value(), *registry.value());
    });

    // Start preview for Source A
    vm.setDraftGain(3.0);
    vm.commitDraft();

    while (executorCalls.load() < 1) {
        QTest::qWait(5);
    }

    // Replace snapshot with Source B and trigger preview
    currentPrepared = preparedB;
    vm.trigger_preview();

    // Unblock Source A preview
    {
        const std::scoped_lock lock{cvMutex};
        blockA = false;
    }
    cv.notify_all();

    for (int i = 0; i < 100 && vm.preview_status() == QStringLiteral("RENDERING"); ++i) {
        QTest::qWait(10);
    }

    QCOMPARE(vm.preview_status(), QStringLiteral("READY"));
    QCOMPARE(publishedResults.size(), std::size_t{1});
    QCOMPARE(publishedResults[0]->view().format().sample_rate().value(), 44100);
}

void EqViewModelTest::testRealProductionPathActiveAndBypass()
{
    auto prepared = make_test_prepared_result();
    std::shared_ptr<render::RenderResult> publishedResult;

    // Use default production PreviewExecutor (nullptr)
    EqViewModel vm{
        [prepared] { return prepared; },
        [&publishedResult](render::RenderResult res) {
            publishedResult = std::make_shared<render::RenderResult>(std::move(res));
            return rgsml::core::Status::success();
        }
    };

    // Active case
    vm.setDraftGain(6.0);
    vm.commitDraft();

    for (int i = 0; i < 100 && vm.preview_status() == QStringLiteral("RENDERING"); ++i) {
        QTest::qWait(10);
    }

    QCOMPARE(vm.preview_status(), QStringLiteral("READY"));
    QVERIFY(publishedResult != nullptr);
    QCOMPARE(publishedResult->signatures().size(), std::size_t{1});
    QCOMPARE(publishedResult->signatures()[0].disposition, render::ModuleExecutionDisposition::PROCESSED);

    // Bypass case
    vm.setBypass(true);

    for (int i = 0; i < 100 && vm.preview_status() == QStringLiteral("RENDERING"); ++i) {
        QTest::qWait(10);
    }

    QCOMPARE(vm.preview_status(), QStringLiteral("READY"));
    QVERIFY(publishedResult != nullptr);
    QCOMPARE(publishedResult->signatures()[0].disposition, render::ModuleExecutionDisposition::BYPASS_IDENTITY);
}

void EqViewModelTest::testCurrentFailurePreservesLastGoodAudio()
{
    auto prepared = make_test_prepared_result();
    int publishCount = 0;

    EqViewModel vm{
        [prepared] { return prepared; },
        [&publishCount](render::RenderResult) {
            ++publishCount;
            return rgsml::core::Status::success();
        }
    };

    bool failNext = false;
    vm.set_preview_executor([prepared, &failNext](const EqViewModel::PreviewJob&) {
        if (failNext) {
            return core::Result<render::RenderResult>::failure(core::Error{
                core::ErrorCode::InvalidState, "Simulated render failure."});
        }
        auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
        auto chain = dsp::ProcessingChain::create(
            *registry.value(),
            dsp::ProcessingChainContext{
                dsp::ProcessingStage::MASTER, dsp::ChainSegment::MANUAL});
        auto request = render::RenderRequest::create(
            prepared->view(), prepared->view().absolute_range(),
            *chain.value(), {}, *core::FrameCount::create(7).value());
        return render::render_preview(*request.value(), *registry.value());
    });

    vm.setDraftGain(3.0);
    vm.commitDraft();

    for (int i = 0; i < 100 && vm.preview_status() == QStringLiteral("RENDERING"); ++i) {
        QTest::qWait(10);
    }

    QCOMPARE(vm.preview_status(), QStringLiteral("READY"));
    QCOMPARE(publishCount, 1);

    failNext = true;
    vm.setDraftGain(6.0);
    vm.commitDraft();

    for (int i = 0; i < 100 && vm.preview_status() == QStringLiteral("RENDERING"); ++i) {
        QTest::qWait(10);
    }

    QCOMPARE(publishCount, 1);
    QCOMPARE(vm.preview_status(), QStringLiteral("ERROR"));
    QVERIFY(!vm.preview_error().isEmpty());
    QCOMPARE(vm.gain(), 6.0);
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

    const auto afterView = prepared->view();
    const auto afterChannel0 = *afterView.channel(0).value();
    for (std::size_t idx = 0; idx < originalSamples.size(); ++idx) {
        QCOMPARE(afterChannel0[idx], originalSamples[idx]);
    }
}

void EqViewModelTest::testSelectionOnlyNoPreview()
{
    EqViewModel vm;
    vm.addBand();
    const quint64 gen = vm.preview_generation();

    vm.selectBand(0);
    QCOMPARE(vm.preview_generation(), gen);
}

void EqViewModelTest::testGraphDragNoPreviewReleaseOneCommit()
{
    EqViewModel vm;
    const quint64 origGen = vm.preview_generation();

    vm.graphDrag(1500.0, 6.0);
    QCOMPARE(vm.frequency(), 1500.0);
    QCOMPARE(vm.gain(), 6.0);
    QCOMPARE(vm.preview_generation(), origGen);

    vm.graphRelease();
    QCOMPARE(vm.preview_generation(), origGen + 1U);
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

}  // namespace
}  // namespace rgsml::tests

QTEST_MAIN(rgsml::tests::EqViewModelTest)

#include "test_eq_view_model.moc"
