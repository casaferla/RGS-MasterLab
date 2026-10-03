#include <rgsml/platform/windows/transactional_audio_exporter.hpp>

#include "internal/sha256.hpp"
#include "internal/transactional_audio_exporter_test_seam.hpp"

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/wav_reader.hpp>
#include <rgsml/audio/wav_writer.hpp>
#include <rgsml/platform/windows/windows_resource_reader.hpp>
#include <rgsml/platform/windows/windows_resource_writer.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QUuid>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace rgsml::platform::windows {
namespace {

using internal::ExportFailurePoint;

thread_local ExportFailurePoint g_failure_point = ExportFailurePoint::NONE;

template <typename T>
[[nodiscard]] core::Result<T> failure(core::ErrorCode code, std::string message)
{
    return core::Result<T>::failure(core::Error{code, std::move(message)});
}

struct FileIdentity final {
    std::uint64_t volume;
    std::uint64_t file;

    friend bool operator==(const FileIdentity&, const FileIdentity&) = default;
};

struct CandidateEvidence final {
    audio::WavContainerKind container_kind;
    std::uint64_t file_size;
    std::string encoded_sha256;
    std::string decoded_sha256;
};

[[nodiscard]] QString qpath(const core::ResourceReference& reference)
{
    return QString::fromUtf8(
        reference.locator().data(),
        static_cast<qsizetype>(reference.locator().size()));
}

[[nodiscard]] core::Result<std::optional<FileIdentity>> file_identity(
    const QString& path,
    bool required)
{
#ifdef _WIN32
    const auto native = QDir::toNativeSeparators(path);
    const HANDLE handle = ::CreateFileW(
        reinterpret_cast<LPCWSTR>(native.utf16()),
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        const auto error = ::GetLastError();
        if (!required
            && (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)) {
            return core::Result<std::optional<FileIdentity>>::success(std::nullopt);
        }
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            return failure<std::optional<FileIdentity>>(
                core::ErrorCode::ResourceNotFound,
                "source_missing: source file does not exist.");
        }
        if (error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION) {
            return failure<std::optional<FileIdentity>>(
                core::ErrorCode::AccessDenied,
                "source_identity_unavailable: file identity cannot be read safely.");
        }
        return failure<std::optional<FileIdentity>>(
            core::ErrorCode::InvalidState,
            "source_identity_unavailable: file identity cannot be established safely.");
    }
    BY_HANDLE_FILE_INFORMATION info{};
    const BOOL ok = ::GetFileInformationByHandle(handle, &info);
    ::CloseHandle(handle);
    if (ok == FALSE) {
        return failure<std::optional<FileIdentity>>(
            core::ErrorCode::InvalidState,
            "source_identity_unavailable: file identity query failed closed.");
    }
    const auto file = (static_cast<std::uint64_t>(info.nFileIndexHigh) << 32U)
        | static_cast<std::uint64_t>(info.nFileIndexLow);
    return core::Result<std::optional<FileIdentity>>::success(FileIdentity{
        static_cast<std::uint64_t>(info.dwVolumeSerialNumber), file});
#else
    QFileInfo info{path};
    if (!info.exists()) {
        if (!required) {
            return core::Result<std::optional<FileIdentity>>::success(std::nullopt);
        }
        return failure<std::optional<FileIdentity>>(
            core::ErrorCode::ResourceNotFound,
            "source_missing: source file does not exist.");
    }
    return core::Result<std::optional<FileIdentity>>::success(FileIdentity{
        1U, static_cast<std::uint64_t>(info.size())});
#endif
}

