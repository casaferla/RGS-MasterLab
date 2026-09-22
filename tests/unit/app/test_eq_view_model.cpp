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
#include <memory>
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
    void testSampleRateUpperFrequencyBound();
    void testDeterministicStaleCompletionRejection();
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

    // Set invalid text drafts
    for (const auto& invalidStr : {QStringLiteral(""), QStringLiteral("-"), QStringLiteral("."), QStringLiteral("abc"), QStringLiteral("1000.abc")}) {
        vm.setDraftFrequencyText(invalidStr);
        QCOMPARE(vm.frequency_text(), invalidStr);
        QCOMPARE(vm.preview_generation(), origGen);

        // Rejected commit
        QVERIFY(!vm.commitDraft());

        // Raw invalid text persists in draft, committed value & generation unchanged, no preview
        QCOMPARE(vm.frequency_text(), invalidStr);
        QCOMPARE(vm.frequency(), origFreq);
        QCOMPARE(vm.preview_generation(), origGen);
    }

    // Cancel restores committed value and text
    vm.cancelDraft();
    QCOMPARE(vm.frequency_text(), QString::number(origFreq));
    QCOMPARE(vm.frequency(), origFreq);

    // Subsequent valid text commit succeeds
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

    // STEREO is accepted
    vm.setRouting(QStringLiteral("STEREO"));
    QCOMPARE(vm.routing_label(), QStringLiteral("STEREO"));

    // MID, SIDE, LEFT, RIGHT are rejected on mono PREPARED
    for (const auto& invalidRoute : {QStringLiteral("MID"), QStringLiteral("SIDE"), QStringLiteral("LEFT"), QStringLiteral("RIGHT")}) {
        vm.setRouting(invalidRoute);
        QCOMPARE(vm.routing_label(), QStringLiteral("STEREO"));
        QCOMPARE(vm.preview_generation(), origGen);
    }
}

void EqViewModelTest::testAllSixFilterTypesAndApplicability()
{
    EqViewModel vm;

    // 1. BELL
    vm.setFilter(QStringLiteral("BELL"));
    QCOMPARE(vm.filter_label(), QStringLiteral("BELL"));
    QVERIFY(vm.gain_applicable());
    QVERIFY(vm.q_applicable());
    QVERIFY(!vm.shelf_slope_applicable());
    QVERIFY(!vm.slope_applicable());
    QCOMPARE(vm.gain(), 0.0);
    QCOMPARE(vm.q(), 0.707);

    // 2. NOTCH
    vm.setFilter(QStringLiteral("NOTCH"));
    QCOMPARE(vm.filter_label(), QStringLiteral("NOTCH"));
    QVERIFY(!vm.gain_applicable());
    QVERIFY(vm.q_applicable());
    QCOMPARE(vm.q(), 0.707);

    // 3. LOW_SHELF
    vm.setFilter(QStringLiteral("LOW_SHELF"));
    QCOMPARE(vm.filter_label(), QStringLiteral("LOW_SHELF"));
    QVERIFY(vm.gain_applicable());
    QVERIFY(vm.shelf_slope_applicable());
    QCOMPARE(vm.gain(), 0.0);
    QCOMPARE(vm.shelf_slope(), 1.0);

    // 4. HIGH_SHELF
    vm.setFilter(QStringLiteral("HIGH_SHELF"));
    QCOMPARE(vm.filter_label(), QStringLiteral("HIGH_SHELF"));
    QVERIFY(vm.gain_applicable());
    QVERIFY(vm.shelf_slope_applicable());

    // 5. HIGH_PASS
    vm.setFilter(QStringLiteral("HIGH_PASS"));
    QCOMPARE(vm.filter_label(), QStringLiteral("HIGH_PASS"));
    QVERIFY(vm.slope_applicable());
    QCOMPARE(vm.slope_db_per_oct(), 12);

    // 6. LOW_PASS
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
    QVERIFY(band0Id != band1Id); // Unique IDs on add

    // Selection does not change bandId
    vm.selectBand(0);
    QCOMPARE(vm.selected_band_id(), band0Id);

    // Filter change does not change bandId
    vm.setFilter(QStringLiteral("NOTCH"));
    QCOMPARE(vm.selected_band_id(), band0Id);

    // Routing change does not change bandId
    vm.setRouting(QStringLiteral("MID"));
    QCOMPARE(vm.selected_band_id(), band0Id);

    // Numeric edits do not change bandId
    vm.setDraftFrequency(3000.0);
    vm.commitDraft();
    QCOMPARE(vm.selected_band_id(), band0Id);
}

void EqViewModelTest::testSampleRateUpperFrequencyBound()
{
    // PREPARED sample rate = 32000 Hz. Max frequency = 0.45 * 32000 = 14400 Hz.
    auto rate32k = make_test_prepared_result(*core::SampleRate::create(32000).value());

    EqViewModel vm{[rate32k] { return rate32k; }};

    // 15000 Hz > 14400 Hz -> rejected
    vm.setDraftFrequency(15000.0);
    QVERIFY(!vm.commitDraft());

    // 14000 Hz <= 14400 Hz -> accepted
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
        [&publishedGenerations](render::RenderResult) {
            return rgsml::core::Status::success();
        }
    };

    std::vector<EqViewModel::PreviewJob> capturedJobs;
    std::mutex executorMutex;

    vm.set_preview_executor([&capturedJobs, &executorMutex, prepared](const EqViewModel::PreviewJob& job) {
        const std::scoped_lock lock{executorMutex};
        capturedJobs.push_back(job);
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

    // Edit 1 -> generation 1
    vm.setDraftGain(3.0);
    vm.commitDraft();

    // Edit 2 -> generation 2
    vm.setDraftGain(6.0);
    vm.commitDraft();

    for (int i = 0; i < 100 && vm.preview_status() == QStringLiteral("RENDERING"); ++i) {
        QTest::qWait(10);
    }

    QCOMPARE(vm.preview_status(), QStringLiteral("READY"));
    QVERIFY(vm.stale_results_discarded() >= 1U);
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

    // 1. Successful commit
    vm.setDraftGain(3.0);
    vm.commitDraft();

    for (int i = 0; i < 100 && vm.preview_status() == QStringLiteral("RENDERING"); ++i) {
        QTest::qWait(10);
    }

    QCOMPARE(vm.preview_status(), QStringLiteral("READY"));
    QCOMPARE(publishCount, 1);

    // 2. Failed commit
    failNext = true;
    vm.setDraftGain(6.0);
    vm.commitDraft();

    for (int i = 0; i < 100 && vm.preview_status() == QStringLiteral("RENDERING"); ++i) {
        QTest::qWait(10);
    }

    // Publisher was NOT called again on failure
    QCOMPARE(publishCount, 1);
    QCOMPARE(vm.preview_status(), QStringLiteral("ERROR"));
    QVERIFY(!vm.preview_error().isEmpty());
    // Committed parameters remain the newer committed parameters (6.0 dB)
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
