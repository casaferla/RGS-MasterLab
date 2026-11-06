#pragma once
// PREPRODUCTION ONLY. Not connected to the GUI, Source/Project epoch, or
// shipping export action. A real Product must first atomically bind every
// authoritative upstream mutation to the identical single-writer epoch.
#include "../terminal_candidate_gate.hpp"
#include "../terminal_private_product_session.hpp"
#include "../master_output_rate_state.hpp"
#include "m18_owner_source_snapshot.hpp"
#include "internal/m18_product_epoch_ticket.hpp"
#include <rgsml/platform/windows/transactional_audio_exporter.hpp>
#include <rgsml/core/error.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace rgsml::app::internal {

// Consumes only an immutable M17 stage plus an UNFORGEABLE Product ticket.
// The caller MUST supply its authoritative TerminalCandidateGate AND the
// identical current staged pointer AND the actual live SOURCE reference
// (provider + locator), then capture the Product epoch ticket on the same
// Qt owner thread, without event-loop reentrancy. Alias/symlink/hardlink
// physical-file protections remain independently enforced by Windows.
// Nothing here may call a QObject or grant a FILE commit;
// the worker's first actual lease is obtained by Windows after encoding.
[[nodiscard]] inline
core::Result<platform::windows::M18TransactionalAudioExportRequest>
make_m18_checked_stage_worker_request(
    const TerminalCandidateGate& currentProductGate,
    std::shared_ptr<const TerminalCandidateGate::StagedCandidate> stage,
    const platform::windows::internal::
        M18ProductEpochTicketAuthority::Ticket& productTicket,
    const core::ResourceReference& authoritativeCurrentSource,
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
       // The Qt Product owner must provide the current SOURCE identity
       // from SourceSelectionViewModel, not the worker's own claimed path.
       ||!authoritativeCurrentSource.permissions().can_read()
       ||!source.permissions().can_read()
       ||!authoritativeCurrentSource.same_resource_identity(source)
       // Immediate logical no-overwrite preflight. Windows must still
       // independently check physical aliases (symlinks/hardlinks) and
       // final filesystem identity again at transactional publication.
       ||!destination.permissions().can_write()
       ||source.same_resource_identity(destination)
       // Product-owner thread MUST supply the exact currently staged M17
       // MASTER from the authoritative candidate gate, never a detached
       // old pointer retained after SOURCE/Project replacement.
       ||currentProductGate.staged()!=stage
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

// Product-facing preproduction overload: obtain SOURCE from the ACTUAL
// current SourceSelectionViewModel on the Qt owner thread. No worker may
// supply or reconstruct this authoritative reference from a file dialog,
// old project snapshot, or matching filename alone. The retained stage and
// epoch ticket are then checked by the shared pure request builder.
[[nodiscard]] inline
core::Result<platform::windows::M18TransactionalAudioExportRequest>
make_m18_checked_stage_worker_request_from_product_source(
    const SourceSelectionViewModel& currentProductSource,
    const TerminalCandidateGate& currentProductGate,
    std::shared_ptr<const TerminalCandidateGate::StagedCandidate> stage,
    const platform::windows::internal::
        M18ProductEpochTicketAuthority::Ticket& productTicket,
    core::ResourceReference source,
    core::ResourceReference destination,
    audio::WavSampleFormat targetFormat,
    std::optional<std::uint64_t> pcm24Seed,
    double ceilingDbTp)
{
    using R=core::Result<
        platform::windows::M18TransactionalAudioExportRequest>;
    const auto provenance=
        capture_m18_current_source_on_product_owner(currentProductSource);
    if(!provenance)return R::failure(*provenance.error());
    return make_m18_checked_stage_worker_request(
        currentProductGate,std::move(stage),productTicket,
        *provenance.value(),std::move(source),std::move(destination),
        targetFormat,pcm24Seed,ceilingDbTp);
}

// Cohesive Product-owner PREPRODUCTION entry point. Reads the actual
// selected Project MASTER rate, the real Product private session's
// current verified M17 candidate, and SOURCE from SourceSelectionViewModel
// on their common Qt owner thread. No GUI activation, no worker QoBject
// access, and no new consent; it consumes only an existing Product ticket.
// The Product must separately install all authoritative mutation gates
// before using the produced cross-thread lease in an actual export.
[[nodiscard]] inline
core::Result<platform::windows::M18TransactionalAudioExportRequest>
make_m18_checked_stage_worker_request_from_live_product(
    const TerminalPrivateProductSession& currentProductSession,
    const SourceSelectionViewModel& currentProductSource,
    const MasterOutputRateState& currentProductMasterRate,
    const TerminalCandidateGate& currentProductGate,
    const platform::windows::internal::
        M18ProductEpochTicketAuthority::Ticket& productTicket,
    core::ResourceReference requestedSource,
    core::ResourceReference destination,
    audio::WavSampleFormat targetFormat,
    std::optional<std::uint64_t> pcm24Seed,
    double ceilingDbTp)
{
    using R=core::Result<
        platform::windows::M18TransactionalAudioExportRequest>;
    if(QThread::currentThread()!=currentProductSession.thread()
       ||QThread::currentThread()!=currentProductMasterRate.thread()
       ||QThread::currentThread()!=currentProductSource.thread()) {
        return R::failure(core::Error{
            core::ErrorCode::InvalidState,
            "M18_WORKER_REQUEST_MISMATCHED_PRODUCT_OWNER_THREADS"});
    }
    // A Source re-selection at the SAME Fs still increments the actual
    // Product rate/Source revision. A retained ticket from the earlier
    // SOURCE must not borrow a newer current selection with a stale
    // M17 MASTER whose PCM happens to have an identical output rate.
    const auto bound=productTicket.identity();
    if(bound.master_rate_revision==0
       ||bound.master_rate_revision
            !=currentProductMasterRate.rate_revision()) {
        return R::failure(core::Error{
            core::ErrorCode::InvalidState,
            "M18_WORKER_REQUEST_STALE_PRODUCT_SOURCE_RATE_REVISION"});
    }
    const auto selectedRate=core::SampleRate::create(
        currentProductMasterRate.master_rate_hz());
    if(!selectedRate) {
        return R::failure(core::Error{
            core::ErrorCode::InvalidState,
            "M18_WORKER_REQUEST_INVALID_CURRENT_MASTER_RATE"});
    }
    const auto stage=currentProductSession.capture_m18_verified_stage_for_worker(
        *selectedRate.value());
    if(!stage) {
        return R::failure(core::Error{
            core::ErrorCode::InvalidState,
            "M18_WORKER_REQUEST_NO_CURRENT_VERIFIED_PRODUCT_STAGE"});
    }
    return make_m18_checked_stage_worker_request_from_product_source(
        currentProductSource,currentProductGate,std::move(stage),
        productTicket,std::move(requestedSource),std::move(destination),
        targetFormat,pcm24Seed,ceilingDbTp);
}
} // namespace rgsml::app::internal
