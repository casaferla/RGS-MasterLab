#pragma once
// L1-M18 Windows PRE-COMMIT readback gate, not an export/merge authority.
// Compares the real WAV decoded PCM with the exact post-quant M18 codec,
// hashes decoded samples, measures the same frozen M16 4x FIR on the
// delivered data and reports ceiling compliance WITHOUT publishing a file.
#include <rgsml/analysis/m18_delivered_true_peak_candidate.hpp>
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
    analysis::M18DeliveredTruePeakEvidence decoded_true_peak;
    double requested_ceiling_db_tp{0.0};
    bool meets_ceiling{false};
};

// Only inspect a PRIVATE, not-yet-published WAV candidate. The calling
// TransactionalAudioExporter is responsible for file identity, encoded-byte
// hash/size, no-overwrite, rollback and verifying the FINAL published file.
// The caller MUST separately revalidate current M17 owner / session epoch
// and decide explicitly what to do with NON-CONFORMING delivered true peak.
[[nodiscard]] core::Result<M18CandidateReadbackEvidence>
validate_m18_candidate_decoded_delivery(
    audio::WavReader& reader,
    audio::AudioBufferView verified_m17_master,
    audio::WavSampleFormat expected_encoding,
    std::optional<std::uint64_t> pcm24_dither_seed,
    double ceiling_db_tp);

} // namespace rgsml::platform::windows::internal
