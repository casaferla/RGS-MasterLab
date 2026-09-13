#include "internal/sha256.hpp"

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/wav_reader.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/platform/windows/transactional_audio_exporter.hpp>
#include <rgsml/platform/windows/windows_resource_reader.hpp>
#include <rgsml/platform/windows/windows_resource_writer.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QTextStream>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>

namespace {

using namespace rgsml;

[[nodiscard]] std::string utf8(const QString& value)
{
    return value.toUtf8().toStdString();
}

[[nodiscard]] core::Result<std::string> file_hash(const QString& path)
{
    QFile file{path};
    if (!file.open(QIODevice::ReadOnly)) {
        return core::Result<std::string>::failure(core::Error{
            core::ErrorCode::IoFailure, "manual_source_hash_failed"});
    }
    platform::windows::internal::Sha256 sha;
    std::array<std::byte, 64U * 1024U> bytes{};
    while (true) {
        const auto read = file.read(
            reinterpret_cast<char*>(bytes.data()), static_cast<qint64>(bytes.size()));
        if (read < 0) {
            return core::Result<std::string>::failure(core::Error{
                core::ErrorCode::IoFailure, "manual_source_hash_failed"});
        }
        if (read == 0) {
            break;
        }
        sha.update(std::span<const std::byte>{bytes.data(), static_cast<std::size_t>(read)});
    }
    return core::Result<std::string>::success(
        platform::windows::internal::sha256_hex(sha.finalize()));
}

[[nodiscard]] bool fail(QTextStream& output, const QString& stage, const core::Error& error)
{
    const auto token = core::error_code_token(error.code());
    output << "MANUAL_FUNCTIONAL_GATE=FAIL\n"
           << "STAGE=" << stage << '\n'
           << "ERROR_TOKEN="
           << QString::fromUtf8(token.data(), static_cast<qsizetype>(token.size())) << '\n'
           << "DETAIL=" << QString::fromStdString(error.message()) << '\n';
    return false;
}

[[nodiscard]] bool run_gate(
    const QString& sourcePath,
    const QString& destinationPath,
    QTextStream& output)
{
    const QFileInfo sourceInfo{sourcePath};
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        output << "MANUAL_FUNCTIONAL_GATE=FAIL\nSTAGE=source_missing\n";
        return false;
    }
    if (QFileInfo::exists(destinationPath)) {
        output << "MANUAL_FUNCTIONAL_GATE=FAIL\nSTAGE=destination_must_be_brand_new\n";
        return false;
    }
    const auto sourceSizeBefore = sourceInfo.size();
    auto sourceHashBefore = file_hash(sourcePath);
    if (!sourceHashBefore) {
        return fail(output, QStringLiteral("source_hash_before"), *sourceHashBefore.error());
    }
    auto sourceReference = platform::windows::WindowsResourceReader::make_read_reference(
        utf8(sourcePath), utf8(sourceInfo.fileName()));
    if (!sourceReference) {
        return fail(output, QStringLiteral("source_reference"), *sourceReference.error());
    }
    auto sourceResource = platform::windows::WindowsResourceReader::open_read_only(
        *sourceReference.value());
    if (!sourceResource) {
        return fail(output, QStringLiteral("source_open"), *sourceResource.error());
    }
    auto reader = audio::WavReader::open(std::move(*sourceResource.value()));
    if (!reader) {
        return fail(output, QStringLiteral("source_decode_open"), *reader.error());
    }
    auto source = audio::AudioBuffer::create(
        (*reader.value())->info().audio_format(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},
        (*reader.value())->info().frame_count());
    if (!source) {
        return fail(output, QStringLiteral("source_buffer"), *source.error());
    }
    auto decoded = (*reader.value())->read_frames(
        core::FrameIndex{0}, source.value()->mutable_view());
    if (!decoded || *decoded.value() != (*reader.value())->info().frame_count()) {
        return decoded
            ? (output << "MANUAL_FUNCTIONAL_GATE=FAIL\nSTAGE=source_decode_truncated\n", false)
            : fail(output, QStringLiteral("source_decode"), *decoded.error());
    }
    static_cast<void>((*reader.value())->close());

    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    if (!registry) {
        return fail(output, QStringLiteral("registry"), *registry.error());
    }
    auto chain = dsp::ProcessingChain::create(
        *registry.value(), {dsp::ProcessingStage::MASTER, dsp::ChainSegment::MANUAL});
    if (!chain) {
        return fail(output, QStringLiteral("chain"), *chain.error());
    }
    auto range = core::FrameRange::create(
        core::FrameIndex{0}, core::FrameIndex{source.value()->view().frame_count().value()});
    auto block = core::FrameCount::create(257);
    if (!range || !block) {
        output << "MANUAL_FUNCTIONAL_GATE=FAIL\nSTAGE=render_contract\n";
        return false;
    }
    auto renderRequest = render::RenderRequest::create(
        source.value()->view(), *range.value(), *chain.value(), {}, *block.value());
    if (!renderRequest) {
        return fail(output, QStringLiteral("render_request"), *renderRequest.error());
    }
    auto rendered = render::render_preview(*renderRequest.value(), *registry.value());
    if (!rendered) {
        return fail(output, QStringLiteral("render"), *rendered.error());
    }

