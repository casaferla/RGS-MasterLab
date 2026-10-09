#pragma once

#include <rgsml/dsp/imodule.hpp>
#include <rgsml/dsp/stereo_ms_parameters.hpp>

#include <memory>

namespace rgsml::dsp {

// M15-A3 isolated broadband/identity kernel. No ModuleRegistry factory is
// installed yet. Active LR12/LR24 on unmuted stereo fail closed until A4
// implements and qualifies their frozen all-pass topology and state.
class StereoMsModule final : public IModule {
public:
    [[nodiscard]] static rgsml::core::Result<std::unique_ptr<StereoMsModule>>
    create(const ModuleDescriptor& descriptor, StereoMsParameters parameters);

    ~StereoMsModule() override;

    [[nodiscard]] const StereoMsParameters& parameters() const noexcept;
    [[nodiscard]] const ModuleDescriptor& descriptor() const noexcept override;

    [[nodiscard]] rgsml::core::Result<DspRuntimeRequirements>
    runtime_requirements(const DspProcessSpec& spec) const override;

    [[nodiscard]] rgsml::core::Status
    prepare(const DspProcessSpec& spec) override;

    void reset() noexcept override;

    [[nodiscard]] rgsml::core::Status process(
        rgsml::audio::AudioBufferView input,
        rgsml::audio::MutableAudioBufferView output,
        const DspProcessContext& context) override;

private:
    struct Impl;
    explicit StereoMsModule(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rgsml::dsp
