#include "stereo_ms_view_model.hpp"

#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/stereo_ms_width.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>
#include "../render/render_test_support.hpp"

#include <QtTest/QTest>

#include <cmath>
#include <limits>
#include <memory>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace rgsml::app;

[[nodiscard]] rgsml::dsp::ModuleInstanceId id(const char* uuid)
{
    return *rgsml::dsp::ModuleInstanceId::from_uuid(
        *rgsml::core::Uuid::parse(uuid).value()).value();
}

class StereoMsViewModelTest final : public QObject {
    Q_OBJECT
private slots:
    void optInOnlyAndExactWidthMacro();
    void draftsAreTransactionalAndCommitOnce();
    void modesMuteAndBypassPreserveStoredValues();
    void responsePointsRespectDraftAndActualSignalFormat();
    void canonicalControlsStageAndInvalidEntryBlocksCommit();
    void audibleTelemetryBindsAcceptedRenderAndRealPlayback();
};


void StereoMsViewModelTest::responsePointsRespectDraftAndActualSignalFormat()
{
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto chainId = *rgsml::core::Uuid::parse(
        "54000000-0000-4000-8000-000000000001").value();
    const auto gainId = id("54000000-0000-4000-8000-000000000010");
    const auto eqId = id("54000000-0000-4000-8000-000000000020");
    const auto compId = id("54000000-0000-4000-8000-000000000030");
    const auto stereoId = id("54000000-0000-4000-8000-000000000040");
    const auto defaults = dsp::StereoMsParameters::create_default();
    QVERIFY(defaults);
    auto state = MasteringChainState::create_with_stereo_ms(
        *registry.value(), chainId, gainId, eqId, compId,
        stereoId, *defaults.value(), false);
    QVERIFY(state);
    MasteringPreviewController controller(state.value());
    StereoMsViewModel vm(state.value(), &controller);

    QCOMPARE(vm.width_response_status(), QStringLiteral("SOURCE_UNAVAILABLE"));
    QVERIFY(vm.width_response_points().isEmpty());
    vm.setSignalFormat(48000.0, 1);
    QCOMPARE(vm.width_response_status(), QStringLiteral("MONO_INPUT"));
    QVERIFY(vm.width_response_points().isEmpty());

    vm.setSignalFormat(48000.0, 2);
    QCOMPARE(vm.width_response_status(), QStringLiteral("READY"));
    const auto original = vm.width_response_points();
    QCOMPARE(original.size(), 129);
    QCOMPARE(original.front().toMap().value(QStringLiteral("frequencyHz")).toDouble(), 20.0);
    for (const auto& point : original)
        QVERIFY(std::abs(point.toMap().value(QStringLiteral("widthPercent")).toDouble()
                         - 100.0) < 1e-10);

    // An OFF filter stays flat, but LR24 draft must shape the low Side
    // response before a single commit; no preview generation during drag.
    QVERIFY(vm.setDraftMonoBassMode(QStringLiteral("LR24")));
    QVERIFY(vm.setDraftLowBandWidthPercent(20.0));
    const auto shaped = vm.width_response_points();
    QCOMPARE(shaped.size(), 129);
    QVERIFY(shaped.front().toMap().value(QStringLiteral("widthPercent")).toDouble()
            < 100.0);
    QVERIFY(shaped.back().toMap().value(QStringLiteral("widthPercent")).toDouble()
            > 90.0);
    QCOMPARE(controller.preview_generation(), std::uint64_t{0});
    QVERIFY(vm.setDraftWidthPercent(0.0));
    const auto mono = vm.width_response_points();
    QCOMPARE(mono.size(), 129);
    for (const auto& point : mono)
        QCOMPARE(point.toMap().value(QStringLiteral("widthPercent")).toDouble(), 0.0);
    QCOMPARE(controller.preview_generation(), std::uint64_t{0});
    QVERIFY(vm.commitDraft());
    QCOMPARE(controller.preview_generation(), std::uint64_t{1});

    vm.setBypass(true);
    QCOMPARE(vm.width_response_status(), QStringLiteral("BYPASSED"));
    QVERIFY(vm.width_response_points().isEmpty());
    vm.setBypass(false);
    QCOMPARE(vm.width_response_status(), QStringLiteral("READY"));

    vm.setSignalFormat(std::numeric_limits<double>::quiet_NaN(), 2);
    QCOMPARE(vm.width_response_status(), QStringLiteral("SOURCE_UNAVAILABLE"));
    QVERIFY(vm.width_response_points().isEmpty());

    vm.setSignalFormat(44100.0, 2);
    vm.resetForNewSource();
    QCOMPARE(vm.width_response_status(), QStringLiteral("SOURCE_UNAVAILABLE"));
    QVERIFY(vm.width_response_points().isEmpty());
}



