#include "mastering_preview_controller.hpp"

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>

#include <QUuid>
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

[[nodiscard]] std::shared_ptr<render::RenderResult> make_unit_prepared_result(
    core::SampleRate rate = *core::SampleRate::create(48000).value(),
    double initialVal = 0.5)
{
    auto format = audio::AudioFormat::create(rate, audio::ChannelLayout::STEREO_LR);
    auto count = core::FrameCount::create(100);
    auto buffer = std::move(*audio::AudioBuffer::create(
        *format.value(), audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0}, *count.value()).value());

    auto mutView = buffer.mutable_view();
    auto leftSpan = *mutView.channel(0).value();
    auto rightSpan = *mutView.channel(1).value();
    for (std::size_t i = 0; i < leftSpan.size(); ++i) {
        leftSpan[i] = initialVal;
        rightSpan[i] = initialVal;
    }

    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = dsp::ProcessingChain::create(
        *registry.value(),
        dsp::ProcessingChainContext{dsp::ProcessingStage::MASTER, dsp::ChainSegment::MANUAL});
    auto request = render::RenderRequest::create(
        buffer.view(), buffer.view().absolute_range(),
        *chain.value(), {}, *core::FrameCount::create(7).value());
    auto result = render::render_preview(*request.value(), *registry.value());
    Q_ASSERT(result);
    return std::make_shared<render::RenderResult>(std::move(*result.value()));
}

class MasteringPreviewControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void testFullChainBindingsAndOrder();
    void testNumericEvidenceFullChainGainAndEq();
    void testFourDispositionPermutations();
    void testStaleJobRejection();
    void testRenderFailurePreservesLastGoodAudio();
    void testPublisherFailureReportsErrorWithoutCorruptingState();
    void testPreparedInputRemainsImmutable();
};

void MasteringPreviewControllerTest::testFullChainBindingsAndOrder()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();
    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id);
    QVERIFY(state);

    auto prepared = make_unit_prepared_result();
    std::shared_ptr<render::RenderResult> publishedResult;

    MasteringPreviewController controller{
        state.value(),
        [prepared] { return prepared; },
        [&publishedResult](render::RenderResult res) {
            publishedResult = std::make_shared<render::RenderResult>(std::move(res));
            return core::Status::success();
        }
    };

    controller.request_preview();

    for (int i = 0; i < 100 && controller.preview_status() == QStringLiteral("RENDERING"); ++i) {
        QTest::qWait(10);
    }

    QCOMPARE(controller.preview_status(), QStringLiteral("READY"));
    QVERIFY(publishedResult != nullptr);

    const auto& sigs = publishedResult->signatures();
    QCOMPARE(sigs.size(), std::size_t{2});

    // Binding 0: Input Gain
    QCOMPARE(sigs[0].instance_id, gain_id);
    QCOMPARE(sigs[0].type_id, std::string("rgsml.dsp.gain"));

    // Binding 1: Parametric EQ
    QCOMPARE(sigs[1].instance_id, eq_id);
    QCOMPARE(sigs[1].type_id, std::string("rgsml.dsp.parametric-eq"));
}

void MasteringPreviewControllerTest::testNumericEvidenceFullChainGainAndEq()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();

    // Gain +6.0 dB (scale = 10^(6/20) ~ 1.99526)
    auto gainParams = dsp::GainParameters::create(+6.0);
    auto eqParams = dsp::ParametricEqParameters::create_legacy_default();

    auto state = MasteringChainState::create(
        *registry.value(), chain_id, gain_id, *gainParams.value(), false, eq_id, *eqParams.value(), false);
    QVERIFY(state);

    auto prepared = make_unit_prepared_result(*core::SampleRate::create(48000).value(), 0.1);
    std::shared_ptr<render::RenderResult> publishedResult;

    MasteringPreviewController controller{
        state.value(),
        [prepared] { return prepared; },
        [&publishedResult](render::RenderResult res) {
            publishedResult = std::make_shared<render::RenderResult>(std::move(res));
            return core::Status::success();
        }
    };

    controller.request_preview();

    for (int i = 0; i < 100 && controller.preview_status() == QStringLiteral("RENDERING"); ++i) {
        QTest::qWait(10);
    }

    QCOMPARE(controller.preview_status(), QStringLiteral("READY"));
    QVERIFY(publishedResult != nullptr);

    // Verify output samples are boosted by Gain (+6 dB ~ 2x)
    auto outSpan = *publishedResult->view().channel(0).value();
    const double expectedBoost = 0.1 * std::pow(10.0, 6.0 / 20.0);
    QVERIFY(std::abs(outSpan[0] - expectedBoost) < 1e-3);
}

