#pragma once

#include <rgsml/core/result.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/render/render_request.hpp>
#include <rgsml/render/render_result.hpp>

namespace rgsml::render {

[[nodiscard]] rgsml::core::Result<RenderResult> render_preview(
    const RenderRequest& request,
    const rgsml::dsp::ModuleRegistry& registry);

// L1+P2 opt-in bounded prefix cache: render only the fifth streaming P2
// from a previously accepted, immutable four-stage PCM realization.
// Strictly restricted to full programme / bypassed Compressor & Stereo/M-S,
// where no upstream telemetry or latency can be lost. All other chain
// topologies MUST use render_preview() instead.
[[nodiscard]] rgsml::core::Result<RenderResult>
render_preview_p2_cached_prefix(
    const RenderRequest& full_request,
    const RenderResult& exact_four_stage_prefix,
    const rgsml::dsp::ModuleRegistry& registry);

}  // namespace rgsml::render
