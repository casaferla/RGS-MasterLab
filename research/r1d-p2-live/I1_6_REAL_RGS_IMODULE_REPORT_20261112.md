# RGS MasterLab — Streaming True Peak Limiter — I1.6 real IModule ABI research

2026-11-12. **Nonproduct / no merge / no M17 / no Product registry or QML wiring.**

## What was actually built

- `research_p2_imodule.hpp`: concrete class `ResearchP2IModule` **deriving from the real** `rgsml::dsp::IModule`. It implements `descriptor`, `runtime_requirements`, `prepare`, `reset`, `process` and `finalize` against RGS `DspProcessSpec`, `DspProcessContext`, `AudioBufferView` and `MutableAudioBufferView`. Checkpoint/restore retain inherited **unsupported** behavior, instead of publishing fabricated DSP state.
- `p2_planar_host_bridge.hpp`: the only existing I1.5 file changed. Added the no-allocation `bind_first_absolute_frame` method to accept nonzero host frame origins on the *first* callback without calling P2 `prepare` inside audio processing. Original P2 engine and pinned headers remain unmodified.
- `tests/test_p2_real_imodule.cpp`: links and invokes actual RGS core/audio/DSP implementations, including `IModule` virtual dispatch and owning, aligned, planar `AudioBuffer`. Test build has **no Qt**, no `ModuleRegistry` changes, no Product audio route.
- `CMakeLists.txt` receives one independent `r1d_p2_real_imodule_tests` CTest target and a scoped research-only library compiling the actual unmodified RGS source files from repo root (`core/src`, `audio/src`, `dsp/src`) rather than copying or forking them.

## RGS ABI and audio semantics verified

- **Descriptor candidate:** `rgsml.dsp.streaming-true-peak-limiter`, display name `Streaming True Peak Limiter`, Dynamics/Master/Manual/Inline; distinct from M16 and frozen M17, not registered, schema not frozen.
- **Domain:** accepts `SOURCE_PROCESSING_RATE` only for the MANUAL DSP proof. Other domains rejected. Six original P2 Fs and mono/stereo accepted; `maximum_block_frames` 1..4096. No inference of portable 64-frame audio device suitability.
- **Latency:** algorithmic delay `D=round(Fs*0.0015)+horizon(eta)+63`, with 2% baseline; `effective_tail_frames=D`, predictive lookahead reported as `D-63` with the batch component not falsely counted as lookahead. For eta2, D=382 frames at 48 kHz, D=598 at 192 kHz. The **host must compensate D frames**. Bypass retains same D, but no ceiling guarantee when bypassed.
- **RT path:** after off-thread `prepare`, `process` does bounded planar->Frame->planar conversion and carries stable epoch token without heap, lock, IO, logging or Qt. Input/output must be disjoint. First actual frame origin can be nonzero; subsequent blocks must be contiguous and range/timebase exact. Invalid / nonfinite PCM causes silent failure and does not resume until reset.
- **EOS:** supports both final input block marked `ends_stream=true` and the **actual M17-style convention** of `process(ends_stream=false)` followed by `finalize`. In the latter case, a zero-frame bridge operation marks EOS without pushing synthetic samples into P2. The pinned P2 safety and controller finish exactly once into D frames of preallocated tail memory. `IModule::finalize` can then return D in **arbitrary valid chunks <= maximum_block_frames**, with frame range `[origin+N,origin+N+D)` and `ends_stream` only on the last chunk; no output from M16's analyzer-only FIR bridge.
- **Reset/seek:** reset must be off callback with quiescent audio host. It constructs a new research epoch and discards old delayed PCM; not click-free and not checkpoint-equivalent. No Product transport coordination or crossfade is claimed.
- **Errors:** `core::Status` failures carry only `ErrorCode` on callback paths to avoid dynamic error string construction. Product diagnostic enrichment should take place off callback. No fake checkpoint/preset or AutoMatch compatibility claimed.

## Actual local evidence

The P2 research sources were unchanged. RGS ABI header/implementation files from baseline main `304f593c89e7079aefa31727055b0880243245e4` were transferred from GitHub and restored byte-exact: **all 19 Git blob SHAs matched** before compiling. The test links directly to their genuine implementations (no mock type adapter).

The local test exercised **6 Fs × mono/stereo × 4 block sizes (1,7,64,512) × 3 cases (active interior, active hot, bypass hot) = 144 real ABI scenarios**, including input origin 97, both EOS conventions, direct real `EpochSession` vs real ABI `IModule` raw delayed PCM **bitwise equality**, native M16 in-memory true-peak <= ceiling on active, delayed bypass, chunked finalize and rejection of double finalize. Additional tests: wrong frame domain, off-thread reset with origin 41, checkpoint explicitly unsupported, nonfinite PCM silent/fault, aliased buffer silent/fault. Callback test witness counted **zero heap allocations in process/finalize success paths**.

| Toolchain | Build/CTest | PCM fingerprint |
|---|---|---|
| GCC 14.2 Release strict FP scalar | 1/1 PASS | `7ca1160a8000fca5` |
| GCC 14.2 Release strict FP SSE2 | 1/1 PASS | `7ca1160a8000fca5` |
| Clang 17 Release strict FP | 1/1 PASS | `7ca1160a8000fca5` |
| GCC 14.2 Debug ASan+UBSan | 1/1 PASS | `7ca1160a8000fca5` |

The local independent CMake project linked against the same actual RGS sources. A second independent CMake build **using repository-relative paths** passed 1/1 CTest and matched the same PCM fingerprint.

## Remaining blockers — explicitly open

This is the real **ABI compatibility subgate**, not an actual RGS `ProcessingChain`/Product route gate. Not completed or claimed: Product ModuleRegistry / serializer / UI, Live transport quiescence and click-free crossfades, exact runtime_checkpoint/restore semantics for reusable AutoMatch evaluations, encoded PCM M16 / Final SRC / M17 qualification, Windows MSVC or physical WASAPI test, portable EXE for the organisation-managed PC, GUI icon rendering at 16px/64px, CI/FQ or merge. The approved waveform-under-ceiling Copper QML remains research-only. No consumer of these code paths is enabled in Product. No Product GO.
