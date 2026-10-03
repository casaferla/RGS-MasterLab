#pragma once

#include <rgsml/core/result.hpp>
#include <rgsml/dsp/compressor_parameters.hpp>
#include <rgsml/dsp/imodule.hpp>

#include <memory>
#include <vector>

namespace rgsml::dsp {

class ModuleDescriptor;

struct CompressorControlTraceFrame final {
    double detector_magnitude_ch0{0.0};
    double detector_magnitude_ch1{0.0};
    double linked_detector_ch0{0.0};
    double linked_detector_ch1{0.0};
    double level_db_ch0{-140.0};
    double level_db_ch1{-140.0};
    bool level_db_valid_ch0{false};
    bool level_db_valid_ch1{false};
    double target_reduction_db_ch0{0.0};
    double target_reduction_db_ch1{0.0};
    double smoothed_reduction_db_ch0{0.0};
    double smoothed_reduction_db_ch1{0.0};
    double linear_gain_ch0{1.0};
    double linear_gain_ch1{1.0};
    double output_sample_ch0{0.0};
    double output_sample_ch1{0.0};
};

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

    [[nodiscard]] rgsml::core::Result<std::vector<CompressorControlTraceFrame>>
    process_diagnostic_traces(
        rgsml::audio::AudioBufferView input,
        rgsml::audio::MutableAudioBufferView output,
        const DspProcessContext& context);

private:
    struct Impl;
    explicit CompressorModule(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};

}  // namespace rgsml::dsp