void StereoMsViewModelTest::audibleTelemetryBindsAcceptedRenderAndRealPlayback()
{
    using namespace rgsml::tests::render_support;
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto chainId = *core::Uuid::parse(
        "56000000-0000-4000-8000-000000000001").value();
    const auto gain = id("56000000-0000-4000-8000-000000000010");
    const auto eq = id("56000000-0000-4000-8000-000000000020");
    const auto comp = id("56000000-0000-4000-8000-000000000030");
    const auto ms = id("56000000-0000-4000-8000-000000000040");
    const auto defaults = dsp::StereoMsParameters::create_default();
    QVERIFY(defaults);
    auto state = MasteringChainState::create_with_stereo_ms(
        *registry.value(), chainId, gain, eq, comp, ms, *defaults.value(), false);
    QVERIFY(state);
    StereoMsViewModel vm(state.value());

    // Materialize actual post-M15 render evidence, not a hand-built UI point.
    auto dspChain = dsp::ProcessingChain::create(
        *registry.value(),
        {dsp::ProcessingStage::MASTER, dsp::ChainSegment::MANUAL});
    QVERIFY(dspChain);
    QVERIFY(dspChain.value()->add(ms, "rgsml.dsp.stereo-ms", 0));
    std::vector<double> left(4800, 0.25), right(4800, -0.125);
    auto input = make_buffer(audio::ChannelLayout::STEREO_LR, 0, left, right);
    QVERIFY(input);
    auto req = render::RenderRequest::create(
        input.value()->view(), frame_range(0, 4800),
        *dspChain.value(), {dsp::ModuleExecutionBinding{ms, *defaults.value()}},
        frame_count(127));
    QVERIFY(req);
    auto rendered = render::render_preview(*req.value(), *registry.value());
    QVERIFY(rendered);
    const core::RealizationId oldId{72};
    QVERIFY(rendered.value()->bind_stereo_ms_stage_output_realization_id(oldId));
    auto acceptedOld = std::make_shared<const render::RenderResult>(
        std::move(*rendered.value()));
    QVERIFY(acceptedOld->stereo_ms_stage_output_sidecars().size() == 1);
    QCOMPARE(acceptedOld->signatures().size(), std::size_t{1});

    StereoMsViewModel::AcceptedRenderEvidence accepted{acceptedOld, oldId};
    core::PlaybackSnapshot snapshot;
    snapshot.state = core::PlaybackState::PLAYING;
    snapshot.position = core::FrameIndex{0};
    snapshot.traversalSerial = 1;
    snapshot.audibleRealization.phase = core::AudibleHandoffPhase::NEW;
    snapshot.audibleRealization.realizationId = oldId;
    bool processed = true;
    vm.setTelemetryProviders([&] { return accepted; },
        [&] { return core::Result<core::PlaybackSnapshot>::success(snapshot); },
        [&] { return processed; });
    QCOMPARE(vm.telemetry_status(), QStringLiteral("ACTIVE"));
    QVERIFY(vm.telemetry_active());
    QVERIFY(vm.telemetry_density_buckets().empty());

    // Real Grid20 50ms bucket is 2400 frames; the cursor admits only
    // completed heard intervals.
    snapshot.position = core::FrameIndex{2500};
    vm.refreshTelemetry();
    QCOMPARE(vm.telemetry_density_buckets().size(), 1);
    const auto first = vm.telemetry_density_buckets().front().toMap();
    QCOMPARE(first.value(QStringLiteral("beginFrame")).toLongLong(), qlonglong{0});
    QCOMPARE(first.value(QStringLiteral("endFrame")).toLongLong(), qlonglong{2400});
    QCOMPARE(first.value(QStringLiteral("occupancy")).toList().size(), 1089);
    QCOMPARE(vm.telemetry_realization_id(), QStringLiteral("72"));
    QVERIFY(vm.telemetry_correlation().isEmpty()); // 400ms not yet heard.

    // A newer accepted preview with no known sidecar cannot steal the OLD
    // audible history while playback still reports the prior realization.
    accepted = {};
    vm.refreshTelemetry();
    QCOMPARE(vm.telemetry_status(), QStringLiteral("ACTIVE"));
    QCOMPARE(vm.telemetry_realization_id(), QStringLiteral("72"));
    QCOMPARE(vm.telemetry_density_buckets().size(), 1);

    snapshot.audibleRealization.phase = core::AudibleHandoffPhase::TRANSITION;
    vm.refreshTelemetry();
    QCOMPARE(vm.telemetry_status(), QStringLiteral("TRANSITION"));
    QVERIFY(!vm.telemetry_active());
    QVERIFY(vm.telemetry_density_buckets().empty());
    QVERIFY(vm.telemetry_correlation().isEmpty());

    snapshot.audibleRealization.phase = core::AudibleHandoffPhase::NEW;
    snapshot.audibleRealization.realizationId = core::RealizationId{73};
    snapshot.position = core::FrameIndex{3000};
    vm.refreshTelemetry();
    QCOMPARE(vm.telemetry_status(), QStringLiteral("UNAVAILABLE"));
    QVERIFY(vm.telemetry_density_buckets().empty());

    processed = false;
    vm.refreshTelemetry();
    QCOMPARE(vm.telemetry_status(), QStringLiteral("NOT AUDITIONED"));
    QVERIFY(vm.telemetry_density_buckets().empty());
}

