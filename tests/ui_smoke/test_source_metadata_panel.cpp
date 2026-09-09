#include "source_selection_view_model.hpp"

#include "../audio_golden/wav/golden_vectors.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QGuiApplication>
#include <QLibraryInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTemporaryDir>
#include <QTest>
#include <QWindow>

namespace rgsml::tests {
namespace {

[[nodiscard]] QString write_file(
    QTemporaryDir& directory,
    const QString& name,
    const QByteArray& bytes)
{
    const auto path = directory.filePath(name);
    QFile file{path};
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)
        || file.write(bytes) != bytes.size()) {
        return {};
    }
    file.close();
    return path;
}

[[nodiscard]] QByteArray valid_wav()
{
    return QByteArray{
        reinterpret_cast<const char*>(audio_golden::kRiffPcm16Mono.data()),
        static_cast<qsizetype>(audio_golden::kRiffPcm16Mono.size()),
    };
}

}  // namespace

class SourceMetadataPanelSmokeTest final : public QObject {
    Q_OBJECT

private slots:
    void emptyReadyErrorAndWindowLifecycle();
};

void SourceMetadataPanelSmokeTest::emptyReadyErrorAndWindowLifecycle()
{
    app::SourceSelectionViewModel model;
    QQmlApplicationEngine engine;
    engine.addImportPath(QLibraryInfo::path(QLibraryInfo::QmlImportsPath));
    engine.rootContext()->setContextProperty(
        QStringLiteral("sourceSelection"), &model);
    engine.loadFromModule("Rgsml.Ui", "Main");
    QCOMPARE(engine.rootObjects().size(), 1);

    auto* root = engine.rootObjects().front();
    QVERIFY(root->findChild<QObject*>(QStringLiteral("sourceOpenButton")));
    QVERIFY(root->findChild<QObject*>(QStringLiteral("sourceFileDialog")));
    auto* empty = root->findChild<QObject*>(QStringLiteral("sourceEmptyState"));
    auto* display = root->findChild<QObject*>(QStringLiteral("sourceDisplayName"));
    auto* readOnly = root->findChild<QObject*>(QStringLiteral("sourceReadOnlyBadge"));
    auto* error = root->findChild<QObject*>(QStringLiteral("sourceErrorMessage"));
    QVERIFY(empty);
    QVERIFY(display);
    QVERIFY(readOnly);
    QVERIFY(error);
    QVERIFY(empty->property("visible").toBool());
    QVERIFY(!display->property("visible").toBool());

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto validPath = write_file(
        directory, QStringLiteral("UI Source.wav"), valid_wav());
    model.selectSource(QUrl::fromLocalFile(validPath));
    QCoreApplication::processEvents();
    QVERIFY(!empty->property("visible").toBool());
    QVERIFY(display->property("visible").toBool());
    QCOMPARE(display->property("text").toString(), QStringLiteral("UI Source.wav"));
    QVERIFY(readOnly->property("visible").toBool());
    QCOMPARE(readOnly->property("text").toString(), QStringLiteral("Read-only"));
    QVERIFY(root->findChild<QObject*>(QStringLiteral("sourceContainerMetadata"))
                ->property("text").toString().contains(QStringLiteral("RIFF")));
    QVERIFY(root->findChild<QObject*>(QStringLiteral("sourceFormatMetadata"))
                ->property("text").toString().contains(QStringLiteral("16-bit")));
    QVERIFY(root->findChild<QObject*>(QStringLiteral("sourceRateMetadata"))
                ->property("text").toString().contains(QStringLiteral("44100")));
    QVERIFY(root->findChild<QObject*>(QStringLiteral("sourceChannelsMetadata"))
                ->property("text").toString().contains(QStringLiteral("Mono")));
    QVERIFY(root->findChild<QObject*>(QStringLiteral("sourceFramesMetadata"))
                ->property("text").toString().contains(QStringLiteral("3")));
    QVERIFY(root->findChild<QObject*>(QStringLiteral("sourceDurationMetadata"))
                ->property("text").toString().contains(QStringLiteral("0:00.000")));

    const auto invalidPath = write_file(
        directory, QStringLiteral("Invalid.wav"), QByteArray{"bad"});
    model.selectSource(QUrl::fromLocalFile(invalidPath));
    QCoreApplication::processEvents();
    QVERIFY(error->property("visible").toBool());
    QVERIFY(!error->property("text").toString().isEmpty());
    QCOMPARE(display->property("text").toString(), QStringLiteral("UI Source.wav"));

    auto* window = qobject_cast<QWindow*>(root);
    QVERIFY(window);
    window->resize(800, 500);
    QCoreApplication::processEvents();
    QCOMPARE(window->size(), QSize(800, 500));
    window->close();
    QCoreApplication::processEvents();
}

}  // namespace rgsml::tests

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    rgsml::tests::SourceMetadataPanelSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_source_metadata_panel.moc"