void MasteringPreviewControllerTest::testFourDispositionPermutations()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();
    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id);
    QVERIFY(state);

    auto prepared = make_unit_prepared_result();
    std::shared_ptr<render::RenderResult> published;

    MasteringPreviewController controller{
        state.value(),
        [prepared] { return prepared; },
        [&published](render::RenderResult res) {
            published = std::make_shared<render::RenderResult>(std::move(res));
            return core::Status::success();
        }
    };

    auto runPreview = [&]() {
        published.reset();
        controller.request_preview();
        for (int i = 0; i < 100 && controller.preview_status() == QStringLiteral("RENDERING"); ++i) {
            QTest::qWait(10);
        }
        QCOMPARE(controller.preview_status(), QStringLiteral("READY"));
        QVERIFY(published != nullptr);
    };

    // 1. Both active
    QVERIFY(state.value()->set_user_bypass(gain_id, false));
    QVERIFY(state.value()->set_user_bypass(eq_id, false));
    runPreview();
    QCOMPARE(published->signatures()[0].disposition, render::ModuleExecutionDisposition::PROCESSED);
    QCOMPARE(published->signatures()[1].disposition, render::ModuleExecutionDisposition::PROCESSED);

    // 2. Gain bypassed + EQ active
    QVERIFY(state.value()->set_user_bypass(gain_id, true));
    QVERIFY(state.value()->set_user_bypass(eq_id, false));
    runPreview();
    QCOMPARE(published->signatures()[0].disposition, render::ModuleExecutionDisposition::BYPASS_IDENTITY);
    QCOMPARE(published->signatures()[1].disposition, render::ModuleExecutionDisposition::PROCESSED);

    // 3. Gain active + EQ bypassed
    QVERIFY(state.value()->set_user_bypass(gain_id, false));
    QVERIFY(state.value()->set_user_bypass(eq_id, true));
    runPreview();
    QCOMPARE(published->signatures()[0].disposition, render::ModuleExecutionDisposition::PROCESSED);
    QCOMPARE(published->signatures()[1].disposition, render::ModuleExecutionDisposition::BYPASS_IDENTITY);

    // 4. Both bypassed
    QVERIFY(state.value()->set_user_bypass(gain_id, true));
    QVERIFY(state.value()->set_user_bypass(eq_id, true));
    runPreview();
    QCOMPARE(published->signatures()[0].disposition, render::ModuleExecutionDisposition::BYPASS_IDENTITY);
    QCOMPARE(published->signatures()[1].disposition, render::ModuleExecutionDisposition::BYPASS_IDENTITY);
}

void MasteringPreviewControllerTest::testStaleJobRejection()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();
    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id);
    QVERIFY(state);

    auto prepared = make_unit_prepared_result();
    std::vector<std::uint64_t> publishedGenerations;

    MasteringPreviewController controller{
        state.value(),
        [prepared] { return prepared; },
        [&publishedGenerations](render::RenderResult res) {
            publishedGenerations.push_back(res.chain_revision());
            return core::Status::success();
        }
    };

    std::atomic<bool> blockJob1{true};
    std::mutex cvMutex;
    std::condition_variable cv;
    std::atomic<int> executorCalls{0};

    controller.set_preview_executor([&](const MasteringPreviewController::PreviewJob& job) {
        const int callNum = ++executorCalls;
        if (callNum == 1) { // Job 1 blocks
            std::unique_lock lock{cvMutex};
            cv.wait(lock, [&] { return !blockJob1.load(); });
        }
        auto reg = dsp::ModuleRegistry::create_dsp_package_v1();
        auto request = render::RenderRequest::create(
            job.preparedSnapshot->view(), job.preparedSnapshot->view().absolute_range(),
            job.chain, job.bindings, *core::FrameCount::create(7).value());
        return render::render_preview(*request.value(), *reg.value());
    });

    // 1. Trigger Job 1
    controller.request_preview();

    while (executorCalls.load() < 1) {
        QTest::qWait(5);
    }

    // 2. Trigger Job 2 (advances generation)
    controller.request_preview();

    // 3. Unblock Job 1
    {
        const std::scoped_lock lock{cvMutex};
        blockJob1 = false;
    }
    cv.notify_all();

    for (int i = 0; i < 100 && controller.preview_status() == QStringLiteral("RENDERING"); ++i) {
        QTest::qWait(10);
    }

    QCOMPARE(controller.preview_status(), QStringLiteral("READY"));
    QCOMPARE(controller.stale_results_discarded(), 1U);
}