void StereoMsViewModelTest::canonicalControlsStageAndInvalidEntryBlocksCommit()
{
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto chainId = *rgsml::core::Uuid::parse(
        "55000000-0000-4000-8000-000000000001").value();
    const auto gain = id("55000000-0000-4000-8000-000000000010");
    const auto eq = id("55000000-0000-4000-8000-000000000020");
    const auto comp = id("55000000-0000-4000-8000-000000000030");
    const auto stereo = id("55000000-0000-4000-8000-000000000040");
    const auto defaults = dsp::StereoMsParameters::create_default();
    QVERIFY(defaults);
    auto state = MasteringChainState::create_with_stereo_ms(
        *registry.value(), chainId, gain, eq, comp, stereo, *defaults.value(), false);
    QVERIFY(state);
    MasteringPreviewController ctrl(state.value());
    StereoMsViewModel vm(state.value(), &ctrl);
    QVERIFY(vm.setDraftFieldValue(QStringLiteral("monoBassCutoffHz"), 150.0));
    QVERIFY(vm.setDraftFieldText(QStringLiteral("lowBandWidthPercent"),
                                 QStringLiteral("35.5")));
    QVERIFY(!vm.setDraftFieldText(QStringLiteral("sideGainDb"), QStringLiteral("-")));
    QCOMPARE(vm.validation_field(), QStringLiteral("sideGainDb"));
    QVERIFY(!vm.commitDraft());
    QCOMPARE(ctrl.preview_generation(), std::uint64_t{0});
    QCOMPARE(*state.value()->stereo_ms_parameters(), *defaults.value());
    QVERIFY(vm.setDraftFieldText(QStringLiteral("sideGainDb"), QStringLiteral("3.0")));
    QVERIFY(vm.commitDraft());
    QCOMPARE(ctrl.preview_generation(), std::uint64_t{1});
    QCOMPARE(state.value()->stereo_ms_parameters()->mono_bass_cutoff_hz(), 150.0);
    QCOMPARE(state.value()->stereo_ms_parameters()->low_band_width_percent(), 35.5);
    QCOMPARE(state.value()->stereo_ms_parameters()->side_gain_db(), 3.0);
    QVERIFY(!vm.setDraftFieldValue(QStringLiteral("wrong"), 3.0));
    QVERIFY(!vm.validation_message().isEmpty());
    QVERIFY(!vm.commitDraft());
    vm.cancelDraft();
    QCOMPARE(vm.validation_message(), QString{});
}

