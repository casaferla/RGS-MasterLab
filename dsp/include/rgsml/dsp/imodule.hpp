#pragma once

#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>

#include <cstdint>

namespace rgsml::dsp {

enum class DspExecutionModel : std::uint8_t {
    STREAMING_CAUSAL,
    WINDOWED_OFFLINE,
    ANALYZE_THEN_PROCESS,
};

struct DspProcessSpec final {
    rgsml::audio::AudioFormat audio_format;
    rgsml::audio::FrameDomainId frame_domain_id;
    rgsml::core::FrameCount maximum_block_frames;
};

struct DspProcessContext final {
    rgsml::core::FrameRange output_frame_range;
    bool begins_stream;
    bool ends_stream;
};

struct DspRuntimeRequirements final {
    DspExecutionModel execution_model;
    rgsml::core::FrameCount algorithmic_latency_frames;
    rgsml::core::FrameCount look_ahead_frames;
    rgsml::core::FrameCount pre_context_frames;
    rgsml::core::FrameCount post_context_frames;
    rgsml::core::FrameCount effective_tail_frames;
    bool requires_prepass;
};

class ModuleDescriptor;

class IModule {
public:
    virtual ~IModule() = default;

    IModule(const IModule&) = delete;
    IModule& operator=(const IModule&) = delete;
    IModule(IModule&&) = delete;
    IModule& operator=(IModule&&) = delete;

    [[nodiscard]] virtual const ModuleDescriptor& descriptor() const noexcept = 0;

    [[nodiscard]] virtual rgsml::core::Result<DspRuntimeRequirements>
    runtime_requirements(const DspProcessSpec& spec) const = 0;

    [[nodiscard]] virtual rgsml::core::Status
    prepare(const DspProcessSpec& spec) = 0;

    virtual void reset() noexcept = 0;

    [[nodiscard]] virtual rgsml::core::Status
    process(
        rgsml::audio::AudioBufferView input,
        rgsml::audio::MutableAudioBufferView output,
        const DspProcessContext& context) = 0;

protected:
    IModule() = default;
};

}  // namespace rgsml::dsp
