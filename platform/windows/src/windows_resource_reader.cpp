#include <rgsml/platform/windows/windows_resource_reader.hpp>

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
[[nodiscard]] core::Result<T> failure(
    core::ErrorCode code,
    std::string message)
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
        value.data(),
        static_cast<qsizetype>(value.size()),
    });
    if (decoder.hasError() || decoded.contains(QChar::Null)) {
        return failure<QString>(core::ErrorCode::InvalidArgument, std::move(message));
    }
    return core::Result<QString>::success(decoded);
}

[[nodiscard]] bool is_ascii_letter(QChar value) noexcept
{
    const auto unicode = value.unicode();
    return (unicode >= static_cast<ushort>('A')
            && unicode <= static_cast<ushort>('Z'))
        || (unicode >= static_cast<ushort>('a')
            && unicode <= static_cast<ushort>('z'));
}

[[nodiscard]] core::Result<QString> canonical_local_path(
    std::string_view pathUtf8)
{
    if (pathUtf8.empty()) {
        return failure<QString>(
            core::ErrorCode::InvalidArgument,
            "Local Source path must not be empty.");
    }

    auto decoded = decode_utf8(pathUtf8, "Local Source path must be valid UTF-8.");
    if (!decoded) {
        return decoded;
    }

    auto normalized = QDir::fromNativeSeparators(*decoded.value());
    if (normalized.startsWith(QStringLiteral("//"))
        || normalized.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive)
        || normalized.contains(QStringLiteral("://"))) {
        return failure<QString>(
            core::ErrorCode::InvalidArgument,
            "Only an absolute local drive path is supported.");
    }
    if (normalized.size() < 3 || !is_ascii_letter(normalized.at(0))
        || normalized.at(1) != QLatin1Char(':')
        || normalized.at(2) != QLatin1Char('/')) {
        return failure<QString>(
            core::ErrorCode::InvalidArgument,
            "Only an absolute local drive path is supported.");
    }

    normalized = QDir::cleanPath(normalized);
    normalized[0] = normalized.at(0).toUpper();
    return core::Result<QString>::success(normalized);
}

[[nodiscard]] core::ErrorCode file_error_code(QFileDevice::FileError error) noexcept
{
    switch (error) {
    case QFileDevice::PermissionsError:
        return core::ErrorCode::AccessDenied;
    case QFileDevice::NoError:
        break;
    default:
        return core::ErrorCode::IoFailure;
    }
    return core::ErrorCode::IoFailure;
}

[[nodiscard]] core::Error closed_error()
{
    return core::Error{
        core::ErrorCode::InvalidState,
        "The Source reader is closed.",
    };
}

}  // namespace

struct WindowsResourceReader::Impl final {
    explicit Impl(QString path)
        : file(std::move(path))
    {
    }

    QFile file;
};

core::Result<core::ResourceReference> WindowsResourceReader::make_read_reference(
    std::string_view absoluteLocalPathUtf8,
    std::string_view displayNameUtf8)
{
    try {
        auto canonical = canonical_local_path(absoluteLocalPathUtf8);
        if (!canonical) {
            return core::Result<core::ResourceReference>::failure(*canonical.error());
        }

        const QFileInfo info{*canonical.value()};
        if (!info.exists()) {
            return failure<core::ResourceReference>(
                core::ErrorCode::ResourceNotFound,
                "The selected Source file does not exist.");
        }
        if (!info.isFile()) {
            return failure<core::ResourceReference>(
                core::ErrorCode::InvalidArgument,
                "The selected Source is not a regular file.");
        }
        if (!info.isReadable()) {
            return failure<core::ResourceReference>(
                core::ErrorCode::AccessDenied,
                "The selected Source cannot be read.");
        }

        std::string displayName;
        if (displayNameUtf8.empty()) {
            displayName = info.fileName().toUtf8().toStdString();
        } else {
            auto decodedName = decode_utf8(
                displayNameUtf8,
                "Source display name must be valid UTF-8.");
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
            false,
            std::move(displayName));
    } catch (const std::bad_alloc&) {
        return failure<core::ResourceReference>(
            core::ErrorCode::IoFailure,
            "Unable to allocate local Source state.");
    }
}

