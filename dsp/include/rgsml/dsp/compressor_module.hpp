#pragma once

#include <rgsml/core/result.hpp>
#include <rgsml/dsp/compressor_parameters.hpp>
#include <rgsml/dsp/imodule.hpp>

#include <memory>

namespace rgsml::dsp {

class ModuleDescriptor;

class CompressorModule final : public IModule {
public:
    [[nodiscard]] static rgsml::core::Result<std::unique_ptr<CompressorModule>> create(
        const ModuleDescriptor& descriptor,
        CompressorParameters parameters);

    ~CompressorModule() override;

    [[nodiscard]] const CompressorParameters& parameters() const noexcept;
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

    [[nodiscard]] rgsml::core::Result<DspRuntimeCheckpoint>
    runtime_checkpoint() const override;

    [[nodiscard]] rgsml::core::Status
    restore_runtime_checkpoint(const DspRuntimeCheckpoint& checkpoint) override;

    [[nodiscard]] rgsml::core::Status
    finalize(
        rgsml::audio::MutableAudioBufferView output,
        const DspProcessContext& context) override;

private:
    struct Impl;
    explicit CompressorModule(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};

}  // namespace rgsml::dsp
