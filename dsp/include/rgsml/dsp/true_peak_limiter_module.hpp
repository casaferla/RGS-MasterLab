#pragma once

#include <rgsml/dsp/imodule.hpp>
#include <rgsml/dsp/true_peak_limiter_parameters.hpp>
#include <memory>
#include <cstdint>
#include <span>

namespace rgsml::dsp {
class ModuleDescriptor;
// Opt-in test-only witness of the gain actually applied to a real frame.
struct TruePeakLimiterAppliedGainFrame final {
    std::int64_t programme_frame{0};
    std::int64_t raw_output_frame{0};
    double reduction_db{0.0};
    double gain{1.0};
};
using TruePeakLimiterAppliedGainSink = void (*)(
    void*, std::span<const TruePeakLimiterAppliedGainFrame>) noexcept;

// M17 core-first streaming audio implementation. Until checkpoint, whole-
// resource verification, and FQ are qualified, no production ModuleRegistry
// factory may expose this module to the product's terminal render.
class TruePeakLimiterModule final : public IModule {
public:
    [[nodiscard]] static core::Result<std::unique_ptr<TruePeakLimiterModule>>
    create(const ModuleDescriptor& descriptor, TruePeakLimiterParameters parameters);
    ~TruePeakLimiterModule() override;
    [[nodiscard]] const ModuleDescriptor& descriptor() const noexcept override;
    [[nodiscard]] const TruePeakLimiterParameters& parameters() const noexcept;
    // Non-invasive read-only test instrumentation, absent by default.
    void set_applied_gain_sink(TruePeakLimiterAppliedGainSink sink,
        void* context) noexcept;
    [[nodiscard]] core::Result<DspRuntimeRequirements>
    runtime_requirements(const DspProcessSpec& spec) const override;
    [[nodiscard]] core::Status prepare(const DspProcessSpec& spec) override;
    void reset() noexcept override;
    [[nodiscard]] core::Status process(audio::AudioBufferView input,
        audio::MutableAudioBufferView output, const DspProcessContext& context) override;
    [[nodiscard]] core::Status finalize(audio::MutableAudioBufferView output,
        const DspProcessContext& context) override;
    [[nodiscard]] core::Result<DspRuntimeCheckpoint> runtime_checkpoint() const override;
    [[nodiscard]] core::Status
    restore_runtime_checkpoint(const DspRuntimeCheckpoint& checkpoint) override;
private:
    struct Impl;
    explicit TruePeakLimiterModule(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
    TruePeakLimiterAppliedGainSink applied_gain_sink_{nullptr};
    void* applied_gain_sink_context_{nullptr};
};
} // namespace rgsml::dsp
