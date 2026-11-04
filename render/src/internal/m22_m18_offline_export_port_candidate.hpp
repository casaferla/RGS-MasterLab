// M22 private, non-production binding to existing M18 transactional WAV exporter.
// NOT proof of M17, actual Source/Gold read-only ownership or canonical Gate I/G/L.
#pragma once
#include "m22_request_plan_candidate.hpp"
#include "internal/m18_offline_export.hpp"

#include <filesystem>
#include <optional>
#include <string_view>

namespace rgsml::m22_private {

// This adapter consumes the separate, frozen M22 request/plan metadata.
// It does not implement a parallel encoder/quantizer or a second writer.
[[nodiscard]] inline std::optional<rgsml::audio::internal::M18WavSpec>
m22_m18_wav_spec(const RenderExecutionPlan& plan) noexcept {
    using namespace rgsml::audio::internal;
    const auto& request=plan.request().intent();
    if (plan.stages().size()<2
        || plan.stages()[plan.stages().size()-2].stage!=Stage::TRUE_PEAK_TERMINAL
        || plan.stages().back().stage!=Stage::OUTPUT_REPRESENTATION)
        return std::nullopt;
    const auto encoding = request.format == Format::PCM24
        ? M18WavEncoding::PCM24 : M18WavEncoding::IEEE_FLOAT32;
    if (request.format!=Format::PCM24 && request.format!=Format::FLOAT32)
        return std::nullopt;
    if (request.format==Format::PCM24 && !request.dither_seed)
        return std::nullopt;
    if (request.format==Format::FLOAT32 && request.dither_seed)
        return std::nullopt;
    M18WavSpec spec{encoding,request.target_rate_hz,request.channels,
        plan.logical_output_frames(),request.dither_seed.value_or(0)};
    if (m18_plan_wav(spec).status!=M18WavStatus::OK)
        return std::nullopt;
    return spec;
}

// Isolated test path. The caller supplies already rendered and authenticated
// binary64 MASTER frames via the existing M18 frame provider API. A REAL M22
// integration MUST verify provider realization/build/chain identity against
// immutable plan; not possible from the current M18 provider interface alone.
[[nodiscard]] inline rgsml::audio::internal::M18OfflineExportResult
m22_export_existing_m18_candidate(
    const RenderExecutionPlan& plan,
    rgsml::audio::internal::M18MasterFrameProvider& master_frames,
    const std::filesystem::path& destination,
    const rgsml::audio::internal::M18ExportStopSignal* stop=nullptr) noexcept
{
    using namespace rgsml::audio::internal;
    const auto spec=m22_m18_wav_spec(plan);
    if(!spec)return {M18OfflineExportStatus::INVALID_PLAN};
    return m18_export_offline_candidate(master_frames,destination,
        *spec,plan.absolute_dither_first_frame(),stop);
}
} // namespace rgsml::m22_private
