#include "audition_region_view_model.hpp"
#include "audition_source_selector.hpp"
#include "eq_view_model.hpp"
#include "gold_selection_view_model.hpp"
#include "playback_transport_view_model.hpp"
#include "project_session_view_model.hpp"
#include "source_selection_view_model.hpp"
#include "source_waveform_view_model.hpp"
#include "windows_window_chrome_helper.hpp"

#include "waveform_presentation.hpp"

#include <rgsml/platform/windows/windows_audio_playback_service.hpp>

#include <QGuiApplication>
#include <QLibraryInfo>
#include <QUuid>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTimer>

#include <cstdio>
#include <cstdlib>
#include <memory>

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

    auto playbackService = std::make_unique<
        rgsml::platform::windows::WindowsAudioPlaybackService>();
    auto* windowsPlayback = playbackService.get();
    rgsml::app::PlaybackTransportViewModel playbackTransport{
        std::move(playbackService)};
    playbackTransport.set_pcm_prepare_handler(
        [windowsPlayback](rgsml::audio::AudioBufferView source) {
            return windowsPlayback->prepare_pcm(source);
        });
    rgsml::ui::WaveformPresentation waveformPresentation;
    rgsml::app::AuditionRegionViewModel auditionRegion{&playbackTransport};
    rgsml::app::SourceWaveformViewModel sourceWaveform{
        &waveformPresentation};
    rgsml::app::SourceSelectionViewModel sourceSelection;
    rgsml::app::AuditionSourceSelector auditionSelector{&playbackTransport};
    auditionSelector.set_source_loop_provider([&auditionRegion] {
        return auditionRegion.loop_enabled()
            ? auditionRegion.region()
            : std::nullopt;
    });
    rgsml::app::EqViewModel eqViewModel{
        [&auditionSelector] {
            return auditionSelector.prepared_realization_snapshot();
        },
        [&auditionSelector](rgsml::render::RenderResult result) {
            return auditionSelector.set_processed_realization(std::move(result));
        },
        [] {
            const auto str = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
            return *rgsml::core::Uuid::parse(str).value();
        }
    };
    rgsml::app::GoldSelectionViewModel goldSelection{
        &auditionSelector};
    rgsml::app::ProjectSessionViewModel projectSession{
        &sourceSelection, &goldSelection, &auditionRegion, &playbackTransport};
    sourceSelection.set_source_committed_handler(
        [&sourceWaveform, &sourceSelection, &auditionRegion,
         &auditionSelector, &goldSelection, &eqViewModel](
            const rgsml::core::ResourceReference& source) {
            const auto frameCount = rgsml::core::FrameCount::create(
                sourceSelection.frame_count());
            const auto sampleRate = rgsml::core::SampleRate::create(
                sourceSelection.sample_rate_hz());
            if (frameCount && sampleRate) {
                auditionRegion.source_committed(
                    *frameCount.value(), *sampleRate.value());
            }
            const auto prepared = auditionSelector.source_committed(source);
            if (prepared) {
                static_cast<void>(auditionSelector.switch_to(
                    rgsml::app::AuditionTarget::PREPARED));
                eqViewModel.trigger_preview();
            }
            goldSelection.sourceChanged();
            sourceWaveform.source_committed(source);
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
        QStringLiteral("eqViewModel"),
        &eqViewModel);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &application,
        [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);
    engine.loadFromModule("Rgsml.Ui", "Main");

#ifdef _WIN32
    if (!engine.rootObjects().isEmpty()) {
        auto* rootWindow = qobject_cast<QQuickWindow*>(engine.rootObjects().front());
        if (rootWindow != nullptr) {
            static rgsml::app::WindowsWindowChromeHelper chromeHelper{rootWindow};
            const std::array exclusionNames{
                QStringLiteral("desktopMenuBar"),
                QStringLiteral("headerAuditionTargetSelector"),
                QStringLiteral("windowMinimizeButton"),
                QStringLiteral("windowMaximizeButton"),
                QStringLiteral("windowCloseButton"),
            };
            for (const auto& name : exclusionNames) {
                if (auto* item = rootWindow->findChild<QQuickItem*>(name)) {
                    chromeHelper.add_exclusion_item(item);
                }
            }
        }
    }
#endif

    if (deploySmoke && !engine.rootObjects().isEmpty()) {
        QTimer::singleShot(0, &application, &QCoreApplication::quit);
    }

    const int result = application.exec();
    return smokeWarningObserved ? EXIT_FAILURE : result;
}
