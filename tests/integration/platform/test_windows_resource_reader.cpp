#include <rgsml/platform/windows/windows_resource_reader.hpp>

#include "../../audio_golden/wav/golden_vectors.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <type_traits>

namespace rgsml::tests {
namespace {

using platform::windows::WindowsResourceReader;

template <typename T>
concept HasWriteSurface = requires(T& value, std::span<const std::byte> bytes) {
    value.write(bytes);
};

static_assert(std::is_final_v<WindowsResourceReader>);
static_assert(std::is_base_of_v<core::IResourceReader, WindowsResourceReader>);
static_assert(!HasWriteSurface<WindowsResourceReader>);

[[nodiscard]] QByteArray golden_bytes()
{
    return QByteArray{
        reinterpret_cast<const char*>(audio_golden::kRiffPcm16Mono.data()),
        static_cast<qsizetype>(audio_golden::kRiffPcm16Mono.size()),
    };
}

[[nodiscard]] QString write_fixture(QTemporaryDir& temporaryDirectory)
{
    const auto path = temporaryDirectory.filePath(
        QString::fromUtf8("nested/../Source ünicode space.wav"));
    QFile file{QDir::cleanPath(path)};
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        return {};
    }
    if (file.write(golden_bytes()) != golden_bytes().size()) {
        return {};
    }
    file.close();
    return QDir::cleanPath(path);
}

[[nodiscard]] QByteArray hash_file(const QString& path)
{
    QFile file{path};
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256);
}

[[nodiscard]] std::string utf8(const QString& value)
{
    const auto bytes = value.toUtf8();
    return std::string{bytes.constData(), static_cast<std::size_t>(bytes.size())};
}

}  // namespace

class WindowsResourceReaderTest final : public QObject {
    Q_OBJECT

private slots:
    void referenceValidationAndCanonicalization();
    void readSeekCloseAndImmutability();
    void rejectsForeignOrWritableReferences();
};

void WindowsResourceReaderTest::referenceValidationAndCanonicalization()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = write_fixture(directory);
    QVERIFY(!path.isEmpty());

    const auto withSegments = QDir::toNativeSeparators(
        directory.path() + QStringLiteral("/nested/../Source ünicode space.wav"));
    auto first = WindowsResourceReader::make_read_reference(
        utf8(withSegments), "Unicode Source");
    auto second = WindowsResourceReader::make_read_reference(
        utf8(path), "Another display name");
    QVERIFY(first);
    QVERIFY(second);
    QCOMPARE(first.value()->provider_id(), std::string{"rgsml.windows.local-file"});
    QCOMPARE(first.value()->locator(), second.value()->locator());
    QVERIFY(first.value()->permissions().can_read());
    QVERIFY(!first.value()->permissions().can_write());
    QCOMPARE(first.value()->display_name(), std::string{"Unicode Source"});
    QVERIFY(first.value()->same_resource_identity(*second.value()));
    QVERIFY(QString::fromUtf8(first.value()->locator()).contains(
        QString::fromUtf8("ünicode")));

    auto lowerDrivePath = path;
    lowerDrivePath[0] = lowerDrivePath.at(0).toLower();
    auto lowerDrive = WindowsResourceReader::make_read_reference(
        utf8(lowerDrivePath), "drive rule");
    QVERIFY(lowerDrive);
    QCOMPARE(lowerDrive.value()->locator(), second.value()->locator());

    auto empty = WindowsResourceReader::make_read_reference({}, "empty");
    QVERIFY(!empty);
    QCOMPARE(empty.error()->code(), core::ErrorCode::InvalidArgument);
    auto relative = WindowsResourceReader::make_read_reference(
        "relative.wav", "relative");
    QVERIFY(!relative);
    QCOMPARE(relative.error()->code(), core::ErrorCode::InvalidArgument);
    auto url = WindowsResourceReader::make_read_reference(
        "file:///C:/source.wav", "url");
    QVERIFY(!url);
    QCOMPARE(url.error()->code(), core::ErrorCode::InvalidArgument);
    auto missing = WindowsResourceReader::make_read_reference(
        utf8(directory.filePath(QStringLiteral("missing.wav"))), "missing");
    QVERIFY(!missing);
    QCOMPARE(missing.error()->code(), core::ErrorCode::ResourceNotFound);
    auto folder = WindowsResourceReader::make_read_reference(
        utf8(directory.path()), "folder");
    QVERIFY(!folder);
    QCOMPARE(folder.error()->code(), core::ErrorCode::InvalidArgument);

    const std::string invalidUtf8{"C:/bad-\xC3\x28.wav", 13};
    auto invalid = WindowsResourceReader::make_read_reference(
        invalidUtf8, "invalid");
    QVERIFY(!invalid);
    QCOMPARE(invalid.error()->code(), core::ErrorCode::InvalidArgument);
}

