#include "audition_region_view_model.hpp"
#include "audition_source_selector.hpp"
#include "compressor_view_model.hpp"
#include "dsp_chain_adapter_model.hpp"
#include "eq_view_model.hpp"
#include "gain_view_model.hpp"
#include "gold_selection_view_model.hpp"
#include "live_spectrum_view_model.hpp"
#include "mastering_chain_state.hpp"
#include "mastering_preview_controller.hpp"
#include "playback_transport_view_model.hpp"
#include <rgsml/analysis/live_spectrum_analyzer.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include "project_session_view_model.hpp"
#include "source_selection_view_model.hpp"
#include "source_waveform_view_model.hpp"
#include "stereo_ms_view_model.hpp"
#include "terminal_candidate_gate.hpp"
#include "terminal_audio_session_gate.hpp"
#include "terminal_private_publication_coordinator.hpp"
#include "terminal_private_render_controller.hpp"
#include "terminal_private_product_session.hpp"
#include "terminal_limiter_state.hpp"
#include "terminal_limiter_view_model.hpp"
#include "true_peak_static_coordinator.hpp"
#include "true_peak_live_coordinator.hpp"

#include "waveform_presentation.hpp"

#include <rgsml/platform/windows/windows_audio_playback_service.hpp>

#include <QGuiApplication>
#include <QLibraryInfo>
#include <QUuid>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include <optional>
#include <variant>
#include <QTimer>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>

namespace {

bool smokeWarningObserved = false;

void smokeMessageHandler(QtMsgType type, const QMessageLogContext&, const QString& message)
{
    if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) {
        smokeWarningObserved = true;
        std::fprintf(stderr, "%s\n", message.toLocal8Bit().constData());
    }
}

}  // namespace

