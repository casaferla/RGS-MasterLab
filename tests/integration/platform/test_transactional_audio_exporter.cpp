#include "internal/transactional_audio_exporter_test_seam.hpp"

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/wav_reader.hpp>
#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/platform/windows/transactional_audio_exporter.hpp>
#include <rgsml/platform/windows/windows_resource_reader.hpp>
#include <rgsml/platform/windows/windows_resource_writer.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest/QTest>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace rgsml;
using platform::windows::ExportCollisionPolicy;
using platform::windows::ExportVerification;
using platform::windows::TransactionalAudioExporter;
using platform::windows::TransactionalAudioExportRequest;
using platform::windows::WindowsResourceReader;
using platform::windows::WindowsResourceWriter;
using platform::windows::internal::ExportFailurePoint;
using platform::windows::internal::set_export_failure_point_for_test;

struct DecodedSource final {
    core::ResourceReference reference;
    audio::AudioBuffer buffer;
};

[[nodiscard]] std::string utf8(const QString& value)
{
    return value.toUtf8().toStdString();
}

[[nodiscard]] QString fixture_path()
{
    return QDir::cleanPath(QString::fromUtf8(RGSML_SOURCE_DIR)
        + QStringLiteral("/tests/audio_golden/playback_src/listening_stereo_48000_pcm16.wav"));
}

[[nodiscard]] core::FrameCount frame_count(std::int64_t value)
{
    auto result = core::FrameCount::create(value);
    Q_ASSERT(result);
    return *result.value();
}

[[nodiscard]] core::FrameRange frame_range(std::int64_t begin, std::int64_t end)
{
    auto result = core::FrameRange::create(core::FrameIndex{begin}, core::FrameIndex{end});
    Q_ASSERT(result);
    return *result.value();
}

[[nodiscard]] audio::AudioFormat format(audio::ChannelLayout layout)
{
    auto rate = core::SampleRate::create(48000);
    Q_ASSERT(rate);
    auto result = audio::AudioFormat::create(*rate.value(), layout);
    Q_ASSERT(result);
    return *result.value();
}

[[nodiscard]] audio::AudioBuffer make_buffer(
    audio::ChannelLayout layout,
    std::span<const double> first,
    std::span<const double> second = {})
{
    auto result = audio::AudioBuffer::create(
        format(layout), audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0}, frame_count(static_cast<std::int64_t>(first.size())));
    Q_ASSERT(result);
    auto firstPlane = result.value()->mutable_view().channel(0U);
    Q_ASSERT(firstPlane);
    std::copy(first.begin(), first.end(), firstPlane.value()->begin());
    if (layout == audio::ChannelLayout::STEREO_LR) {
        Q_ASSERT(second.size() == first.size());
        auto secondPlane = result.value()->mutable_view().channel(1U);
        Q_ASSERT(secondPlane);
        std::copy(second.begin(), second.end(), secondPlane.value()->begin());
    }
    return std::move(*result.value());
}

[[nodiscard]] std::vector<std::uint64_t> bits(audio::AudioBufferView view)
{
    std::vector<std::uint64_t> result;
    for (std::size_t channel = 0; channel < view.format().channel_count(); ++channel) {
        auto plane = view.channel(channel);
        Q_ASSERT(plane);
        for (const auto sample : *plane.value()) {
            result.push_back(std::bit_cast<std::uint64_t>(sample));
        }
    }
    return result;
}

[[nodiscard]] QByteArray file_bytes(const QString& path)
{
    QFile file{path};
    const bool opened = file.open(QIODevice::ReadOnly);
    Q_ASSERT(opened);
    return file.readAll();
}

[[nodiscard]] DecodedSource decode_source(const QString& path)
{
    auto reference = WindowsResourceReader::make_read_reference(
        utf8(path), utf8(QFileInfo{path}.fileName()));
    Q_ASSERT(reference);
    auto resource = WindowsResourceReader::open_read_only(*reference.value());
    Q_ASSERT(resource);
    auto wav = audio::WavReader::open(std::move(*resource.value()));
    Q_ASSERT(wav);
    auto buffer = audio::AudioBuffer::create(
        (*wav.value())->info().audio_format(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},
        (*wav.value())->info().frame_count());
    Q_ASSERT(buffer);
    auto read = (*wav.value())->read_frames(
        core::FrameIndex{0}, buffer.value()->mutable_view());
    Q_ASSERT(read);
    Q_ASSERT(*read.value() == (*wav.value())->info().frame_count());
    const auto close = (*wav.value())->close();
    Q_ASSERT(close);
    return DecodedSource{*reference.value(), std::move(*buffer.value())};
}