    auto destinationReference = platform::windows::WindowsResourceWriter::make_write_reference(
        utf8(destinationPath), utf8(QFileInfo{destinationPath}.fileName()));
    if (!destinationReference) {
        return fail(output, QStringLiteral("destination_reference"), *destinationReference.error());
    }
    auto exportRequest = platform::windows::TransactionalAudioExportRequest::create(
        *sourceReference.value(), rendered.value()->view(), *destinationReference.value(),
        audio::WavSampleFormat::IEEE_F64,
        platform::windows::ExportCollisionPolicy::FAIL_IF_EXISTS);
    if (!exportRequest) {
        return fail(output, QStringLiteral("export_request"), *exportRequest.error());
    }
    auto exported = platform::windows::TransactionalAudioExporter::export_new_file(
        *exportRequest.value());
    if (!exported) {
        return fail(output, QStringLiteral("export"), *exported.error());
    }

    auto reopenedReference = platform::windows::WindowsResourceReader::make_read_reference(
        utf8(destinationPath), utf8(QFileInfo{destinationPath}.fileName()));
    auto reopenedResource = reopenedReference
        ? platform::windows::WindowsResourceReader::open_read_only(*reopenedReference.value())
        : core::Result<std::unique_ptr<platform::windows::WindowsResourceReader>>::failure(
            *reopenedReference.error());
    if (!reopenedResource) {
        return fail(output, QStringLiteral("destination_reopen"), *reopenedResource.error());
    }
    auto reopened = audio::WavReader::open(std::move(*reopenedResource.value()));
    if (!reopened) {
        return fail(output, QStringLiteral("destination_wav"), *reopened.error());
    }

    auto sourceAsDestination = platform::windows::WindowsResourceWriter::make_write_reference(
        utf8(sourcePath), utf8(sourceInfo.fileName()));
    if (!sourceAsDestination) {
        return fail(output, QStringLiteral("source_collision_reference"), *sourceAsDestination.error());
    }
    auto collisionRequest = platform::windows::TransactionalAudioExportRequest::create(
        *sourceReference.value(), rendered.value()->view(), *sourceAsDestination.value(),
        audio::WavSampleFormat::IEEE_F64,
        platform::windows::ExportCollisionPolicy::FAIL_IF_EXISTS);
    if (!collisionRequest) {
        return fail(output, QStringLiteral("source_collision_request"), *collisionRequest.error());
    }
    auto sourceCollision = platform::windows::TransactionalAudioExporter::export_new_file(
        *collisionRequest.value());
    if (sourceCollision || sourceCollision.error()->code() != core::ErrorCode::InvalidState) {
        output << "MANUAL_FUNCTIONAL_GATE=FAIL\nSTAGE=source_collision_not_rejected\n";
        return false;
    }

    const auto existingBytes = QFileInfo{destinationPath}.size();
    auto existingCollision = platform::windows::TransactionalAudioExporter::export_new_file(
        *exportRequest.value());
    if (existingCollision
        || existingCollision.error()->code() != core::ErrorCode::InvalidState
        || QFileInfo{destinationPath}.size() != existingBytes) {
        output << "MANUAL_FUNCTIONAL_GATE=FAIL\nSTAGE=existing_destination_changed\n";
        return false;
    }
    auto sourceHashAfter = file_hash(sourcePath);
    if (!sourceHashAfter
        || *sourceHashAfter.value() != *sourceHashBefore.value()
        || QFileInfo{sourcePath}.size() != sourceSizeBefore) {
        output << "MANUAL_FUNCTIONAL_GATE=FAIL\nSTAGE=source_immutability\n";
        return false;
    }
    const auto candidates = QDir{QFileInfo{destinationPath}.dir()}.entryList(
        {QStringLiteral("*.rgsml-export-*.tmp")}, QDir::Files | QDir::Hidden);
    if (!candidates.isEmpty()) {
        output << "MANUAL_FUNCTIONAL_GATE=FAIL\nSTAGE=candidate_leak\n";
        return false;
    }

    const auto& info = (*reopened.value())->info();
    output << "MANUAL_FUNCTIONAL_GATE=PASS\n"
           << "SOURCE_SHA256=" << QString::fromStdString(*sourceHashBefore.value()) << '\n'
           << "SOURCE_SIZE=" << sourceSizeBefore << '\n'
           << "DESTINATION=" << QDir::toNativeSeparators(destinationPath) << '\n'
           << "SAMPLE_FORMAT=IEEE_F64\n"
           << "CONTAINER=" << (info.container_kind() == audio::WavContainerKind::RIFF ? "RIFF" : "RF64") << '\n'
           << "SAMPLE_RATE=" << info.audio_format().sample_rate().value() << '\n'
           << "CHANNEL_LAYOUT=" << (info.audio_format().channel_layout() == audio::ChannelLayout::MONO_C ? "MONO_C" : "STEREO_LR") << '\n'
           << "FRAME_COUNT=" << info.frame_count().value() << '\n'
           << "EXACT_FILE_SIZE=" << exported.value()->exact_file_size_bytes() << '\n'
           << "ENCODED_SHA256=" << QString::fromStdString(exported.value()->encoded_file_sha256()) << '\n'
           << "DECODED_RGSDAU1_SHA256=" << QString::fromStdString(exported.value()->decoded_audio_sha256()) << '\n'
           << "SOURCE_COLLISION=REJECTED\n"
           << "EXISTING_DESTINATION=REJECTED_UNCHANGED\n"
           << "SOURCE=UNCHANGED\n"
           << "CANDIDATE_SUCCESS_LEAK=NONE\n";
    return true;
}

}  // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application{argc, argv};
    QTextStream output{stdout};
    const auto arguments = application.arguments();
    if (arguments.size() != 3) {
        output << "Usage: rgsml_export_functional_gate.exe <checked-in-source.wav> <brand-new-destination.wav>\n";
        return 2;
    }
    return run_gate(
        QDir::cleanPath(arguments.at(1)),
        QDir::cleanPath(arguments.at(2)),
        output)
        ? 0
        : 1;
}
