#pragma once
// L1-M18 Windows PRE-COMMIT decoded-file validation.
// M17 alone owns the verified True Peak ceiling. M18 proves that the WAV
// contains EXACTLY the deterministic post-quantization copy of that current
// M17 Master; no second true-peak measurement or ceiling veto.
// TransactionalAudioExporter owns no-overwrite, atomic publication,
// original SOURCE identity, encoded SHA, rollback and Product epoch lease.
#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/audio/wav_reader.hpp>
#include <rgsml/core/result.hpp>
#include <cstdint>
#include <optional>
#include <string>

namespace rgsml::platform::windows::internal {

struct M18CandidateReadbackEvidence final {
    audio::WavContainerKind container_kind{audio::WavContainerKind::RIFF};
    audio::WavSampleFormat encoded_format{audio::WavSampleFormat::PCM_S24};
    std::uint64_t exact_frames{0};
    std::uint64_t decoded_data_bytes{0};
    std::string canonical_decoded_sha256;
};

// Only inspect the PRIVATE candidate, not a delivered path. The caller
// must own a current M17 Product ticket, use an immutable verified Master
// and revalidate SOURCE/Project/epoch before committing the decoded file.
[[nodiscard]] core::Result<M18CandidateReadbackEvidence>
validate_m18_candidate_decoded_delivery(
    audio::WavReader& reader,
    audio::AudioBufferView verified_m17_master,
    audio::WavSampleFormat expected_encoding,
    std::optional<std::uint64_t> pcm24_dither_seed,
    double ceiling_db_tp);

} // namespace rgsml::platform::windows::internal
