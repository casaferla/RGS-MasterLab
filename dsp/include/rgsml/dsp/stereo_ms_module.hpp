#pragma once

#include <rgsml/dsp/imodule.hpp>
#include <rgsml/dsp/stereo_ms_parameters.hpp>

#include <memory>

namespace rgsml::dsp {

// M15 Stereo/M-S + Mono Bass: frozen broadband, LR12/LR24 and streamed
// recursive checkpoint DSP. Registered in ModuleRegistry by the separate
// M15-B1 candidate. Visual editor and live stage telemetry are independent
// later product integrations, not properties of this DSP implementation.
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

    [[nodiscard]] rgsml::core::Result<DspRuntimeCheckpoint>
    runtime_checkpoint() const override;

    [[nodiscard]] rgsml::core::Status
    restore_runtime_checkpoint(const DspRuntimeCheckpoint& checkpoint) override;

private:
    struct Impl;
    explicit StereoMsModule(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rgsml::dsp
