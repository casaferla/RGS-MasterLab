#include <rgsml/platform/windows/windows_resource_identity.hpp>
#include <rgsml/platform/windows/windows_resource_reader.hpp>

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QTest>

#include <Windows.h>

#include <string_view>

namespace rgsml::tests {
namespace {

[[nodiscard]] core::ResourceReference reference_for(const QString& path)
{
    const auto pathUtf8 = path.toUtf8();
    const auto nameUtf8 = QFileInfo{path}.fileName().toUtf8();
    auto reference = platform::windows::WindowsResourceReader::make_read_reference(
        std::string_view{pathUtf8.constData(), static_cast<std::size_t>(pathUtf8.size())},
        std::string_view{nameUtf8.constData(), static_cast<std::size_t>(nameUtf8.size())});
    Q_ASSERT(reference);
    return std::move(*reference.value());
}

[[nodiscard]] QString write_file(
    QTemporaryDir& directory, const QString& name, const QByteArray& bytes)
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

}  // namespace

class WindowsResourceIdentityTest final : public QObject {
    Q_OBJECT

private slots:
    void exactHardlinkAndDistinctIdentityAreAuthoritative();
    void queryFailureFailsClosedWithoutMutatingFiles();
};

void WindowsResourceIdentityTest::exactHardlinkAndDistinctIdentityAreAuthoritative()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto sourcePath = write_file(directory, QStringLiteral("source.wav"), "source");
    const auto distinctPath = write_file(directory, QStringLiteral("gold.wav"), "gold");
    QVERIFY(!sourcePath.isEmpty() && !distinctPath.isEmpty());
    const auto hardlinkPath = directory.filePath(QStringLiteral("source-alias.wav"));
    QVERIFY(::CreateHardLinkW(
        reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(hardlinkPath).utf16()),
        reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(sourcePath).utf16()),
        nullptr));

    const auto source = reference_for(sourcePath);
    const auto hardlink = reference_for(hardlinkPath);
    const auto distinct = reference_for(distinctPath);
    auto exact = platform::windows::same_underlying_local_file(source, source);
    auto alias = platform::windows::same_underlying_local_file(source, hardlink);
    auto other = platform::windows::same_underlying_local_file(source, distinct);
    QVERIFY(exact && *exact.value());
    QVERIFY(alias && *alias.value());
    QVERIFY(other && !*other.value());
    QCOMPARE(QFile{sourcePath}.size(), qint64{6});
    QCOMPARE(QFile{distinctPath}.size(), qint64{4});
}

void WindowsResourceIdentityTest::queryFailureFailsClosedWithoutMutatingFiles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto sourcePath = write_file(directory, QStringLiteral("source.wav"), "immutable");
    const auto source = reference_for(sourcePath);
    const auto missingPath = QDir::cleanPath(directory.filePath(QStringLiteral("missing.wav")));
    const auto missingUtf8 = missingPath.toUtf8();
    auto missing = core::ResourceReference::create(
        std::string{platform::windows::WindowsResourceReader::provider_id()},
        missingUtf8.toStdString(), true, false, "missing.wav");
    QVERIFY(missing);
    auto comparison = platform::windows::same_underlying_local_file(
        source, *missing.value());
    QVERIFY(!comparison);
    QCOMPARE(comparison.error()->code(), core::ErrorCode::ResourceNotFound);
    QFile verify{sourcePath};
    QVERIFY(verify.open(QIODevice::ReadOnly));
    QCOMPARE(verify.readAll(), QByteArray{"immutable"});
}

}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::WindowsResourceIdentityTest)

#include "test_windows_resource_identity.moc"
