#pragma once
// M18 canonical Project OUTPUT preference. This is a choice for a future
// WAV delivery, NEVER proof that the chosen file has been verified or
// that a new M17 render was performed. Both fields are isolated from
// MasterOutputRatePreference and preserve unknown output JSON entries.
#include <rgsml/project/opaque_json_value.hpp>
#include <rgsml/core/result.hpp>
#include <cstdint>
#include <optional>

namespace rgsml::project {

enum class M18MasterWavEncoding : std::uint8_t {
    PCM24_TPDF,
    IEEE_FLOAT32
};

struct M18MasterWavPreference final {
    M18MasterWavEncoding encoding{M18MasterWavEncoding::PCM24_TPDF};
    // Full uint64 representation: not a QML JS Number or floating point.
    // PCM24 requires it; IEEE_FLOAT32 strictly forbids it.
    std::optional<std::uint64_t> pcm24_dither_seed;
    friend bool operator==(const M18MasterWavPreference&,
                           const M18MasterWavPreference&) = default;
};

class M18MasterWavPreferenceCodec final {
public:
    // Stable v1 keys: finalMasterWavEncoding, finalMasterPcm24TpdfSeed.
    // Absent pair is a legacy project, not implicit export permission.
    [[nodiscard]] static core::Result<
        std::optional<M18MasterWavPreference>>
    read(const OpaqueJsonValue& output);

    // Preserve targetMasterOutputRateHz and independent future fields.
    // Invalid or partial existing M18 fields must never be overwritten
    // silently. A Float32 update removes the old PCM24 seed entirely.
    [[nodiscard]] static core::Result<OpaqueJsonValue>
    with_choice(const OpaqueJsonValue& existing,
                const M18MasterWavPreference& choice);

    [[nodiscard]] static bool valid(
        const M18MasterWavPreference& choice) noexcept;

    [[nodiscard]] static constexpr std::uint64_t default_pcm24_seed() noexcept {
        return UINT64_C(0x5247534D31380001);
    }
};

} // namespace rgsml::project