[[nodiscard]] core::ResourceReference destination_reference(const QString& path)
{
    auto reference = WindowsResourceWriter::make_write_reference(
        utf8(path), utf8(QFileInfo{path}.fileName()));
    Q_ASSERT(reference);
    return *reference.value();
}

[[nodiscard]] core::Result<platform::windows::TransactionalAudioExportResult> export_to(
    const core::ResourceReference& source,
    audio::AudioBufferView payload,
    const QString& destination)
{
    auto request = TransactionalAudioExportRequest::create(
        source,
        payload,
        destination_reference(destination),
        audio::WavSampleFormat::IEEE_F64,
        ExportCollisionPolicy::FAIL_IF_EXISTS);
    Q_ASSERT(request);
    return TransactionalAudioExporter::export_new_file(*request.value());
}

[[nodiscard]] audio::AudioBuffer reopen_export(const QString& path)
{
    return decode_source(path).buffer;
}

[[nodiscard]] dsp::ModuleInstanceId gain_id()
{
    auto uuid = core::Uuid::parse("39000000-0000-0000-0000-000000000001");
    Q_ASSERT(uuid);
    auto id = dsp::ModuleInstanceId::from_uuid(*uuid.value());
    Q_ASSERT(id);
    return *id.value();
}

[[nodiscard]] bool no_candidates(const QString& directory)
{
    const QDir dir{directory};
    return dir.entryList(
        {QStringLiteral("*.rgsml-export-*.tmp")},
        QDir::Files | QDir::Hidden).isEmpty();
}

class TransactionalAudioExporterTest final : public QObject {
    Q_OBJECT

private slots:
    void windowsWriterCreateNewLifecycle();
    void monoStereoRoundTripAndEvidence();
    void task011IdentityGainRoundTripAndSourceImmutability();
    void sourceAndDestinationCollisionProtection();
    void injectedFailureStagesCleanUp();
};

void TransactionalAudioExporterTest::windowsWriterCreateNewLifecycle()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("writer.bin"));
    auto reference = destination_reference(path);
    auto writer = WindowsResourceWriter::open_create_new(reference);
    QVERIFY(writer);
    QVERIFY((*writer.value())->capabilities().supports(core::ResourceCapability::CanSeek));
    QVERIFY((*writer.value())->capabilities().supports(core::ResourceCapability::CanResize));
    QVERIFY((*writer.value())->capabilities().supports(core::ResourceCapability::CanFlush));
    constexpr std::array<std::byte, 4> first{
        std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    auto write = (*writer.value())->write(first);
    QVERIFY(write);
    QCOMPARE(*write.value(), std::size_t{4});
    QVERIFY((*writer.value())->seek_bytes(1U));
    constexpr std::array<std::byte, 1> replacement{std::byte{9}};
    QVERIFY((*writer.value())->write(replacement));
    QVERIFY((*writer.value())->resize_bytes(3U));
    QVERIFY((*writer.value())->flush());
    QVERIFY((*writer.value())->close());
    QVERIFY((*writer.value())->close());
    QCOMPARE(file_bytes(path), QByteArray::fromRawData("\x01\x09\x03", 3));

    auto existing = WindowsResourceWriter::open_create_new(reference);
    QVERIFY(existing.error() != nullptr);
    QCOMPARE(existing.error()->code(), core::ErrorCode::InvalidState);
    QCOMPARE(file_bytes(path), QByteArray::fromRawData("\x01\x09\x03", 3));
}

