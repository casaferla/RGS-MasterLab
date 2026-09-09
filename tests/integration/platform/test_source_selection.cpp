#include "source_selection_view_model.hpp"

#include "../../audio_golden/wav/golden_vectors.hpp"
#include "../../unit/audio/wav_test_support.hpp"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace rgsml::tests {
namespace {

using namespace wav_support;

[[nodiscard]] QByteArray to_byte_array(std::span<const std::byte> bytes)
{
    return QByteArray{
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<qsizetype>(bytes.size()),
    };
}

template <std::size_t Size>
[[nodiscard]] QByteArray to_byte_array(const std::array<std::uint8_t, Size>& bytes)
{
    return QByteArray{
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<qsizetype>(bytes.size()),
    };
}

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

}  // namespace

class SourceSelectionTest final : public QObject {
    Q_OBJECT

private slots:
    void initialSuccessCancelFailureRetry();
    void acceptedFormatMappings_data();
    void acceptedFormatMappings();
};

void SourceSelectionTest::initialSuccessCancelFailureRetry()
{
    app::SourceSelectionViewModel model;
    QCOMPARE(model.source_state(), QStringLiteral("NO_SOURCE"));
    QVERIFY(!model.has_source());
    QVERIFY(model.display_name().isEmpty());
    QVERIFY(model.error_message().isEmpty());
    QVERIFY(!model.read_only());

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto validPath = write_file(
        directory,
        QStringLiteral("Valid Source.wav"),
        to_byte_array(audio_golden::kRiffPcm16Mono));
    QVERIFY(!validPath.isEmpty());

    QSignalSpy sourceSpy{&model, &app::SourceSelectionViewModel::sourceChanged};
    model.selectSource(QUrl::fromLocalFile(validPath));
    QCOMPARE(sourceSpy.count(), 1);
    QCOMPARE(model.source_state(), QStringLiteral("SOURCE_READY"));
    QVERIFY(model.has_source());
    QCOMPARE(model.display_name(), QStringLiteral("Valid Source.wav"));
    QCOMPARE(model.container_label(), QStringLiteral("RIFF/WAVE"));
    QCOMPARE(model.sample_format_label(), QStringLiteral("PCM 16-bit"));
    QCOMPARE(model.sample_rate_hz(), qint64{44100});
    QCOMPARE(model.channel_layout_label(), QStringLiteral("Mono (C)"));
    QCOMPARE(model.channel_count(), 1);
    QCOMPARE(model.frame_count(), qint64{3});
    QCOMPARE(model.duration_label(), QStringLiteral("0:00.000"));
    QVERIFY(model.read_only());

    const auto stateBeforeCancel = model.source_state();
    const auto nameBeforeCancel = model.display_name();
    const auto errorBeforeCancel = model.error_message();
    model.cancelSourceSelection();
    QCOMPARE(sourceSpy.count(), 1);
    QCOMPARE(model.source_state(), stateBeforeCancel);
    QCOMPARE(model.display_name(), nameBeforeCancel);
    QCOMPARE(model.error_message(), errorBeforeCancel);

    model.selectSource(QUrl{QStringLiteral("https://example.invalid/source.wav")});
    QCOMPARE(model.source_state(), QStringLiteral("SOURCE_ERROR"));
    QVERIFY(model.has_source());
    QCOMPARE(model.display_name(), nameBeforeCancel);
    QVERIFY(!model.error_message().isEmpty());
    QCOMPARE(sourceSpy.count(), 1);

    const auto malformedPath = write_file(
        directory,
        QStringLiteral("Malformed.wav"),
        QByteArray{"not a wav"});
    model.selectSource(QUrl::fromLocalFile(malformedPath));
    QCOMPARE(model.source_state(), QStringLiteral("SOURCE_ERROR"));
    QCOMPARE(model.display_name(), nameBeforeCancel);
    QCOMPARE(sourceSpy.count(), 1);

    const auto retryPath = write_file(
        directory,
        QStringLiteral("Retry RF64.wav"),
        to_byte_array(audio_golden::kRf64F64NegativeZero));
    model.selectSource(QUrl::fromLocalFile(retryPath));
    QCOMPARE(sourceSpy.count(), 2);
    QCOMPARE(model.source_state(), QStringLiteral("SOURCE_READY"));
    QCOMPARE(model.display_name(), QStringLiteral("Retry RF64.wav"));
    QCOMPARE(model.container_label(), QStringLiteral("RF64/WAVE"));
    QCOMPARE(model.sample_format_label(), QStringLiteral("IEEE float 64-bit"));
    QVERIFY(model.error_message().isEmpty());
}

void SourceSelectionTest::acceptedFormatMappings_data()
{
    QTest::addColumn<QByteArray>("wav");
    QTest::addColumn<QString>("formatLabel");
    QTest::addColumn<QString>("layoutLabel");
    QTest::addColumn<int>("channels");

    const auto pcm16 = make_wav(
        1, 16, 1, 48000,
        pcm_payload(std::array<std::int64_t, 1>{0}, 16));
    const auto pcm24 = make_wav(
        1, 24, 2, 48000,
        pcm_payload(std::array<std::int64_t, 2>{0, 1}, 24));
    const auto pcm32 = make_wav(
        1, 32, 1, 48000,
        pcm_payload(std::array<std::int64_t, 1>{0}, 32));
    const auto f32 = make_wav(
        3, 32, 2, 48000,
        f32_payload(std::array<std::uint32_t, 2>{0, 0x3f000000U}));
    const auto f64 = make_wav(
        3, 64, 1, 48000,
        f64_payload(std::array<std::uint64_t, 1>{0}));

    QTest::newRow("pcm16") << to_byte_array(pcm16) << QStringLiteral("PCM 16-bit")
                            << QStringLiteral("Mono (C)") << 1;
    QTest::newRow("pcm24") << to_byte_array(pcm24) << QStringLiteral("PCM 24-bit")
                            << QStringLiteral("Stereo (L/R)") << 2;
    QTest::newRow("pcm32") << to_byte_array(pcm32) << QStringLiteral("PCM 32-bit")
                            << QStringLiteral("Mono (C)") << 1;
    QTest::newRow("f32") << to_byte_array(f32) << QStringLiteral("IEEE float 32-bit")
                          << QStringLiteral("Stereo (L/R)") << 2;
    QTest::newRow("f64") << to_byte_array(f64) << QStringLiteral("IEEE float 64-bit")
                          << QStringLiteral("Mono (C)") << 1;
}

void SourceSelectionTest::acceptedFormatMappings()
{
    QFETCH(QByteArray, wav);
    QFETCH(QString, formatLabel);
    QFETCH(QString, layoutLabel);
    QFETCH(int, channels);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = write_file(directory, QStringLiteral("mapping.wav"), wav);
    QVERIFY(!path.isEmpty());
    app::SourceSelectionViewModel model;
    model.selectSource(QUrl::fromLocalFile(path));
    QVERIFY(model.has_source());
    QCOMPARE(model.sample_format_label(), formatLabel);
    QCOMPARE(model.channel_layout_label(), layoutLabel);
    QCOMPARE(model.channel_count(), channels);
    QCOMPARE(model.sample_rate_hz(), qint64{48000});
    QCOMPARE(model.frame_count(), qint64{1});
}

}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::SourceSelectionTest)

#include "test_source_selection.moc"