core::Result<std::unique_ptr<WindowsResourceReader>>
WindowsResourceReader::open_read_only(core::ResourceReference reference)
{
    if (reference.provider_id() != provider_id()) {
        return failure<std::unique_ptr<WindowsResourceReader>>(
            core::ErrorCode::InvalidArgument,
            "The resource provider is not the Windows local-file provider.");
    }
    if (!reference.permissions().can_read()) {
        return failure<std::unique_ptr<WindowsResourceReader>>(
            core::ErrorCode::AccessDenied,
            "The Source reference has no read permission.");
    }
    if (reference.permissions().can_write()) {
        return failure<std::unique_ptr<WindowsResourceReader>>(
            core::ErrorCode::InvalidArgument,
            "A Source reference must be read-only.");
    }

    try {
        auto canonical = canonical_local_path(reference.locator());
        if (!canonical) {
            return core::Result<std::unique_ptr<WindowsResourceReader>>::failure(
                *canonical.error());
        }
        if (canonical.value()->toUtf8().toStdString() != reference.locator()) {
            return failure<std::unique_ptr<WindowsResourceReader>>(
                core::ErrorCode::InvalidArgument,
                "The local Source locator is not canonical.");
        }

        const QFileInfo info{*canonical.value()};
        if (!info.exists()) {
            return failure<std::unique_ptr<WindowsResourceReader>>(
                core::ErrorCode::ResourceNotFound,
                "The selected Source file does not exist.");
        }
        if (!info.isFile()) {
            return failure<std::unique_ptr<WindowsResourceReader>>(
                core::ErrorCode::InvalidArgument,
                "The selected Source is not a regular file.");
        }

        auto implementation = std::make_unique<Impl>(*canonical.value());
        if (!implementation->file.open(QIODevice::ReadOnly)) {
            return failure<std::unique_ptr<WindowsResourceReader>>(
                file_error_code(implementation->file.error()),
                "The selected Source could not be opened read-only.");
        }

        return core::Result<std::unique_ptr<WindowsResourceReader>>::success(
            std::unique_ptr<WindowsResourceReader>{new WindowsResourceReader{
                std::move(reference),
                std::move(implementation),
            }});
    } catch (const std::bad_alloc&) {
        return failure<std::unique_ptr<WindowsResourceReader>>(
            core::ErrorCode::IoFailure,
            "Unable to allocate local Source reader state.");
    }
}

WindowsResourceReader::WindowsResourceReader(
    core::ResourceReference reference,
    std::unique_ptr<Impl> implementation) noexcept
    : reference_(std::move(reference))
    , implementation_(std::move(implementation))
{
}

WindowsResourceReader::~WindowsResourceReader() noexcept
{
    if (implementation_ && implementation_->file.isOpen()) {
        implementation_->file.close();
    }
}

const core::ResourceReference& WindowsResourceReader::reference() const noexcept
{
    return reference_;
}

core::ResourceCapabilities WindowsResourceReader::capabilities() const noexcept
{
    return core::ResourceCapabilities::create(true, true, false, false);
}

core::Result<std::uint64_t> WindowsResourceReader::size_bytes() const
{
    if (closed_) {
        return core::Result<std::uint64_t>::failure(closed_error());
    }
    const auto size = implementation_->file.size();
    if (size < 0) {
        return failure<std::uint64_t>(
            core::ErrorCode::IoFailure,
            "Unable to determine Source size.");
    }
    return core::Result<std::uint64_t>::success(static_cast<std::uint64_t>(size));
}

core::Result<std::uint64_t> WindowsResourceReader::position_bytes() const
{
    if (closed_) {
        return core::Result<std::uint64_t>::failure(closed_error());
    }
    const auto position = implementation_->file.pos();
    if (position < 0) {
        return failure<std::uint64_t>(
            core::ErrorCode::IoFailure,
            "Unable to determine Source position.");
    }
    return core::Result<std::uint64_t>::success(
        static_cast<std::uint64_t>(position));
}

core::Result<std::size_t> WindowsResourceReader::read(
    std::span<std::byte> destination)
{
    if (closed_) {
        return core::Result<std::size_t>::failure(closed_error());
    }
    if (destination.empty()) {
        return core::Result<std::size_t>::success(0U);
    }

    const auto maximumTransfer = static_cast<std::size_t>(
        std::numeric_limits<qint64>::max());
    const auto transferSize = std::min(destination.size(), maximumTransfer);
    const auto transferred = implementation_->file.read(
        reinterpret_cast<char*>(destination.data()),
        static_cast<qint64>(transferSize));
    if (transferred < 0) {
        return failure<std::size_t>(
            file_error_code(implementation_->file.error()),
            "Unable to read Source data.");
    }
    return core::Result<std::size_t>::success(
        static_cast<std::size_t>(transferred));
}

core::Status WindowsResourceReader::seek_bytes(std::uint64_t absoluteOffset)
{
    if (closed_) {
        return core::Status::failure(closed_error());
    }
    if (absoluteOffset > static_cast<std::uint64_t>(
            std::numeric_limits<qint64>::max())) {
        return status_failure(
            core::ErrorCode::OutOfRange,
            "Source seek offset is out of range.");
    }

    const auto size = implementation_->file.size();
    if (size < 0) {
        return status_failure(
            core::ErrorCode::IoFailure,
            "Unable to determine Source size before seeking.");
    }
    if (absoluteOffset > static_cast<std::uint64_t>(size)) {
        return status_failure(
            core::ErrorCode::OutOfRange,
            "Source seek offset is out of range.");
    }

    const auto priorPosition = implementation_->file.pos();
    if (!implementation_->file.seek(static_cast<qint64>(absoluteOffset))) {
        if (priorPosition >= 0) {
            static_cast<void>(implementation_->file.seek(priorPosition));
        }
        return status_failure(
            file_error_code(implementation_->file.error()),
            "Unable to seek within Source data.");
    }
    return core::Status::success();
}

core::Status WindowsResourceReader::close()
{
    if (closed_) {
        return core::Status::success();
    }
    implementation_->file.close();
    closed_ = true;
    return core::Status::success();
}

}  // namespace rgsml::platform::windows
