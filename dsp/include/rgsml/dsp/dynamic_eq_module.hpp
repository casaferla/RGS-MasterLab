#pragma once
// L3-M01 P01-P08/C01: IModule wrapper for direct explicit creation.
// Product DSP registry, editor, parameter codec and AutoMatch binding are
// deliberately not modified by this staging integration.
#include <rgsml/dsp/imodule.hpp>
#include <rgsml/dsp/linked_dsp_activity_observation.hpp>
#include <rgsml/dsp/dynamic_eq_parameters.hpp>
#include <memory>

namespace rgsml::dsp {
class DynamicEqModule final : public IModule {
public:
    [[nodiscard]] static core::Result<std::unique_ptr<DynamicEqModule>>
    create(const ModuleDescriptor& descriptor,DynamicEqParameters parameters);
    ~DynamicEqModule() override;

    [[nodiscard]] const DynamicEqParameters& parameters() const noexcept;
    [[nodiscard]] const ModuleDescriptor& descriptor() const noexcept override;
    [[nodiscard]] core::Result<DspRuntimeRequirements>
    runtime_requirements(const DspProcessSpec& spec) const override;
    // Borrowed render-local sink; attach before prepare, detach at any
    // point. No observer allocation or publication in ordinary rendering.
    [[nodiscard]] bool set_activity_sink(internal::IC01ActivitySink* sink) noexcept;
    [[nodiscard]] core::Status prepare(const DspProcessSpec& spec) override;
    void reset() noexcept override;
    [[nodiscard]] core::Status process(
        audio::AudioBufferView input,audio::MutableAudioBufferView output,
        const DspProcessContext& context) override;
    [[nodiscard]] core::Result<DspRuntimeCheckpoint>
    runtime_checkpoint() const override;
    [[nodiscard]] core::Status
    restore_runtime_checkpoint(const DspRuntimeCheckpoint& cp) override;
private:
    struct Impl;
    explicit DynamicEqModule(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};
} // namespace rgsml::dsp
