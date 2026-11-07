#pragma once

#include "terminal_limiter_state.hpp"

#include <rgsml/core/realization_identity.hpp>
#include <rgsml/render/render_result.hpp>
#include <rgsml/render/true_peak_limiter_terminal_candidate.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

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
        // Captured from the *accepted* M17 job, not reconstructed from the
        // current UI. Ceiling A->B->A still advances this revision and
        // must NOT re-authorize an older binary64 terminal MASTER.
        std::uint64_t accepted_terminal_parameter_revision{0};
        double accepted_ceiling_db_tp{0.0};
    };

    struct StagedMeasuredCandidate final {
        render::TruePeakLimiterOwnedMeasuredNonconformingCandidate rendered;
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

    // Retention gate only, NEVER grants playback, verification or export.
    [[nodiscard]] core::Status accept_measured(
        const Capture& capture,
        render::TruePeakLimiterOwnedMeasuredNonconformingCandidate completed,
        std::shared_ptr<const render::RenderResult> current_prepared,
        std::shared_ptr<const render::RenderResult> current_manual,
        std::optional<core::RealizationId> current_manual_id,
        const dsp::ProcessingChain& current_manual_chain,
        const TerminalLimiterState& current_terminal_state);
    // M18 opt-in Product epoch interface, NOT installed in production.
    // This status-returning entry allows Product to reject a destructive
    // candidate invalidation while a worker holds the exact commit epoch.
    // Legacy M17 invalidate() remains separate until all call sites are
    // upgraded to honor BUSY; do NOT enable async export before then.
    using M18MutationAdmission=std::function<core::Status(const char*)>;
    void set_private_m18_mutation_admission(M18MutationAdmission guard)
    {
        // A callback cannot erase the very authority whose epoch is
        // being consumed, then recursively mutate a checked M17 stage.
        if(std::this_thread::get_id()!=ownerThread_)return;
        if(m18AdmissionActive_)return;
        m18MutationAdmission_=std::move(guard);
    }
    [[nodiscard]] core::Status try_invalidate_for_m18();
    [[nodiscard]] bool m18_owner_thread_current() const noexcept {
        // This identity test is safe on another thread because the owner
        // thread ID is immutable for this candidate's entire lifetime.
        return std::this_thread::get_id()==ownerThread_;
    }
    [[nodiscard]] bool m18_mutation_admission_installed() const noexcept {
        return static_cast<bool>(m18MutationAdmission_);
    }
    void invalidate() noexcept;
    [[nodiscard]] std::shared_ptr<const StagedMeasuredCandidate>
    staged_measured() const noexcept {
        // std::shared_ptr stage publication is owner-thread serialized,
        // NOT an atomic reader port. A detached M18 export worker must
        // consume the retained immutable capture, never touch this gate.
        if(std::this_thread::get_id()!=ownerThread_)return {};
        return stagedMeasured_;
    }
    [[nodiscard]] std::shared_ptr<const StagedCandidate> staged() const noexcept
    {
        if(std::this_thread::get_id()!=ownerThread_)return {};
        return staged_;
    }

private:
    // Only the status-returning M18 path may call the actual reset while
    // an export epoch is configured. Legacy void invalidation is disabled
    // rather than silently discarding a worker-owned verified candidate.
    void invalidate_unchecked() noexcept;
    [[nodiscard]] core::Status admit_m18_candidate_mutation(
        const char* operation) const;
    const std::thread::id ownerThread_{std::this_thread::get_id()};
    M18MutationAdmission m18MutationAdmission_;
    mutable bool m18AdmissionActive_{false};
    std::uint64_t ticket_{0};
    std::shared_ptr<const StagedCandidate> staged_;
    std::shared_ptr<const StagedMeasuredCandidate> stagedMeasured_;
};

} // namespace rgsml::app
