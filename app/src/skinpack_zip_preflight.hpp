#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rgsml::app {

struct SkinZipEntry final {
    std::string path;
    std::uint64_t compressedSize = 0;
    std::uint64_t uncompressedSize = 0;
};

struct SkinZipCheck final {
    bool ok = false;
    std::string reason;
    std::vector<SkinZipEntry> entries;
};

// Strict v0.1 format preflight. No ZIP64, encryption, SFX, directory entries,
// data descriptors, additional fields, archive comments, or embedded code.
// Call BEFORE opening an untrusted .rgsskin archive with minizip-ng.
// CRC and inflate-stream validation remain the responsibility of the ZIP reader.
[[nodiscard]] SkinZipCheck preflight_skinpack_zip(std::span<const std::byte> bytes);

} // namespace rgsml::app
