#pragma once

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/dsp/module_instance.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace rgsml::dsp {
class ModuleRegistry;
}

namespace rgsml::render {

class RenderRequest;
class RenderResult;

enum class GainExecutionDisposition : std::uint8_t {
    PROCESSED,
    BYPASS_IDENTITY,
};

struct GainExecutionSignature final {
    rgsml::dsp::ModuleInstanceId instance_id;
    std::string type_id;
    std::string algorithm_version;
    std::string parameter_schema_id;
    double gain_db;
    GainExecutionDisposition disposition;

    friend bool operator==(const GainExecutionSignature&, const GainExecutionSignature&) = default;
};

class RenderResult final {
public:
    RenderResult(RenderResult&&) noexcept = default;
    RenderResult& operator=(RenderResult&&) noexcept = default;
    RenderResult(const RenderResult&) = delete;
    RenderResult& operator=(const RenderResult&) = delete;
    ~RenderResult() = default;

    [[nodiscard]] const rgsml::audio::AudioBuffer& buffer() const noexcept;
    [[nodiscard]] rgsml::audio::AudioBufferView view() const noexcept;
    [[nodiscard]] rgsml::core::FrameRange render_window() const noexcept;
    [[nodiscard]] rgsml::audio::FrameDomainId frame_domain_id() const noexcept;
    [[nodiscard]] std::uint64_t chain_revision() const noexcept;
    [[nodiscard]] const std::vector<GainExecutionSignature>&
    gain_signatures() const noexcept;

private:
    friend rgsml::core::Result<RenderResult> render_preview(
        const RenderRequest& request,
        const rgsml::dsp::ModuleRegistry& registry);

    RenderResult(
        rgsml::audio::AudioBuffer buffer,
        rgsml::core::FrameRange render_window,
        rgsml::audio::FrameDomainId frame_domain_id,
        std::uint64_t chain_revision,
        std::vector<GainExecutionSignature> gain_signatures) noexcept;

    rgsml::audio::AudioBuffer buffer_;
    rgsml::core::FrameRange render_window_;
    rgsml::audio::FrameDomainId frame_domain_id_;
    std::uint64_t chain_revision_;
    std::vector<GainExecutionSignature> gain_signatures_;
};

}  // namespace rgsml::render
