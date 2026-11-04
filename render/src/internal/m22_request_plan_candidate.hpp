// L1-M22 STRICTLY PRIVATE PREPRODUCTION. Not a product render API.
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace rgsml::m22_private {

enum class State : std::uint8_t { PROCESSED };
enum class Mode : std::uint8_t { FULL, PREVIEW };
enum class Format : std::uint8_t { PCM24, FLOAT32 };
enum class InlineKind : std::uint8_t { GAIN, PARAMETRIC_EQ, COMPRESSOR, STEREO_MS };
enum class Failure : std::uint8_t {
    NONE, INVALID_STATE, MISSING_PROVENANCE, INVALID_CLOCK, INVALID_CHANNELS,
    INVALID_WINDOW, INVALID_NODE, DUPLICATE_INSTANCE, INVALID_TERMINAL,
    INVALID_DITHER_POLICY, LATENCY_OVERFLOW
};
enum class StateOrigin : std::uint8_t { REPLAY_FROM_CANONICAL_ZERO };

struct Window final {
    std::uint64_t first{0};
    std::uint64_t end{0};
    friend bool operator==(const Window&, const Window&) = default;
};
struct InlineSnapshot final {
    std::string instance_id;
    InlineKind kind{InlineKind::GAIN};
    std::string algorithm_signature;
    std::string parameter_signature;
    bool user_bypassed{false};
    std::uint64_t active_latency_frames{0};
    friend bool operator==(const InlineSnapshot&, const InlineSnapshot&) = default;
};
struct Intent final {
    State state{State::PROCESSED};
    Mode mode{Mode::FULL};
    std::string source_identity;
    std::string source_revision;
    std::string prepared_identity;
    std::string prepared_revision;
    std::string chain_identity;
    std::uint64_t chain_revision{0};
    std::uint64_t source_frames{0};
    Window window{};
    std::uint32_t processing_rate_hz{0};
    std::uint32_t target_rate_hz{0};
    std::uint8_t channels{0};
    std::vector<InlineSnapshot> stored_order;
    std::string terminal_limiter_signature;
    std::string terminal_limiter_parameters;
    std::uint64_t terminal_limiter_latency_frames{0};
    Format format{Format::FLOAT32};
    std::optional<std::uint64_t> dither_seed;
    std::string execution_policy_signature;
    friend bool operator==(const Intent&, const Intent&) = default;
};

inline bool supported_clock(std::uint32_t rate) noexcept {
    constexpr std::array<std::uint32_t, 6> rates{
        44100, 48000, 88200, 96000, 176400, 192000
    };
    return std::find(rates.begin(), rates.end(), rate) != rates.end();
}
inline bool nonempty(const std::string& s) noexcept { return !s.empty(); }
inline Failure validate(const Intent& in) {
    if (in.state != State::PROCESSED) return Failure::INVALID_STATE;
    if (in.mode != Mode::FULL && in.mode != Mode::PREVIEW)
        return Failure::INVALID_WINDOW;
    if (in.format != Format::PCM24 && in.format != Format::FLOAT32)
        return Failure::INVALID_DITHER_POLICY;
    if (!nonempty(in.source_identity) || !nonempty(in.source_revision)
        || !nonempty(in.prepared_identity) || !nonempty(in.prepared_revision)
        || !nonempty(in.chain_identity) || !nonempty(in.execution_policy_signature))
        return Failure::MISSING_PROVENANCE;
    if (!supported_clock(in.processing_rate_hz)
        || in.processing_rate_hz != in.target_rate_hz)
        return Failure::INVALID_CLOCK;
    if (in.channels != 1 && in.channels != 2)
        return Failure::INVALID_CHANNELS;
    if (in.source_frames == 0
        || in.source_frames > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())
        || in.window.first >= in.window.end
        || in.window.end > in.source_frames
        || (in.mode == Mode::FULL && (in.window.first != 0 || in.window.end != in.source_frames)))
        return Failure::INVALID_WINDOW;
    if (!nonempty(in.terminal_limiter_signature)
        || !nonempty(in.terminal_limiter_parameters))
        return Failure::INVALID_TERMINAL;
    if ((in.format == Format::PCM24) != in.dither_seed.has_value())
        return Failure::INVALID_DITHER_POLICY;
    std::unordered_set<std::string> ids;
    for (const auto& node : in.stored_order) {
        switch (node.kind) {
            case InlineKind::GAIN:
            case InlineKind::PARAMETRIC_EQ:
            case InlineKind::COMPRESSOR:
            case InlineKind::STEREO_MS: break;
            default: return Failure::INVALID_NODE;
        }
        if (!nonempty(node.instance_id) || !nonempty(node.algorithm_signature)
            || !nonempty(node.parameter_signature))
            return Failure::INVALID_NODE;
        if (!ids.insert(node.instance_id).second)
            return Failure::DUPLICATE_INSTANCE;
    }
    return Failure::NONE;
}