void TransactionalAudioExporterTest::monoStereoRoundTripAndEvidence()
{
    auto source = decode_source(fixture_path());
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto subnormal = std::bit_cast<double>(std::uint64_t{1});
    const std::array<double, 6> mono{0.0, -0.0, subnormal, -0.5, 1.0, 2.5};
    auto monoBuffer = make_buffer(audio::ChannelLayout::MONO_C, mono);
    const auto monoPath = directory.filePath(QStringLiteral("mono.wav"));
    auto monoResult = export_to(source.reference, monoBuffer.view(), monoPath);
    QVERIFY(monoResult);
    QCOMPARE(monoResult.value()->container_kind(), audio::WavContainerKind::RIFF);
    QCOMPARE(monoResult.value()->sample_format(), audio::WavSampleFormat::IEEE_F64);
    QCOMPARE(monoResult.value()->audio_format(), monoBuffer.view().format());
    QCOMPARE(monoResult.value()->frame_count(), monoBuffer.view().frame_count());
    QCOMPARE(monoResult.value()->exact_file_size_bytes(), std::uint64_t{58 + 6 * 8});
    QCOMPARE(monoResult.value()->encoded_file_sha256().size(), std::size_t{64});
    QCOMPARE(monoResult.value()->decoded_audio_sha256().size(), std::size_t{64});
    QCOMPARE(monoResult.value()->verification(), ExportVerification::VERIFIED);
    auto reopenedMono = reopen_export(monoPath);
    QCOMPARE(bits(reopenedMono.view()), bits(monoBuffer.view()));

    const std::array<double, 4> left{0.25, -0.25, -0.0, 1.5};
    const std::array<double, 4> right{-0.75, 0.75, 0.0, -2.5};
    auto stereoBuffer = make_buffer(audio::ChannelLayout::STEREO_LR, left, right);
    const auto stereoPath = directory.filePath(QStringLiteral("stereo.wav"));
    auto stereoResult = export_to(source.reference, stereoBuffer.view(), stereoPath);
    QVERIFY(stereoResult);
    auto reopenedStereo = reopen_export(stereoPath);
    QCOMPARE(bits(reopenedStereo.view()), bits(stereoBuffer.view()));
    QCOMPARE(reopenedStereo.view().format(), stereoBuffer.view().format());
    QCOMPARE(reopenedStereo.view().frame_count(), stereoBuffer.view().frame_count());
}

void TransactionalAudioExporterTest::task011IdentityGainRoundTripAndSourceImmutability()
{
    const auto sourcePath = fixture_path();
    const auto sourceBefore = file_bytes(sourcePath);
    auto source = decode_source(sourcePath);
    const auto sourceBits = bits(source.buffer.view());
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto chain = dsp::ProcessingChain::create(
        *registry.value(), {dsp::ProcessingStage::MASTER, dsp::ChainSegment::MANUAL});
    QVERIFY(chain);
    auto identityRequest = render::RenderRequest::create(
        source.buffer.view(),
        frame_range(0, source.buffer.view().frame_count().value()),
        *chain.value(), {}, frame_count(257));
    QVERIFY(identityRequest);
    auto identity = render::render_preview(*identityRequest.value(), *registry.value());
    QVERIFY(identity);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto identityPath = directory.filePath(QStringLiteral("identity.wav"));
    auto identityExport = export_to(source.reference, identity.value()->view(), identityPath);
    QVERIFY(identityExport);
    auto identityDecoded = reopen_export(identityPath);
    QCOMPARE(bits(identityDecoded.view()), bits(identity.value()->view()));
    QCOMPARE(identityDecoded.view().frame_count().value(), identity.value()->render_window().length().value()->value());

    const auto id = gain_id();
    QVERIFY(chain.value()->add(id, "rgsml.dsp.gain", 0));
    auto gain = dsp::GainParameters::create(-12.0);
    QVERIFY(gain);
    auto gainRequest = render::RenderRequest::create(
        source.buffer.view(),
        frame_range(0, source.buffer.view().frame_count().value()),
        *chain.value(), {{id, *gain.value()}}, frame_count(64));
    QVERIFY(gainRequest);
    auto gained = render::render_preview(*gainRequest.value(), *registry.value());
    QVERIFY(gained);
    const auto gainPath = directory.filePath(QStringLiteral("gain.wav"));
    auto gainExport = export_to(source.reference, gained.value()->view(), gainPath);
    QVERIFY(gainExport);
    auto gainDecoded = reopen_export(gainPath);
    QCOMPARE(bits(gainDecoded.view()), bits(gained.value()->view()));

    QCOMPARE(bits(source.buffer.view()), sourceBits);
    QCOMPARE(file_bytes(sourcePath), sourceBefore);
}

