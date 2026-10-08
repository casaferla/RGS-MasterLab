#pragma once

#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/realization_identity.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/dsp/module_execution_binding.hpp>
#include <rgsml/dsp/module_instance.hpp>
#include <rgsml/dsp/processing_chain.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace rgsml::render {

enum class RenderMode : std::uint8_t {
    PREVIEW,
};

class RenderRequest final {
public:
    [[nodiscard]] static rgsml::core::Result<RenderRequest> create(
        rgsml::audio::AudioBufferView source,
        rgsml::core::FrameRange render_window,
        const rgsml::dsp::ProcessingChain& chain,
        std::vector<rgsml::dsp::ModuleExecutionBinding> bindings,
        rgsml::core::FrameCount maximum_block_frames,
        std::optional<std::size_t> max_telemetry_bytes = std::nullopt,
        std::optional<rgsml::core::RealizationId> realization_id = std::nullopt);

    [[nodiscard]] rgsml::audio::AudioBufferView source() const noexcept;
    [[nodiscard]] rgsml::core::FrameRange render_window() const noexcept;
    [[nodiscard]] rgsml::dsp::ProcessingChainContext chain_context() const noexcept;
    [[nodiscard]] std::uint64_t chain_revision() const noexcept;
    [[nodiscard]] std::span<const rgsml::dsp::ModuleInstance>
    chain_instances() const noexcept;
    [[nodiscard]] std::span<const rgsml::dsp::ModuleExecutionBinding>
    bindings() const noexcept;
    [[nodiscard]] rgsml::core::FrameCount maximum_block_frames() const noexcept;
    [[nodiscard]] RenderMode mode() const noexcept;
    [[nodiscard]] std::optional<std::size_t> max_telemetry_bytes() const noexcept;
    [[nodiscard]] std::optional<rgsml::core::RealizationId> realization_id() const noexcept;

private:
    RenderRequest(
        rgsml::audio::AudioBufferView source,
        rgsml::core::FrameRange render_window,
        rgsml::dsp::ProcessingChainContext chain_context,
        std::uint64_t chain_revision,
        std::vector<rgsml::dsp::ModuleInstance> chain_instances,
        std::vector<rgsml::dsp::ModuleExecutionBinding> bindings,
        rgsml::core::FrameCount maximum_block_frames,
        std::optional<std::size_t> max_telemetry_bytes,
        std::optional<rgsml::core::RealizationId> realization_id) noexcept;

    rgsml::audio::AudioBufferView source_;
    rgsml::core::FrameRange render_window_;
    rgsml::dsp::ProcessingChainContext chain_context_;
    std::uint64_t chain_revision_;
    std::vector<rgsml::dsp::ModuleInstance> chain_instances_;
    std::vector<rgsml::dsp::ModuleExecutionBinding> bindings_;
    rgsml::core::FrameCount maximum_block_frames_;
    std::optional<std::size_t> max_telemetry_bytes_;
    std::optional<rgsml::core::RealizationId> realization_id_;
};

}  // namespace rgsml::render
