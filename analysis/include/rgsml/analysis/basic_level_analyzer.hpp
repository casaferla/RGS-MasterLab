#pragma once
// L1-M19 Basic Level Analyzer: read-only Product-facing numerical adapter.
// Exactly five static AnalysisScope metrics; no live side-channel, implicit
// zero for unavailable results, private audio renderer or duplicated M16 TP.
// Mathematical/official Gate G remains separate from mere API compilation.
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace rgsml::analysis {

enum class BasicLevelValueState : std::uint8_t {
    AVAILABLE,
    SILENCE,
    INSUFFICIENT_DURATION,
    BELOW_LOUDNESS_GATE,
    NUMERIC_UNDEFINED,
    // M16 permits a numerical TP reading but its public official
    // qualification_status() has not yet been promoted to QUALIFIED.
    M16_SUPPORTED_UNQUALIFIED,
    // M16's existing typed API accepts complete owning resources only:
    // no regional extrapolation or second oversampler is permitted.
    M16_SCOPE_UNAVAILABLE
};

struct BasicLevelValue final {
    std::optional<double> db{};
    BasicLevelValueState state{BasicLevelValueState::NUMERIC_UNDEFINED};
};

struct BasicLevelRetainedInterval final {
    std::uint64_t begin{0};
    std::uint64_t end{0};
};

struct BasicLevelAnalysisResult final {
    // Absolute, frame-domain-specific AnalysisScope identity. Never
    // silently compare SOURCE, PREPARED and OUTPUT_RATE frame positions.
    std::int64_t scope_begin_frame{0};
    std::int64_t scope_frame_count{0};
    std::int64_t sample_rate_hz{0};
    std::size_t channel_count{0};
    audio::FrameDomainId frame_domain{audio::FrameDomainId::SOURCE_PROCESSING_RATE};
    std::string_view k_weighting_bank_member_id{};
    std::string_view k_weighting_bank_sha256{};
    BasicLevelValue integrated_lufs{};
    BasicLevelValue full_scope_rms_dbfs{};
    BasicLevelValue sample_peak_dbfs{};
    BasicLevelValue true_peak_db_tp{};
    BasicLevelValue sample_peak_crest_db{};
    // Exact final retained 400-ms block union from THIS M19 producer.
    // An unavailable gate is distinct from an available but empty union.
    // These are observation-only scope positions; still NOT a canonical
    // Phase-6 FeatureStatus or producer signature.
    bool retained_block_disclosure_complete{false};
    std::vector<BasicLevelRetainedInterval> retained_block_union{};
};

// Selected read-only AnalysisScope: four metrics and a typed explicit
// M16_SCOPE_UNAVAILABLE for TP. M16 is NOT called on a fabricated slice.
// Strict input-domain, legal Fs and true subview checks precede every
// calculation. Never changes the PCM or the existing Project/SOURCE epoch.
[[nodiscard]] core::Result<BasicLevelAnalysisResult>
measure_basic_level_scope(
    const audio::AudioBufferView& resource,
    core::FrameIndex absoluteScopeBegin,
    core::FrameCount scopeFrames,
    audio::FrameDomainId expectedDomain);

// Whole REAL owning AudioBuffer: same exact M19 four metrics plus the
// current public M16 full-resource TruePeakMeasurement API. Its own
// qualification_status is preserved; numerical support is NOT a Gate G.
[[nodiscard]] core::Result<BasicLevelAnalysisResult>
measure_basic_level_full_resource(const audio::AudioBuffer& resource);

} // namespace rgsml::analysis
