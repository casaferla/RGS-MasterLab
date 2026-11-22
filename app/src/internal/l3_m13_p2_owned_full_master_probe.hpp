#pragma once
// M13 fresh combined Product PCM probe with a fully anchored exact
// M10 P2 winner, never an anonymous raw Ceiling/Drive override.
// Neither the preflight receipt nor a successful probe is a persisted
// Product M12 receipt, M13 VERIFIED result, or permission to export.
#include "l3_m12_p2_exact_winner_preflight.hpp"
#include "l3_m13_fresh_full_master_probe.hpp"
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <span>

namespace rgsml::app::l3_automatch::m13_frozen {
[[nodiscard]] inline core::Result<FreshNativeMasterCompositionProbe>
probe_fresh_m13_full_master_using_exact_p2_winner(
    const m12_frozen::StagedRefLinkedSegment& staged,
    const m12_frozen::StagedExactP2Winner& accepted_p2,
    const dsp::ProcessingChain& current_protected_manual,
    std::span<const dsp::ModuleExecutionBinding> current_bindings,
    const render::RenderResult& prepared,
    core::FrameCount max_block_frames,
    double independent_m16_safety_ceiling_db_tp,
    bool real_m12_materialization_receipt_current,
    bool m11_fixed_protection_current)
{
    if(!accepted_p2.matches(
        staged,current_protected_manual,current_bindings))
        return core::Result<FreshNativeMasterCompositionProbe>::failure(
            core::Error{core::ErrorCode::InvalidState,
                "L3_M13_P2_M10_WINNER_OR_PROTECTED_OWNER_STALE"});
    return probe_fresh_m13_full_native_master_once(
        staged,current_protected_manual,current_bindings,prepared,
        max_block_frames,independent_m16_safety_ceiling_db_tp,
        real_m12_materialization_receipt_current,
        m11_fixed_protection_current,accepted_p2.winning_parameters);
}
} // namespace rgsml::app::l3_automatch::m13_frozen
