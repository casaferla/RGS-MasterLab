#include <rgsml/platform/windows/windows_resource_writer.hpp>

#include <QByteArrayView>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QStringDecoder>

#include <algorithm>
#include <limits>
#include <new>
#include <string>
#include <utility>

namespace rgsml::platform::windows {
namespace {

template <typename T>
[[nodiscard]] core::Result<T> failure(core::ErrorCode code, std::string message)
{
    return core::Result<T>::failure(core::Error{code, std::move(message)});
}

[[nodiscard]] core::Status status_failure(
    core::ErrorCode code,
    std::string message)
{
    return core::Status::failure(core::Error{code, std::move(message)});
}

[[nodiscard]] core::Result<QString> decode_utf8(
    std::string_view value,
    std::string message)
{
    if (value.size() > static_cast<std::size_t>(
            std::numeric_limits<qsizetype>::max())) {
        return failure<QString>(core::ErrorCode::OutOfRange, std::move(message));
    }
    QStringDecoder decoder{QStringDecoder::Utf8};
    const QString decoded = decoder.decode(QByteArrayView{
        value.data(), static_cast<qsizetype>(value.size())});
    if (decoder.hasError() || decoded.contains(QChar::Null)) {
        return failure<QString>(core::ErrorCode::InvalidArgument, std::move(message));
    }
    return core::Result<QString>::success(decoded);
}

[[nodiscard]] bool is_ascii_letter(QChar value) noexcept
{
    const auto code = value.unicode();
    return (code >= static_cast<ushort>('A') && code <= static_cast<ushort>('Z'))
        || (code >= static_cast<ushort>('a') && code <= static_cast<ushort>('z'));
}

[[nodiscard]] core::Result<QString> canonical_local_path(std::string_view pathUtf8)
{
    if (pathUtf8.empty()) {
        return failure<QString>(
            core::ErrorCode::InvalidArgument,
            "Destination path must not be empty.");
    }
    auto decoded = decode_utf8(pathUtf8, "Destination path must be valid UTF-8.");
    if (!decoded) {
        return decoded;
    }
    auto normalized = QDir::fromNativeSeparators(*decoded.value());
    if (normalized.startsWith(QStringLiteral("//"))
        || normalized.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive)
        || normalized.contains(QStringLiteral("://"))
        || normalized.size() < 3
        || !is_ascii_letter(normalized.at(0))
        || normalized.at(1) != QLatin1Char(':')
        || normalized.at(2) != QLatin1Char('/')) {
        return failure<QString>(
            core::ErrorCode::InvalidArgument,
            "Only an absolute local drive destination is supported.");
    }
    normalized = QDir::cleanPath(normalized);
    normalized[0] = normalized.at(0).toUpper();
    return core::Result<QString>::success(normalized);
}

[[nodiscard]] core::ErrorCode file_error_code(QFileDevice::FileError error) noexcept
{
    if (error == QFileDevice::PermissionsError) {
        return core::ErrorCode::AccessDenied;
    }
    return core::ErrorCode::IoFailure;
}

[[nodiscard]] core::Error closed_error()
{
    return core::Error{
        core::ErrorCode::InvalidState,
        "windows_writer_closed: the destination writer is closed."};
}

}  // namespace

struct WindowsResourceWriter::Impl final {
    explicit Impl(QString path)
        : file(std::move(path))
    {
    }

    QFile file;
};

