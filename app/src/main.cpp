#include "playback_transport_view_model.hpp"
#include "source_selection_view_model.hpp"

#include <rgsml/platform/windows/windows_audio_playback_service.hpp>

#include <QGuiApplication>
#include <QLibraryInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
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
    QGuiApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("RGS MasterLab"));
    const bool deploySmoke = application.arguments().contains(QStringLiteral("--rgsml-deploy-smoke"));
    if (deploySmoke) {
        qInstallMessageHandler(smokeMessageHandler);
    }

    auto playbackService = std::make_unique<
        rgsml::platform::windows::WindowsAudioPlaybackService>();
    rgsml::app::PlaybackTransportViewModel playbackTransport{
        std::move(playbackService)};
    rgsml::app::SourceSelectionViewModel sourceSelection;
    sourceSelection.set_playback_transport(&playbackTransport);
    QQmlApplicationEngine engine;
    engine.addImportPath(QLibraryInfo::path(QLibraryInfo::QmlImportsPath));
    engine.rootContext()->setContextProperty(
        QStringLiteral("sourceSelection"),
        &sourceSelection);
    engine.rootContext()->setContextProperty(
        QStringLiteral("playbackTransport"),
        &playbackTransport);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &application,
        [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);
    engine.loadFromModule("Rgsml.Ui", "Main");

    if (deploySmoke && !engine.rootObjects().isEmpty()) {
        QTimer::singleShot(0, &application, &QCoreApplication::quit);
    }

    const int result = application.exec();
    return smokeWarningObserved ? EXIT_FAILURE : result;
}