// Immutable, owned, deep-copied intent. It cannot observe further editor/UI changes.
// No QObject, QML, raw pointers, output path or live DSP state is stored.
class ResolvedRenderRequest final {
public:
    static std::optional<ResolvedRenderRequest> freeze(const Intent& in, Failure& error) {
        error = validate(in);
        if (error != Failure::NONE) return std::nullopt;
        return ResolvedRenderRequest(in);
    }
    ResolvedRenderRequest(const ResolvedRenderRequest&) = default;
    ResolvedRenderRequest(ResolvedRenderRequest&&) = default;
    ResolvedRenderRequest& operator=(const ResolvedRenderRequest&) = delete;
    ResolvedRenderRequest& operator=(ResolvedRenderRequest&&) = delete;
    const Intent& intent() const noexcept { return intent_; }
private:
    explicit ResolvedRenderRequest(const Intent& in): intent_(in) {}
    const Intent intent_;
};

enum class Stage : std::uint8_t { INLINE, TRUE_PEAK_TERMINAL, OUTPUT_REPRESENTATION };
struct PlannedStage final {
    Stage stage{Stage::INLINE};
    std::string instance_or_signature;
    bool bypass_identity{false};
    std::uint64_t effective_latency_frames{0};
    friend bool operator==(const PlannedStage&, const PlannedStage&) = default;
};

// Candidate metadata-only preflight: DOES NOT execute/render sound.
// Real module factories, DSP state, M17/M18/M09 binding remain integration blockers.
class RenderExecutionPlan final {
public:
    static std::optional<RenderExecutionPlan> preflight(
        const ResolvedRenderRequest& request, Failure& error) {
        error = Failure::NONE;
        std::vector<PlannedStage> stages;
        std::uint64_t total_latency = 0;
        const auto& in = request.intent();
        for (const auto& node : in.stored_order) {
            const auto effective = node.user_bypassed ? 0 : node.active_latency_frames;
            if (effective > std::numeric_limits<std::uint64_t>::max() - total_latency) {
                error = Failure::LATENCY_OVERFLOW;
                return std::nullopt;
            }
            total_latency += effective;
            stages.push_back({Stage::INLINE, node.instance_id, node.user_bypassed, effective});
        }
        if (in.terminal_limiter_latency_frames >
            std::numeric_limits<std::uint64_t>::max() - total_latency) {
            error = Failure::LATENCY_OVERFLOW;
            return std::nullopt;
        }
        total_latency += in.terminal_limiter_latency_frames;
        if (in.source_frames > std::numeric_limits<std::uint64_t>::max() - total_latency) {
            error = Failure::LATENCY_OVERFLOW;
            return std::nullopt;
        }
        stages.push_back({Stage::TRUE_PEAK_TERMINAL, in.terminal_limiter_signature, false,
                          in.terminal_limiter_latency_frames});
        stages.push_back({Stage::OUTPUT_REPRESENTATION,
                          in.format == Format::PCM24 ? "PCM24_M18" : "FLOAT32_M18", false, 0});
        return RenderExecutionPlan(request, std::move(stages), total_latency);
    }
    RenderExecutionPlan(const RenderExecutionPlan&) = default;
    RenderExecutionPlan(RenderExecutionPlan&&) = default;
    RenderExecutionPlan& operator=(const RenderExecutionPlan&) = delete;
    RenderExecutionPlan& operator=(RenderExecutionPlan&&) = delete;

    const ResolvedRenderRequest& request() const noexcept { return request_; }
    const std::vector<PlannedStage>& stages() const noexcept { return stages_; }
    std::uint64_t total_active_latency_frames() const noexcept { return total_latency_; }
    std::uint64_t logical_output_first_frame() const noexcept { return request_.intent().window.first; }
    std::uint64_t logical_output_frames() const noexcept {
        return request_.intent().window.end - request_.intent().window.first;
    }
    // Any stateful preview must replay from canonical origin or supply a verified exact checkpoint.
    // This FIRST candidate implements only the safe replay-from-zero policy.
    StateOrigin state_origin_policy() const noexcept {
        return StateOrigin::REPLAY_FROM_CANONICAL_ZERO;
    }
    std::uint64_t replay_input_first_frame() const noexcept { return 0; }
    // M17 final M16 verification/global trim is computed from the COMPLETE output-rate
    // programme. A region-only terminal run is NOT an acceptable exact preview.
    // Either reuse the same full-programme verified artifact or process to true EOS.
    std::uint64_t required_terminal_processing_end_frame() const noexcept {
        return request_.intent().source_frames;
    }
    bool terminal_requires_true_source_eos() const noexcept { return true; }
    // M18 Philox MUST be addressed by absolute output frame, never preview-local zero.
    std::uint64_t absolute_dither_first_frame() const noexcept { return logical_output_first_frame(); }
private:
    RenderExecutionPlan(const ResolvedRenderRequest& req,
                        std::vector<PlannedStage> nodes, std::uint64_t latency)
      : request_(req), stages_(std::move(nodes)), total_latency_(latency) {}
    const ResolvedRenderRequest request_;
    const std::vector<PlannedStage> stages_;
    const std::uint64_t total_latency_;
};
} // namespace rgsml::m22_private