core::Result<core::ResourceReference> WindowsResourceWriter::make_write_reference(
    std::string_view absoluteLocalPathUtf8,
    std::string_view displayNameUtf8)
{
    try {
        auto canonical = canonical_local_path(absoluteLocalPathUtf8);
        if (!canonical) {
            return core::Result<core::ResourceReference>::failure(*canonical.error());
        }
        const QFileInfo destination{*canonical.value()};
        const auto parent = destination.dir();
        if (!parent.exists()) {
            return failure<core::ResourceReference>(
                core::ErrorCode::ResourceNotFound,
                "destination_parent_missing: the destination directory does not exist.");
        }
        if (!parent.isReadable()) {
            return failure<core::ResourceReference>(
                core::ErrorCode::AccessDenied,
                "destination_parent_denied: the destination directory is inaccessible.");
        }

        std::string displayName;
        if (displayNameUtf8.empty()) {
            displayName = destination.fileName().toUtf8().toStdString();
        } else {
            auto decodedName = decode_utf8(
                displayNameUtf8,
                "Destination display name must be valid UTF-8.");
            if (!decodedName) {
                return core::Result<core::ResourceReference>::failure(
                    *decodedName.error());
            }
            displayName.assign(displayNameUtf8);
        }
        return core::ResourceReference::create(
            std::string{provider_id()},
            canonical.value()->toUtf8().toStdString(),
            true,
            true,
            std::move(displayName));
    } catch (const std::bad_alloc&) {
        return failure<core::ResourceReference>(
            core::ErrorCode::IoFailure,
            "destination_reference_allocation_failed: unable to allocate writer state.");
    }
}

core::Result<std::unique_ptr<WindowsResourceWriter>>
WindowsResourceWriter::open_create_new(core::ResourceReference reference)
{
    if (reference.provider_id() != provider_id()) {
        return failure<std::unique_ptr<WindowsResourceWriter>>(
            core::ErrorCode::InvalidArgument,
            "destination_provider_invalid: provider is not Windows local-file.");
    }
    if (!reference.permissions().can_write()) {
        return failure<std::unique_ptr<WindowsResourceWriter>>(
            core::ErrorCode::AccessDenied,
            "destination_write_denied: reference has no write permission.");
    }
    try {
        auto canonical = canonical_local_path(reference.locator());
        if (!canonical) {
            return core::Result<std::unique_ptr<WindowsResourceWriter>>::failure(
                *canonical.error());
        }
        if (canonical.value()->toUtf8().toStdString() != reference.locator()) {
            return failure<std::unique_ptr<WindowsResourceWriter>>(
                core::ErrorCode::InvalidArgument,
                "destination_locator_noncanonical: local locator is not canonical.");
        }
        if (QFileInfo::exists(*canonical.value())) {
            return failure<std::unique_ptr<WindowsResourceWriter>>(
                core::ErrorCode::InvalidState,
                "destination_exists: create-new writer refuses an existing file.");
        }
        auto implementation = std::make_unique<Impl>(*canonical.value());
        if (!implementation->file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
            if (QFileInfo::exists(*canonical.value())) {
                return failure<std::unique_ptr<WindowsResourceWriter>>(
                    core::ErrorCode::InvalidState,
                    "destination_exists: create-new race detected.");
            }
            return failure<std::unique_ptr<WindowsResourceWriter>>(
                file_error_code(implementation->file.error()),
                "candidate_create_failed: unable to create the destination exclusively.");
        }
        return core::Result<std::unique_ptr<WindowsResourceWriter>>::success(
            std::unique_ptr<WindowsResourceWriter>{new WindowsResourceWriter{
                std::move(reference), std::move(implementation)}});
    } catch (const std::bad_alloc&) {
        return failure<std::unique_ptr<WindowsResourceWriter>>(
            core::ErrorCode::IoFailure,
            "candidate_create_failed: unable to allocate writer state.");
    }
}

WindowsResourceWriter::WindowsResourceWriter(
    core::ResourceReference reference,
    std::unique_ptr<Impl> implementation) noexcept
    : reference_(std::move(reference))
    , implementation_(std::move(implementation))
{
}

WindowsResourceWriter::~WindowsResourceWriter() noexcept
{
    if (implementation_ && implementation_->file.isOpen()) {
        implementation_->file.close();
    }
}

const core::ResourceReference& WindowsResourceWriter::reference() const noexcept
{
    return reference_;
}