int main(int argc, char* argv[])
{
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("RGS MasterLab"));
    const bool deploySmoke = application.arguments().contains(QStringLiteral("--rgsml-deploy-smoke"));
    if (deploySmoke) {
        qInstallMessageHandler(smokeMessageHandler);
    }

    rgsml::analysis::LiveSpectrumAnalyzer spectrumAnalyzer;
    spectrumAnalyzer.start();

    auto playbackService = std::make_unique<
        rgsml::platform::windows::WindowsAudioPlaybackService>();
    auto* windowsPlayback = playbackService.get();
    windowsPlayback->attach_analyzer(&spectrumAnalyzer);

    rgsml::app::LiveSpectrumViewModel liveSpectrumViewModel{&spectrumAnalyzer};
    rgsml::app::PlaybackTransportViewModel playbackTransport{
        std::move(playbackService)};
    playbackTransport.set_pcm_prepare_handler(
        [windowsPlayback](
            rgsml::audio::AudioBufferView source,
            std::shared_ptr<const void> lifetime,
            std::optional<rgsml::core::RealizationId> realizationId) {
            return windowsPlayback->prepare_pcm(
                source, std::move(lifetime), realizationId);
        });
    playbackTransport.set_pcm_handoff_handler(
        [windowsPlayback](
            rgsml::audio::AudioBufferView source,
            std::shared_ptr<const void> lifetime,
            std::optional<rgsml::core::RealizationId> realizationId) {
            return windowsPlayback->handoff_pcm(
                source, std::move(lifetime), realizationId);
        });
    rgsml::ui::WaveformPresentation waveformPresentation;
    rgsml::app::AuditionRegionViewModel auditionRegion{&playbackTransport};
    rgsml::app::SourceWaveformViewModel sourceWaveform{
        &waveformPresentation};
    rgsml::app::SourceSelectionViewModel sourceSelection;
    rgsml::app::AuditionSourceSelector auditionSelector{&playbackTransport};
    rgsml::app::TruePeakStaticCoordinator staticTruePeak;
    // C2D7: audio device callback untouched; only independently bound
    // PROCESSED sink-consumed samples reach the off-thread M16 FIR worker.
    rgsml::app::TruePeakLiveCoordinator liveTruePeak{
        [windowsPlayback] {
            return windowsPlayback->consumed_output_prefix();
        },
        [&playbackTransport] {
            return playbackTransport.playback_snapshot();
        },
        [&auditionSelector, &sourceSelection]()
            -> std::optional<rgsml::app::TruePeakLiveSource> {
            if (auditionSelector.active_target()
                    != rgsml::app::AuditionTarget::PROCESSED)
                return std::nullopt;
            const auto render = auditionSelector.processed_realization_snapshot();
            const auto id = auditionSelector.processed_realization_id();
            const auto count = rgsml::core::FrameCount::create(
                sourceSelection.frame_count());
            const auto rate = rgsml::core::SampleRate::create(
                sourceSelection.sample_rate_hz());
            const int channels = sourceSelection.channel_count();
            if (!render || !id || !count || !rate ||
                (channels != 1 && channels != 2)) return std::nullopt;
            return rgsml::app::TruePeakLiveSource{
                render, *id, rgsml::app::TruePeakExpectedSource{
                    *count.value(), *rate.value(),
                    channels == 1 ? rgsml::audio::ChannelLayout::MONO_C
                                  : rgsml::audio::ChannelLayout::STEREO_LR}};
        }};
    // Source/target/render change invalidates LIVE in the SAME Qt event,
    // before any later worker publication or the next 50-ms observer tick.
    QObject::connect(
        &auditionSelector, &rgsml::app::AuditionSourceSelector::changed,
        &liveTruePeak, &rgsml::app::TruePeakLiveCoordinator::invalidate);
    // Publication of PROCESSED never implies that it is already audible.
    QObject::connect(
        &auditionSelector, &rgsml::app::AuditionSourceSelector::changed,
        &staticTruePeak, [&auditionSelector, &staticTruePeak] {
            staticTruePeak.processed_changed(
                auditionSelector.processed_realization_snapshot(),
                auditionSelector.processed_realization_id());
        });
    auditionSelector.set_source_loop_provider([&auditionRegion] {
        return auditionRegion.loop_enabled()
            ? auditionRegion.region()
            : std::nullopt;
    });
    auto moduleRegistry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    const auto chainUuid = *rgsml::core::Uuid::parse(QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString()).value();
    const auto gainUuid = *rgsml::core::Uuid::parse(QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString()).value();
    const auto eqUuid = *rgsml::core::Uuid::parse(QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString()).value();
    const auto compUuid = *rgsml::core::Uuid::parse(QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString()).value();
    const auto stereoMsUuid = *rgsml::core::Uuid::parse(QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString()).value();
    const auto terminalLimiterUuid = *rgsml::core::Uuid::parse(
        QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString()).value();
    const auto gainInstanceId = *rgsml::dsp::ModuleInstanceId::from_uuid(gainUuid).value();
    const auto eqInstanceId = *rgsml::dsp::ModuleInstanceId::from_uuid(eqUuid).value();
    const auto compInstanceId = *rgsml::dsp::ModuleInstanceId::from_uuid(compUuid).value();
    const auto stereoMsInstanceId = *rgsml::dsp::ModuleInstanceId::from_uuid(stereoMsUuid).value();
    const auto terminalLimiterInstanceId = *rgsml::dsp::ModuleInstanceId::from_uuid(
        terminalLimiterUuid).value();
    // Product startup: four real DSP nodes, milestone order, M15 safely
    // bypassed. Historic saved three-node projects restore unchanged.
    auto masteringChainStateRes = rgsml::app::MasteringChainState::create_product_default(
        *moduleRegistry.value(), chainUuid, gainInstanceId, eqInstanceId,
        compInstanceId, stereoMsInstanceId);
    auto masteringChainState = std::move(*masteringChainStateRes.value());
    // M17 has its own real MASTER/TERMINAL DSP segment, distinct from
    // manual Gain/EQ/Compressor/Stereo-M/S and the M16 analysis-only meter.
    // The current Product audition renderer is still SOURCE_PROCESSING_RATE.
    // Do NOT route this terminal state through Render Preview or pretend
    // that it participates in audible PROCESSED until the output-rate
    // resource/identity handoff is integrated and verified.
    auto terminalLimiterStateResult =
        rgsml::app::TerminalLimiterState::create_default(
            *moduleRegistry.value(), terminalLimiterInstanceId);
    if (!terminalLimiterStateResult) {
        std::fprintf(stderr, "M17 terminal bootstrap refused: %s\n",
                     terminalLimiterStateResult.error()->message().c_str());
        return EXIT_FAILURE;
    }
    auto terminalLimiterState = std::move(*terminalLimiterStateResult.value());
    // Real typed Ceiling VM, but no accepted-render callback until the
    // production terminal job and its stale-result guard exist. QML editor
    // still remains fail-closed with limiterViewModel: null at this milestone.
    rgsml::app::TerminalLimiterViewModel terminalLimiterViewModel{
        &terminalLimiterState};
    // Product-owned but still PRIVATE: no terminal PCM/RealizationId is
    // published by this gate. It will only stage future independently
    // accepted complete OUTPUT_RATE candidates.
    rgsml::app::TerminalCandidateGate terminalCandidateGate;
    // The exact-rate Windows device seam is registered but cannot be
    // invoked by the product until a separate Processed publisher allocates
    // a unique M17 identity and authorizes an end-to-end SOURCE controller.
    rgsml::app::TerminalAudioSessionGate terminalAudioSessionGate{
        terminalCandidateGate,*windowsPlayback,
        [windowsPlayback](
            rgsml::audio::AudioBufferView pcm,
            std::shared_ptr<const void> lifetime,
            rgsml::core::RealizationId terminalIdentity) {
            return windowsPlayback->prepare_terminal_output_pcm(
                pcm,std::move(lifetime),terminalIdentity);
        }};
    // Product-owned but not callable from the UI yet: prepare a single
    // transaction entrypoint that reserves the actual selector's common
    // RealizationId namespace before installing an independently owned PCM.
    rgsml::app::TerminalPrivatePublicationCoordinator privateTerminalPublisher{
        terminalCandidateGate, terminalAudioSessionGate, playbackTransport,
        [&auditionSelector](
            std::shared_ptr<const rgsml::render::RenderResult> prepared,
            std::shared_ptr<const rgsml::render::RenderResult> manual,
            std::optional<rgsml::core::RealizationId> id) {
            return auditionSelector.reserve_private_terminal_realization_id(
                std::move(prepared),std::move(manual),id);
        }};
    static_cast<void>(privateTerminalPublisher);
    // Product-private rendering worker. OUTPUT_RATE is an explicit caller
    // parameter; no assumption about the active Windows device rate.
    rgsml::app::TerminalPrivateRenderController privateTerminalRenderer{
        terminalCandidateGate,terminalLimiterState,
        [&auditionSelector] {return auditionSelector.prepared_realization_snapshot();},
        [&auditionSelector] {return auditionSelector.processed_realization_snapshot();},
        [&auditionSelector] {return auditionSelector.processed_realization_id();},
        [&masteringChainState] {return masteringChainState.chain();}};

    rgsml::app::MasteringPreviewController previewController{
        &masteringChainState,
        [&auditionSelector] {
            return auditionSelector.prepared_realization_snapshot();
        },
        [&auditionSelector](rgsml::render::RenderResult result) {
            return auditionSelector.set_processed_realization(std::move(result));
        }
    };

    // All invalidations atomically revoke terminal device ownership and
    // discard pending OUTPUT_RATE renders before any future publication.
    // This PRIVATE session coordinator is never exposed to QML/UI yet.
    rgsml::app::TerminalPrivateProductSession privateProductSession{
        privateTerminalRenderer,privateTerminalPublisher,
        terminalCandidateGate,playbackTransport,
        [&auditionSelector] {return auditionSelector.prepared_realization_snapshot();},
        [&auditionSelector] {return auditionSelector.processed_realization_snapshot();},
        [&auditionSelector] {return auditionSelector.processed_realization_id();},
        [&auditionRegion] {
            return auditionRegion.loop_enabled()
                ? auditionRegion.region() : std::nullopt;
        },
        [&auditionSelector] {
            return auditionSelector.active_target()
                ==rgsml::app::AuditionTarget::PROCESSED;
        }};
    static_cast<void>(privateProductSession);
    QObject::connect(&auditionSelector,
        &rgsml::app::AuditionSourceSelector::changed,
        &privateProductSession,
        [&privateProductSession] {
            static_cast<void>(privateProductSession.cancel());
        });
    QObject::connect(&terminalLimiterViewModel,
        &rgsml::app::TerminalLimiterViewModel::acceptedStateChanged,
        &privateProductSession,
        [&privateProductSession] {
            static_cast<void>(privateProductSession.cancel());
        });
    QObject::connect(&previewController,
        &rgsml::app::MasteringPreviewController::changed,
        &privateProductSession,
        [&privateProductSession] {
            static_cast<void>(privateProductSession.cancel());
        });
    QObject::connect(&auditionRegion,
        &rgsml::app::AuditionRegionViewModel::changed,
        &privateProductSession,
        &rgsml::app::TerminalPrivateProductSession::cancel_pending_on_region_change);
    rgsml::app::GainViewModel gainViewModel{&masteringChainState, &previewController};
    rgsml::app::EqViewModel eqViewModel{&masteringChainState, &previewController};
    rgsml::app::CompressorViewModel compressorViewModel{&masteringChainState, &previewController};
    compressorViewModel.setTelemetryProviders(
        [&auditionSelector]()
            -> std::shared_ptr<const rgsml::render::CompressorTelemetrySidecar> {
            const auto processed =
                auditionSelector.processed_realization_snapshot();
            if (processed == nullptr) {
                return {};
            }
            const auto& sidecar =
                processed->compressor_telemetry_sidecar();
            if (!sidecar.has_value() || !sidecar->valid) {
                return {};
            }
            return std::shared_ptr<const rgsml::render::CompressorTelemetrySidecar>(
                processed, &*sidecar);
        },
        [&playbackTransport]() {
            return playbackTransport.playback_snapshot();
        },
        [&auditionSelector]() {
            const auto target = auditionSelector.active_target();
            if (!target.has_value()) {
                return rgsml::render::AuditionTarget::PREPARED;
            }
            switch (*target) {
            case rgsml::app::AuditionTarget::PREPARED:
                return rgsml::render::AuditionTarget::PREPARED;
            case rgsml::app::AuditionTarget::PROCESSED:
                return rgsml::render::AuditionTarget::PROCESSED;
            case rgsml::app::AuditionTarget::GOLD:
                return rgsml::render::AuditionTarget::GOLD;
            }
            return rgsml::render::AuditionTarget::PREPARED;
        },
        [&auditionSelector, &masteringChainState]()
            -> std::optional<rgsml::core::RealizationId> {
            const auto accepted = auditionSelector.processed_realization_snapshot();
            const auto id = auditionSelector.processed_realization_id();
            if (!accepted || !id) {
                return std::nullopt;
            }
            for (const auto& signature : accepted->signatures()) {
                if (signature.instance_id == masteringChainState.compressor_instance_id() &&
                    signature.type_id == "rgsml.dsp.compressor" &&
                    signature.disposition ==
                        rgsml::render::ModuleExecutionDisposition::BYPASS_IDENTITY &&
                    std::holds_alternative<
                        rgsml::render::CompressorExecutionSignaturePayload>(
                            signature.payload)) {
                    // This is a published immutable render signature; the
                    // resolver still checks that ID is ACTUALLY audible.
                    return id;
                }
            }
            return std::nullopt;
        });
    // B4c2c: bind M/S observation to PUBLISHED render identity and the
    // independent audible playback clock. Creating the observer does not
    // opt a Stereo/M-S DSP node into legacy three-module projects.
    rgsml::app::StereoMsViewModel stereoMsViewModel{
        &masteringChainState, &previewController};
    stereoMsViewModel.setTelemetryProviders(
        [&auditionSelector]() -> rgsml::app::StereoMsViewModel::AcceptedRenderEvidence {
            return {
                auditionSelector.processed_realization_snapshot(),
                auditionSelector.processed_realization_id()};
        },
        [&playbackTransport]() {
            return playbackTransport.playback_snapshot();
        },
        [&auditionSelector]() {
            const auto target = auditionSelector.active_target();
            return target.has_value() &&
                   *target == rgsml::app::AuditionTarget::PROCESSED;
        });
    // The adapter registers Stereo/M-S only if the current chain already
    // contains that opt-in module. Do not alter create_default() or the
    // accepted module order merely to make the telemetry visible.
    rgsml::app::DspChainAdapterModel dspChainAdapterModel{
        &gainViewModel, &eqViewModel, &compressorViewModel,
        &masteringChainState, QStringLiteral("Mastering"),
        nullptr, &stereoMsViewModel, true,
        &terminalLimiterState, &terminalLimiterViewModel};

    rgsml::app::GoldSelectionViewModel goldSelection{
        &auditionSelector};
    rgsml::app::ProjectSessionViewModel projectSession{
        &sourceSelection, &goldSelection, &auditionRegion, &playbackTransport,
        &masteringChainState, &gainViewModel, &eqViewModel, &dspChainAdapterModel, &previewController};
    sourceSelection.set_source_committed_handler(
        [&sourceWaveform, &sourceSelection, &auditionRegion,
         &auditionSelector, &goldSelection, &dspChainAdapterModel, &projectSession,
         &stereoMsViewModel, &compressorViewModel,
         &staticTruePeak, &liveTruePeak](
            const rgsml::core::ResourceReference& source) {
            // Prevent a prior source's slow C1 and C2 work becoming current.
            liveTruePeak.invalidate();
            staticTruePeak.invalidate_source();
            const auto frameCount = rgsml::core::FrameCount::create(
                sourceSelection.frame_count());
            const auto sampleRate = rgsml::core::SampleRate::create(
                sourceSelection.sample_rate_hz());
            if (frameCount && sampleRate) {
                auditionRegion.source_committed(
                    *frameCount.value(), *sampleRate.value());
            }
            const auto prepared = auditionSelector.source_committed(source);
            if (prepared && frameCount && sampleRate) {
                const int channelCount = sourceSelection.channel_count();
                if (channelCount == 1 || channelCount == 2) {
                    staticTruePeak.source_committed(
                        source, rgsml::app::TruePeakExpectedSource{
                            *frameCount.value(), *sampleRate.value(),
                            channelCount == 1
                                ? rgsml::audio::ChannelLayout::MONO_C
                                : rgsml::audio::ChannelLayout::STEREO_LR});
                    staticTruePeak.processed_changed(
                        auditionSelector.processed_realization_snapshot(),
                        auditionSelector.processed_realization_id());
                }
            }
            if (prepared) {
                // New source identity: clear previous GR regardless of whether
                // project-open is restoring existing DSP parameter values.
                compressorViewModel.resetTelemetryForNewSource();
                static_cast<void>(auditionSelector.switch_to(
                    rgsml::app::AuditionTarget::PREPARED));
                if (!projectSession.is_committing_project_open()) {
                    dspChainAdapterModel.resetForNewSource();
                }
            }
            goldSelection.sourceChanged();
            sourceWaveform.source_committed(source);
            // Use the committed source's real format AFTER the adapter's
            // new-source reset has invalidated prior format evidence.
            stereoMsViewModel.setSignalFormat(
                static_cast<double>(sourceSelection.sample_rate_hz()),
                sourceSelection.channel_count());
        });
    waveformPresentation.set_seek_handler(
        [&auditionRegion](rgsml::core::FrameIndex position) {
            return auditionRegion.seek(position);
        });
    waveformPresentation.set_region_commit_handler(
        [&auditionRegion](rgsml::core::FrameRange candidate) {
            return auditionRegion.set_region(candidate);
        });
    QObject::connect(
        &auditionRegion,
        &rgsml::app::AuditionRegionViewModel::changed,
        &waveformPresentation,
        [&auditionRegion, &waveformPresentation] {
            waveformPresentation.set_displayed_region(auditionRegion.region());
        });
    QObject::connect(
        &waveformPresentation,
        &rgsml::ui::WaveformPresentation::changed,
        &auditionRegion,
        [&waveformPresentation, &auditionRegion] {
            auditionRegion.set_waveform_ready(waveformPresentation.ready());
        });
    QQmlApplicationEngine engine;
    engine.addImportPath(QLibraryInfo::path(QLibraryInfo::QmlImportsPath));
    engine.rootContext()->setContextProperty(
        QStringLiteral("sourceSelection"),
        &sourceSelection);
    engine.rootContext()->setContextProperty(
        QStringLiteral("playbackTransport"),
        &playbackTransport);
    engine.rootContext()->setContextProperty(
        QStringLiteral("sourceWaveform"),
        &waveformPresentation);
    engine.rootContext()->setContextProperty(
        QStringLiteral("auditionRegion"),
        &auditionRegion);
    engine.rootContext()->setContextProperty(
        QStringLiteral("auditionSelector"),
        &auditionSelector);
    engine.rootContext()->setContextProperty(
        QStringLiteral("goldSelection"),
        &goldSelection);
    engine.rootContext()->setContextProperty(
        QStringLiteral("projectSession"),
        &projectSession);
    engine.rootContext()->setContextProperty(
        QStringLiteral("gainViewModel"),
        &gainViewModel);
    engine.rootContext()->setContextProperty(
        QStringLiteral("eqViewModel"),
        &eqViewModel);
    engine.rootContext()->setContextProperty(
        QStringLiteral("compressorViewModel"),
        &compressorViewModel);
    // Read-only telemetry properties are ready for B4c3 QML; no fake
    // visual points or default DSP activation are introduced here.
    engine.rootContext()->setContextProperty(
        QStringLiteral("stereoMsViewModel"),
        &stereoMsViewModel);
    // Real candidate state is inspectable, but no M17 sonic adapter or
    // UI control may activate until PROCESSED routes authentic terminal PCM.
    engine.rootContext()->setContextProperty(
        QStringLiteral("terminalLimiterViewModel"),
        &terminalLimiterViewModel);
    engine.rootContext()->setContextProperty(
        QStringLiteral("dspChainAdapterModel"),
        &dspChainAdapterModel);
    engine.rootContext()->setContextProperty(
        QStringLiteral("truePeakStatic"), &staticTruePeak);
    engine.rootContext()->setContextProperty(
        QStringLiteral("truePeakLive"), &liveTruePeak);
    engine.rootContext()->setContextProperty(
        QStringLiteral("liveSpectrumViewModel"),
        &liveSpectrumViewModel);
#ifdef RGSML_BUILD_PROVENANCE
    engine.rootContext()->setContextProperty(
        QStringLiteral("buildProvenance"),
        QStringLiteral(RGSML_BUILD_PROVENANCE));
#endif
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &application,
        [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);
    QObject::connect(
        &engine,
        &QQmlEngine::quit,
        &application,
        &QCoreApplication::quit,
        Qt::QueuedConnection);
    engine.loadFromModule("Rgsml.Ui", "Main");

    if (deploySmoke && !engine.rootObjects().isEmpty()) {
        QTimer::singleShot(0, &application, &QCoreApplication::quit);
    }

    const int result = application.exec();
    return smokeWarningObserved ? EXIT_FAILURE : result;
}
