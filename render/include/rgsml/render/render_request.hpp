#pragma once

#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/module_instance.hpp>
#include <rgsml/dsp/processing_chain.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace rgsml::render {

enum class RenderMode : std::uint8_t {
    PREVIEW,
};

struct GainParameterBinding final {
    rgsml::dsp::ModuleInstanceId instance_id;
    rgsml::dsp::GainParameters parameters;

    friend bool operator==(const GainParameterBinding&, const GainParameterBinding&) = default;
};

class RenderRequest final {
public:
    [[nodiscard]] static rgsml::core::Result<RenderRequest> create(
        rgsml::audio::AudioBufferView source,
        rgsml::core::FrameRange render_window,
        const rgsml::dsp::ProcessingChain& chain,
        std::vector<GainParameterBinding> gain_bindings,
        rgsml::core::FrameCount maximum_block_frames);

    [[nodiscard]] rgsml::audio::AudioBufferView source() const noexcept;
    [[nodiscard]] rgsml::core::FrameRange render_window() const noexcept;
    [[nodiscard]] rgsml::dsp::ProcessingChainContext chain_context() const noexcept;
    [[nodiscard]] std::uint64_t chain_revision() const noexcept;
    [[nodiscard]] std::span<const rgsml::dsp::ModuleInstance>
    chain_instances() const noexcept;
    [[nodiscard]] std::span<const GainParameterBinding> gain_bindings() const noexcept;
    [[nodiscard]] rgsml::core::FrameCount maximum_block_frames() const noexcept;
    [[nodiscard]] RenderMode mode() const noexcept;

private:
    RenderRequest(
        rgsml::audio::AudioBufferView source,
        rgsml::core::FrameRange render_window,
        rgsml::dsp::ProcessingChainContext chain_context,
        std::uint64_t chain_revision,
        std::vector<rgsml::dsp::ModuleInstance> chain_instances,
        std::vector<GainParameterBinding> gain_bindings,
        rgsml::core::FrameCount maximum_block_frames) noexcept;

    rgsml::audio::AudioBufferView source_;
    rgsml::core::FrameRange render_window_;
    rgsml::dsp::ProcessingChainContext chain_context_;
    std::uint64_t chain_revision_;
    std::vector<rgsml::dsp::ModuleInstance> chain_instances_;
    std::vector<GainParameterBinding> gain_bindings_;
    rgsml::core::FrameCount maximum_block_frames_;
};

}  // namespace rgsml::render
