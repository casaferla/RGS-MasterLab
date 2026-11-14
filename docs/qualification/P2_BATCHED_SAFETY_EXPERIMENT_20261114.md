# P2 64-frame safety submission experiment — 2026-11-14

**STATUS: RESEARCH CANDIDATE ONLY. NOT PRODUCT, NOT VERIFIED SPEEDUP,
NOT CERTIFIED PHYSICAL CALLBACK / NOT FQ.**

## Baseline and motivation

Measured through the research CI #43630606330 (7/7 SUCCESS), on
the 2-second, quiet, two-sample-rate synthetic test:

| Runner | Rate | Baseline CallbackEngine | PredictiveMusic | Stream safety |
|---|---:|---:|---:|---:|
| Ubuntu 24.04 P2 focused | 44.1k | 45.02 ms | 5.78 ms | 39.40 ms |
| Ubuntu 24.04 P2 focused | 48k | 49.02 ms | 6.30 ms | 42.89 ms |
| Win2022 P2 focused | 44.1k | 52.33 ms | 8.45 ms | 41.39 ms |
| Win2022 P2 focused | 48k | 56.92 ms | 9.20 ms | 45.00 ms |
| Win2022 GUI smoke | 44.1k | 41.37 ms | 6.32 ms | 33.28 ms |
| Win2022 GUI smoke | 48k | 45.14 ms | 6.85 ms | 36.35 ms |

The component times do not sum exactly to the baseline due to separate
instances, call boundaries and timestamp overhead. CI runners are not
the PO's Windows PC, nor an accurate long-run or physical timing guarantee.

## Hypothesis and implementation

The original `CallbackEngine` sends every post-predictive musical sample
individually to pinned `Stream::submit(span<Frame>{&frame,1})`, while
`Stream` natively accepts up to 64 samples and commits output in
64-frame quanta. This may produce repeated preflight and small-array
setup overhead.

An **independent, compile-gated research-only**
`CallbackEngine::process_batched` experiment accumulates up to 64
post-predictive frames and submits them in one call to the UNMODIFIED
`Stream` math. It drains the FIFO after each raw 64-frame chunk to
respect the existing bounded memory limit and output transport timing.
The default `process` is unchanged and is independently instantiated
as a fresh original PCM oracle.

The macro `RESEARCH_P2_BATCHED_CALLBACK` is compiled ONLY into
`r1d_p2_batched_safety_perf_probe` by its isolated research CMake target.
It is **not** compiled into Product DSP/Qt, regular P2 IModule, tests,
M16/M17, Final SRC or `main`. All edits preserve pinned `music_p2.hpp`
and `stream.hpp`, meaning the TP post-gain calculation remains
physically identical C++ source.

## Gate and measurement

The next quick CI gate covers six 44.1–192k rates, mono/stereo, seven
quiet/normal/hot/Nyquist12/Nyquist24/sparse-impulse24/amplified16-drive18
synthetic excitations, seven callback block
sizes (1, 7, 64, 511, 512, 1024, 4096): **588 full-program and EOS
bitwise comparisons**, plus independent post-gain ITU 4x True Peak
measurement across all emitted PCM and the final 11 FIR samples.
Callbacks shorter than 64 frames revert to the unchanged per-sample
path to avoid measured short-block batching overhead. Every experiment
must pass; no numerical tolerance, fingerprint-only comparison or
ignored negative tests.
Physical standalone `Run_P2_Batched_Safety.cmd` measures 300s stereo
at 44.1/48kHz, three excitation levels, blocks 512 and 4096; writes
`bin/P2_BATCHED_SAFETY_5MIN.tsv`.

The previous **336-case** quick CI #43632611322 completed 7/7 SUCCESS,
and all 336 cases preserved the exact PCM. The 1-second per-case
timings are preliminary, CI-runner-specific, and the new 588-case TP
gate is not yet qualified. Across those 336 cases, median runtime
reductions for block sizes 64–4096 on the independent original/experiment
kernel pair were approximately 6.2% Ubuntu, 9.4% Windows focused,
and 7.0% Windows GUI smoke. One-frame blocks were sometimes slower,
which motivates the adaptive fallback. No claim of physical speedup
is made until the original/experimental paths are timed on the PO
computer, ideally repeated with counterbalanced run order.

Even a positive result must be validated on actual music and additional
adversarial fixtures (including REPAIRED/projection paths), worker
epoch/seek/loop, 1–4096 block segmentation, all six rates and the
physical Windows device. Neither P2 musical behavior nor M17 terminal
limiting can be silently changed to improve performance.

**STOP** if the benchmark fails, its PCM differs, a rare safety path
isn't covered, or a performance delta is inconclusive. Do not merge
or enable the experimental path in Product.

Priority remains completion of L1 M24, minimum L2 bridge, L3 AutoMatch.
