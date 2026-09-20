#include "raw_zip_validator.hpp"

#include <rgsml/core/error.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <span>
#include <string_view>
#include <utility>

namespace rgsml::project::internal {
namespace {

constexpr std::uint64_t kRawMax = 320ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t kAggregateMax = 256ULL * 1024ULL * 1024ULL;

[[nodiscard]] core::Result<std::vector<RawEntry>> reject(
    core::ErrorCode code, std::string_view reason)
{
    return core::Result<std::vector<RawEntry>>::failure(core::Error{
        code, "Invalid RGSML ZIP container", {{"phase", "zip_raw"},
            {"reason", std::string(reason)}}});
}

[[nodiscard]] std::uint16_t le16(std::span<const std::byte> b, std::size_t p)
{
    return static_cast<std::uint16_t>(std::to_integer<unsigned>(b[p]) |
        (std::to_integer<unsigned>(b[p + 1]) << 8U));
}
[[nodiscard]] std::uint32_t le32(std::span<const std::byte> b, std::size_t p)
{
    return static_cast<std::uint32_t>(le16(b, p) | (std::uint32_t{le16(b, p + 2)} << 16U));
}
[[nodiscard]] std::uint64_t le64(std::span<const std::byte> b, std::size_t p)
{
    return std::uint64_t{le32(b, p)} | (std::uint64_t{le32(b, p + 4)} << 32U);
}

[[nodiscard]] bool read_at(core::IResourceReader& reader, std::uint64_t offset,
                           std::span<std::byte> dst)
{
    if (!reader.seek_bytes(offset)) return false;
    std::size_t done = 0;
    while (done < dst.size()) {
        auto part = reader.read(dst.subspan(done));
        if (!part || *part.value() == 0) return false;
        done += *part.value();
    }
    return true;
}

[[nodiscard]] bool utf8(std::string_view s)
{
    for (std::size_t i = 0; i < s.size();) {
        const auto a = static_cast<unsigned char>(s[i]);
        if (a < 0x80U) { ++i; continue; }
        std::size_t width = 0;
        std::uint32_t cp = 0;
        if (a >= 0xC2U && a <= 0xDFU) { width = 2; cp = a & 0x1fU; }
        else if (a >= 0xE0U && a <= 0xEFU) { width = 3; cp = a & 0x0fU; }
        else if (a >= 0xF0U && a <= 0xF4U) { width = 4; cp = a & 0x07U; }
        else return false;
        if (width > s.size() - i) return false;
        for (std::size_t k = 1; k < width; ++k) {
            const auto b = static_cast<unsigned char>(s[i + k]);
            if ((b & 0xC0U) != 0x80U) return false;
            cp = (cp << 6U) | (b & 0x3fU);
        }
        if ((width == 2 && cp < 0x80U) || (width == 3 && cp < 0x800U) ||
            (width == 4 && cp < 0x10000U) || cp > 0x10ffffU ||
            (cp >= 0xD800U && cp <= 0xDFFFU)) return false;
        i += width;
    }
    return true;
}

[[nodiscard]] bool parse_extra(std::span<const std::byte> extra,
                               std::uint32_t size32, std::uint32_t usize32,
                               std::uint32_t offset32, std::uint16_t disk16,
                               std::uint64_t& size, std::uint64_t& usize,
                               std::uint64_t& offset, std::uint32_t& disk)
{
    size = size32; usize = usize32; offset = offset32; disk = disk16;
    bool seenZip64 = false;
    for (std::size_t p = 0; p < extra.size();) {
        if (extra.size() - p < 4U) return false;
        const auto kind = le16(extra, p);
        const auto length = le16(extra, p + 2);
        p += 4;
        if (length > extra.size() - p) return false;
        if (kind == 0x9901U) return false; // AES
        if (kind == 0x0001U) {
            if (seenZip64) return false;
            seenZip64 = true;
            auto q = p;
            const auto end = p + length;
            if (usize32 == 0xffffffffU) {
                if (end - q < 8U) return false;
                usize = le64(extra, q); q += 8;
            }
            if (size32 == 0xffffffffU) {
                if (end - q < 8U) return false;
                size = le64(extra, q); q += 8;
            }
            if (offset32 == 0xffffffffU) {
                if (end - q < 8U) return false;
                offset = le64(extra, q); q += 8;
            }
            if (disk16 == 0xffffU) {
                if (end - q < 4U) return false;
                disk = le32(extra, q);
            }
        }
        p += length;
    }
    return (size32 != 0xffffffffU && usize32 != 0xffffffffU &&
            offset32 != 0xffffffffU && disk16 != 0xffffU) || seenZip64;
}

}  // namespace

bool safe_package_path(std::string_view path)
{
    if (path.empty() || path.size() > 512 || path.front() == '/' ||
        path.back() == '/' || !utf8(path)) return false;
    if (path != "mimetype" && path != "manifest.json" && path != "project.json" &&
        !path.starts_with("artifacts/") && !path.starts_with("definitions/") &&
        !path.starts_with("thumbnails/")) return false;
    std::size_t begin = 0;
    while (begin < path.size()) {
        const auto end = path.find('/', begin);
        const auto segment = path.substr(begin, end == std::string_view::npos ?
            path.size() - begin : end - begin);
        if (segment.empty() || segment == "." || segment == "..") return false;
        if (end == std::string_view::npos) break;
        begin = end + 1;
    }
    return std::none_of(path.begin(), path.end(), [](unsigned char c) {
        return c == '\\' || c == ':' || c < 0x20U || c == 0x7fU;
    });
}

core::Result<std::vector<RawEntry>> validate_raw_zip(core::IResourceReader& reader)
{
    if (!reader.capabilities().supports(core::ResourceCapability::CanSeek) ||
        !reader.capabilities().supports(core::ResourceCapability::HasKnownSize)) {
        return reject(core::ErrorCode::UnsupportedOperation, "seek_and_size_required");
    }
    auto sizeResult = reader.size_bytes();
    if (!sizeResult) return core::Result<std::vector<RawEntry>>::failure(*sizeResult.error());
    const auto rawSize = *sizeResult.value();
    if (rawSize > kRawMax) return reject(core::ErrorCode::OutOfRange, "raw_container_size");
    if (rawSize < 22U) return reject(core::ErrorCode::ParseFailure, "missing_eocd");
    std::array<std::byte, 22> eocd{};
    if (!read_at(reader, rawSize - eocd.size(), eocd))
        return reject(core::ErrorCode::IoFailure, "eocd_read");
    if (le32(eocd, 0) != 0x06054b50U || le16(eocd, 20) != 0 ||
        le16(eocd, 4) != 0 || le16(eocd, 6) != 0) {
        return reject(core::ErrorCode::ParseFailure, "eocd_or_multidisk");
    }
    std::uint64_t count = le16(eocd, 10);
    std::uint64_t cdSize = le32(eocd, 12);
    std::uint64_t cdOffset = le32(eocd, 16);
    if (le16(eocd, 8) != count) return reject(core::ErrorCode::ParseFailure, "split_entries");
    std::uint64_t cdEnd = rawSize - 22U;
    if (count == 0xffffU || cdSize == 0xffffffffU || cdOffset == 0xffffffffU) {
        if (rawSize < 98U) return reject(core::ErrorCode::ParseFailure, "zip64_truncated");
        std::array<std::byte, 20> locator{};
        if (!read_at(reader, rawSize - 42U, locator))
            return reject(core::ErrorCode::IoFailure, "zip64_locator_read");
        if (le32(locator, 0) != 0x07064b50U || le32(locator, 4) != 0 ||
            le32(locator, 16) != 1U) return reject(core::ErrorCode::ParseFailure, "zip64_multidisk");
        const auto zip64Offset = le64(locator, 8);
        std::array<std::byte, 56> zip64{};
        if (zip64Offset > rawSize - 98U || !read_at(reader, zip64Offset, zip64))
            return reject(core::ErrorCode::ParseFailure, "zip64_record");
        if (le32(zip64, 0) != 0x06064b50U || le64(zip64, 4) != 44U ||
            le32(zip64, 16) != 0 || le32(zip64, 20) != 0 ||
            le64(zip64, 24) != le64(zip64, 32) || zip64Offset + 56U != rawSize - 42U)
            return reject(core::ErrorCode::ParseFailure, "zip64_fields");
        count = le64(zip64, 32);
        cdSize = le64(zip64, 40);
        cdOffset = le64(zip64, 48);
        cdEnd = zip64Offset;
    }
    if (count > 1024U) return reject(core::ErrorCode::OutOfRange, "entry_count");
    if (cdOffset > cdEnd || cdSize != cdEnd - cdOffset)
        return reject(core::ErrorCode::ParseFailure, "central_directory_range");
    std::vector<RawEntry> entries;
    entries.reserve(static_cast<std::size_t>(count));
    std::set<std::string> paths;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> localRanges;
    std::uint64_t aggregate = 0;
    std::uint64_t cursor = cdOffset;
    for (std::uint64_t i = 0; i < count; ++i) {
        if (cursor > cdEnd || cdEnd - cursor < 46U)
            return reject(core::ErrorCode::ParseFailure, "central_truncated");
        std::array<std::byte, 46> h{};
        if (!read_at(reader, cursor, h)) return reject(core::ErrorCode::IoFailure, "central_read");
        if (le32(h, 0) != 0x02014b50U) return reject(core::ErrorCode::ParseFailure, "central_signature");
        const auto flags = le16(h, 8);
        const auto method = le16(h, 10);
        const auto nameLen = le16(h, 28);
        const auto extraLen = le16(h, 30);
        const auto commentLen = le16(h, 32);
        if (method != 0 || (flags & ~std::uint16_t{0x0808U}) != 0 ||
            commentLen != 0 || nameLen == 0 || nameLen > 512 || extraLen > 4096 ||
            (le32(h, 38) & 0x10U) != 0 ||
            ((le32(h, 38) >> 16U) & 0xf000U) == 0x4000U ||
            ((le32(h, 38) >> 16U) & 0xf000U) == 0xa000U) {
            return reject(core::ErrorCode::ParseFailure, "entry_metadata");
        }
        const auto recordLen = 46ULL + nameLen + extraLen;
        if (recordLen > cdEnd - cursor) return reject(core::ErrorCode::ParseFailure, "central_lengths");
        std::vector<std::byte> variable(nameLen + extraLen);
        if (!read_at(reader, cursor + 46U, variable))
            return reject(core::ErrorCode::IoFailure, "central_variable_read");
        std::string path;
        path.reserve(nameLen);
        for (std::size_t n = 0; n < nameLen; ++n)
            path.push_back(static_cast<char>(std::to_integer<unsigned char>(variable[n])));
        if (!safe_package_path(path) || !paths.insert(path).second)
            return reject(core::ErrorCode::ParseFailure, "entry_path");
        std::uint64_t csize = 0, usize = 0, localOffset = 0;
        std::uint32_t disk = 0;
        if (!parse_extra(std::span{variable}.subspan(nameLen), le32(h, 20), le32(h, 24),
                         le32(h, 42), le16(h, 34), csize, usize, localOffset, disk) ||
            disk != 0 || csize != usize) {
            return reject(core::ErrorCode::ParseFailure, "entry_size_or_disk");
        }
        const auto maxEntry = path == "mimetype" ? 64ULL :
            path == "manifest.json" ? 1ULL * 1024ULL * 1024ULL :
            path == "project.json" ? 16ULL * 1024ULL * 1024ULL :
            64ULL * 1024ULL * 1024ULL;
        if (usize > maxEntry || usize > kAggregateMax - aggregate)
            return reject(core::ErrorCode::OutOfRange, "entry_or_aggregate_size");
        aggregate += usize;
        if (localOffset > cdOffset || cdOffset - localOffset < 30U)
            return reject(core::ErrorCode::ParseFailure, "local_range");
        std::array<std::byte, 30> local{};
        if (!read_at(reader, localOffset, local))
            return reject(core::ErrorCode::IoFailure, "local_read");
        const auto localNameLen = le16(local, 26);
        const auto localExtraLen = le16(local, 28);
        if (le32(local, 0) != 0x04034b50U || le16(local, 6) != flags ||
            le16(local, 8) != method || localNameLen != nameLen ||
            localExtraLen > 4096 ||
            30ULL + localNameLen + localExtraLen > cdOffset - localOffset ||
            csize > cdOffset - localOffset - 30ULL - localNameLen - localExtraLen) {
            return reject(core::ErrorCode::ParseFailure, "local_fixed_fields");
        }
        std::vector<std::byte> localVariable(localNameLen + localExtraLen);
        if (!read_at(reader, localOffset + 30U, localVariable) ||
            !std::equal(variable.begin(), variable.begin() + nameLen,
                        localVariable.begin())) {
            return reject(core::ErrorCode::ParseFailure, "local_name");
        }
        std::uint64_t localSize = 0, localUsize = 0, ignoredOffset = 0;
        std::uint32_t ignoredDisk = 0;
        if (!parse_extra(std::span{localVariable}.subspan(localNameLen),
                         le32(local, 18), le32(local, 22), 0, 0,
                         localSize, localUsize, ignoredOffset, ignoredDisk)) {
            return reject(core::ErrorCode::ParseFailure, "local_extra");
        }
        if ((flags & 0x0008U) == 0 &&
            (le32(local, 14) != le32(h, 16) || localSize != csize || localUsize != usize)) {
            return reject(core::ErrorCode::ParseFailure, "local_central_mismatch");
        }
        const auto payloadEnd = localOffset + 30ULL + localNameLen +
            localExtraLen + csize;
        localRanges.emplace_back(localOffset, payloadEnd);
        entries.push_back({std::move(path), csize, usize});
        cursor += recordLen;
    }
    if (cursor != cdEnd || !paths.contains("mimetype") ||
        !paths.contains("manifest.json") || !paths.contains("project.json")) {
        return reject(core::ErrorCode::ParseFailure, "central_or_required_roots");
    }
    std::sort(localRanges.begin(), localRanges.end());
    for (std::size_t i = 1; i < localRanges.size(); ++i) {
        if (localRanges[i].first < localRanges[i - 1].second)
            return reject(core::ErrorCode::ParseFailure, "overlapping_local_entries");
    }
    return core::Result<std::vector<RawEntry>>::success(std::move(entries));
}

}  // namespace rgsml::project::internal
