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
#include "skinpack_loader.hpp"
#include <rgsml/analysis/live_spectrum_analyzer.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include "project_session_view_model.hpp"
#include "source_selection_view_model.hpp"
#include "source_waveform_view_model.hpp"
#include "stereo_ms_view_model.hpp"
#include "true_peak_static_coordinator.hpp"
#include "true_peak_live_coordinator.hpp"

#include "waveform_presentation.hpp"

#include <rgsml/platform/windows/windows_audio_playback_service.hpp>

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQuickItem>
#include <QRectF>
#include <QVector>
#include <QStringList>
#include <QLibraryInfo>
#include <QUuid>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
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
    const QString skinSmokePrefix = QStringLiteral("--rgsml-skinpack-smoke=");
    QString skinSmokePath;
    for (const QString& arg : application.arguments()) {
        if (arg.startsWith(skinSmokePrefix)) {
            skinSmokePath = arg.mid(skinSmokePrefix.size());
        }
    }
    const bool skinSmoke = !skinSmokePath.isEmpty();
    const bool deploySmoke = skinSmoke ||
        application.arguments().contains(QStringLiteral("--rgsml-deploy-smoke"));
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
    const auto gainInstanceId = *rgsml::dsp::ModuleInstanceId::from_uuid(gainUuid).value();
    const auto eqInstanceId = *rgsml::dsp::ModuleInstanceId::from_uuid(eqUuid).value();
    const auto compInstanceId = *rgsml::dsp::ModuleInstanceId::from_uuid(compUuid).value();
    auto masteringChainStateRes = rgsml::app::MasteringChainState::create_default(
        *moduleRegistry.value(), chainUuid, gainInstanceId, eqInstanceId, compInstanceId);
    auto masteringChainState = std::move(*masteringChainStateRes.value());

    rgsml::app::MasteringPreviewController previewController{
        &masteringChainState,
        [&auditionSelector] {
            return auditionSelector.prepared_realization_snapshot();
        },
        [&auditionSelector](rgsml::render::RenderResult result) {
            return auditionSelector.set_processed_realization(std::move(result));
        }
    };

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
        nullptr, &stereoMsViewModel, true};

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
    auto skinImageStore = std::make_shared<rgsml::app::SkinImageStore>();
    rgsml::app::SkinPackLoader skinPackLoader(skinImageStore);
    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("rgs-skin"),
        new rgsml::app::SkinImageProvider(skinImageStore));
    engine.addImportPath(QLibraryInfo::path(QLibraryInfo::QmlImportsPath));
    engine.rootContext()->setContextProperty(QStringLiteral("skinPackLoader"), &skinPackLoader);
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

    // Experimental R1 import/rollback diagnostic. Not a normal startup path.
    // Exercise the real Qt loader and its QML transport presentation on Windows.
    if (skinSmoke && !engine.rootObjects().isEmpty()) {
        QTimer::singleShot(0, &application, [&] {
            bool passed = true;
            const auto fail = [&](const char* reason) {
                std::fprintf(stderr, "SKINPACK_R1_IMPORT_SMOKE_FAIL: %s\\n", reason);
                passed = false;
            };
            QObject* texture = engine.rootObjects().first()->findChild<QObject*>(
                QStringLiteral("rgsSkinTransportTexture"));
            if (texture == nullptr) fail("transport texture QML item missing");

            // Compare the actual Qt Quick item bounds against the original layout.
            // Cosmetic SkinPack R1 must not alter geometry or touch target hitboxes.
            const QStringList geometryNames{
                QStringLiteral("controlStrip"),
                QStringLiteral("controlStripContent"),
                QStringLiteral("rgsSkinTransportTexture"),
                QStringLiteral("stopButton"),
                QStringLiteral("playPauseButton"),
                QStringLiteral("transportTimeModule"),
                QStringLiteral("waveformZoomOutButton")
            };
            const auto captureBounds = [&]() {
                QVector<QRectF> rects;
                for (const QString& name : geometryNames) {
                    auto* item = engine.rootObjects().first()->findChild<QQuickItem*>(name);
                    if (item == nullptr) {
                        fail("canonical transport item missing");
                        rects.append(QRectF());
                    } else {
                        rects.append(QRectF(item->x(), item->y(),
                                           item->width(), item->height()));
                    }
                }
                return rects;
            };
            const auto baselineBounds = captureBounds();
            if (engine.rootObjects().first()->property("minimumWidth").toInt() != 1184 ||
                engine.rootObjects().first()->property("minimumHeight").toInt() != 688) {
                fail("minimum window geometry changed");
            }
            const auto verifyBounds = [&]() {
                QCoreApplication::processEvents();
                const auto next = captureBounds();
                for (qsizetype i = 0; i < next.size(); ++i) {
                    const auto before = baselineBounds.at(i);
                    const auto after = next.at(i);
                    if (qAbs(before.x() - after.x()) > 0.01 ||
                        qAbs(before.y() - after.y()) > 0.01 ||
                        qAbs(before.width() - after.width()) > 0.01 ||
                        qAbs(before.height() - after.height()) > 0.01) {
                        std::fprintf(stderr, "Geometry shifted for %s\\n",
                                     geometryNames.at(i).toUtf8().constData());
                        fail("SkinPack changed canonical transport geometry");
                    }
                }
            };

            if (!skinPackLoader.openFromUrl(QUrl::fromLocalFile(skinSmokePath))) {
                fail("valid STORE fixture rejected by Qt loader");
                std::fprintf(stderr, "loader: %s\\n",
                             skinPackLoader.errorString().toUtf8().constData());
            }
            if (!skinPackLoader.active() ||
                skinPackLoader.skinName() != QStringLiteral("SkinPack R1 Native Smoke") ||
                skinPackLoader.revision() != 1 ||
                skinPackLoader.backgroundColor() != QColor(QStringLiteral("#F5F1EB"))) {
                fail("successful import did not publish expected surface state");
            }
            {
                QReadLocker guard(&skinImageStore->lock);
                if (skinImageStore->transport.isNull() ||
                    skinImageStore->transport.size() != QSize(4, 4)) {
                    fail("native PNG decode or image provider store invalid");
                }
            }
            if (texture && !texture->property("visible").toBool()) {
                fail("QML transport texture not visible after import");
            }
            verifyBounds();

            const QString invalid = QFileInfo(skinSmokePath).dir().filePath(
                QStringLiteral("r1-invalid.rgsskin"));
            if (skinPackLoader.openFromUrl(QUrl::fromLocalFile(invalid))) {
                fail("corrupt ZIP unexpectedly accepted");
            }
            if (!skinPackLoader.active() || skinPackLoader.revision() != 1 ||
                skinPackLoader.errorString().isEmpty()) {
                fail("bad ZIP did not preserve previously accepted skin");
            }

            const QStringList negativeFixtures{
                QStringLiteral("r1-bad-hash.rgsskin"),
                QStringLiteral("r1-bad-schema.rgsskin"),
                QStringLiteral("r1-bad-host.rgsskin"),
                QStringLiteral("r1-bad-trust.rgsskin"),
                QStringLiteral("r1-bad-png.rgsskin"),
                QStringLiteral("r1-deflate.rgsskin"),
                QStringLiteral("r1-unsafe-path.rgsskin")
            };
            for (const QString& filename : negativeFixtures) {
                const QString path = QFileInfo(skinSmokePath).dir().filePath(filename);
                if (skinPackLoader.openFromUrl(QUrl::fromLocalFile(path))) {
                    std::fprintf(stderr, "Unexpectedly accepted: %s\\n",
                                 filename.toUtf8().constData());
                    fail("hostile skin accepted");
                }
                if (!skinPackLoader.active() || skinPackLoader.revision() != 1 ||
                    skinPackLoader.errorString().isEmpty() ||
                    skinPackLoader.skinName() != QStringLiteral("SkinPack R1 Native Smoke")) {
                    fail("hostile skin modified the accepted state");
                }
                {
                    QReadLocker guard(&skinImageStore->lock);
                    if (skinImageStore->transport.isNull() ||
                        skinImageStore->transport.size() != QSize(4, 4) ||
                        skinImageStore->transport.pixelColor(0, 0) != QColor(QStringLiteral("#F5F1EB"))) {
                        fail("hostile skin modified the decoded texture");
                    }
                }
                if (texture && !texture->property("visible").toBool())
                    fail("hostile skin hid the accepted QML surface");
                verifyBounds();
            }
            if (passed) std::puts("SKINPACK_R1_SEVEN_NEGATIVE_ARCHIVE_CASES_PASS");
            skinPackLoader.reset();
            if (skinPackLoader.active() ||
                skinPackLoader.skinName() != QStringLiteral("RGS Original") ||
                !skinPackLoader.errorString().isEmpty() ||
                skinPackLoader.revision() != 2) {
                fail("RGS Original reset state incorrect");
            }
            {
                QReadLocker guard(&skinImageStore->lock);
                if (!skinImageStore->transport.isNull()) {
                    fail("reset retained the decoded texture");
                }
            }
            if (texture && texture->property("visible").toBool()) {
                fail("QML transport texture still visible after reset");
            }
            verifyBounds();
            if (passed) std::puts("SKINPACK_R1_CANONICAL_TRANSPORT_GEOMETRY_PASS");
            if (passed) std::puts("SKINPACK_R1_NATIVE_IMPORT_ROLLBACK_RESET_SMOKE_PASS");
            QCoreApplication::exit(passed ? EXIT_SUCCESS : EXIT_FAILURE);
        });
    } else if (deploySmoke && !engine.rootObjects().isEmpty()) {
        QTimer::singleShot(0, &application, &QCoreApplication::quit);
    }

    const int result = application.exec();
    return smokeWarningObserved ? EXIT_FAILURE : result;
}
