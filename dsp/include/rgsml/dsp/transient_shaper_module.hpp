#pragma once
// L3-M03 frozen P01-P08 explicit IModule wrapper: zero-delay causal
// Transient Shaper, two controls, exact full-prefix streaming. The Product
// registry/UI/persistence and M04 listening qualification remain separate.
#include <rgsml/dsp/imodule.hpp>
#include <rgsml/dsp/transient_shaper_parameters.hpp>
#include <memory>

namespace rgsml::dsp {
class TransientShaperModule final:public IModule {
public:
    [[nodiscard]] static core::Result<std::unique_ptr<TransientShaperModule>>
    create(const ModuleDescriptor& descriptor,
           TransientShaperParameters params);
    ~TransientShaperModule() override;
    [[nodiscard]] const ModuleDescriptor& descriptor() const noexcept override;
    [[nodiscard]] const TransientShaperParameters& parameters() const noexcept;
    [[nodiscard]] core::Result<DspRuntimeRequirements>
    runtime_requirements(const DspProcessSpec& spec) const override;
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
    explicit TransientShaperModule(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};
} // namespace rgsml::dsp