[[nodiscard]] core::Result<std::string> encoded_file_sha256(const QString& path)
{
    QFile file{path};
    if (!file.open(QIODevice::ReadOnly)) {
        return failure<std::string>(
            file.error() == QFileDevice::PermissionsError
                ? core::ErrorCode::AccessDenied
                : core::ErrorCode::IoFailure,
            "encoded_checksum_open_failed: candidate cannot be opened for hashing.");
    }
    internal::Sha256 sha;
    std::array<std::byte, 64U * 1024U> buffer{};
    while (true) {
        const auto count = file.read(
            reinterpret_cast<char*>(buffer.data()),
            static_cast<qint64>(buffer.size()));
        if (count < 0) {
            return failure<std::string>(
                core::ErrorCode::IoFailure,
                "encoded_checksum_read_failed: candidate hash read failed.");
        }
        if (count == 0) {
            break;
        }
        sha.update(std::span<const std::byte>{
            buffer.data(), static_cast<std::size_t>(count)});
    }
    return core::Result<std::string>::success(
        internal::sha256_hex(sha.finalize()));
}

[[nodiscard]] core::Result<QString> create_candidate_path(const QString& destination)
{
    const QFileInfo info{destination};
    const auto directory = info.dir();
    for (int attempt = 0; attempt < 32; ++attempt) {
        const auto name = QStringLiteral(".%1.rgsml-export-%2.tmp")
            .arg(info.fileName(), QUuid::createUuid().toString(QUuid::WithoutBraces));
        const auto candidate = directory.filePath(name);
        if (!QFileInfo::exists(candidate)) {
            return core::Result<QString>::success(candidate);
        }
    }
    return failure<QString>(
        core::ErrorCode::IoFailure,
        "candidate_create_failed: unable to allocate a unique sibling name.");
}

[[nodiscard]] core::Result<TransactionalAudioExportResult> cleanup_failure(
    core::Error original,
    const QString& taskOwnedPath,
    bool forceCleanupFailure)
{
    bool removed = true;
    if (QFileInfo::exists(taskOwnedPath)) {
        removed = QFile::remove(taskOwnedPath);
    }
    if (forceCleanupFailure || !removed) {
        return failure<TransactionalAudioExportResult>(
            core::ErrorCode::IoFailure,
            "candidate_cleanup_failed: task-owned partial artifact could not be cleaned; prior_stage="
                + original.message());
    }
    return core::Result<TransactionalAudioExportResult>::failure(std::move(original));
}