void StereoMsViewModelTest::optInOnlyAndExactWidthMacro()
{
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto chainId = *rgsml::core::Uuid::parse(
        "51000000-0000-4000-8000-000000000001").value();
    const auto gainId = id("51000000-0000-4000-8000-000000000010");
    const auto eqId = id("51000000-0000-4000-8000-000000000020");
    const auto compId = id("51000000-0000-4000-8000-000000000030");
    const auto msId = id("51000000-0000-4000-8000-000000000040");
    auto legacy = MasteringChainState::create_default(
        *registry.value(), chainId, gainId, eqId, compId);
    QVERIFY(legacy);
    StereoMsViewModel absent(legacy.value());
    QVERIFY(!absent.available());
    QVERIFY(!absent.setDraftWidthPercent(80.0));
    QVERIFY(!absent.commitDraft());
    QCOMPARE(legacy.value()->module_count(), std::size_t{3});

    auto initial = rgsml::dsp::StereoMsParameters::create(
        -3.0, 8.0, false, rgsml::dsp::MonoBassMode::LR12, 140.0, 50.0);
    QVERIFY(initial);
    auto state = MasteringChainState::create_with_stereo_ms(
        *registry.value(), chainId, gainId, eqId, compId,
        msId, *initial.value(), false);
    QVERIFY(state);
    MasteringPreviewController controller(state.value());
    StereoMsViewModel vm(state.value(), &controller);
    QVERIFY(vm.available());
    const double originalCommon = 0.5 * (vm.mid_gain_db() + vm.side_gain_db());
    const auto before = state.value()->stereo_ms_parameters();
    QVERIFY(before);
    QVERIFY(vm.setDraftWidthPercent(0.0));
    QVERIFY(vm.side_muted());
    QCOMPARE(vm.draft_width_percent(), 0.0);
    QCOMPARE(controller.preview_generation(), std::uint64_t{0});
    QCOMPARE(state.value()->stereo_ms_parameters(), before);
    QVERIFY(vm.commitDraft());
    QCOMPARE(controller.preview_generation(), std::uint64_t{1});
    QVERIFY(state.value()->stereo_ms_parameters()->side_muted());
    QCOMPARE(state.value()->stereo_ms_parameters()->side_gain_db(),
             before->side_gain_db());

    QVERIFY(vm.setDraftWidthPercent(100.0));
    QVERIFY(!vm.side_muted());
    QCOMPARE(controller.preview_generation(), std::uint64_t{1});
    QVERIFY(vm.commitDraft());
    QCOMPARE(controller.preview_generation(), std::uint64_t{2});
    const auto& committed = *state.value()->stereo_ms_parameters();
    QVERIFY(std::abs((committed.mid_gain_db() + committed.side_gain_db()) / 2.0
                     - originalCommon) < 1e-12);
    QVERIFY(std::abs(dsp::stereo_ms_width_coordinates(committed).current_width_percent
                     - 100.0) < 1e-10);
}

