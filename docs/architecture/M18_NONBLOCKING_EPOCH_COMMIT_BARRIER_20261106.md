# RGS MasterLab — M18 Nonblocking Export Epoch Commit Barrier (PREPRODUCTION)
Date: 2026-11-06
Status: isolated source + deterministic Windows native tests. NOT Product integrated, NOT FQ.

## Problem
M18 PCM24/Float32 filesystem export works in the canonical TransactionalAudioExporter, including delivered-WAV M16 TP and rollback. The existing Product-to-M18 bridge revalidates QObject state on the Qt owning thread; it may NOT be called from a long-running worker. Calling the synchronous transaction on Qt for an entire master would freeze UI. Simply retaining an M17 immutable buffer is not commit authority.

## First guarded primitive
`platform/windows/src/internal/m18_epoch_commit_barrier.hpp` provides a C++ mutex-guarded, Qt-independent `M18EpochCommitBarrier` for a Product-issued monotonic epoch, with (1) `try_acquire(expectedEpoch)` yielding a movable `M18ExportCommitLease` accepted by the canonical M18 exporter, (2) nonblocking `try_advance_epoch(expected,next)` and (3) nonblocking `try_retire(expected)`. An active lease reserves an epoch across destination rename and final encoded-file readback. The mutex is held ONLY for brief state transitions/checks, never across the actual I/O or a Qt callback; no event-loop pumping or worker QObject access. A second commit, stale epoch, revoke or retire while commit-active fails closed; after RAII lease retirement the owner may advance; previously issued tokens cannot reacquire. No default factory or bypass.

## Mandatory integration invariant (NOT implemented yet)
The Qt Product session must assign a monotonic epoch with immutable Source/Project/selected-MASTER identity and must funnel *every* relevant mutation, reset, reprepare, retirement, replacement or Source selection through the same barrier BEFORE changing observable state. It MUST honor rejected/active transitions by queueing or refusing them on the UI thread, never silently changing the owner. The captured immutable Master is retained through the worker. Approval/consent must be captured on Qt thread and represented in a revocable Product-owned state that is safe to inspect on a worker; the current synchronous QPointer consent callback MUST NOT be repurposed cross-thread. Epoch and identity need authorization at the start and immediately before commit; actual source FILE identity must be checked separately by the existing exporter.

## Safety / limits
This first class is a FOUNDATION ONLY. It does not claim that Product's mutations are registered; using it while bypass mutations exist would be UNSAFE and is prohibited. There is no QML activation, default M18 output, project schema write or cross-thread dispatch. If an active commit blocks a Product mutation, caller must return a deterministic BUSY result and optionally enqueue, not wait on the Qt event loop. Lease must be held through final readback or rollback, not merely the rename. Avoid an ABA epoch reuse. Document sequencing and cancellation in future product integration.

## Regression
Existing Windows M18 candidate-delivery test gains `crossThreadEpochBarrierPreventsCommitRaceWithoutQtBlocking`: std::thread acquires lease, owner tries concurrent acquire/advance/retire (all denied), worker validates/releases, owner advances; old ticket denied, new ticket admitted, retirement denies during active commit and succeeds after release. The worker is joined before any Qt assertion. No new Focused target/CI workflow; Linux focused remains unchanged. This is an isolated prequalification test, not FQ.
