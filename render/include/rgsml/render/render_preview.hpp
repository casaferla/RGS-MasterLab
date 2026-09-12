#pragma once

#include <rgsml/core/result.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/render/render_request.hpp>
#include <rgsml/render/render_result.hpp>

namespace rgsml::render {

[[nodiscard]] rgsml::core::Result<RenderResult> render_preview(
    const RenderRequest& request,
    const rgsml::dsp::ModuleRegistry& registry);

}  // namespace rgsml::render
