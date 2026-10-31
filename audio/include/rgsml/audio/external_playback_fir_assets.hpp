#pragma once
// Frozen playback FIRs are application resources, NEVER compiled coefficients.
// Loading and SHA verification occur OFF the realtime audio callback.
// The two playback paths remain separate from 14 private Final SRC FIRs.
#include <rgsml/core/result.hpp>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>
namespace rgsml::audio {
struct ExternalPlaybackFir final {
    std::shared_ptr<const std::vector<std::uint64_t>> bits;
    std::string_view sha256; // trust anchor is compiled, NOT read from manifest
};
// Set once from the EXE's real application-resource directory at startup.
// Configuring another root is supported for offline/native negative tests.
void configure_external_playback_fir_root(std::filesystem::path root);
[[nodiscard]] core::Result<ExternalPlaybackFir> load_external_playback_fir(
    std::int64_t source_hz,std::int64_t device_hz);
} // namespace rgsml::audio
