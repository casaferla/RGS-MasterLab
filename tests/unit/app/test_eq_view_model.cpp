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
    void testBandSummariesAndValidationPresentation();
    void testPreparedSampleRate44100ResponseGridRegression();
    void testSelectionOnlyNoCommitOrPreview();
    void testGraphDragReleaseAndPersistenceAcrossSelection();
    void testNonGainFilterGraphDragNoGainMutation();
    void testDensifiedResponseGridIncludesExactF0AndLocalRefinement();
    void testSecondaryParameterWheelAdjustmentAndDebounceCommit();
    void testNewSourceResetClearsStateAndHistory();
    void testResetToFlatAlreadyFlatIsNoOp();
    void testResetToFlatUndoRedo();
    void testHistoryStackDepthCapAt50();
    void testNewEditInvalidatesRedoStack();
    void testWholeEqCombinedResponseEvaluation();
    void testOverallToggleIsViewStateOnly();
    void testIsDefaultSemantics();
    void testAttachingToExistingAuthorityPreservesState();
    void testResetToFlatWhenBypassedClearsBypass();
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
    QCOMPARE(publishedResult->signatures().size(), std::size_t{2});
    QCOMPARE(publishedResult->signatures()[0].type_id, std::string("rgsml.dsp.gain"));
    QCOMPARE(publishedResult->signatures()[1].type_id, std::string("rgsml.dsp.parametric-eq"));
    QCOMPARE(publishedResult->signatures()[1].disposition, render::ModuleExecutionDisposition::PROCESSED);

    // Bypass case
    vm.setBypass(true);

    for (int i = 0; i < 100 && vm.preview_status() == QStringLiteral("RENDERING"); ++i) {
        QTest::qWait(10);
    }

    QCOMPARE(vm.preview_status(), QStringLiteral("READY"));
    QVERIFY(publishedResult != nullptr);
    QCOMPARE(publishedResult->signatures()[1].disposition, render::ModuleExecutionDisposition::BYPASS_IDENTITY);
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
    QVERIFY(vm.selected_band_response_points().size() >= 512);
}

void EqViewModelTest::testBandSummariesAndValidationPresentation()
{
    EqViewModel vm;
    QCOMPARE(vm.band_summaries().size(), 1);
    QVERIFY(vm.validation_field().isEmpty());
    QVERIFY(vm.validation_message().isEmpty());

    auto summary0 = vm.band_summaries().at(0).toMap();
    QCOMPARE(summary0.value("index").toInt(), 0);
    QVERIFY(!summary0.value("bandId").toString().isEmpty());
    QVERIFY(summary0.value("enabled").toBool());
    QCOMPARE(summary0.value("filter").toString(), QStringLiteral("BELL"));
    QCOMPARE(summary0.value("routing").toString(), QStringLiteral("STEREO"));
    QCOMPARE(summary0.value("frequency").toDouble(), 1000.0);
    QCOMPARE(summary0.value("gain").toDouble(), 0.0);
    QVERIFY(summary0.value("gainApplicable").toBool());

    vm.setDraftFrequencyText(QStringLiteral("abc"));
    QCOMPARE(vm.validation_field(), QStringLiteral("frequency"));
    QVERIFY(!vm.validation_message().isEmpty());

    vm.cancelDraft();
    QVERIFY(vm.validation_field().isEmpty());
    QVERIFY(vm.validation_message().isEmpty());

    vm.setDraftGainText(QStringLiteral("25"));
    QCOMPARE(vm.validation_field(), QStringLiteral("gain"));
    QVERIFY(!vm.validation_message().isEmpty());

    vm.cancelDraft();
    QVERIFY(vm.validation_field().isEmpty());
}

void EqViewModelTest::testPreparedSampleRate44100ResponseGridRegression()
{
    auto rate441k = make_test_prepared_result(*core::SampleRate::create(44100).value());
    EqViewModel vm{[rate441k] { return rate441k; }};

    const auto points = vm.selected_band_response_points();
    QVERIFY(!points.isEmpty());
    QVERIFY(points.size() >= 512);

    const auto firstPt = points.first().toMap();
    const auto lastPt = points.last().toMap();

    const double firstFreq = firstPt.value("frequency").toDouble();
    const double lastFreq = lastPt.value("frequency").toDouble();

    QVERIFY(firstFreq >= 20.0);
    QVERIFY(lastFreq <= 19845.0);
    QCOMPARE(lastFreq, 19845.0);
}

