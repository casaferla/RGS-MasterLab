#pragma once
// L3-M01 P03-P08/C01 offline frame engine, intentionally below IModule.
// No Qt, Product mutation, AutoMatch policy, chain registration or persistence.
// IModule timeline, authoritative checkpoint carrier and DYN_EQ_EFFECTIVE_MEMORY_V1
// certificate are separate integration responsibilities, not fabricated here.
#include "dynamic_eq_c01_coefficients.hpp"
#include <rgsml/dsp/dynamic_eq_parameters.hpp>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace rgsml::dsp::internal {

struct C01BiquadState final {
    double s1{0.0},s2{0.0};
    friend bool operator==(const C01BiquadState&,const C01BiquadState&)=default;
};
struct C01LaneState final {
    C01BiquadState detector{};
    C01BiquadState audio{};
    double power{0.0};
    friend bool operator==(const C01LaneState&,const C01LaneState&)=default;
};
struct C01BandState final {
    // Lane 0: mono, Left or Mid; lane 1: Right or Side.
    std::array<C01LaneState,2> lanes{};
    double unsigned_magnitude_db{0.0};
    friend bool operator==(const C01BandState&,const C01BandState&)=default;
};

// Isolated research-only observational carrier: the six arrays preserve
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

class C01FrameEngine final {
public:
    [[nodiscard]] static core::Result<C01FrameEngine> create(
        const DynamicEqParameters& params,
        std::uint32_t sample_rate_hz,
        std::uint32_t channels);

    // Input/output are planar and disjoint. Right planes must be empty for
    // mono and equal length for stereo. Success produces exactly N frames.
    // Failure preserves both live DSP state and ALL caller output bytes.
    [[nodiscard]] core::Status process_block(
        std::span<const double> input_left,
        std::span<const double> input_right,
        std::span<double> output_left,
        std::span<double> output_right,
        IC01ActivitySink* activity_sink=nullptr,
        std::uint64_t absolute_first_frame=0,
        std::span<C01FrameActivitySample> activity_scratch={});

    void reset() noexcept;
    [[nodiscard]] const std::vector<C01BandState>& states() const noexcept;
    [[nodiscard]] std::uint32_t channels() const noexcept;
    // Atomic fixed-state restoration for IModule checkpoint decoding.
    // Reject corrupt or incompatible state without changing the engine.
    [[nodiscard]] core::Status restore_states(
        const std::vector<C01BandState>& candidate);
private:
    struct Band final {
        core::Uuid band_id;
        EqRouting routing;
        DynamicEqControl control;
        C01CoefficientGenerator generator;
        BiquadCoefficients detector;
        // Verified neutral-control fast path: a fixed effective gain
        // yields the same binary64 RBJ coefficients on every frame.
        std::optional<BiquadCoefficients> fixed_audio;
        double rms_a, attack_a, release_a;
    };
    explicit C01FrameEngine(std::uint32_t channels,std::vector<Band> bands);
    [[nodiscard]] core::Status run(
        std::span<const double> left,std::span<const double> right,
        std::vector<double>& out_left,std::vector<double>& out_right,
        std::vector<C01BandState>& staged,
        std::span<C01FrameActivitySample> observed) const;
    [[nodiscard]] static bool update_lane(
        const Band& band,C01LaneState& lane,double x);
    [[nodiscard]] static bool process_section(
        const BiquadCoefficients& coeff,C01BiquadState& state,
        double x,double& y);
    [[nodiscard]] static bool apply_audio(
        const Band& band,C01BandState& state,double gain,
        double& left,double& right,std::uint32_t channels);

    std::uint32_t channels_;
    std::vector<Band> bands_;
    std::vector<C01BandState> states_;
};

} // namespace rgsml::dsp::internal