void TransactionalAudioExporterTest::sourceAndDestinationCollisionProtection()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto copiedSource = directory.filePath(QStringLiteral("source.wav"));
    QVERIFY(QFile::copy(fixture_path(), copiedSource));
    auto source = decode_source(copiedSource);
    const auto sourceBefore = file_bytes(copiedSource);

    auto same = export_to(source.reference, source.buffer.view(), copiedSource);
    QVERIFY(same.error() != nullptr);
    QCOMPARE(same.error()->code(), core::ErrorCode::InvalidState);
    QVERIFY(same.error()->message().find("source_collision") != std::string::npos);
    QCOMPARE(file_bytes(copiedSource), sourceBefore);

    const auto existingPath = directory.filePath(QStringLiteral("existing.wav"));
    QFile existing{existingPath};
    QVERIFY(existing.open(QIODevice::WriteOnly | QIODevice::NewOnly));
    QCOMPARE(existing.write("existing", 8), qint64{8});
    existing.close();
    const auto existingBefore = file_bytes(existingPath);
    auto existingResult = export_to(source.reference, source.buffer.view(), existingPath);
    QVERIFY(existingResult.error() != nullptr);
    QCOMPARE(existingResult.error()->code(), core::ErrorCode::InvalidState);
    QVERIFY(existingResult.error()->message().find("destination_exists") != std::string::npos);
    QCOMPARE(file_bytes(existingPath), existingBefore);

    const auto hardlinkPath = directory.filePath(QStringLiteral("source-alias.wav"));
    const auto sourceNative = QDir::toNativeSeparators(copiedSource);
    const auto hardlinkNative = QDir::toNativeSeparators(hardlinkPath);
    QVERIFY(::CreateHardLinkW(
        reinterpret_cast<LPCWSTR>(hardlinkNative.utf16()),
        reinterpret_cast<LPCWSTR>(sourceNative.utf16()), nullptr) != FALSE);
    auto hardlinkResult = export_to(source.reference, source.buffer.view(), hardlinkPath);
    QVERIFY(hardlinkResult.error() != nullptr);
    QCOMPARE(hardlinkResult.error()->code(), core::ErrorCode::InvalidState);
    QVERIFY(hardlinkResult.error()->message().find("source_collision") != std::string::npos);
    QCOMPARE(file_bytes(copiedSource), sourceBefore);
}

void TransactionalAudioExporterTest::injectedFailureStagesCleanUp()
{
    auto source = decode_source(fixture_path());
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    struct Case final {
        ExportFailurePoint point;
        std::string_view token;
        core::ErrorCode code;
    };
    constexpr std::array cases{
        Case{ExportFailurePoint::CANDIDATE_CREATE, "candidate_create_failed", core::ErrorCode::IoFailure},
        Case{ExportFailurePoint::WRITER_IO, "writer_io_failed", core::ErrorCode::IoFailure},
        Case{ExportFailurePoint::VALIDATION, "candidate_validation_failed", core::ErrorCode::MalformedAudioContainer},
        Case{ExportFailurePoint::ENCODED_CHECKSUM, "encoded_checksum_mismatch", core::ErrorCode::IoFailure},
        Case{ExportFailurePoint::DECODED_CHECKSUM, "decoded_checksum_mismatch", core::ErrorCode::IoFailure},
        Case{ExportFailurePoint::COMMIT, "commit_failed", core::ErrorCode::IoFailure},
        Case{ExportFailurePoint::DESTINATION_READBACK, "destination_readback_mismatch", core::ErrorCode::IoFailure},
        Case{ExportFailurePoint::CLEANUP, "candidate_cleanup_failed", core::ErrorCode::IoFailure},
    };
    int index = 0;
    for (const auto& item : cases) {
        const auto path = directory.filePath(QStringLiteral("failure-%1.wav").arg(index++));
        set_export_failure_point_for_test(item.point);
        auto result = export_to(source.reference, source.buffer.view(), path);
        set_export_failure_point_for_test(ExportFailurePoint::NONE);
        QVERIFY(result.error() != nullptr);
        QCOMPARE(result.error()->code(), item.code);
        QVERIFY(result.error()->message().find(item.token) != std::string::npos);
        QVERIFY(!QFileInfo::exists(path));
        QVERIFY(no_candidates(directory.path()));
    }
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::TransactionalAudioExporterTest)
#include "test_transactional_audio_exporter.moc"
