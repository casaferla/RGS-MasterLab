#pragma once
// Frozen playback FIRs are application resources, NEVER compiled coefficients.
// Loading and SHA verification occur OFF the realtime audio callback.
// The two playback paths remain separate from 14 private Final SRC FIRs.
#include <rgsml/core/result.hpp>
#include <cstdint>
#include <string>
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
// Portable UTF-16 path value at the public audio boundary: filesystem is
// deliberately private to the implementation per forbidden-include gate.
void configure_external_playback_fir_root(std::u16string root);
[[nodiscard]] core::Result<ExternalPlaybackFir> load_external_playback_fir(
    std::int64_t source_hz,std::int64_t device_hz);
} // namespace rgsml::audio