[[nodiscard]] core::Result<CandidateEvidence> validate_candidate(
    const QString& candidate,
    audio::AudioBufferView expected,
    std::uint64_t expectedSize)
{
    const auto candidateUtf8 = candidate.toUtf8().toStdString();
    auto readReference = WindowsResourceReader::make_read_reference(
        candidateUtf8,
        QFileInfo{candidate}.fileName().toUtf8().toStdString());
    if (!readReference) {
        return core::Result<CandidateEvidence>::failure(*readReference.error());
    }
    auto resource = WindowsResourceReader::open_read_only(*readReference.value());
    if (!resource) {
        return core::Result<CandidateEvidence>::failure(*resource.error());
    }
    auto wav = audio::WavReader::open(std::move(*resource.value()));
    if (!wav) {
        return core::Result<CandidateEvidence>::failure(*wav.error());
    }
    const auto& info = wav.value()->get()->info();
    if (info.encoded_sample_format() != audio::WavSampleFormat::IEEE_F64
        || info.audio_format() != expected.format()
        || info.frame_count() != expected.frame_count()) {
        return failure<CandidateEvidence>(
            core::ErrorCode::MalformedAudioContainer,
            "candidate_validation_failed: WAV metadata differs from the export request.");
    }
    const auto size = QFileInfo{candidate}.size();
    if (size < 0 || static_cast<std::uint64_t>(size) != expectedSize) {
        return failure<CandidateEvidence>(
            core::ErrorCode::MalformedAudioContainer,
            "candidate_validation_failed: exact file size differs from the writer plan.");
    }
    auto encoded = encoded_file_sha256(candidate);
    if (!encoded) {
        return core::Result<CandidateEvidence>::failure(*encoded.error());
    }
    auto expectedDecoded = internal::canonical_decoded_audio_sha256(expected);
    if (!expectedDecoded) {
        return core::Result<CandidateEvidence>::failure(*expectedDecoded.error());
    }
    auto decodedHasher = internal::CanonicalDecodedAudioHasher::create(
        expected.format(), expected.frame_count());
    if (!decodedHasher) {
        return core::Result<CandidateEvidence>::failure(*decodedHasher.error());
    }
    constexpr std::int64_t kValidationFrames = 4096;
    std::int64_t offset = 0;
    while (offset < expected.frame_count().value()) {
        const auto countValue = std::min(
            kValidationFrames, expected.frame_count().value() - offset);
        auto count = core::FrameCount::create(countValue);
        if (!count) {
            return core::Result<CandidateEvidence>::failure(*count.error());
        }
        auto decoded = audio::AudioBuffer::create(
            expected.format(),
            audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{offset},
            *count.value());
        if (!decoded) {
            return core::Result<CandidateEvidence>::failure(*decoded.error());
        }
        auto read = wav.value()->get()->read_frames(
            core::FrameIndex{offset}, decoded.value()->mutable_view());
        if (!read) {
            return core::Result<CandidateEvidence>::failure(*read.error());
        }
        if (*read.value() != *count.value()) {
            return failure<CandidateEvidence>(
                core::ErrorCode::TruncatedAudioData,
                "candidate_validation_failed: decoded WAV ended before its declared frame count.");
        }
        const auto decodedView = decoded.value()->view();
        for (std::size_t channel = 0; channel < expected.format().channel_count(); ++channel) {
            auto actualPlane = decodedView.channel(channel);
            auto expectedPlane = expected.channel(channel);
            if (!actualPlane || !expectedPlane) {
                return failure<CandidateEvidence>(
                    core::ErrorCode::InvalidArgument,
                    "candidate_validation_failed: invalid canonical channel access.");
            }
            for (std::size_t frame = 0; frame < static_cast<std::size_t>(countValue); ++frame) {
                const auto expectedBits = std::bit_cast<std::uint64_t>(
                    (*expectedPlane.value())[static_cast<std::size_t>(offset) + frame]);
                const auto actualBits = std::bit_cast<std::uint64_t>(
                    (*actualPlane.value())[frame]);
                if (expectedBits != actualBits) {
                    return failure<CandidateEvidence>(
                        core::ErrorCode::IoFailure,
                        "decoded_checksum_mismatch: decoded IEEE_F64 payload differs bit-for-bit.");
                }
            }
        }
        auto update = decodedHasher.value()->update(decodedView);
        if (!update) {
            return core::Result<CandidateEvidence>::failure(*update.error());
        }
        offset += countValue;
    }
    auto decodedHash = decodedHasher.value()->finalize();
    if (!decodedHash) {
        return core::Result<CandidateEvidence>::failure(*decodedHash.error());
    }
    if (*decodedHash.value() != *expectedDecoded.value()) {
        return failure<CandidateEvidence>(
            core::ErrorCode::IoFailure,
            "decoded_checksum_mismatch: canonical decoded hashes differ.");
    }
    auto close = wav.value()->get()->close();
    if (!close) {
        return core::Result<CandidateEvidence>::failure(*close.error());
    }
    return core::Result<CandidateEvidence>::success(CandidateEvidence{
        info.container_kind(), expectedSize, *encoded.value(), *decodedHash.value()});
}

}  // namespace

namespace internal {

void set_export_failure_point_for_test(ExportFailurePoint point) noexcept
{
    g_failure_point = point;
}

}  // namespace internal