void EqViewModelTest::testSelectionOnlyNoCommitOrPreview()
{
    EqViewModel vm;
    vm.addBand(); // Adds Band 1 at index 1, preview_generation = 1
    const quint64 genBeforeSelection = vm.preview_generation();
    const auto paramsBeforeSelection = vm.committed_parameters();

    // Select Band 0
    vm.selectBand(0);
    QCOMPARE(vm.selected_index(), 0);
    QCOMPARE(vm.preview_generation(), genBeforeSelection);
    QCOMPARE(vm.committed_parameters(), paramsBeforeSelection);

    // Select Band 1
    vm.selectBand(1);
    QCOMPARE(vm.selected_index(), 1);
    QCOMPARE(vm.preview_generation(), genBeforeSelection);
    QCOMPARE(vm.committed_parameters(), paramsBeforeSelection);
}

void EqViewModelTest::testGraphDragReleaseAndPersistenceAcrossSelection()
{
    EqViewModel vm;
    vm.addBand(); // Band 1 at index 1
    const quint64 genBeforeDrag = vm.preview_generation();

    // Drag Band 1 to 2500 Hz, +6 dB
    vm.graphDrag(2500.0, 6.0);
    QCOMPARE(vm.frequency(), 2500.0);
    QCOMPARE(vm.gain(), 6.0);
    QCOMPARE(vm.preview_generation(), genBeforeDrag);

    // Release commits Band 1
    vm.graphRelease();
    QCOMPARE(vm.preview_generation(), genBeforeDrag + 1U);
    QCOMPARE(vm.frequency(), 2500.0);
    QCOMPARE(vm.gain(), 6.0);

    // Switch to Band 0 and then back to Band 1
    vm.selectBand(0);
    QCOMPARE(vm.selected_index(), 0);

    vm.selectBand(1);
    QCOMPARE(vm.selected_index(), 1);
    QCOMPARE(vm.frequency(), 2500.0);
    QCOMPARE(vm.gain(), 6.0);
}

void EqViewModelTest::testNonGainFilterGraphDragNoGainMutation()
{
    EqViewModel vm;
    vm.setFilter(QStringLiteral("HIGH_PASS"));
    QVERIFY(!vm.gain_applicable());
    const double originalGain = vm.gain();

    const quint64 genBeforeDrag = vm.preview_generation();
    vm.graphDrag(500.0, 12.0); // Pass +12 dB gain attempt to non-gain filter
    QCOMPARE(vm.frequency(), 500.0);
    QCOMPARE(vm.gain(), originalGain); // Gain must not mutate for HIGH_PASS

    vm.graphRelease();
    QCOMPARE(vm.preview_generation(), genBeforeDrag + 1U);
    QCOMPARE(vm.frequency(), 500.0);
    QCOMPARE(vm.gain(), originalGain);
}

void EqViewModelTest::testDensifiedResponseGridIncludesExactF0AndLocalRefinement()
{
    EqViewModel vm;
    vm.setDraftFrequency(1234.5);
    vm.setDraftQ(12.0); // High Q
    vm.commitDraft();

    const auto points = vm.selected_band_response_points();
    QVERIFY(points.size() >= 512);
    QVERIFY(points.size() <= 1024);

    bool exactF0Found = false;
    for (const auto& varPt : points) {
        const double f = varPt.toMap().value("frequency").toDouble();
        if (std::abs(f - 1234.5) < 1e-5) {
            exactF0Found = true;
            break;
        }
    }
    QVERIFY2(exactF0Found, "Exact f0 (1234.5 Hz) must be present in the response grid");
}

