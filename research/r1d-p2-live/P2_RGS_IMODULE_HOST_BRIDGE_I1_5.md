# Streaming True Peak Limiter — I1.5: planar host bridge and Product boundary

**2026-11-12. RESEARCH-ONLY, NOT A FROZEN PRODUCT MODULE.** PO-approved visible name: **Streaming True Peak Limiter** (never “Streaming TP Limiter”). Copper waveform-under-ceiling concept in `StreamingTruePeakLimiterIconResearch.qml`. No Product UI changes.

## Verified RGS public contracts (read only)

- `dsp/include/rgsml/dsp/imodule.hpp` — `DspProcessSpec` has `audio_format`, `frame_domain_id`, `maximum_block_frames`; `DspProcessContext` has `output_frame_range`, `begins_stream`, `ends_stream`. The `IModule` virtual methods are `runtime_requirements`, `prepare`, `reset`, `process`, `finalize`, `runtime_checkpoint`, `restore_runtime_checkpoint`.
- `audio/include/rgsml/audio/audio_buffer_view.hpp` — source and destination channel planes are `std::span<const double>` and `std::span<double>`, with absolute frame ranges and timebase.
- `dsp/include/rgsml/dsp/dsp_runtime_checkpoint.hpp` — checkpoint contains algorithm, fingerprint, domain, `next_input_frame`, `backend_identity`, byte payload. The research EpochSession does not yet implement checkpoint/restore. Do not return fabricated checkpoints.
- `dsp/include/rgsml/dsp/module_descriptor.hpp` — candidate belongs to `ProcessingStage::MASTER`, `ChainSegment::MANUAL`, `PlacementClass::INLINE_CHAIN`, `ModuleCategory::DYNAMICS`; unique `type_id` candidate `rgsml.dsp.streaming-true-peak-limiter` is not frozen. It is visually and semantically distinct from M16 live *meter* and M17 *terminal limiter*.
- `app/src/dsp_chain_adapter_model.cpp` builds manual DSP entries, then analysis-only M16, then M17 terminal and Monitor service; adding a Product module is **outside this research phase**.

## Implemented in this isolated batch

`p2_planar_host_bridge.hpp` provides `PlanarHostBridge<Session, Config, EpochCode>`; the production candidate would instantiate it with `EpochSession`, `Configuration`, `EpochStatus` from I1. It does **not** create an `IModule` yet.

- Off-callback epoch begin, strict unique token, configured mono/stereo. Host must quiesce callbacks before reconfiguration.
- Each `process` converts exactly N planar binary64 frames into fixed `Frame[4096]` scratch and back, with no allocations/locks/filesystem in this bridge. It enforces disjoint planes, length, N <=4096, absolute index continuity and exact `begins_stream`/`ends_stream` lifecycle. Source frame origins are host supplied; a seek is a new epoch, never a jump inside one active epoch.
- Invalid input or mismatched context is rejected and the available output planes silenced; an old epoch token is rejected without altering state. No unvalidated frame reaches the backend.
- Explicit `finish` requires the host to provide **exactly D** output frames in the same domain at absolute source-end position; returns the delayed tail. It does not silently trim latency, invent output, reuse M16 bridge zeros or compensate the output domain behind the host's back.
- Bypass goes through the I1 Session mode (identity after latency) and has **no True Peak safety guarantee** while bypassed; M17 after Final SRC remains responsible for the final master.

## Mapping required for the *next* actual IModule subgate

1. `runtime_requirements(spec)` validates one of the six sample rates, mono/stereo and **actual manual chain frame domain**, then reports `STREAMING_CAUSAL`, actual P2 latency `D = round(Fs*0.0015) + horizon(eta) + 63`, pre/post context 0, effective tail D, no prepass. Do not confuse the internal 64-frame batch quantum with a 64-frame device callback.
2. `prepare(spec)` allocates the session + scratch off-thread and creates a fresh epoch. Source/seek epoch identity and selected bypass remain owned by the host, not guessed from `begins_stream` alone.
3. `process(input,output,context)` validates `input.format()==output.format()==spec.audio_format`, matching `timebase.frame_domain_id`, both absolute ranges equal `context.output_frame_range`, N<=`maximum_block_frames`, no overlap, and absolute frame continuity. Only then dispatch through `PlanarHostBridge`. Do not allocate even error `Status` descriptions on a true real-time callback; map status to host/control thread policy.
4. `finalize` must issue D output frames in the contract-approved output range; coordinate host latency compensation and source/tail frame origin **before** Product wiring. If existing generic host cannot supply D tail frames, integration must STOP and get an explicit orchestration decision; no truncation or synthetic invisible PCM.
5. `reset` is currently a new epoch requiring off-thread quiescence; no promise of gapless seek/loop/bypass. `runtime_checkpoint` must remain unsupported until a faithful snapshot/restore implementation is measured and verified. Do not fake checkpoint authority.
6. The Product registry, serialization/preset/undo, GUI, AutoMatch L3 and final M17 are frozen boundaries in this phase; no modification authorized. Future Product wiring requires explicit Single Writer clearance.

## Local tests and boundaries

`tests/test_p2_planar_bridge.cpp` uses a **mock epoch backend**, not the real P2 controller. It tests **864** parameterized host-flow paths (six Fs × mono/stereo × bypass/active × six programme lengths × six block sizes), plus 4096/4097-frame boundaries, stale token, invalid config transaction, wrong absolute source frame, invalid PCM, in-place/cross-plane alias, double finalize and rearm. Local GCC14 -Werror, Clang17 -Werror, GCC AddressSanitizer+UndefinedBehaviorSanitizer: 1/1 CTest PASS in each build. These tests establish only the **planar bridge's own correctness** under the mock. The supplementary `tests/test_p2_planar_real.cpp` **does** instantiate the bridge with the real GitHub-pinned I1 `EpochSession` and authentic P2 source (`p2_callback_engine.hpp`/`pinned/stream.hpp`/`pinned/music_p2.hpp`): six Fs, mono/stereo, bypass and active processing, interior/hot material, callback sizes 1/7/64/511/512/1024, byte-for-byte equality against the direct I1 epoch path, and in-memory true-peak ceiling in active mode. Both original header blobs were locally reconstructed and checked with `git hash-object` against the exact GitHub blob SHA. GCC14 scalar and SSE2, Clang17, GCC ASan/UBSan all passed 2/2 tests; scalar and SSE2 PCM FNV64 = `3f935d3414b7e93b`. **Still NOT an actual RGS IModule/product-host test.**

No Windows build, real callback, QML rendering, device deadline, FI/Q0/L3 AutoMatch or encoded PCM is asserted. Status: I1.5 host planar-mapping subgate LOCAL PASS; IModule end-to-end integration still OPEN.
