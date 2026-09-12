#pragma once

#include <rgsml/core/result.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/imodule.hpp>

#include <memory>

namespace rgsml::dsp {

class GainModule final : public IModule {
public:
    [[nodiscard]] static rgsml::core::Result<std::unique_ptr<GainModule>>
    create(const ModuleDescriptor& descriptor, GainParameters parameters);

    ~GainModule() override;

    [[nodiscard]] const GainParameters& parameters() const noexcept;

    [[nodiscard]] const ModuleDescriptor& descriptor() const noexcept override;

    [[nodiscard]] rgsml::core::Result<DspRuntimeRequirements>
    runtime_requirements(const DspProcessSpec& spec) const override;

    [[nodiscard]] rgsml::core::Status
    prepare(const DspProcessSpec& spec) override;

    void reset() noexcept override;

    [[nodiscard]] rgsml::core::Status
    process(
        rgsml::audio::AudioBufferView input,
        rgsml::audio::MutableAudioBufferView output,
        const DspProcessContext& context) override;

private:
    struct Impl;

    explicit GainModule(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};

}  // namespace rgsml::dsp
