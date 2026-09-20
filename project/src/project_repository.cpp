#include <rgsml/project/project_repository.hpp>

#include "internal/canonical_json.hpp"
#include "internal/minizip_resource_stream.hpp"
#include "internal/raw_zip_validator.hpp"

#include <nlohmann/json.hpp>

#include <rgsml/core/error.hpp>
#include <rgsml/core/sha256.hpp>

extern "C" {
#include <mz.h>
#include <mz_zip.h>
}

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rgsml::project {
namespace {

using Json = nlohmann::json;
using PayloadMap = std::map<std::string, std::vector<std::byte>>;
constexpr std::string_view kMime = "application/vnd.rgs.masterlab.project+zip";

[[nodiscard]] core::Error failure(core::ErrorCode code, std::string_view phase,
                                  std::string_view reason, std::string_view entry = {})
{
    return core::Error{code, "RGSML project package failure",
        {{"phase", std::string(phase)}, {"reason", std::string(reason)},
         {"entry", std::string(entry)}}};
}

[[nodiscard]] std::span<const std::byte> bytes(std::string_view text)
{
    return {reinterpret_cast<const std::byte*>(text.data()), text.size()};
}

[[nodiscard]] std::string text(const std::vector<std::byte>& content)
{
    return {reinterpret_cast<const char*>(content.data()), content.size()};
}

[[nodiscard]] std::vector<std::byte> as_bytes(std::string_view value)
{
    const auto span = bytes(value);
    return {span.begin(), span.end()};
}

[[nodiscard]] std::uint64_t json_uint(const Json& j)
{
    if (!j.is_number_integer() || j.is_number_float()) throw std::invalid_argument("integer");
    if (j.is_number_unsigned()) return j.get<std::uint64_t>();
    const auto signedValue = j.get<std::int64_t>();
    if (signedValue < 0) throw std::invalid_argument("negative");
    return static_cast<std::uint64_t>(signedValue);
}

[[nodiscard]] bool lowercase_sha(std::string_view value)
{
    return value.size() == 64U && std::all_of(value.begin(), value.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

[[nodiscard]] core::Status validate_manifest(
    const PayloadMap& payloads, std::vector<OptionalEntry>& optionalEntries)
{
    const auto found = payloads.find("manifest.json");
    if (found == payloads.end()) return core::Status::failure(
        failure(core::ErrorCode::ParseFailure, "manifest", "missing"));
    auto canonical = internal::canonicalize_json(text(found->second), 1024U * 1024U);
    if (!canonical) return core::Status::failure(*canonical.error());
    try {
        const auto manifest = Json::parse(*canonical.value());
        if (!manifest.is_object() || manifest.size() != 3U ||
            !manifest.contains("formatId") || !manifest.contains("containerVersion") ||
            !manifest.contains("entries") ||
            manifest.at("formatId") != "rgsml-project" ||
            json_uint(manifest.at("containerVersion")) != 1U ||
            !manifest.at("entries").is_array()) {
            throw std::invalid_argument("header");
        }
        const auto& entries = manifest.at("entries");
        if (entries.size() != payloads.size() - 1U) throw std::invalid_argument("entry_count");
        std::map<std::string, std::string> mediaTypes;
        for (const auto& item : entries) {
            if (!item.is_object() || item.size() != 5U ||
                !item.contains("path") || !item.contains("mediaType") ||
                !item.contains("uncompressedSize") || !item.contains("sha256") ||
                !item.contains("required")) throw std::invalid_argument("entry_schema");
            const auto path = item.at("path").get<std::string>();
            const auto mediaType = item.at("mediaType").get<std::string>();
            const auto digest = item.at("sha256").get<std::string>();
            const auto required = item.at("required").get<bool>();
            if (!internal::safe_package_path(path) || path == "manifest.json" ||
                mediaType.empty() || !lowercase_sha(digest) ||
                !mediaTypes.emplace(path, mediaType).second) {
                throw std::invalid_argument("entry_path_or_hash");
            }
            const auto payload = payloads.find(path);
            if (payload == payloads.end() ||
                json_uint(item.at("uncompressedSize")) != payload->second.size() ||
                core::sha256_hex(payload->second) != digest) {
                throw std::invalid_argument("entry_integrity");
            }
            const auto root = path == "mimetype" || path == "project.json";
            if (required != root || (path == "mimetype" && mediaType != "text/plain") ||
                (path == "project.json" && mediaType != "application/json")) {
                throw std::invalid_argument("entry_type");
            }
            if (!root) optionalEntries.push_back({path, mediaType, payload->second});
        }
        if (!mediaTypes.contains("mimetype") || !mediaTypes.contains("project.json"))
            throw std::invalid_argument("required_manifest_entries");
    } catch (const std::exception&) {
        return core::Status::failure(failure(core::ErrorCode::ParseFailure,
            "manifest", "schema_or_integrity"));
    }
    return core::Status::success();
}

[[nodiscard]] std::string build_manifest(const PayloadMap& payloads,
                                         const std::vector<OptionalEntry>& optionalEntries)
{
    Json entries = Json::array();
    for (const auto& [path, payload] : payloads) {
        if (path == "manifest.json") continue;
        auto mediaType = path == "mimetype" ? "text/plain" :
            path == "project.json" ? "application/json" : "application/octet-stream";
        for (const auto& entry : optionalEntries) {
            if (entry.path == path) { mediaType = entry.mediaType.c_str(); break; }
        }
        entries.push_back({{"path", path}, {"mediaType", mediaType},
                           {"uncompressedSize", payload.size()},
                           {"sha256", core::sha256_hex(payload)},
                           {"required", path == "mimetype" || path == "project.json"}});
    }
    Json manifest = {{"formatId", "rgsml-project"},
                     {"containerVersion", 1}, {"entries", std::move(entries)}};
    return manifest.dump(-1, ' ', false, Json::error_handler_t::strict);
}

[[nodiscard]] core::Result<PayloadMap> read_payloads(
    core::IResourceReader& reader, const std::vector<internal::RawEntry>& rawEntries)
{
    if (!reader.seek_bytes(0)) return core::Result<PayloadMap>::failure(
        failure(core::ErrorCode::IoFailure, "zip", "rewind"));
    auto stream = internal::ResourceStream::reading(reader);
    void* zip = mz_zip_create();
    if (!zip) return core::Result<PayloadMap>::failure(
        failure(core::ErrorCode::IoFailure, "zip", "create"));
    const auto close = [&zip]() {
        if (zip) { (void)mz_zip_close(zip); mz_zip_delete(&zip); }
    };
    if (mz_zip_open(zip, &stream.stream, MZ_OPEN_MODE_READ) != MZ_OK) {
        close();
        return core::Result<PayloadMap>::failure(
            failure(core::ErrorCode::ParseFailure, "zip", "open"));
    }
    std::uint64_t count = 0;
    if (mz_zip_get_number_entry(zip, &count) != MZ_OK || count != rawEntries.size() ||
        mz_zip_goto_first_entry(zip) != MZ_OK) {
        close();
        return core::Result<PayloadMap>::failure(
            failure(core::ErrorCode::ParseFailure, "zip", "entry_count"));
    }
    PayloadMap payloads;
    for (std::size_t i = 0; i < rawEntries.size(); ++i) {
        mz_zip_file* info = nullptr;
        if (mz_zip_entry_get_info(zip, &info) != MZ_OK || !info || !info->filename ||
            std::string(info->filename, info->filename_size) != rawEntries[i].path ||
            info->compression_method != MZ_COMPRESS_METHOD_STORE ||
            (info->flag & MZ_ZIP_FLAG_ENCRYPTED) != 0 || info->aes_version != 0 ||
            info->comment_size != 0 || info->disk_number != 0 ||
            info->compressed_size < 0 || info->uncompressed_size < 0 ||
            static_cast<std::uint64_t>(info->uncompressed_size) != rawEntries[i].uncompressedSize ||
            mz_zip_entry_is_dir(zip) == MZ_OK || mz_zip_entry_is_symlink(zip) == MZ_OK ||
            mz_zip_entry_read_open(zip, 0, nullptr) != MZ_OK) {
            close();
            return core::Result<PayloadMap>::failure(failure(
                core::ErrorCode::ParseFailure, "zip", "entry_metadata", rawEntries[i].path));
        }
        std::vector<std::byte> payload(static_cast<std::size_t>(rawEntries[i].uncompressedSize));
        std::size_t position = 0;
        while (position < payload.size()) {
            const auto countToRead = static_cast<std::int32_t>(
                std::min<std::size_t>(65536U, payload.size() - position));
            const auto got = mz_zip_entry_read(zip, payload.data() + position, countToRead);
            if (got <= 0) {
                close();
                return core::Result<PayloadMap>::failure(failure(
                    core::ErrorCode::ParseFailure, "zip", "entry_truncated", rawEntries[i].path));
            }
            position += static_cast<std::size_t>(got);
        }
        if (mz_zip_entry_read_close(zip, nullptr, nullptr, nullptr) != MZ_OK) {
            close();
            return core::Result<PayloadMap>::failure(failure(
                core::ErrorCode::ParseFailure, "zip", "entry_close", rawEntries[i].path));
        }
        payloads.emplace(rawEntries[i].path, std::move(payload));
        if (i + 1U < rawEntries.size() && mz_zip_goto_next_entry(zip) != MZ_OK) {
            close();
            return core::Result<PayloadMap>::failure(failure(
                core::ErrorCode::ParseFailure, "zip", "entry_next"));
        }
    }
    const auto closeStatus = mz_zip_close(zip);
    mz_zip_delete(&zip);
    if (closeStatus != MZ_OK) return core::Result<PayloadMap>::failure(
        failure(core::ErrorCode::ParseFailure, "zip", "close"));
    return core::Result<PayloadMap>::success(std::move(payloads));
}

[[nodiscard]] core::Status write_entry(void* zip, std::string_view path,
                                       std::span<const std::byte> payload)
{
    mz_zip_file info{};
    const std::string name(path);
    info.filename = name.c_str();
    info.flag = MZ_ZIP_FLAG_UTF8;
    info.compression_method = MZ_COMPRESS_METHOD_STORE;
    info.uncompressed_size = static_cast<std::int64_t>(payload.size());
    info.compressed_size = static_cast<std::int64_t>(payload.size());
    info.zip64 = MZ_ZIP64_AUTO;
    if (mz_zip_entry_write_open(zip, &info, 0, 0, nullptr) != MZ_OK) {
        return core::Status::failure(failure(core::ErrorCode::IoFailure,
            "zip_write", "entry_open", path));
    }
    for (std::size_t position = 0; position < payload.size();) {
        const auto chunk = static_cast<std::int32_t>(
            std::min<std::size_t>(65536U, payload.size() - position));
        const auto count = mz_zip_entry_write(zip, payload.data() + position, chunk);
        if (count != chunk) return core::Status::failure(failure(
            core::ErrorCode::IoFailure, "zip_write", "entry_write", path));
        position += static_cast<std::size_t>(count);
    }
    if (mz_zip_entry_write_close(zip, 0, -1, -1) != MZ_OK) {
        return core::Status::failure(failure(core::ErrorCode::IoFailure,
            "zip_write", "entry_close", path));
    }
    return core::Status::success();
}

}  // namespace

core::Result<ProjectSnapshot> ProjectRepository::open(
    std::unique_ptr<core::IResourceReader> reader)
{
    if (!reader) return core::Result<ProjectSnapshot>::failure(
        failure(core::ErrorCode::InvalidArgument, "open", "null_reader"));
    auto raw = internal::validate_raw_zip(*reader);
    if (!raw) return core::Result<ProjectSnapshot>::failure(*raw.error());
    auto payloads = read_payloads(*reader, *raw.value());
    if (!payloads) return core::Result<ProjectSnapshot>::failure(*payloads.error());
    const auto& map = *payloads.value();
    const auto mime = map.find("mimetype");
    const auto project = map.find("project.json");
    if (mime == map.end() || project == map.end() || text(mime->second) != kMime) {
        return core::Result<ProjectSnapshot>::failure(failure(
            core::ErrorCode::ParseFailure, "package", "mimetype_or_project"));
    }
    std::vector<OptionalEntry> optionalEntries;
    auto manifest = validate_manifest(map, optionalEntries);
    if (!manifest) return core::Result<ProjectSnapshot>::failure(*manifest.error());
    auto document = internal::parse_project_json(text(project->second));
    if (!document) return core::Result<ProjectSnapshot>::failure(*document.error());
    auto snapshot = ProjectSnapshot::create(std::move(*document.value()),
                                            std::move(optionalEntries));
    if (!snapshot) return core::Result<ProjectSnapshot>::failure(failure(
        core::ErrorCode::ParseFailure, "project", "semantic_validation"));
    auto closed = reader->close();
    if (!closed) return core::Result<ProjectSnapshot>::failure(*closed.error());
    return snapshot;
}

core::Status ProjectRepository::save_create_new(
    const ProjectSnapshot& snapshot,
    std::unique_ptr<core::IResourceWriter> writer,
    ReopenReader reopen)
{
    if (!writer || !reopen) return core::Status::failure(
        failure(core::ErrorCode::InvalidArgument, "save", "endpoint_or_reopen"));
    if (!writer->capabilities().supports(core::ResourceCapability::CanSeek) ||
        !writer->capabilities().supports(core::ResourceCapability::CanResize) ||
        !writer->capabilities().supports(core::ResourceCapability::CanFlush)) {
        return core::Status::failure(failure(core::ErrorCode::UnsupportedOperation,
            "save", "writer_capability"));
    }
    auto project = internal::write_project_json(snapshot.document());
    if (!project) return core::Status::failure(*project.error());
    PayloadMap payloads;
    payloads.emplace("mimetype", as_bytes(kMime));
    payloads.emplace("project.json", as_bytes(*project.value()));
    for (const auto& entry : snapshot.optional_entries()) {
        if (!internal::safe_package_path(entry.path)) return core::Status::failure(
            failure(core::ErrorCode::InvalidArgument, "save", "optional_path", entry.path));
        payloads.emplace(entry.path, entry.bytes);
    }
    if (payloads.size() + 1U > 1024U) return core::Status::failure(
        failure(core::ErrorCode::OutOfRange, "save", "entry_count"));
    const auto manifest = build_manifest(payloads, snapshot.optional_entries());
    if (manifest.size() > 1024U * 1024U) return core::Status::failure(
        failure(core::ErrorCode::OutOfRange, "save", "manifest_size"));
    payloads.emplace("manifest.json", as_bytes(manifest));
    auto stream = internal::ResourceStream::writing(*writer);
    void* zip = mz_zip_create();
    if (!zip) return core::Status::failure(
        failure(core::ErrorCode::IoFailure, "save", "zip_create"));
    if (mz_zip_open(zip, &stream.stream, MZ_OPEN_MODE_WRITE | MZ_OPEN_MODE_CREATE) != MZ_OK) {
        mz_zip_delete(&zip);
        return core::Status::failure(failure(core::ErrorCode::IoFailure, "save", "zip_open"));
    }
    const auto emit = [&](std::string_view path) {
        return write_entry(zip, path, payloads.at(std::string(path)));
    };
    auto status = emit("mimetype");
    if (status) status = emit("manifest.json");
    if (status) status = emit("project.json");
    for (const auto& [path, content] : payloads) {
        (void)content;
        if (status && path != "mimetype" && path != "manifest.json" &&
            path != "project.json") status = emit(path);
    }
    const auto zipClosed = mz_zip_close(zip);
    mz_zip_delete(&zip);
    if (!status) return status;
    if (zipClosed != MZ_OK) return core::Status::failure(
        failure(core::ErrorCode::IoFailure, "save", "zip_close"));
    auto flushed = writer->flush();
    if (!flushed) return flushed;
    auto closed = writer->close();
    if (!closed) return closed;
    writer.reset();
    auto reopened = reopen();
    if (!reopened) return core::Status::failure(*reopened.error());
    auto checked = open(std::move(*reopened.value()));
    if (!checked) return core::Status::failure(*checked.error());
    const auto original = internal::write_project_json(snapshot.document());
    const auto actual = internal::write_project_json(checked.value()->document());
    if (!original || !actual || *original.value() != *actual.value() ||
        snapshot.optional_entries().size() != checked.value()->optional_entries().size()) {
        return core::Status::failure(failure(core::ErrorCode::InvalidState,
            "save", "reopen_semantic_mismatch"));
    }
    for (const auto& entry : snapshot.optional_entries()) {
        const auto& actualEntries = checked.value()->optional_entries();
        const auto found = std::find_if(actualEntries.begin(), actualEntries.end(),
            [&](const OptionalEntry& candidate) { return candidate.path == entry.path; });
        if (found == actualEntries.end() || found->mediaType != entry.mediaType ||
            found->bytes != entry.bytes) {
            return core::Status::failure(failure(core::ErrorCode::InvalidState,
                "save", "reopen_optional_mismatch", entry.path));
        }
    }
    return core::Status::success();
}

}  // namespace rgsml::project