void MasteringPreviewControllerTest::testRenderFailurePreservesLastGoodAudio()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();
    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id);
    QVERIFY(state);

    auto prepared = make_unit_prepared_result();
    int publishCount = 0;

    MasteringPreviewController controller{
        state.value(),
        [prepared] { return prepared; },
        [&publishCount](render::RenderResult) {
            ++publishCount;
            return core::Status::success();
        }
    };

    bool failNext = false;
    controller.set_preview_executor([prepared, &failNext](const MasteringPreviewController::PreviewJob& job) {
        if (failNext) {
            return core::Result<render::RenderResult>::failure(core::Error{
                core::ErrorCode::InvalidState, "Simulated render error."});
        }
        auto reg = dsp::ModuleRegistry::create_dsp_package_v1();
        auto request = render::RenderRequest::create(
            job.preparedSnapshot->view(), job.preparedSnapshot->view().absolute_range(),
            job.chain, job.bindings, *core::FrameCount::create(7).value());
        return render::render_preview(*request.value(), *reg.value());
    });

    // Good render
    controller.request_preview();
    for (int i = 0; i < 100 && controller.preview_status() == QStringLiteral("RENDERING"); ++i) {
        QTest::qWait(10);
    }
    QCOMPARE(controller.preview_status(), QStringLiteral("READY"));
    QCOMPARE(publishCount, 1);

    // Failed render
    failNext = true;
    controller.request_preview();
    for (int i = 0; i < 100 && controller.preview_status() == QStringLiteral("RENDERING"); ++i) {
        QTest::qWait(10);
    }
    QCOMPARE(controller.preview_status(), QStringLiteral("ERROR"));
    QCOMPARE(publishCount, 1); // Publisher not called on failure
    QVERIFY(!controller.preview_error().isEmpty());
}

void MasteringPreviewControllerTest::testPublisherFailureReportsErrorWithoutCorruptingState()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();
    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id);
    QVERIFY(state);

    auto prepared = make_unit_prepared_result();

    MasteringPreviewController controller{
        state.value(),
        [prepared] { return prepared; },
        [](render::RenderResult) {
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidArgument, "Publisher rejected candidate."});
        }
    };

    controller.request_preview();
    for (int i = 0; i < 100 && controller.preview_status() == QStringLiteral("RENDERING"); ++i) {
        QTest::qWait(10);
    }

    QCOMPARE(controller.preview_status(), QStringLiteral("ERROR"));
    QCOMPARE(controller.preview_error(), QStringLiteral("Publisher rejected candidate."));

    // State remains valid and active
    QCOMPARE(state.value()->gain_parameters().gain_db(), 0.0);
}

void MasteringPreviewControllerTest::testPreparedInputRemainsImmutable()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();
    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id);
    QVERIFY(state);

    auto prepared = make_unit_prepared_result(*core::SampleRate::create(48000).value(), 0.333);
    const auto channel0Before = *prepared->view().channel(0).value();
    std::vector<double> samplesBefore(channel0Before.begin(), channel0Before.end());

    MasteringPreviewController controller{
        state.value(),
        [prepared] { return prepared; },
        [](render::RenderResult) { return core::Status::success(); }
    };

    controller.request_preview();
    for (int i = 0; i < 100 && controller.preview_status() == QStringLiteral("RENDERING"); ++i) {
        QTest::qWait(10);
    }

    const auto channel0After = *prepared->view().channel(0).value();
    for (std::size_t i = 0; i < samplesBefore.size(); ++i) {
        QCOMPARE(channel0After[i], samplesBefore[i]);
    }
}

}  // namespace
}  // namespace rgsml::tests

QTEST_MAIN(rgsml::tests::MasteringPreviewControllerTest)

#include "test_mastering_preview_controller.moc"
