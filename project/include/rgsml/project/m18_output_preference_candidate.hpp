#pragma once

// M18 OUTPUT-PREFERENCE SCHEMA CANDIDATE, not an active .rgsmaster contract.
// Neither ProjectSession nor the production writer is allowed to call it
// until the Product Owner approves the exact final output key mapping.
#include <rgsml/project/opaque_json_value.hpp>
#include <rgsml/core/result.hpp>
#include <cstdint>
#include <optional>

namespace rgsml::project {

enum class M18OutputEncodingCandidate : std::uint8_t {
    PCM24, IEEE_FLOAT32
};

struct M18OutputPreferenceCandidate final {
    M18OutputEncodingCandidate encoding{M18OutputEncodingCandidate::PCM24};
    // Required for PCM24. MUST be absent for Float32: Float32 has no dither.
    std::optional<std::uint64_t> dither_seed{};
    friend bool operator==(const M18OutputPreferenceCandidate&,
                           const M18OutputPreferenceCandidate&) = default;
};

class M18OutputPreferenceCandidateCodec final {
public:
    // An absent draft format is a legacy project, NOT a default PCM24
    // delivery choice. Reject malformed/partial candidates, fail closed.
    [[nodiscard]] static core::Result<
        std::optional<M18OutputPreferenceCandidate>>
    read_candidate(const OpaqueJsonValue& output);

    // Opt-in, offline candidate-only operation. Preserves arbitrary unrelated
    // output fields, including the accepted targetMasterOutputRateHz.
    // Never wire this to ProjectSession autosave or live Export UI.
    [[nodiscard]] static core::Result<OpaqueJsonValue>
    with_candidate(const OpaqueJsonValue& existing,
                   const M18OutputPreferenceCandidate& selection);
};

} // namespace rgsml::project