void EqViewModelTest::testSecondaryParameterWheelAdjustmentAndDebounceCommit()
{
    EqViewModel vm;
    const quint64 genStart = vm.preview_generation();

    // 1. Bell filter Q adjustment (normal step vs shift step)
    vm.setFilter(QStringLiteral("BELL"));
    const double initialQ = vm.q();
    vm.adjustSecondaryParameter(1, false); // Wheel up
    QVERIFY(vm.q() > initialQ);
    QCOMPARE(vm.preview_generation(), genStart); // No preview render per wheel step

    vm.adjustSecondaryParameter(-1, true); // Shift wheel down
    QVERIFY(vm.q() < initialQ * 1.03);

    // 2. Shelf filter slope adjustment
    vm.setFilter(QStringLiteral("LOW_SHELF"));
    const double initialShelfSlope = vm.shelf_slope();
    vm.adjustSecondaryParameter(-1, false); // Wheel down
    QCOMPARE(vm.shelf_slope(), initialShelfSlope - 0.05);

    // 3. High pass slope discrete stepping
    vm.setFilter(QStringLiteral("HIGH_PASS"));
    QCOMPARE(vm.slope_db_per_oct(), 12);
    vm.adjustSecondaryParameter(1, false); // Wheel up
    QCOMPARE(vm.slope_db_per_oct(), 18);
    vm.adjustSecondaryParameter(-1, false); // Wheel down
    QCOMPARE(vm.slope_db_per_oct(), 12);
}

void EqViewModelTest::testNewSourceResetClearsStateAndHistory()
{
    EqViewModel vm;
    vm.addBand();
    vm.setDraftGain(6.0);
    vm.commitDraft();
    vm.setBypass(true);

    QCOMPARE(vm.band_count(), 2);
    QVERIFY(vm.can_undo());

    vm.resetForNewSource();

    QCOMPARE(vm.band_count(), 1);
    QCOMPARE(vm.selected_index(), 0);
    QCOMPARE(vm.filter_label(), QStringLiteral("BELL"));
    QCOMPARE(vm.routing_label(), QStringLiteral("STEREO"));
    QCOMPARE(vm.frequency(), 1000.0);
    QCOMPARE(vm.gain(), 0.0);
    QCOMPARE(vm.q(), 0.707);
    QVERIFY(!vm.bypass());
    QVERIFY(!vm.can_undo());
    QVERIFY(!vm.can_redo());
}

void EqViewModelTest::testResetToFlatAlreadyFlatIsNoOp()
{
    EqViewModel vm;
    const QString origId = vm.selected_band_id();
    const quint64 origGen = vm.preview_generation();

    // 1. Calling resetToFlat on initial Flat EQ is a no-op
    vm.resetToFlat();
    QCOMPARE(vm.selected_band_id(), origId);
    QCOMPARE(vm.preview_generation(), origGen);
    QVERIFY(!vm.can_undo());
    QVERIFY(!vm.can_redo());

    // 2. Create non-flat state, then Undo back to canonical Flat so Redo is available
    vm.setDraftGain(6.0);
    vm.commitDraft();
    QVERIFY(vm.can_undo());

    vm.undo(); // Now back to canonical Flat, and Redo is available
    QVERIFY(vm.can_redo());
    const QString flatIdAfterUndo = vm.selected_band_id();
    const quint64 genAfterUndo = vm.preview_generation();

    // Call resetToFlat while in canonical Flat state with active Redo stack
    vm.resetToFlat();

    // Verify it is a true no-op
    QCOMPARE(vm.selected_band_id(), flatIdAfterUndo);
    QCOMPARE(vm.preview_generation(), genAfterUndo);
    QVERIFY(vm.can_redo()); // Redo stack MUST remain preserved!

    // Verify Redo can still be executed to restore the non-flat state
    vm.redo();
    QCOMPARE(vm.gain(), 6.0);
}

void EqViewModelTest::testResetToFlatUndoRedo()
{
    EqViewModel vm;
    vm.addBand();
    vm.setDraftFrequency(2500.0);
    vm.setDraftGain(6.0);
    vm.commitDraft();

    QCOMPARE(vm.band_count(), 2);
    QCOMPARE(vm.frequency(), 2500.0);
    QCOMPARE(vm.gain(), 6.0);

    vm.resetToFlat();

    QCOMPARE(vm.band_count(), 1);
    QCOMPARE(vm.frequency(), 1000.0);
    QCOMPARE(vm.gain(), 0.0);
    QVERIFY(vm.can_undo());

    vm.undo();

    QCOMPARE(vm.band_count(), 2);
    QCOMPARE(vm.selected_index(), 1);
    QCOMPARE(vm.frequency(), 2500.0);
    QCOMPARE(vm.gain(), 6.0);
    QVERIFY(vm.can_redo());

    vm.redo();

    QCOMPARE(vm.band_count(), 1);
    QCOMPARE(vm.frequency(), 1000.0);
    QCOMPARE(vm.gain(), 0.0);
}

