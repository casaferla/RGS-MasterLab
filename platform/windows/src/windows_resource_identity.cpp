#include <rgsml/platform/windows/windows_resource_identity.hpp>

#include <rgsml/platform/windows/windows_resource_reader.hpp>

#include <QDir>
#include <QString>
#include <QStringDecoder>

#ifdef _WIN32
#include <Windows.h>
#endif

#include <cstdint>
#include <string_view>

namespace rgsml::platform::windows {
namespace {

struct FileIdentity final {
    std::uint64_t volume;
    std::uint64_t index;

    [[nodiscard]] bool operator==(const FileIdentity&) const = default;
};

[[nodiscard]] core::Result<FileIdentity> identity_of(
    const core::ResourceReference& reference)
{
    if (reference.provider_id() != WindowsResourceReader::provider_id()
        || !reference.permissions().can_read()
        || reference.permissions().can_write()) {
        return core::Result<FileIdentity>::failure(core::Error{
            core::ErrorCode::InvalidArgument,
            "Gold identity requires a read-only Windows local-file reference."});
    }

    const auto& locator = reference.locator();
    QStringDecoder decoder{QStringDecoder::Utf8};
    const QString decoded = decoder.decode(QByteArrayView{
        locator.data(), static_cast<qsizetype>(locator.size())});
    if (decoder.hasError() || decoded.contains(QChar::Null)
        || decoded.size() < 3
        || !decoded.at(0).isLetter()
        || decoded.at(1) != QLatin1Char(':')
        || decoded.at(2) != QLatin1Char('/')
        || decoded != QDir::cleanPath(decoded)) {
        return core::Result<FileIdentity>::failure(core::Error{
            core::ErrorCode::InvalidArgument,
            "Gold identity requires a canonical absolute local drive path."});
    }

#ifdef _WIN32
    const auto native = QDir::toNativeSeparators(decoded);
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
        const auto code = error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND
            ? core::ErrorCode::ResourceNotFound
            : (error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION
                ? core::ErrorCode::AccessDenied
                : core::ErrorCode::InvalidState);
        return core::Result<FileIdentity>::failure(core::Error{
            code, "Gold identity could not be established; selection rejected."});
    }

    BY_HANDLE_FILE_INFORMATION info{};
    const BOOL queried = ::GetFileInformationByHandle(handle, &info);
    ::CloseHandle(handle);
    if (queried == FALSE) {
        return core::Result<FileIdentity>::failure(core::Error{
            core::ErrorCode::InvalidState,
            "Gold identity query failed closed."});
    }
    return core::Result<FileIdentity>::success(FileIdentity{
        static_cast<std::uint64_t>(info.dwVolumeSerialNumber),
        (static_cast<std::uint64_t>(info.nFileIndexHigh) << 32U)
            | static_cast<std::uint64_t>(info.nFileIndexLow)});
#else
    QFileInfo info{decoded};
    if (!info.exists()) {
        return core::Result<FileIdentity>::failure(core::Error{
            core::ErrorCode::ResourceNotFound,
            "Gold identity could not be established; selection rejected."});
    }
    return core::Result<FileIdentity>::success(FileIdentity{
        1U, static_cast<std::uint64_t>(info.size())});
#endif
}

}  // namespace

core::Result<bool> same_underlying_local_file(
    const core::ResourceReference& first,
    const core::ResourceReference& second)
{
    auto firstIdentity = identity_of(first);
    if (!firstIdentity) {
        return core::Result<bool>::failure(*firstIdentity.error());
    }
    auto secondIdentity = identity_of(second);
    if (!secondIdentity) {
        return core::Result<bool>::failure(*secondIdentity.error());
    }
    return core::Result<bool>::success(
        *firstIdentity.value() == *secondIdentity.value());
}

}  // namespace rgsml::platform::windows