core::Result<TransactionalAudioExportRequest> TransactionalAudioExportRequest::create(
    core::ResourceReference source,
    audio::AudioBufferView payload,
    core::ResourceReference destination,
    audio::WavSampleFormat sampleFormat,
    ExportCollisionPolicy collisionPolicy)
{
    if (sampleFormat != audio::WavSampleFormat::IEEE_F64) {
        return failure<TransactionalAudioExportRequest>(
            core::ErrorCode::UnsupportedAudioEncoding,
            "export_unsupported_encoding: L1-M09 supports IEEE_F64 only.");
    }
    if (collisionPolicy != ExportCollisionPolicy::FAIL_IF_EXISTS) {
        return failure<TransactionalAudioExportRequest>(
            core::ErrorCode::UnsupportedOperation,
            "export_unsupported_collision_policy: only FAIL_IF_EXISTS is supported.");
    }
    if (!source.permissions().can_read()) {
        return failure<TransactionalAudioExportRequest>(
            core::ErrorCode::AccessDenied,
            "export_source_read_denied: source reference must be readable.");
    }
    if (!destination.permissions().can_write()) {
        return failure<TransactionalAudioExportRequest>(
            core::ErrorCode::AccessDenied,
            "export_destination_write_denied: destination reference must be writable.");
    }
    return core::Result<TransactionalAudioExportRequest>::success(
        TransactionalAudioExportRequest{
            std::move(source), payload, std::move(destination), sampleFormat, collisionPolicy});
}

TransactionalAudioExportRequest::TransactionalAudioExportRequest(
    core::ResourceReference source,
    audio::AudioBufferView payload,
    core::ResourceReference destination,
    audio::WavSampleFormat sampleFormat,
    ExportCollisionPolicy collisionPolicy) noexcept
    : source_(std::move(source))
    , payload_(payload)
    , destination_(std::move(destination))
    , sample_format_(sampleFormat)
    , collision_policy_(collisionPolicy)
{
}

const core::ResourceReference& TransactionalAudioExportRequest::source() const noexcept
{
    return source_;
}

audio::AudioBufferView TransactionalAudioExportRequest::payload() const noexcept
{
    return payload_;
}

const core::ResourceReference& TransactionalAudioExportRequest::destination() const noexcept
{
    return destination_;
}

audio::WavSampleFormat TransactionalAudioExportRequest::sample_format() const noexcept
{
    return sample_format_;
}

ExportCollisionPolicy TransactionalAudioExportRequest::collision_policy() const noexcept
{
    return collision_policy_;
}

TransactionalAudioExportResult::TransactionalAudioExportResult(
    core::ResourceReference destination,
    audio::WavContainerKind containerKind,
    audio::WavSampleFormat sampleFormat,
    audio::AudioFormat audioFormat,
    core::FrameCount frameCount,
    std::uint64_t exactFileSizeBytes,
    std::string encodedFileSha256,
    std::string decodedAudioSha256) noexcept
    : destination_(std::move(destination))
    , container_kind_(containerKind)
    , sample_format_(sampleFormat)
    , audio_format_(audioFormat)
    , frame_count_(frameCount)
    , exact_file_size_bytes_(exactFileSizeBytes)
    , encoded_file_sha256_(std::move(encodedFileSha256))
    , decoded_audio_sha256_(std::move(decodedAudioSha256))
{
}

const core::ResourceReference& TransactionalAudioExportResult::destination() const noexcept
{
    return destination_;
}

audio::WavContainerKind TransactionalAudioExportResult::container_kind() const noexcept
{
    return container_kind_;
}

audio::WavSampleFormat TransactionalAudioExportResult::sample_format() const noexcept
{
    return sample_format_;
}

const audio::AudioFormat& TransactionalAudioExportResult::audio_format() const noexcept
{
    return audio_format_;
}

core::FrameCount TransactionalAudioExportResult::frame_count() const noexcept
{
    return frame_count_;
}

std::uint64_t TransactionalAudioExportResult::exact_file_size_bytes() const noexcept
{
    return exact_file_size_bytes_;
}

const std::string& TransactionalAudioExportResult::encoded_file_sha256() const noexcept
{
    return encoded_file_sha256_;
}

const std::string& TransactionalAudioExportResult::decoded_audio_sha256() const noexcept
{
    return decoded_audio_sha256_;
}

ExportVerification TransactionalAudioExportResult::verification() const noexcept
{
    return ExportVerification::VERIFIED;
}