void EqViewModelTest::testHistoryStackDepthCapAt50()
{
    EqViewModel vm;
    for (int i = 1; i <= 60; ++i) {
        vm.setDraftGain(static_cast<double>(i) * 0.1);
        vm.commitDraft();
    }

    QVERIFY(vm.can_undo());

    int undoCount = 0;
    while (vm.can_undo()) {
        vm.undo();
        ++undoCount;
    }

    QCOMPARE(undoCount, 50); // Capped at exactly 50 history steps
}

void EqViewModelTest::testNewEditInvalidatesRedoStack()
{
    EqViewModel vm;
    vm.setDraftGain(3.0);
    vm.commitDraft();
    vm.setDraftGain(6.0);
    vm.commitDraft();

    QVERIFY(vm.can_undo());
    vm.undo();
    QVERIFY(vm.can_redo());

    // Perform a new edit after undo
    vm.setDraftGain(9.0);
    vm.commitDraft();

    QVERIFY(!vm.can_redo()); // Redo stack must be cleared
}

void EqViewModelTest::testWholeEqCombinedResponseEvaluation()
{
    EqViewModel vm;
    // Flat 1-band initial state -> combined response should be ~0 dB
    const auto flatPoints = vm.combined_response_points();
    QVERIFY(!flatPoints.isEmpty());
    for (const auto& varPt : flatPoints) {
        const double mag = varPt.toMap().value("magnitudeDb").toDouble();
        QVERIFY2(std::abs(mag) < 1e-3, "Flat EQ combined response must be ~0 dB");
    }

    // Add Band 2: High Shelf 8 kHz +6 dB and commit
    vm.addBand();
    vm.setFilter(QStringLiteral("HIGH_SHELF"));
    vm.setDraftFrequency(8000.0);
    vm.setDraftGain(6.0);
    vm.commitDraft();

    // Select Band 1 (index 0): Bell 1 kHz +6 dB and commit
    vm.selectBand(0);
    vm.setDraftGain(6.0);
    vm.commitDraft();

    const auto combinedCommitted1 = vm.combined_response_points();

    // Verify selection change alone does not change combined response points
    vm.selectBand(1);
    const auto combinedCommitted2 = vm.combined_response_points();
    QCOMPARE(combinedCommitted1, combinedCommitted2);

    // Verify uncommitted draft change on selected band does NOT alter combined response
    vm.setDraftGain(12.0); // draft edit only, not committed!
    const auto combinedDuringDraft = vm.combined_response_points();
    QCOMPARE(combinedDuringDraft, combinedCommitted1);

    // Commit the edit -> combined response DOES update
    vm.commitDraft();
    const auto combinedAfterCommit = vm.combined_response_points();
    QVERIFY(combinedAfterCommit != combinedCommitted1);

    // Disable Band 2 (index 1) -> combined response changes
    vm.setEnabled(false);
    const auto combinedDisabled = vm.combined_response_points();
    QVERIFY(combinedDisabled != combinedAfterCommit);
}

void EqViewModelTest::testOverallToggleIsViewStateOnly()
{
    EqViewModel vm;
    const quint64 genBefore = vm.preview_generation();
    const bool canUndoBefore = vm.can_undo();
    const bool canRedoBefore = vm.can_redo();

    QVERIFY(!vm.show_combined_response());

    vm.setShowCombinedResponse(true);

    QVERIFY(vm.show_combined_response());
    QCOMPARE(vm.preview_generation(), genBefore); // No preview request
    QCOMPARE(vm.can_undo(), canUndoBefore); // No undo entry
    QCOMPARE(vm.can_redo(), canRedoBefore);

    vm.setShowCombinedResponse(false);
    QVERIFY(!vm.show_combined_response());
    QCOMPARE(vm.preview_generation(), genBefore);
}

void EqViewModelTest::testIsDefaultSemantics()
{
    EqViewModel vm;

    // 1. Initial canonical Flat EQ state
    QVERIFY(vm.is_default());
    QCOMPARE(vm.property("isDefault").toBool(), true);

    // 2. Bypass state does not alter is_default()
    vm.setBypass(true);
    QVERIFY(vm.bypass());
    QVERIFY(vm.is_default());

    vm.setBypass(false);
    QVERIFY(!vm.bypass());
    QVERIFY(vm.is_default());

    // 3. Manual parameter change makes state non-default
    vm.setDraftGain(3.0);
    vm.commitDraft();
    QVERIFY(!vm.is_default());
    QCOMPARE(vm.property("isDefault").toBool(), false);

    // 4. Reset to Flat restores default status
    vm.resetToFlat();
    QVERIFY(vm.is_default());

    // 5. Adding a second band makes state non-default
    vm.addBand();
    QVERIFY(!vm.is_default());

    // 6. Undo restores default status
    vm.undo();
    QVERIFY(vm.is_default());
}

