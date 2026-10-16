#include "skinpack_zip_preflight.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <set>
#include <string_view>
#include <utility>

namespace rgsml::app {
namespace {
constexpr std::uint64_t maxArchive = 20ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t maxExpandedTotal = 60ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t maxExpandedEntry = 16ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t maxExpansionRatio = 250ULL;
constexpr std::size_t maxEntryCount = 128;
constexpr std::size_t eocdSize = 22;
constexpr std::size_t centralHeaderSize = 46;
constexpr std::size_t localHeaderSize = 30;

[[nodiscard]] SkinZipCheck fail(std::string reason) {
    return SkinZipCheck{false, std::move(reason), {}};
}

[[nodiscard]] std::uint16_t le16(std::span<const std::byte> b, std::size_t i) {
    return static_cast<std::uint16_t>(std::to_integer<unsigned>(b[i]) |
        (std::to_integer<unsigned>(b[i + 1]) << 8U));
}
[[nodiscard]] std::uint32_t le32(std::span<const std::byte> b, std::size_t i) {
    return static_cast<std::uint32_t>(le16(b, i) | (std::uint32_t{le16(b, i + 2)} << 16U));
}

[[nodiscard]] bool safeName(std::string_view name) {
    if (name.empty() || name.size() > 512 || name.front() == '/' || name.back() == '/')
        return false;
    std::size_t start = 0;
    while (start < name.size()) {
        const auto end = name.find('/', start);
        const auto seg = name.substr(start, end == std::string_view::npos ?
            name.size() - start : end - start);
        if (seg.empty() || seg == "." || seg == "..") return false;
        for (unsigned char c : seg) {
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
                return false;
        }
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    if (name == "manifest.json" || name == "skin.json" || name == "README.md" ||
        name == "preview.png") return true;
    constexpr std::string_view prefix = "assets/textures/";
    if (!name.starts_with(prefix)) return false;
    return name.ends_with(".png") || name.ends_with(".webp");
}

[[nodiscard]] std::string lowerAscii(std::string_view input) {
    std::string s(input);
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    }
    return s;
}

struct Range { std::uint64_t first; std::uint64_t end; };
} // namespace

SkinZipCheck preflight_skinpack_zip(std::span<const std::byte> b) {
    if (b.size() < eocdSize || b.size() > maxArchive) return fail("zip_size");
    const auto endOfCentral = b.size() - eocdSize;
    if (le32(b, endOfCentral) != 0x06054b50U) return fail("eocd_missing_or_comment");
    if (le16(b, endOfCentral + 4) != 0 || le16(b, endOfCentral + 6) != 0 ||
        le16(b, endOfCentral + 20) != 0) return fail("multidisk_or_comment");
    const auto records = le16(b, endOfCentral + 10);
    if (records == 0 || records != le16(b, endOfCentral + 8) ||
        records > maxEntryCount || records == 0xffffU) return fail("entry_count");
    const auto centralSize = le32(b, endOfCentral + 12);
    const auto centralStart = le32(b, endOfCentral + 16);
    if (centralStart == 0xffffffffU || centralSize == 0xffffffffU ||
        centralStart > endOfCentral || centralSize != endOfCentral - centralStart)
        return fail("central_directory_bounds");
    std::uint64_t cursor = centralStart;
    std::uint64_t aggregate = 0;
    std::set<std::string> names;
    std::vector<Range> ranges;
    std::vector<SkinZipEntry> entries;
    entries.reserve(records);
    for (std::uint64_t i = 0; i < records; ++i) {
        if (cursor > endOfCentral || endOfCentral - cursor < centralHeaderSize ||
            le32(b, static_cast<std::size_t>(cursor)) != 0x02014b50U)
            return fail("central_header");
        const auto p = static_cast<std::size_t>(cursor);
        const auto flags = le16(b, p + 8);
        const auto method = le16(b, p + 10);
        const auto crc = le32(b, p + 16);
        const auto compressed = le32(b, p + 20);
        const auto expanded = le32(b, p + 24);
        const auto nameLen = le16(b, p + 28);
        const auto extraLen = le16(b, p + 30);
        const auto commentLen = le16(b, p + 32);
        const auto diskStart = le16(b, p + 34);
        const auto attributes = le32(b, p + 38);
        const auto localStart = le32(b, p + 42);
        if ((flags & ~std::uint16_t{0x0800U}) != 0 ||
            (method != 0 && method != 8) || extraLen != 0 || commentLen != 0 ||
            diskStart != 0 || nameLen == 0 || nameLen > 512 ||
            compressed == 0xffffffffU || expanded == 0xffffffffU ||
            localStart == 0xffffffffU) return fail("unsupported_zip_features");
        const auto type = (attributes >> 16U) & 0170000U;
        if (type != 0 && type != 0100000U) return fail("nonregular_file");
        if ((attributes & 0x10U) != 0) return fail("directory_flag");
        if (expanded > maxExpandedEntry || expanded > maxExpandedTotal - aggregate ||
            (expanded > 0 && compressed == 0) ||
            (compressed > 0 && expanded / compressed > maxExpansionRatio) ||
            (method == 0 && expanded != compressed)) return fail("expansion_limits");
        aggregate += expanded;
        const auto namePos = cursor + centralHeaderSize;
        if (namePos > endOfCentral || nameLen > endOfCentral - namePos)
            return fail("central_name_bounds");
        const auto name = std::string(reinterpret_cast<const char*>(b.data() + namePos), nameLen);
        if (!safeName(name) || !names.insert(lowerAscii(name)).second)
            return fail("unsafe_or_duplicate_path");
        if (localStart > centralStart || centralStart - localStart < localHeaderSize)
            return fail("local_header_bounds");
        const auto lp = static_cast<std::size_t>(localStart);
        if (le32(b, lp) != 0x04034b50U || le16(b, lp + 6) != flags ||
            le16(b, lp + 8) != method || le32(b, lp + 14) != crc ||
            le32(b, lp + 18) != compressed || le32(b, lp + 22) != expanded ||
            le16(b, lp + 26) != nameLen || le16(b, lp + 28) != 0)
            return fail("local_central_mismatch");
        const std::uint64_t dataStart = std::uint64_t{localStart} + localHeaderSize + nameLen;
        if (dataStart > centralStart || compressed > centralStart - dataStart)
            return fail("local_payload_bounds");
        const auto localName = std::string_view(reinterpret_cast<const char*>(b.data() + lp + localHeaderSize), nameLen);
        if (localName != name) return fail("local_name_mismatch");
        ranges.push_back({localStart, dataStart + compressed});
        entries.push_back({name, compressed, expanded});
        cursor = namePos + nameLen;
    }
    if (cursor != endOfCentral || !names.contains("manifest.json") ||
        !names.contains("skin.json")) return fail("central_or_required_roots");
    std::sort(ranges.begin(), ranges.end(), [](const Range& a, const Range& b) {
        return a.first < b.first;
    });
    std::uint64_t previousEnd = 0;
    for (const auto& range : ranges) {
        if (range.first != previousEnd) return fail("overlap_gap_or_prefix");
        previousEnd = range.end;
    }
    if (previousEnd != centralStart) return fail("trailing_local_payload");
    return SkinZipCheck{true, {}, std::move(entries)};
}
} // namespace rgsml::app
