# RGS R1d-P2 — I1.7 genuine ProcessingChain/ModuleRegistry isolated proof

**Date:** 2026-11-12. **Gate:** targeted I1.7 RESEARCH ONLY, not Product I2, no main/CI/FQ/merge/GUI, no alterations to M17 or frozen contracts.

## Key architectural finding

RGS `ProcessingChain` is a **metadata and ordering/revision container**. It does **not** execute audio or own `IModule` instances. DSP execution is a distinct host responsibility. The research test therefore exercises **both real components**:

1. A genuine `ModuleRegistry` created via the unchanged `ModuleRegistry::create(std::vector<ModuleRegistration>)` with a **local research-only registration set** (Compressor, Stereo/M-S, live P2, terminal M17 placeholder). No change to `ModuleRegistry::create_dsp_package_v1()`; no production registration. Factory for P2 instantiates the actual I1.6 `ResearchP2IModule`.
2. A genuine `ProcessingChain` using the unchanged original `dsp/src/processing_chain.cpp`, with `MASTER/MANUAL` context. A test host selects the active live-P2 `ModuleInstance` from the chain, asks the real registry factory for `IModule`, then dispatches real `AudioBuffer` sample blocks through `prepare/process/finalize`. The resulting full delayed PCM is compared **bit for bit** against the original pinned P2 `EpochSession` (not a mock).

The test registration contains M17 only as an **uninstantiable TERMINAL descriptor placeholder** to verify domain separation; it does not create or execute M17.

## Facts tested

- P2 has the approved display name **Streaming True Peak Limiter**, candidate type ID `rgsml.dsp.streaming-true-peak-limiter`, `MASTER/MANUAL/INLINE_CHAIN/DYNAMICS`, `must_be_last=true`, `duplicable=false`, `single_active_instance=true`. These are **provisional research descriptor fields**, not Product-frozen metadata.
- Add Compressor, Stereo/M-S then P2 at final index; moving P2 ahead or adding an audio processor after P2 fails atomically without revision mutation. Duplicating P2 fails.
- User bypass state toggles `ModuleInstance::active()` and changes the chain revision. This is **metadata**, not itself the runtime P2 bypass implementation nor host latency compensation.
- M17 is permitted in a **separate TERMINAL** chain and P2 is refused there. M17 is refused in the MANUAL chain; failed actions preserve the chain revision.
- Chain-selected registry factory creates the genuine `ResearchP2IModule` via `ModuleRegistry::create_module`. At 48 kHz and 192 kHz, mono and stereo, a hot synthetic signal of 1,009 frames and callbacks of 73 frames, at absolute origin 1309, the complete output (process + chunked D-frame finalize) is bitwise identical to direct original P2 session. `D=382` frames at 48k / `D=598` at 192k for the eta 2% baseline.
- This small focused I1.7 proof does not repeat the broader I1.6 sample-rate matrix (all 6 Fs), which remains separately available unchanged.

## Actual local build/test results

The local mirror contains baseline `main` 304f593c89e7079aefa31727055b0880243245e4 source semantics, transferred from GitHub. CMake compiles the **original** RGS `processing_chain.cpp`, `module_registry.cpp`, `module_descriptor.cpp`, `imodule.cpp`, audio and core support; only test registration and P2 are research-only. Local transfer to the testing container introduced blank-line normalization in some source files; therefore this report does **not claim byte-for-byte Git blob hashes for the temporary mirror**. The code committed into GitHub's research branch references the original files untouched via repo-relative paths.

| Toolchain | Focused CTest | Outcome |
|---|---|---|
| GCC 14.2 Release, strict FP scalar | 1/1 | PASS |
| GCC 14.2 Release, SSE2 phase | 1/1 | PASS |
| Clang 17 Release strict FP | 1/1 | PASS |
| GCC 14.2 Debug, ASan+UBSan | 1/1 | PASS, **limited sanitizer scope** |

All four executable stdout SHA256 = `ead1981042d441d69266b16bdb97dc392002b827366b49af8a45da287643a054`.

**Sanitizer nuance (must remain visible):** The unreferenced Product built-in DSP factories in the original `module_registry.cpp` cause linker/typeinfo dependencies when UBSan instruments unused factory/destructor code, although the targeted research execution does not use them. To compile a small isolated target without linking the entire frozen DSP package, this **one translation unit** is built with `-fno-sanitize=all` under GCC; the test executable, P2 code, real ProcessingChain and remaining RGS sources stay ASan+UBSan instrumented. Full sanitizer coverage of Product registry code is **not claimed**. The first all-sanitized registry link failed with unresolved unrelated Product symbols, not a P2 runtime test failure.

## Open gates and STOP conditions

- This is a **test-host wiring proof**, not Product host scheduler/runtime rendering. The actual GUI, DSP module selection, latency compensation and QML icon are not wired. No Product `create_dsp_package_v1` edits, changes to M17, settings/presets, M10/M13 or frozen format.
- AutoMatch `runtime_checkpoint()` and restore **remain unsupported**. Conservative first implementation may evaluate each canonical candidate from a **fresh, deterministic initial epoch**, without inventing P2 snapshots. Evaluate actual performance before designing checkpoint serialization. No integration into AutoMatch or claims of solver qualification.
- No real click-free crossfade/seek/loop handoff, no Windows/MSVC/WASAPI callback measurement, no encoded PCM/M16 Final SRC evidence, no PC-organisation executable yet.
- Before Product I2, confirm Single-Writer release with L1 and verify a focused end-to-end host render (input P2 -> latency-compensated output -> Final SRC -> M17) without modifying M17.
- `must_be_last` is a hard manual-chain constraint in this test; future DSP should be inserted before P2. In the true host, when P2 is bypassed, latency mapping must still be addressed explicitly.

**Status: I1.7 isolated real-chain/registry gate PASS local. Product integration / AutoMatch / Windows still OPEN.**