void WindowsResourceReaderTest::readSeekCloseAndImmutability()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = write_fixture(directory);
    QVERIFY(!path.isEmpty());
    const auto hashBefore = hash_file(path);
    const auto sizeBefore = QFileInfo{path}.size();

    auto reference = WindowsResourceReader::make_read_reference(
        utf8(path), "Source fixture.wav");
    QVERIFY(reference);
    auto opened = WindowsResourceReader::open_read_only(std::move(*reference.value()));
    QVERIFY(opened);
    auto& reader = **opened.value();
    QVERIFY(reader.capabilities().supports(core::ResourceCapability::CanSeek));
    QVERIFY(reader.capabilities().supports(core::ResourceCapability::HasKnownSize));
    QVERIFY(!reader.capabilities().supports(core::ResourceCapability::CanResize));
    const auto size = reader.size_bytes();
    QVERIFY(size);
    QCOMPARE(*size.value(), static_cast<std::uint64_t>(golden_bytes().size()));
    QCOMPARE(*reader.position_bytes().value(), std::uint64_t{0});

    std::span<std::byte> empty;
    auto emptyRead = reader.read(empty);
    QVERIFY(emptyRead);
    QCOMPARE(*emptyRead.value(), std::size_t{0});
    QCOMPARE(*reader.position_bytes().value(), std::uint64_t{0});

    std::array<std::byte, 12> header{};
    auto headerRead = reader.read(header);
    QVERIFY(headerRead);
    QCOMPARE(*headerRead.value(), header.size());
    QCOMPARE(static_cast<unsigned char>(header[0]), static_cast<unsigned char>('R'));

    QVERIFY(reader.seek_bytes(static_cast<std::uint64_t>(golden_bytes().size() - 2)));
    std::array<std::byte, 8> tail{};
    auto tailRead = reader.read(tail);
    QVERIFY(tailRead);
    QCOMPARE(*tailRead.value(), std::size_t{2});
    auto eofRead = reader.read(tail);
    QVERIFY(eofRead);
    QCOMPARE(*eofRead.value(), std::size_t{0});

    const auto positionBeforeFailure = *reader.position_bytes().value();
    auto outOfRange = reader.seek_bytes(
        std::numeric_limits<std::uint64_t>::max());
    QVERIFY(!outOfRange);
    QCOMPARE(outOfRange.error()->code(), core::ErrorCode::OutOfRange);
    QCOMPARE(*reader.position_bytes().value(), positionBeforeFailure);

    QVERIFY(reader.seek_bytes(0));
    QVERIFY(reader.seek_bytes(static_cast<std::uint64_t>(golden_bytes().size())));
    QVERIFY(reader.close());
    QVERIFY(reader.close());
    auto closedRead = reader.read(header);
    QVERIFY(!closedRead);
    QCOMPARE(closedRead.error()->code(), core::ErrorCode::InvalidState);
    auto closedPosition = reader.position_bytes();
    QVERIFY(!closedPosition);
    QCOMPARE(closedPosition.error()->code(), core::ErrorCode::InvalidState);

    opened.value()->reset();
    QFile handleProbe{path};
    QVERIFY(handleProbe.open(QIODevice::ReadWrite));
    handleProbe.close();
    QCOMPARE(QFileInfo{path}.size(), sizeBefore);
    QCOMPARE(hash_file(path), hashBefore);
}

void WindowsResourceReaderTest::rejectsForeignOrWritableReferences()
{
    auto foreign = core::ResourceReference::create(
        "test.foreign", "opaque", true, false, "foreign");
    QVERIFY(foreign);
    auto foreignOpen = WindowsResourceReader::open_read_only(
        std::move(*foreign.value()));
    QVERIFY(!foreignOpen);
    QCOMPARE(foreignOpen.error()->code(), core::ErrorCode::InvalidArgument);

    auto noRead = core::ResourceReference::create(
        "rgsml.windows.local-file",
        "C:/does-not-matter.wav",
        false,
        true,
        "no read");
    QVERIFY(noRead);
    auto noReadOpen = WindowsResourceReader::open_read_only(
        std::move(*noRead.value()));
    QVERIFY(!noReadOpen);
    QCOMPARE(noReadOpen.error()->code(), core::ErrorCode::AccessDenied);

    auto writable = core::ResourceReference::create(
        "rgsml.windows.local-file", "C:/does-not-matter.wav", true, true, "writable");
    QVERIFY(writable);
    auto writableOpen = WindowsResourceReader::open_read_only(
        std::move(*writable.value()));
    QVERIFY(!writableOpen);
    QCOMPARE(writableOpen.error()->code(), core::ErrorCode::InvalidArgument);

    auto missing = core::ResourceReference::create(
        "rgsml.windows.local-file",
        "C:/rgsml-task-006-definitely-missing.wav",
        true,
        false,
        "missing");
    QVERIFY(missing);
    auto missingOpen = WindowsResourceReader::open_read_only(
        std::move(*missing.value()));
    QVERIFY(!missingOpen);
    QCOMPARE(missingOpen.error()->code(), core::ErrorCode::ResourceNotFound);
}

}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::WindowsResourceReaderTest)

#include "test_windows_resource_reader.moc"
