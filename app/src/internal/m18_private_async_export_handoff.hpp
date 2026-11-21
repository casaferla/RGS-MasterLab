#pragma once
// M18 Product-owner-thread to Qt-free Windows WORKER handoff.
// Invoked through the main.cpp-owned private action and UI ViewModel,
// never directly from QML. All SOURCE/Project/MASTER mutation gates
// MUST share one epoch, and explicit user consent must be current.
// No QObject, QPointer or mutable Product session crosses threads.
#include "m18_checked_stage_worker_bridge.hpp"
#include "internal/m18_async_export_worker.hpp"

#include <rgsml/core/result.hpp>
#include <memory>
#include <optional>
#include <utility>

namespace rgsml::app::internal {

// One-shot typed bridge: capture the current M17 Master / SOURCE / Project
// rate ON the Product Qt thread and immediately launch a detached worker.
// A failed preflight MUST NOT start the worker or touch the output path.
[[nodiscard]] inline core::Result<std::shared_ptr<
    platform::windows::internal::M18AsyncExportWorker>>
launch_m18_async_export_from_live_product(
    const TerminalPrivateProductSession& session,
    const SourceSelectionViewModel& source,
    const MasterOutputRateState& rate,
    const TerminalCandidateGate& gate,
    const platform::windows::internal::
        M18ProductEpochTicketAuthority::Ticket& ticket,
    core::ResourceReference requestedSource,
    core::ResourceReference destination,
    audio::WavSampleFormat format,
    std::optional<std::uint64_t> seed,
    double ceilingDbTp)
{
    using Worker=platform::windows::internal::M18AsyncExportWorker;
    using R=core::Result<std::shared_ptr<Worker>>;
    auto request=make_m18_checked_stage_worker_request_from_live_product(
        session,source,rate,gate,ticket,
        std::move(requestedSource),std::move(destination),
        format,seed,ceilingDbTp);
    if(!request)return R::failure(*request.error());
    // The request retains immutable M17 stage ownership, actual SOURCE
    // identity and a Qt-free, revocable Product epoch lease factory.
    // Worker code never captures one of the QObject references above.
    return Worker::launch(std::move(*request.value()));
}

} // namespace rgsml::app::internal
