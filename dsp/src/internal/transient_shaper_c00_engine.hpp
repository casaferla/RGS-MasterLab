#pragma once
// L3-M03 P03-P08 frozen causal Transient Shaper C00 core, BELOW IModule.
// No Qt, AUTO MATCH policy, persistence, public checkpoint/seek certificate.
// Three envelope states + two gain-smoother states, zero lookahead/tail.
// Input/output can alias; failed process leaves both authoritative state and
// caller output unchanged. All DSP staging allocated by create().
#include <rgsml/dsp/transient_shaper_parameters.hpp>
#include <rgsml/core/result.hpp>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::dsp::internal {
struct TransientShaperC00State final {
    double f{0.0};
    double m{0.0};
    double l{0.0};
    double g_attack_db{0.0};
    double g_sustain_db{0.0};
    friend bool operator==(const TransientShaperC00State&,
                           const TransientShaperC00State&)=default;
};

class TransientShaperC00Engine final {
public:
    [[nodiscard]] static core::Result<TransientShaperC00Engine> create(
        const TransientShaperParameters& params,
        std::uint32_t sample_rate_hz,std::uint32_t channels,
        std::size_t maximum_block_frames);
    [[nodiscard]] core::Status process_block(
        std::span<const double> input_left,
        std::span<const double> input_right,
        std::span<double> output_left,
        std::span<double> output_right);
    [[nodiscard]] core::Status restore_state(
        const TransientShaperC00State& candidate) noexcept;
    void reset() noexcept {state_={};}
    [[nodiscard]] const TransientShaperC00State& state() const noexcept {
        return state_;
    }
    [[nodiscard]] std::uint32_t sample_rate_hz() const noexcept {
        return sample_rate_hz_;
    }
    [[nodiscard]] std::uint32_t channels() const noexcept {
        return channels_;
    }
    [[nodiscard]] std::size_t maximum_block_frames() const noexcept {
        return maximum_block_frames_;
    }
    [[nodiscard]] const TransientShaperParameters& parameters() const noexcept {
        return params_;
    }
private:
    TransientShaperC00Engine(
        TransientShaperParameters params,std::uint32_t fs,
        std::uint32_t channels,std::size_t max,
        double a_f,double a_m,double a_l,double a_g,
        std::vector<double> left,std::vector<double> right)
        :params_(params),sample_rate_hz_(fs),channels_(channels),
         maximum_block_frames_(max),a_f_(a_f),a_m_(a_m),
         a_l_(a_l),a_g_(a_g),scratch_left_(std::move(left)),
         scratch_right_(std::move(right)){}
    TransientShaperParameters params_;
    std::uint32_t sample_rate_hz_{};
    std::uint32_t channels_{};
    std::size_t maximum_block_frames_{};
    double a_f_{},a_m_{},a_l_{},a_g_{};
    TransientShaperC00State state_{};
    std::vector<double> scratch_left_;
    std::vector<double> scratch_right_;
};
} // namespace rgsml::dsp::internal