core::ResourceCapabilities WindowsResourceWriter::capabilities() const noexcept
{
    return core::ResourceCapabilities::create(true, true, true, true);
}

core::Result<std::uint64_t> WindowsResourceWriter::position_bytes() const
{
    if (closed_) {
        return core::Result<std::uint64_t>::failure(closed_error());
    }
    const auto position = implementation_->file.pos();
    if (position < 0) {
        return failure<std::uint64_t>(
            core::ErrorCode::IoFailure,
            "writer_position_failed: unable to determine destination position.");
    }
    return core::Result<std::uint64_t>::success(static_cast<std::uint64_t>(position));
}

core::Result<std::size_t> WindowsResourceWriter::write(
    std::span<const std::byte> source)
{
    if (closed_) {
        return core::Result<std::size_t>::failure(closed_error());
    }
    if (source.empty()) {
        return core::Result<std::size_t>::success(0U);
    }
    const auto transfer = std::min(
        source.size(),
        static_cast<std::size_t>(std::numeric_limits<qint64>::max()));
    const auto written = implementation_->file.write(
        reinterpret_cast<const char*>(source.data()),
        static_cast<qint64>(transfer));
    if (written < 0) {
        return failure<std::size_t>(
            file_error_code(implementation_->file.error()),
            "writer_io_failed: unable to write destination bytes.");
    }
    return core::Result<std::size_t>::success(static_cast<std::size_t>(written));
}

core::Status WindowsResourceWriter::seek_bytes(std::uint64_t absoluteOffset)
{
    if (closed_) {
        return core::Status::failure(closed_error());
    }
    if (absoluteOffset > static_cast<std::uint64_t>(
            std::numeric_limits<qint64>::max())) {
        return status_failure(
            core::ErrorCode::OutOfRange,
            "writer_seek_out_of_range: destination offset is not representable.");
    }
    const auto prior = implementation_->file.pos();
    if (!implementation_->file.seek(static_cast<qint64>(absoluteOffset))) {
        if (prior >= 0) {
            static_cast<void>(implementation_->file.seek(prior));
        }
        return status_failure(
            file_error_code(implementation_->file.error()),
            "writer_seek_failed: unable to seek destination.");
    }
    return core::Status::success();
}

core::Status WindowsResourceWriter::resize_bytes(std::uint64_t sizeBytes)
{
    if (closed_) {
        return core::Status::failure(closed_error());
    }
    if (sizeBytes > static_cast<std::uint64_t>(
            std::numeric_limits<qint64>::max())) {
        return status_failure(
            core::ErrorCode::OutOfRange,
            "writer_resize_out_of_range: destination size is not representable.");
    }
    const auto priorPosition = implementation_->file.pos();
    const auto priorSize = implementation_->file.size();
    if (!implementation_->file.resize(static_cast<qint64>(sizeBytes))) {
        if (priorSize >= 0 && implementation_->file.size() != priorSize) {
            static_cast<void>(implementation_->file.resize(priorSize));
        }
        if (priorPosition >= 0) {
            static_cast<void>(implementation_->file.seek(priorPosition));
        }
        return status_failure(
            file_error_code(implementation_->file.error()),
            "writer_resize_failed: unable to resize destination.");
    }
    if (priorPosition >= 0
        && priorPosition <= static_cast<qint64>(sizeBytes)) {
        static_cast<void>(implementation_->file.seek(priorPosition));
    }
    return core::Status::success();
}

core::Status WindowsResourceWriter::flush()
{
    if (closed_) {
        return core::Status::failure(closed_error());
    }
    if (!implementation_->file.flush()) {
        return status_failure(
            file_error_code(implementation_->file.error()),
            "writer_flush_failed: unable to flush destination bytes.");
    }
    return core::Status::success();
}

core::Status WindowsResourceWriter::close()
{
    if (closed_) {
        return core::Status::success();
    }
    implementation_->file.close();
    closed_ = true;
    return core::Status::success();
}

}  // namespace rgsml::platform::windows
