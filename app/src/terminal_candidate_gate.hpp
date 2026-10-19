#pragma once

#include "terminal_limiter_state.hpp"

#include <rgsml/core/realization_identity.hpp>
#include <rgsml/render/render_result.hpp>
#include <rgsml/render/true_peak_limiter_terminal_candidate.hpp>

#include <cstdint>
#include <memory>
#include <optional>

namespace rgsml::app {

// M17 is NOT yet publishable as audible PROCESSED. This gate accepts ONLY
// a completed, individually identity-checked terminal candidate for later
// product integration. It deliberately has no playback/selector publisher
// and never issues a new audible RealizationId.
class TerminalCandidateGate final {
public:
    struct Capture final {
        std::shared_ptr<const render::RenderResult> prepared_owner;
        std::shared_ptr<const render::RenderResult> manual_owner;
        core::RealizationId manual_realization_id;
        std::uint64_t ticket;
        std::uint64_t manual_chain_revision;
        std::uint64_t terminal_chain_revision;
        std::uint64_t terminal_parameter_revision;
        dsp::ModuleInstanceId terminal_instance_id;
        bool terminal_bypassed;
        double ceiling_db_tp;
        core::FrameCount full_source_frames;
        core::SampleRate output_rate;
    };

    struct StagedCandidate final {
        render::TruePeakLimiterOwnedSourceCandidate rendered;
        core::RealizationId original_manual_realization_id;
        std::shared_ptr<const render::RenderResult> prepared_owner;
    };

    [[nodiscard]] core::Result<Capture> capture(
        std::shared_ptr<const render::RenderResult> prepared,
        std::shared_ptr<const render::RenderResult> accepted_manual,
        std::optional<core::RealizationId> accepted_manual_id,
        const dsp::ProcessingChain& manual_chain,
        const TerminalLimiterState& terminal_state,
        core::SampleRate requested_output_rate);

    // Call on the owning thread AFTER the DSP worker has finished.
    // Current input pointers/ID and typed states must still be authoritative.
    // Failure never replaces a newer stage; a failed current ticket
    // invalidates that ticket. Nothing reaches the playback sink here.
    [[nodiscard]] core::Status accept(
        const Capture& capture,
        render::TruePeakLimiterOwnedSourceCandidate completed,
        std::shared_ptr<const render::RenderResult> current_prepared,
        std::shared_ptr<const render::RenderResult> current_manual,
        std::optional<core::RealizationId> current_manual_id,
        const dsp::ProcessingChain& current_manual_chain,
        const TerminalLimiterState& current_terminal_state);

    void invalidate() noexcept;
    [[nodiscard]] std::shared_ptr<const StagedCandidate> staged() const noexcept
    { return staged_; }

private:
    std::uint64_t ticket_{0};
    std::shared_ptr<const StagedCandidate> staged_;
};

} // namespace rgsml::app
