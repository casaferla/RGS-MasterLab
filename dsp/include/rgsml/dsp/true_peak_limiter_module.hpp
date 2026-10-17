#pragma once

#include <rgsml/dsp/imodule.hpp>
#include <rgsml/dsp/true_peak_limiter_parameters.hpp>
#include <memory>

namespace rgsml::dsp {
class ModuleDescriptor;

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
    [[nodiscard]] core::Result<DspRuntimeRequirements>
    runtime_requirements(const DspProcessSpec& spec) const override;
    [[nodiscard]] core::Status prepare(const DspProcessSpec& spec) override;
    void reset() noexcept override;
    [[nodiscard]] core::Status process(audio::AudioBufferView input,
        audio::MutableAudioBufferView output, const DspProcessContext& context) override;
    [[nodiscard]] core::Status finalize(audio::MutableAudioBufferView output,
        const DspProcessContext& context) override;
private:
    struct Impl;
    explicit TruePeakLimiterModule(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};
} // namespace rgsml::dsp
