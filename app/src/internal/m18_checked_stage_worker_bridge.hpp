#pragma once
// PREPRODUCTION ONLY. Not connected to the GUI, Source/Project epoch, or
// shipping export action. A real Product must first atomically bind every
// authoritative upstream mutation to the identical single-writer epoch.
#include "../terminal_candidate_gate.hpp"
#include "internal/m18_product_epoch_ticket.hpp"
#include <rgsml/platform/windows/transactional_audio_exporter.hpp>
#include <rgsml/core/error.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace rgsml::app::internal {

// Consumes only an immutable M17 stage plus an UNFORGEABLE Product ticket.
// The caller MUST capture the exact currently verified Product stage on its
// Qt owner thread. Nothing here may call a QObject or grant a FILE commit;
// the worker's first actual lease is obtained by Windows after encoding.
[[nodiscard]] inline
core::Result<platform::windows::M18TransactionalAudioExportRequest>
make_m18_checked_stage_worker_request(
    std::shared_ptr<const TerminalCandidateGate::StagedCandidate> stage,
    const platform::windows::internal::
        M18ProductEpochTicketAuthority::Ticket& productTicket,
    core::ResourceReference source,
    core::ResourceReference destination,
    audio::WavSampleFormat targetFormat,
    std::optional<std::uint64_t> pcm24Seed,
    double ceilingDbTp)
{
    using Request=platform::windows::M18TransactionalAudioExportRequest;
    using R=core::Result<Request>;
    const auto id=productTicket.identity();
    if(!id.valid() ||productTicket.epoch()==0 ||!stage
       ||!stage->prepared_owner ||!stage->rendered.manual_owner
       ||stage->original_manual_realization_id.value==0
       // Zero is an admissible initial M17/MANUAL chain revision. The
       // next equality check pins it exactly, including zero.
       ||stage->rendered.manual_chain_revision!=id.chain_revision
       ||stage->rendered.output_rate.value()!=id.master_rate_hz
       ||!stage->rendered.terminal.checked.final_check
       ||stage->rendered.terminal.checked.terminal.bypassed
       ||stage->rendered.terminal.checked.terminal.compensated_pcm
            .view().format().sample_rate()!=stage->rendered.output_rate) {
        return R::failure(core::Error{
            core::ErrorCode::InvalidState,
            "M18_WORKER_PREPRODUCTION_M17_VERIFIED_STAGE_OR_EPOCH_MISMATCH"});
    }
    // Aliasing shared_ptr retains exact M17 PCM WITH its immutable stage:
    // no second full render, allocation, worker Qt callback or source copy.
    const auto* pcm=
        &stage->rendered.terminal.checked.terminal.compensated_pcm;
    std::shared_ptr<const audio::AudioBuffer> retained(stage,pcm);
    return Request::create(
        std::move(source),std::move(retained),std::move(destination),
        targetFormat,pcm24Seed,ceilingDbTp,
        platform::windows::ExportCollisionPolicy::FAIL_IF_EXISTS,
        productTicket.commit_lease_factory());
}
} // namespace rgsml::app::internal
