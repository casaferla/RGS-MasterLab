# RGS R1d-P2 — I1.8 fresh-epoch candidate replay, 2026-11-12

**RESEARCH ONLY; no canonical AutoMatch implementation or product merge.**

## Reason
I1.6 `ResearchP2IModule` declares runtime checkpoint unsupported. Before introducing a complex serialized controller/safety/FIR snapshot, test whether full-replay of each candidate from a fresh P2 instance is deterministic and affordable enough to *investigate* for AutoMatch. This does not authorize a change to M10/M13, 27 canonical L3 features, solver evaluation limits, Q0/Fresh Verify or M17.

## Code and local result
`tests/test_p2_fresh_evaluation.cpp` uses actual RGS `DspProcessSpec`, `AudioBuffer`, `DspProcessContext`, virtual `IModule::process/finalize` through the already verified research adapter. For each 48k/192k and mono/stereo it evaluates 5 independent fresh modules at Drive dB `[0,1.5,3,5,0]`, default research TP ceiling -1 dBTP, eta 2; 8192-frame hot synthetic source, 512-frame blocks, nonzero absolute origin 937; runtime latency 382 frame at 48k, 598 at 192k. Last A must return bit-for-bit to first A after B/C/D; Drive 5 must produce a different PCM hash. EOS is drained in exact chunks. No checkpoint/restore is invoked.

Commands compiled and **actually ran** in local Linux x86_64 container, `g++`/`clang++` C++20 strict FP (`-ffp-contract=off -fno-fast-math -frounding-math -msse2 -mfpmath=sse`): GCC scalar PASS; GCC `RESEARCH_P2_PHASE_SSE` PASS; Clang PASS. `g++` ASan+UBSan Debug PASS with no error. Each run contains 4 scenarios * 5 evaluations = **20 full fresh-evaluation sessions**. All A returns were bit-identical; reference PCM hashes exactly equal between GCC scalar/SSE2 and Clang:

| Fs | Layout | A FNV64 PCM hash |
|---|---|---|
| 48,000 | mono | `4e516365fda977c4` |
| 48,000 | stereo | `59fe782d616c45ca` |
| 192,000 | mono | `ea940d64e2537030` |
| 192,000 | stereo | `17de6fe19ab4d15f` |

Wall-clock samples are printed in the test per candidate; **not** statistically stable, not WCET, and not representative of full-song/full-solver cost. E.g. GCC Release measured maximum process time across five 8192-frame candidates: 2.95 ms/48k mono, 4.54 ms/48k stereo, 2.74 ms/192k mono, 4.87 ms/192k stereo on this container; these are measurements, not promises for Windows or different CPU. Sanitizer times are much larger and not performance evidence. Windows WASAPI 512 callback and full AutoMatch canonical costs remain untested in I1.8.

## Decision constrained by evidence
**No state checkpoint is required for deterministic full-resource fresh replay in the synthetic I1.8 scenario.** This does not prove AutoMatch will meet its time budget for hundreds of full-length renders. Select fresh-epoch candidate replay for the FIRST *isolated* AutoMatch integration candidate; benchmark representative track durations and caching only when canonical solver exists. Do not block manual GUI preview on the missing AutoMatch checkpoint.

## Exact preview gate, not an ETA
1. **Product preview branch with Single-Writer clearance**: isolated Product `ModuleRegistry` factory and parameter codec, `MasteringChainState` authority, `ModuleExecutionBinding`/actual audio preview route, `DspChainAdapterModel`, user-visible QML editor/title/icon, latency + compensated transport + click-safe parameter/bypass state. No effect may appear as a working DSP if its audio is unconnected.
2. **Windows evidence**: Qt startup and 1184x688 layout, native x64 CI for the candidate plus real audio preview and finalization, M16 output TP check. Only then make a preview build available to PO. M17 remains final terminal; no Product FQ/merge without separate PO GO.
3. **Later quality**: canonical AutoMatch 27-feature and real full-track cost; persistence/presets, M13 Fresh Verify, mono/stereo/6Fs, device timing on target WASAPI. These are not prerequisites to a clearly marked isolated manual preview unless an actual hard safety/transport fault is found.

CURRENT STATUS: I1.8 local fresh epoch PASS, Product preview NOT yet wired, no Windows/Qt run, no main/PR L1/M17 modification, no CI/FQ.