void EqViewModelTest::testAttachingToExistingAuthorityPreservesState()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    const auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    const auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    const auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();

    const auto band0Id = *core::Uuid::parse("20000000-0000-4000-8000-000000000001").value();
    const auto band1Id = *core::Uuid::parse("20000000-0000-4000-8000-000000000002").value();

    auto band0 = *dsp::EqBandParameters::create(
        band0Id, true, dsp::EqFilterType::HIGH_PASS, dsp::EqRouting::STEREO,
        dsp::PassPayload{80.0, dsp::SlopeDbPerOctave::DB_18}).value();
    auto band1 = *dsp::EqBandParameters::create(
        band1Id, true, dsp::EqFilterType::HIGH_SHELF, dsp::EqRouting::SIDE,
        dsp::ShelfPayload{12000.0, +4.5, 0.75}).value();

    auto customEq = *dsp::ParametricEqParameters::create({band0, band1}).value();

    auto state = MasteringChainState::create(
        *registry.value(), chain_id, gain_id, *dsp::GainParameters::create(0.0).value(), false,
        eq_id, customEq, true);
    QVERIFY(state);

    const auto origEqParams = state.value()->parametric_eq_parameters();

    EqViewModel vm{state.value()};

    QCOMPARE(state.value()->parametric_eq_parameters(), origEqParams);
    QCOMPARE(vm.band_count(), 2);
    QCOMPARE(vm.instance_id(), eq_id);
    QCOMPARE(vm.selected_band_id(), QString::fromStdString(band0Id.to_string()));
    QCOMPARE(vm.filter_label(), QStringLiteral("HIGH_PASS"));
    QCOMPARE(vm.frequency(), 80.0);

    vm.selectBand(1);
    QCOMPARE(vm.selected_band_id(), QString::fromStdString(band1Id.to_string()));
    QCOMPARE(vm.filter_label(), QStringLiteral("HIGH_SHELF"));
    QCOMPARE(vm.routing_label(), QStringLiteral("SIDE"));
    QCOMPARE(vm.frequency(), 12000.0);
    QCOMPARE(vm.gain(), +4.5);

    QVERIFY(vm.bypass());
    QVERIFY(*state.value()->is_bypassed(eq_id).value());
    QCOMPARE(vm.instance_id(), eq_id);
}

void EqViewModelTest::testResetToFlatWhenBypassedClearsBypass()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    const auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    const auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    const auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();
    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id);
    QVERIFY(state);

    MasteringPreviewController previewController{state.value()};
    EqViewModel vm{state.value(), &previewController};

    // 1. Start from canonical flat EQ
    QVERIFY(vm.is_default());
    QVERIFY(!vm.bypass());

    // 2. Set EQ bypass = true
    vm.setBypass(true);
    QVERIFY(vm.bypass());
    QVERIFY(*state.value()->is_bypassed(eq_id).value());
    const std::uint64_t genBypassed = vm.preview_generation();

    // 3. Call resetToFlat()
    vm.resetToFlat();

    // 4. Verify bypass becomes false
    QVERIFY(!vm.bypass());

    // 5. Verify authoritative MasteringChainState reflects false
    QVERIFY(!*state.value()->is_bypassed(eq_id).value());

    // 6. Verify exactly one new preview request occurs
    QCOMPARE(vm.preview_generation(), genBypassed + 1U);

    // 7. Verify undo/redo semantics remain coherent
    QVERIFY(vm.can_undo());
    vm.undo();
    QVERIFY(vm.bypass());
    QVERIFY(*state.value()->is_bypassed(eq_id).value());

    QVERIFY(vm.can_redo());
    vm.redo();
    QVERIFY(!vm.bypass());
    QVERIFY(!*state.value()->is_bypassed(eq_id).value());
}

}  // namespace
}  // namespace rgsml::tests

QTEST_MAIN(rgsml::tests::EqViewModelTest)

#include "test_eq_view_model.moc"
