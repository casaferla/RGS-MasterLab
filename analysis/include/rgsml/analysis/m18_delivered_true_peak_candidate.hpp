#pragma once
// M18 post-quantization delivered-WAV True Peak candidate.
// Reuses the EXACT M16 private 4x / 12-tap / 11-zero EOS FIR; never
// certifies QUALIFIED status or modifies the frozen M16 production API.
#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/analysis/true_peak_measurement.hpp>
#include <rgsml/core/result.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace rgsml::analysis {

struct M18DeliveredTruePeakEvidence final {
    std::int64_t sample_rate_hz{0};
    std::size_t channel_count{0};
    std::uint64_t programme_frames{0};
    std::array<double,2> per_channel_linear_peak{};
    double global_linear_peak{0.0};
    std::optional<double> global_db_tp{};
    TruePeakPeakChannel peak_channel{TruePeakPeakChannel::NONE};
    bool is_exact_silence{true};
    TruePeakQualificationStatus qualification_status{
        TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED};
};

class M18DeliveredTruePeakCandidate final {
public:
    // Exact FULL decoded WAV frame count; never a preview/region or
    // source pre-dither Master. Only six supported master sample rates.
    [[nodiscard]] static core::Result<std::unique_ptr<M18DeliveredTruePeakCandidate>>
    create(audio::AudioFormat decodedFormat, core::FrameCount fullFrames);

    M18DeliveredTruePeakCandidate(const M18DeliveredTruePeakCandidate&)=delete;
    M18DeliveredTruePeakCandidate& operator=(const M18DeliveredTruePeakCandidate&)=delete;
    ~M18DeliveredTruePeakCandidate() noexcept;

    // Chronological, nonoverlapping, SOURCE_PROCESSING_RATE decoded blocks.
    // Both channels are committed atomically after the FIR update succeeds.
    [[nodiscard]] core::Status push_decoded(audio::AudioBufferView chunk);

    // Exactly once after every declared programme frame is supplied.
    // Same M16 11-zero EOS. Numeric evidence remains UNQUALIFIED.
    [[nodiscard]] core::Result<M18DeliveredTruePeakEvidence> finalize();

private:
    struct State;
    explicit M18DeliveredTruePeakCandidate(std::unique_ptr<State> state) noexcept;
    std::unique_ptr<State> state_;
};

} // namespace rgsml::analysis
