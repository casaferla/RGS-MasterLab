#pragma once
// Optional native observation contract for render-owned collectors.
// Not a processing parameter, checkpoint, project schema or playback grant.
#include <rgsml/dsp/dynamic_eq_parameters.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace rgsml::dsp::internal {
// C00 observation is optional, block-scoped and below IModule. The sink
// receives EXACT gains already calculated by this engine, only after a
// successful transactional block. Never store the provided span.
struct TransientC00AppliedGains final {
    double attack_db{};
    double sustain_db{};
};
struct ITransientC00ActivitySink {
    virtual ~ITransientC00ActivitySink() = default;
    virtual void on_successful_block(
        std::uint64_t absolute_first_frame,
        std::span<const TransientC00AppliedGains> gains) noexcept = 0;
};

// Native observational carrier: the six arrays preserve
// ENABLED-band execution order, each with its frozen UUID and routing.
// Zero power is tagged by has_detector_level=false, never -inf or NaN.
// These are C01 engine states, NOT a Qt/QML detector or inferred response.
struct C01BandActivitySample final {
    core::Uuid band_id{};
    EqRouting routing{EqRouting::STEREO};
    double linked_power{0.0};
    bool has_detector_level{false};
    double detector_level_dbfs{0.0};
    double unsigned_target_db{0.0};
    double signed_target_dynamic_gain_db{0.0};
    double applied_dynamic_gain_db{0.0};
    double effective_gain_db{0.0};
};
struct C01FrameActivitySample final {
    std::size_t band_count{0};
    std::array<C01BandActivitySample,6> bands{};
};
struct IC01ActivitySink {
    virtual ~IC01ActivitySink() = default;
    // SUCCESS-ONLY callback, after all PCM + DSP states were committed.
    // The caller supplies preallocated scratch; sink may not retain it.
    virtual void on_successful_block(
        std::uint64_t absolute_first_frame,
        std::span<const C01FrameActivitySample> observed) noexcept=0;
};

 } // namespace rgsml::dsp::internal
