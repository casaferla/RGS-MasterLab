#pragma once

#include <rgsml/core/result.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/render/render_request.hpp>
#include <rgsml/render/render_result.hpp>

namespace rgsml::render {

[[nodiscard]] rgsml::core::Result<RenderResult> render_preview(
    const RenderRequest& request,
    const rgsml::dsp::ModuleRegistry& registry);

// Exact bounded prefix cache: render only the final streaming P2.
// MANUAL retains the original five-node/bypassed Compressor & Stereo-M/S
// restriction. REF_LINKED permits only Gain/EQ upstream. No telemetry or
// latency may be lost; callers pin the full upstream graph and PCM owner.
// Other topologies MUST use render_preview() instead.
[[nodiscard]] rgsml::core::Result<RenderResult>
render_preview_p2_cached_prefix(
    const RenderRequest& full_request,
    const RenderResult& exact_four_stage_prefix,
    const rgsml::dsp::ModuleRegistry& registry);

}  // namespace rgsml::render