void StereoMsViewModelTest::draftsAreTransactionalAndCommitOnce()
{
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto chainId = *rgsml::core::Uuid::parse(
        "52000000-0000-4000-8000-000000000001").value();
    const auto gainId = id("52000000-0000-4000-8000-000000000010");
    const auto eqId = id("52000000-0000-4000-8000-000000000020");
    const auto compId = id("52000000-0000-4000-8000-000000000030");
    const auto msId = id("52000000-0000-4000-8000-000000000040");
    auto defaults = rgsml::dsp::StereoMsParameters::create_default();
    QVERIFY(defaults);
    auto state = MasteringChainState::create_with_stereo_ms(
        *registry.value(), chainId, gainId, eqId, compId,
        msId, *defaults.value(), false);
    QVERIFY(state);
    MasteringPreviewController controller(state.value());
    StereoMsViewModel vm(state.value(), &controller);
    QVERIFY(vm.setDraftMidGainDb(-3.0));
    QVERIFY(vm.setDraftSideGainDb(8.0));
    QVERIFY(vm.setDraftMonoBassCutoffHz(200.0));
    QVERIFY(vm.setDraftLowBandWidthPercent(25.0));
    QVERIFY(vm.setDraftMonoBassMode(QStringLiteral("LR24")));
    QCOMPARE(controller.preview_generation(), std::uint64_t{0});
    QCOMPARE(*state.value()->stereo_ms_parameters(), *defaults.value());
    const double oldSide = vm.side_gain_db();
    QVERIFY(!vm.setDraftSideGainDb(std::numeric_limits<double>::infinity()));
    QCOMPARE(vm.side_gain_db(), oldSide);
    QVERIFY(!vm.validation_message().isEmpty());
    QVERIFY(!vm.setDraftWidthPercent(9999.0));
    QVERIFY(!vm.validation_message().isEmpty());
    QCOMPARE(controller.preview_generation(), std::uint64_t{0});
    // Cancel invalid input, then restage the valid values and commit once.
    vm.cancelDraft();
    QCOMPARE(vm.validation_message(), QString{});
    QCOMPARE(*state.value()->stereo_ms_parameters(), *defaults.value());
    QVERIFY(vm.setDraftMidGainDb(-3.0));
    QVERIFY(vm.setDraftSideGainDb(8.0));
    QVERIFY(vm.setDraftMonoBassMode(QStringLiteral("LR24")));
    QVERIFY(vm.commitDraft());
    QCOMPARE(controller.preview_generation(), std::uint64_t{1});
    QCOMPARE(state.value()->stereo_ms_parameters()->mid_gain_db(), -3.0);
    QCOMPARE(state.value()->stereo_ms_parameters()->side_gain_db(), 8.0);
    QCOMPARE(state.value()->stereo_ms_parameters()->mono_bass_mode(),
             rgsml::dsp::MonoBassMode::LR24);
    QVERIFY(vm.commitDraft());
    QCOMPARE(controller.preview_generation(), std::uint64_t{1});
}

void StereoMsViewModelTest::modesMuteAndBypassPreserveStoredValues()
{
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto chainId = *rgsml::core::Uuid::parse(
        "53000000-0000-4000-8000-000000000001").value();
    const auto gainId = id("53000000-0000-4000-8000-000000000010");
    const auto eqId = id("53000000-0000-4000-8000-000000000020");
    const auto compId = id("53000000-0000-4000-8000-000000000030");
    const auto msId = id("53000000-0000-4000-8000-000000000040");
    auto initial = rgsml::dsp::StereoMsParameters::create(
        -3.0, 8.0, false, rgsml::dsp::MonoBassMode::LR24, 190.0, 25.0);
    QVERIFY(initial);
    auto state = MasteringChainState::create_with_stereo_ms(
        *registry.value(), chainId, gainId, eqId, compId,
        msId, *initial.value(), false);
    QVERIFY(state);
    MasteringPreviewController controller(state.value());
    StereoMsViewModel vm(state.value(), &controller);
    QVERIFY(vm.mono_bass_controls_effective());
    QVERIFY(vm.setDraftMonoBassMode(QStringLiteral("OFF")));
    QVERIFY(!vm.mono_bass_controls_effective());
    QCOMPARE(vm.mono_bass_cutoff_hz(), 190.0);
    QCOMPARE(vm.low_band_width_percent(), 25.0);
    QVERIFY(vm.commitDraft());
    QCOMPARE(controller.preview_generation(), std::uint64_t{1});
    QVERIFY(vm.setDraftMonoBassMode(QStringLiteral("LR24")));
    QVERIFY(vm.setDraftSideMuted(true));
    QVERIFY(!vm.mono_bass_controls_effective());
    QVERIFY(vm.commitDraft());
    QCOMPARE(controller.preview_generation(), std::uint64_t{2});
    QCOMPARE(state.value()->stereo_ms_parameters()->mono_bass_cutoff_hz(), 190.0);
    QCOMPARE(state.value()->stereo_ms_parameters()->low_band_width_percent(), 25.0);
    vm.setBypass(true);
    QVERIFY(vm.bypass());
    QCOMPARE(controller.preview_generation(), std::uint64_t{3});
    vm.setBypass(true);
    QCOMPARE(controller.preview_generation(), std::uint64_t{3});
    vm.resetForNewSource();
    QCOMPARE(state.value()->stereo_ms_parameters()->side_muted(), false);
    QVERIFY(vm.bypass());
    QCOMPARE(controller.preview_generation(), std::uint64_t{3});
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::StereoMsViewModelTest)
#include "test_stereo_ms_view_model.moc"
