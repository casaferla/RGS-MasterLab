# R1d-P2 I1 — Epoch/session and transport contract (research-only)

**2026-11-12; nonproduction, no frozen contract change.** The existing `p2_callback_engine.hpp` is unchanged. `p2_epoch_session.hpp` is a host-facing transport research wrapper, **not** an `IModule`, not a Qt playback binding, and not a replacement for M17.

## Epoch lifecycle

- `begin_epoch(token, config, bypassed)` **off the callback thread only**. Valid tokens are nonzero, strictly monotonically increasing `uint64_t` values. A valid next instance is fully constructed before publishing; an invalid attempted reconfiguration preserves the old active epoch. It does not reuse the P2 controller/M16 history.
- `process(token, input, output)` **inside the callback**. Lengths must match and be <=4096 frames. Valid tokens must identify the current epoch. Stale callbacks emit all-zero output, return `stale_epoch`, and do not change counters or history. Invalid PCM / span / frame limits fault the active epoch and emit silence; the host may only recover by beginning a new epoch off-thread.
- `finish(token, tail)` requires exactly `latency_frames()` frames at EOS. `N` inputs emit `N` callback outputs plus `D` tail outputs, where the first `D` output frames are intentional latency prefill; the compensated programme has exactly `N` frames. Double finish is rejected and silenced. Analyzer-only FIR bridge zeros are not emitted.
- `cancel_epoch(token)` is OFF-thread and requires all callbacks on this instance to be quiescent; it discards the delayed tail instead of publishing frames from an abandoned source. No token can be reused after cancel.
- **Concurrency precondition:** no `begin_epoch`, `cancel_epoch` or destruction may race with `process` or `finish`; the host must implement an explicit stop/barrier/hand-off. The wrapper has no locks or hazard-pointer protocol and provides NO race-freedom under concurrent host calls.
- **Audio seam precondition:** an abandoned epoch may cause an audible discontinuity. The host must mute/crossfade at source, seek, loop-wrap, sample-rate, channel layout, ceiling, eta, drive or bypass changes. This phase does NOT certify click-free switching and does not implement an automatic crossfade. No control update is made retroactively to PCM already emitted.

## Bypass semantics

- A session is born as active P2 **or** bypass. Bypass passes every valid sample bit-exact at the same deterministic delay as P2 at the same Fs/eta; it does *not* hard-limit and does *not* provide a True Peak guarantee. Mono's unused second frame lane is zero. Bypass changes require a new epoch and explicit host handoff, not mutation inside the callback.
- The delayed bypass ring is fixed at 4096 stereo frames. All memory allocation or deallocation is confined to off-thread `begin_epoch`/`cancel_epoch`. The callback is synchronous, bounded and requires no heap, locks, Qt events, filesystem or logs.
- The hard ceiling remains the independent final M17 after Final SRC. A bypassed in-chain P2 intentionally cannot be cited as a guarantee of the master ceiling.

## Host/IModule gaps (NOT claimed done)

1. No `IModule::prepare/process/finalize/runtime_checkpoint` adapter or Product `ModuleRegistry` registration. No runtime checkpoint serialization. A seek/loop is currently implemented only as **fresh epoch restart**, not exact checkpoint/restore or gapless looping.
2. No real Qt transport callback synchronization, epoch barrier, read/write memory-order protocol, session swap or crossfade. No Windows device callback or ETW measurement in I1.
3. No live parameter automation, smoothing, persistence or parameter codec; these need separate Product and AutoMatch mapping once upstream Single-Writer stage is free.
4. Tests measure the **double-precision in-memory PCM**. Final exported/encoded PCM M16, L3/Q0/Fresh Verify and actual AutoMatch integration remain separate gates.
5. Build uses Linux strict FP and same pinned 4x M16 FIR. No claim of universal 64-frame-device qualification; the existing device proof is 192k/512-frame native WASAPI exclusive only.

## Evidence and acceptance

- Reproducible tests in `tests/test_p2_lifecycle.cpp`, target `r1d_p2_lifecycle_tests`, added alongside the unchanged I0 `r1d_p2_callback_tests`.
- Test matrices: six Fs, mono/stereo, input lengths 0/1/5/121/512/1697, callback blocks 1/7/64/511/512/1024; active session bit-exact with direct I0 engine; bypass bit-exact with source after delay; in-memory M16 peak <= ceiling; stale callbacks and cancel, invalid configure preserving active session, loop/seek/reconfigure and reset of delayed history, malformed/NaN/Inf, single EOS and explicit latency accounting, allocation witness after prepare at 192k stereo/512-frame callbacks for both modes.
- `-O2` strict FP scalar vs SSE2 phase-target emitted byte-identical stdout; separate ASan + UBSan build/test passed. See `I1_LOCAL_TEST_RECEIPT_20261112.md` for reproducible commands and evidence boundaries.

**Status:** I1 *epoch/transport wrapper research subgate* locally passing; entire Product I1 gate remains OPEN until real integration and Windows verification. No PR, CI, FQ, merge, or change to M17/M16 meter/main/Single-Writer L1 is authorized by this document.
