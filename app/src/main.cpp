#include "audition_region_view_model.hpp"
#include "rgsml_build_provenance.hpp"
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
#include "prepare_memory_advice_view_model.hpp"
#include "master_output_rate_state.hpp"
#include "project_session_view_model.hpp"
#include "source_selection_view_model.hpp"
#include "source_waveform_view_model.hpp"
#include "stereo_ms_view_model.hpp"
#include "terminal_candidate_gate.hpp"
#include "terminal_audio_session_gate.hpp"
#include "terminal_private_publication_coordinator.hpp"
#include "terminal_private_render_controller.hpp"
#include "terminal_consumed_output_binding.hpp"
#include "terminal_private_gr_view_model.hpp"
#include "terminal_post_limiter_true_peak_coordinator.hpp"
#include "terminal_product_activation_coordinator.hpp"
#include "terminal_private_product_session.hpp"
#include "terminal_monitor_view_model.hpp"
#include "terminal_limiter_state.hpp"
#include "terminal_limiter_view_model.hpp"
#include "true_peak_static_coordinator.hpp"
#include "true_peak_live_coordinator.hpp"

#include "waveform_presentation.hpp"

#include <rgsml/platform/windows/windows_audio_playback_service.hpp>

#include <QGuiApplication>
#include <QIcon>
#include <QLibraryInfo>
#include <QUuid>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QImage>
#include <QMetaObject>
#include <cmath>
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
    // Matching icon for the Qt window/task switcher. The QML module embeds
    // the identical approved C artwork under its resource URL.
    application.setWindowIcon(QIcon(QStringLiteral(
        ":/qt/qml/Rgsml/Ui/resources/RGS_ML_C_header_64.png")));
    const bool deploySmoke = application.arguments().contains(QStringLiteral("--rgsml-deploy-smoke"));
    // Opt-in CI-only REAL product Main Qt screenshot; never a user render path.
    const bool m17MainEvidence = application.arguments().contains(
        QStringLiteral("--rgsml-m17-ui-evidence"));
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
    rgsml::app::MasterOutputRateState masterOutputRate;
    rgsml::app::PrepareMemoryAdviceViewModel prepareRateAdvisor{
        &sourceSelection, windowsPlayback};
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
    privateProductSession.set_offline_final_master_selection_checker(
        [&auditionSelector,&previewController,&masteringChainState] {
            const auto manual=auditionSelector.processed_realization_snapshot();
            return manual
                &&previewController.preview_status()==QStringLiteral("READY")
                &&manual->chain_revision()==masteringChainState.chain().revision()
                &&auditionSelector.offline_processed_selected_for_final_master();
        });
    privateProductSession.set_offline_source_provenance_checker(
        [&auditionSelector,&previewController,&masteringChainState] {
            const auto manual=auditionSelector.processed_realization_snapshot();
            return manual
                &&previewController.preview_status()==QStringLiteral("READY")
                &&manual->chain_revision()==masteringChainState.chain().revision()
                &&auditionSelector.offline_processed_provenance_current();
        });
    privateProductSession.set_preferred_output_rate_probe(
        [windowsPlayback](rgsml::audio::ChannelLayout layout) {
            return windowsPlayback->preferred_terminal_output_rate(layout);
        });
    // Real Windows OS-backed IMMINENT monitoring allocation is connected to
    // the verified MASTER private bridge, but NOT invoked automatically.
    // Future explicit caller MUST supply its own memory decision. There is
    // no silent admission, no rate retarget and no monitor sink installation.
    privateProductSession.set_monitoring_preflight_materializer(
        [windowsPlayback](
            rgsml::audio::AudioBufferView immutableMaster,
            rgsml::core::FrameCount block,
            std::uint64_t otherReserveBytes,
            const rgsml::app::TerminalPrivateProductSession::
                MonitoringPreallocationDecision& decision) {
            return windowsPlayback->materialize_monitoring_pcm_with_preflight(
                immutableMaster,block,otherReserveBytes,decision);
        });
    // Private-only STOPPED handoff with exact conditional device retirement
    // and MANUAL restoration via the existing selector gate. No QML entry,
    // implicit Play/Pause, SOURCE-loop mapping or M16 LIVE certificate.
    privateProductSession.set_private_monitor_transport(
        [windowsPlayback](auto pcm) {
            return windowsPlayback->prepare_monitoring_output_pcm(
                std::move(pcm));
        },
        [windowsPlayback](auto pcm) {
            return windowsPlayback->monitoring_output_is_owned(
                std::move(pcm));
        },
        [windowsPlayback](auto pcm) {
            return windowsPlayback->retire_monitoring_output_if_owned(
                std::move(pcm));
        },
        [&auditionSelector](auto manual,rgsml::core::RealizationId id,
                            rgsml::core::FrameIndex cue) {
            return auditionSelector.restore_manual_processed_after_terminal(
                std::move(manual),id,cue);
        });
    // Exact monitor retirement precedes any SOURCE-region mutation.
    // Reject a competing region instead of silently overwriting user intent.
    privateProductSession.set_private_monitor_source_loop_reconciliation(
        [&auditionRegion](rgsml::core::FrameRange projected) {
            if(auditionRegion.loop_enabled()
               ||(auditionRegion.has_region()
                  &&auditionRegion.region()!=std::optional{projected}))
                return rgsml::core::Status::failure(rgsml::core::Error{
                    rgsml::core::ErrorCode::InvalidState,
                    "MONITOR_SOURCE_REGION_CONFLICT"});
            auto region=auditionRegion.set_region(projected);
            if(!region)return region;
            return auditionRegion.set_loop_enabled(true);
        });
    // Monitor-specific exact owner command/snapshot: Play/Pause/Stop and
    // MASTER-coordinate seek/loop. This is still PRIVATE, not QML visible;
    // no LIVE measurement or device sample rate substituted for MASTER.
    privateProductSession.set_private_monitor_controls(
        [windowsPlayback](auto owned,auto request) {
            return windowsPlayback->control_monitoring_output_if_owned(
                std::move(owned),request);
        },
        [windowsPlayback](auto owned) {
            return windowsPlayback->monitoring_master_snapshot_if_owned(
                std::move(owned));
        });
    // Explicit user-only Windows MASTER->DEVICE monitor, with no LIVE identity.
    rgsml::app::TerminalMonitorViewModel terminalMonitorViewModel{
        privateProductSession,playbackTransport};
    terminalMonitorViewModel.set_offline_master_authorizer(
        [&auditionSelector,&previewController,&masteringChainState]
            () -> rgsml::core::Status {
            if(auditionSelector.active_target()
                ==rgsml::app::AuditionTarget::PROCESSED)
                return rgsml::core::Status::success();
            const auto manual=auditionSelector.processed_realization_snapshot();
            if(!manual
               ||previewController.preview_status()!=QStringLiteral("READY")
               ||manual->chain_revision()!=masteringChainState.chain().revision())
                return rgsml::core::Status::failure(rgsml::core::Error{
                    rgsml::core::ErrorCode::InvalidState,
                    "OFFLINE_MASTER_REQUIRES_CURRENT_MANUAL_CHAIN_REVISION"});
            return auditionSelector.authorize_offline_processed_for_final_master();
        });
    // Product-private output rate is now explicitly discoverable but no
    // automatic call or QML sonic activation takes place.
    // A PRIVATE read-only GR presenter. It observes actual Windows-worker
    // consumed OUTPUT_RATE frames and their exact active M17 applied gains.
    // No user-accessible activation/publishing is introduced by this wiring;
    // absent private sink ownership remains UNAVAILABLE (never fake zero).
    rgsml::app::TerminalPrivateGrViewModel terminalPrivateGrViewModel{
        [&privateProductSession,windowsPlayback] {
            return rgsml::app::observe_private_terminal_consumed_output(
                privateProductSession,
                [windowsPlayback](std::optional<std::uint64_t> generation) {
                    return windowsPlayback->consumed_output_prefix(generation);
                });
        }};
    rgsml::app::TerminalPostLimiterTruePeakCoordinator terminalPostMeter{
        [&privateProductSession,windowsPlayback] {
            return rgsml::app::observe_private_terminal_consumed_output(
                privateProductSession,
                [windowsPlayback](std::optional<std::uint64_t> generation) {
                    return windowsPlayback->consumed_output_prefix(generation);
                });
        }};
    QObject::connect(&privateProductSession,
        &rgsml::app::TerminalPrivateProductSession::changed,
        &terminalPostMeter,
        &rgsml::app::TerminalPostLimiterTruePeakCoordinator::invalidate);
    QObject::connect(&playbackTransport,
        &rgsml::app::PlaybackTransportViewModel::playbackChanged,
        &terminalPrivateGrViewModel,
        &rgsml::app::TerminalPrivateGrViewModel::refresh);
    QObject::connect(&privateProductSession,
        &rgsml::app::TerminalPrivateProductSession::changed,
        &terminalPrivateGrViewModel,
        &rgsml::app::TerminalPrivateGrViewModel::invalidate);
    // The only product ON/BYPASS conductor. The adapter calls the typed
    // ViewModel; its accepted-state signal now drives exact-source,
    // reversible, output-owned device installation (no modal rate prompt).
    // Device-rate SRC applies only to audition, never to final WAV master.
    rgsml::app::TerminalProductActivationCoordinator terminalProductActivation{
        terminalLimiterState,terminalLimiterViewModel,auditionSelector,
        playbackTransport,privateProductSession};
    terminalProductActivation.set_master_rate_state(masterOutputRate);
    // Historical ACT is visible ONLY when the current MANUAL PCM is still
    // the exact chain revision and the M17 terminal state/parameters are
    // unchanged. A->B->A Ceiling edits never resurrect old measurements.
    prepareRateAdvisor.set_active_revision_provider(
        [&auditionSelector,&masteringChainState,&terminalLimiterState]()
            ->std::optional<rgsml::app::AcceptedPrepareRevisionKey> {
            const auto manual=auditionSelector.processed_realization_snapshot();
            if(!manual
               ||manual->chain_revision()!=masteringChainState.chain().revision())
                return std::nullopt;
            return rgsml::app::AcceptedPrepareRevisionKey{
                manual->chain_revision(),
                terminalLimiterState.chain().revision(),
                terminalLimiterState.parameter_revision()};
        });
    QObject::connect(&terminalLimiterViewModel,
        &rgsml::app::TerminalLimiterViewModel::acceptedStateChanged,
        &prepareRateAdvisor,
        &rgsml::app::PrepareMemoryAdviceViewModel::current_revision_changed);
    QObject::connect(&previewController,
        &rgsml::app::MasteringPreviewController::changed,
        &prepareRateAdvisor,
        &rgsml::app::PrepareMemoryAdviceViewModel::current_revision_changed);
    // Read-only exact-owner telemetry: real immutable MASTER has already
    // passed the terminal M16 gate. No re-render, no device or PCM allocation.
    QObject::connect(&privateTerminalRenderer,
        &rgsml::app::TerminalPrivateRenderController::changed,
        &prepareRateAdvisor,
        [&privateTerminalRenderer,&prepareRateAdvisor] {
            const auto x=privateTerminalRenderer.last_approved_prepare_timing();
            if(!x)return;
            if(x->layout!=rgsml::audio::ChannelLayout::MONO_C
               &&x->layout!=rgsml::audio::ChannelLayout::STEREO_LR)return;
            const auto channels=x->layout==rgsml::audio::ChannelLayout::MONO_C
                ?1:2;
            const rgsml::app::AcceptedPrepareObservation observed{
                privateTerminalRenderer.render_invocation_count(),
                x->source_rate.value(),x->master_rate.value(),
                x->source_frames.value(),channels,x->ceiling_db_tp,
                x->manual_chain_revision,x->terminal_chain_revision,
                x->terminal_parameter_revision,
                x->queue_us,x->render_wall_us,x->src_us,
                x->m17_core_us,x->m16_primary_us};
            prepareRateAdvisor.observe_approved_prepare(observed);
        });
    // A real full-track M17 Prepare supersedes disposable host benchmarks.
    // Cancel at the exact request signal, before any renderer CPU work,
    // without touching the source/MANUAL playback identity.
    QObject::connect(&terminalLimiterViewModel,
        &rgsml::app::TerminalLimiterViewModel::prepareRequested,
        &prepareRateAdvisor,
        &rgsml::app::PrepareMemoryAdviceViewModel::cancel_calibration);
    // Never let a disposable high-CPU microbenchmark overlap an exact
    // DEVICE audition lease. The verified MASTER itself is untouched.
    QObject::connect(&privateProductSession,
        &rgsml::app::TerminalPrivateProductSession::changed,
        &prepareRateAdvisor,
        [&privateProductSession,&prepareRateAdvisor] {
            if(privateProductSession.monitoring_transaction_active())
                prepareRateAdvisor.cancel_calibration();
        });
    masterOutputRate.set_target_selection_guard(
        [&privateProductSession]() noexcept {
            return !privateProductSession.monitoring_transaction_active();
        });
    QObject::connect(&privateProductSession,
        &rgsml::app::TerminalPrivateProductSession::changed,
        &masterOutputRate,
        [&masterOutputRate] { masterOutputRate.refresh_selection_availability(); });
    // Ordinary manual-preview updates are not terminal AUDIO errors. The
    // conductor retires a live terminal before publication of a new MANUAL
    // owner; the old blanket cancel-on-changed caused ERROR/BYPASS instead.
    QObject::connect(&previewController,
        &rgsml::app::MasteringPreviewController::changed,
        &terminalProductActivation,
        [&previewController,&terminalProductActivation] {
            terminalProductActivation.manual_preview_changed(
                previewController.preview_status());
        });
    QObject::connect(&auditionRegion,
        &rgsml::app::AuditionRegionViewModel::changed,
        &terminalProductActivation,
        &rgsml::app::TerminalProductActivationCoordinator::loop_context_changed);
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
    // PO 2026-10-22: restore standalone REAL-TIME M16 ANALYSIS entry.
    // It is NOT a ProcessingChain DSP and modifies no PCM. M17 is a
    // distinct MASTER/TERMINAL finalization row, below the LIVE chain.
    // This supersedes ONLY the former shared-editor UI presentation.
    rgsml::app::DspChainAdapterModel dspChainAdapterModel{
        &gainViewModel, &eqViewModel, &compressorViewModel,
        &masteringChainState, QStringLiteral("Mastering"),
        nullptr, &stereoMsViewModel, true,
        &terminalLimiterState, &terminalLimiterViewModel,
        &terminalMonitorViewModel};

    rgsml::app::GoldSelectionViewModel goldSelection{
        &auditionSelector};
    rgsml::app::ProjectSessionViewModel projectSession{
        &sourceSelection, &goldSelection, &auditionRegion, &playbackTransport,
        &masteringChainState, &gainViewModel, &eqViewModel, &dspChainAdapterModel, &previewController};
    projectSession.set_master_output_rate_state(&masterOutputRate);
    sourceSelection.set_source_committed_handler(
        [&sourceWaveform, &sourceSelection, &auditionRegion,
         &auditionSelector, &goldSelection, &dspChainAdapterModel, &projectSession,
         &stereoMsViewModel, &compressorViewModel,
         &staticTruePeak, &liveTruePeak, &terminalProductActivation,
         &masterOutputRate](
            const rgsml::core::ResourceReference& source) {
            // Prevent a prior source's slow C1 and C2 work becoming current.
            liveTruePeak.invalidate();
            staticTruePeak.invalidate_source();
            const auto frameCount = rgsml::core::FrameCount::create(
                sourceSelection.frame_count());
            // Preferred Project/Render rate only; M17 device-derived render
            // remains unchanged until master/device ownership is integrated.
            masterOutputRate.source_committed(sourceSelection.sample_rate_hz());
            const auto sampleRate = rgsml::core::SampleRate::create(
                sourceSelection.sample_rate_hz());
            if (frameCount && sampleRate) {
                auditionRegion.source_committed(
                    *frameCount.value(), *sampleRate.value());
            }
            const auto prepared = auditionSelector.source_committed(source);
            if (prepared) {
                // New accepted SOURCE provenance may clear a stale M17
                // audio error, never resurrect an old private/device owner.
                terminalProductActivation.source_committed_successfully();
            }
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
        QStringLiteral("masterOutputRateState"), &masterOutputRate);
    engine.rootContext()->setContextProperty(
        QStringLiteral("prepareRateAdvisor"),
        &prepareRateAdvisor);
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
        QStringLiteral("terminalMonitorViewModel"),
        &terminalMonitorViewModel);
    engine.rootContext()->setContextProperty(
        QStringLiteral("terminalPrivateGrViewModel"),
        &terminalPrivateGrViewModel);
    engine.rootContext()->setContextProperty(
        QStringLiteral("terminalPostLimiterTruePeak"),
        &terminalPostMeter);
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
    // ALWAYS publish the configured binary identity. In FQ80 the old
    // conditional -D could omit the property and silently show "DEV".
    engine.rootContext()->setContextProperty(
        QStringLiteral("buildProvenance"),
        rgsml::app::configured_build_provenance());
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
    if (m17MainEvidence && !engine.rootObjects().isEmpty()) {
        auto* mainWindow=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if (!mainWindow) return EXIT_FAILURE;
        // Open ACTUAL Main.qml at the PO logical geometry, not a QML mockup.
        mainWindow->setWidth(1184);
        mainWindow->setHeight(688);
        dspChainAdapterModel.setSelectedIndex(5); // real M17 chain adapter
        mainWindow->show();
        QTimer::singleShot(900, &application, [mainWindow,&application] {
            auto* popup=mainWindow->findChild<QObject*>(
                QStringLiteral("m17RateTimePopup"));
            if (!popup || !QMetaObject::invokeMethod(popup,"open")) {
                std::fprintf(stderr,"M17_REAL_MAIN_POPUP_NOT_FOUND\n");
                application.exit(EXIT_FAILURE);
                return;
            }
            QTimer::singleShot(450, &application, [mainWindow,&application] {
                const auto file=qEnvironmentVariable("RGSML_M17_MAIN_EVIDENCE_PATH");
                const double ratio=mainWindow->devicePixelRatio();
                const auto image=mainWindow->grabWindow();
                const bool geometry=mainWindow->width()==1184
                    && mainWindow->height()==688;
                const bool dpi=std::abs(ratio-1.5)<0.051;
                const bool saved=geometry && dpi && !file.isEmpty()
                    && !image.isNull() && image.save(file,"PNG");
                std::fprintf(stdout,
                    "M17_REAL_MAIN_QT_LOGICAL=%dx%d DPR=%.3f PHYSICAL=%dx%d SAVED=%d\n",
                    mainWindow->width(),mainWindow->height(),ratio,
                    image.width(),image.height(),saved?1:0);
                std::fflush(stdout);
                application.exit(saved?EXIT_SUCCESS:EXIT_FAILURE);
            });
        });
    }

    const int result = application.exec();
    return smokeWarningObserved ? EXIT_FAILURE : result;
}