core::Result<TransactionalAudioExportResult>
TransactionalAudioExporter::export_new_file(
    const TransactionalAudioExportRequest& request)
{
    if (request.source().provider_id() != WindowsResourceReader::provider_id()
        || request.destination().provider_id() != WindowsResourceWriter::provider_id()) {
        return failure<TransactionalAudioExportResult>(
            core::ErrorCode::InvalidArgument,
            "export_provider_invalid: source and destination must use Windows local-file.");
    }
    if (request.sample_format() != audio::WavSampleFormat::IEEE_F64
        || request.collision_policy() != ExportCollisionPolicy::FAIL_IF_EXISTS) {
        return failure<TransactionalAudioExportResult>(
            core::ErrorCode::UnsupportedOperation,
            "export_request_unsupported: request violates the L1-M09 frozen policy.");
    }

    const auto sourcePath = qpath(request.source());
    const auto destinationPath = qpath(request.destination());
    auto sourceIdentity = file_identity(sourcePath, true);
    if (!sourceIdentity) {
        return core::Result<TransactionalAudioExportResult>::failure(*sourceIdentity.error());
    }
    if (QString::compare(sourcePath, destinationPath, Qt::CaseInsensitive) == 0) {
        return failure<TransactionalAudioExportResult>(
            core::ErrorCode::InvalidState,
            "source_collision: destination resolves to the Source path.");
    }
    auto destinationIdentity = file_identity(destinationPath, false);
    if (!destinationIdentity) {
        return core::Result<TransactionalAudioExportResult>::failure(*destinationIdentity.error());
    }
    if (destinationIdentity.value()->has_value()) {
        if (**sourceIdentity.value() == **destinationIdentity.value()) {
            return failure<TransactionalAudioExportResult>(
                core::ErrorCode::InvalidState,
                "source_collision: destination is an alias of the Source file.");
        }
        return failure<TransactionalAudioExportResult>(
            core::ErrorCode::InvalidState,
            "destination_exists: FAIL_IF_EXISTS refuses replacement.");
    }

    auto candidatePath = create_candidate_path(destinationPath);
    if (!candidatePath) {
        return core::Result<TransactionalAudioExportResult>::failure(*candidatePath.error());
    }
    if (g_failure_point == ExportFailurePoint::CANDIDATE_CREATE) {
        return failure<TransactionalAudioExportResult>(
            core::ErrorCode::IoFailure,
            "candidate_create_failed: injected candidate creation failure.");
    }
    auto candidateReference = WindowsResourceWriter::make_write_reference(
        candidatePath.value()->toUtf8().toStdString(),
        QFileInfo{*candidatePath.value()}.fileName().toUtf8().toStdString());
    if (!candidateReference) {
        return core::Result<TransactionalAudioExportResult>::failure(*candidateReference.error());
    }
    auto destination = WindowsResourceWriter::open_create_new(*candidateReference.value());
    if (!destination) {
        return core::Result<TransactionalAudioExportResult>::failure(*destination.error());
    }
    const bool forceCleanupFailure = g_failure_point == ExportFailurePoint::CLEANUP;
    if (g_failure_point == ExportFailurePoint::WRITER_IO || forceCleanupFailure) {
        static_cast<void>(destination.value()->get()->close());
        return cleanup_failure(
            core::Error{core::ErrorCode::IoFailure,
                forceCleanupFailure
                    ? "cleanup_probe_failure: injected pre-cleanup failure."
                    : "writer_io_failed: injected writer failure."},
            *candidatePath.value(),
            forceCleanupFailure);
    }

    audio::WavWriteSpec spec{
        request.payload().format(),
        request.payload().frame_count(),
        request.sample_format()};
    auto writer = audio::WavWriter::open(std::move(*destination.value()), spec);
    if (!writer) {
        return cleanup_failure(*writer.error(), *candidatePath.value(), false);
    }
    const auto expectedSize = writer.value()->get()->expected_file_size_bytes();
    constexpr std::int64_t kWriteFrames = 4096;
    std::int64_t offset = 0;
    while (offset < request.payload().frame_count().value()) {
        const auto countValue = std::min(
            kWriteFrames, request.payload().frame_count().value() - offset);
        auto count = core::FrameCount::create(countValue);
        if (!count) {
            return cleanup_failure(*count.error(), *candidatePath.value(), false);
        }
        auto view = request.payload().subview(
            core::FrameIndex{request.payload().absolute_start_frame().value() + offset},
            *count.value());
        if (!view) {
            return cleanup_failure(*view.error(), *candidatePath.value(), false);
        }
        auto write = writer.value()->get()->write_frames(*view.value());
        if (!write) {
            static_cast<void>(writer.value()->get()->close());
            return cleanup_failure(*write.error(), *candidatePath.value(), false);
        }
        offset += countValue;
    }
    auto finalize = writer.value()->get()->finalize();
    if (!finalize) {
        static_cast<void>(writer.value()->get()->close());
        return cleanup_failure(*finalize.error(), *candidatePath.value(), false);
    }
    auto close = writer.value()->get()->close();
    if (!close) {
        return cleanup_failure(*close.error(), *candidatePath.value(), false);
    }
    if (g_failure_point == ExportFailurePoint::VALIDATION) {
        return cleanup_failure(
            core::Error{core::ErrorCode::MalformedAudioContainer,
                "candidate_validation_failed: injected structural failure."},
            *candidatePath.value(), false);
    }

    auto evidence = validate_candidate(
        *candidatePath.value(), request.payload(), expectedSize);
    if (!evidence) {
        return cleanup_failure(*evidence.error(), *candidatePath.value(), false);
    }
    if (g_failure_point == ExportFailurePoint::ENCODED_CHECKSUM) {
        return cleanup_failure(
            core::Error{core::ErrorCode::IoFailure,
                "encoded_checksum_mismatch: injected encoded hash mismatch."},
            *candidatePath.value(), false);
    }
    if (g_failure_point == ExportFailurePoint::DECODED_CHECKSUM) {
        return cleanup_failure(
            core::Error{core::ErrorCode::IoFailure,
                "decoded_checksum_mismatch: injected decoded hash mismatch."},
            *candidatePath.value(), false);
    }
    if (QFileInfo::exists(destinationPath)) {
        return cleanup_failure(
            core::Error{core::ErrorCode::InvalidState,
                "destination_exists: commit race detected; replacement refused."},
            *candidatePath.value(), false);
    }
    if (g_failure_point == ExportFailurePoint::COMMIT
        || !QFile::rename(*candidatePath.value(), destinationPath)) {
        return cleanup_failure(
            core::Error{core::ErrorCode::IoFailure,
                "commit_failed: same-volume create-new rename failed."},
            *candidatePath.value(), false);
    }
    if (g_failure_point == ExportFailurePoint::DESTINATION_READBACK) {
        return cleanup_failure(
            core::Error{core::ErrorCode::IoFailure,
                "destination_readback_mismatch: injected final checksum mismatch."},
            destinationPath, false);
    }
    auto readback = encoded_file_sha256(destinationPath);
    if (!readback || *readback.value() != evidence.value()->encoded_sha256) {
        const auto error = readback
            ? core::Error{core::ErrorCode::IoFailure,
                "destination_readback_mismatch: committed bytes differ from the candidate."}
            : *readback.error();
        return cleanup_failure(error, destinationPath, false);
    }
    return core::Result<TransactionalAudioExportResult>::success(
        TransactionalAudioExportResult{
            request.destination(),
            evidence.value()->container_kind,
            request.sample_format(),
            request.payload().format(),
            request.payload().frame_count(),
            evidence.value()->file_size,
            evidence.value()->encoded_sha256,
            evidence.value()->decoded_sha256});
}

}  // namespace rgsml::platform::windows
