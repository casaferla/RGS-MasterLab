# RGS MasterLab — M17/M18 Final Master vs. Hardware Monitoring Rate

**Status:** PHASE 1 / CONTRACT ONLY / NOT A FUNCTIONAL SRC RELEASE  
**Date:** 2026-10-24  
**Candidate base:** `a3bb5bdfb335c716a28e84bb8371673dc2bc7bc1` (PR #56, FQ #78)  
**Branch:** `experimental/m17-m18-master-monitor-rate-split-20261024`

## Frozen ownership boundaries

- The *chosen final master sample rate* is authoritative for Final Output
  SRC, M17 True Peak Limiter, its M16 post-limiter verification, M18
  quantization/dither, and final export.
- The Windows audio interface rate is only an **audition** constraint,
  never a master-format selection input.
- A device at 48 kHz must be able to audition an independently selected
  192 kHz final master **after** a separately verified 192 -> 48 kHz
  monitoring SRC is implemented. This path must not alter the stored 192
  kHz master PCM, M17 measurement or render provenance.
- Monitoring uses a distinct ephemeral realization/ownership and must be
  audibly bound to the real device rate. Source and Loop positions require
  exact rational frame mapping between master and device clock domains.
- A mismatched device with no approved monitoring SRC must remain
  fail-closed; **never** silently retarget M17 to the device or install a
  192 kHz buffer as though it were 48 kHz.
- Master candidate verification *precedes* device monitoring SRC. M18
  affects PCM encoding; any change to exported samples requires a
  separate final True Peak acceptance rule after quantization.

## Phase 1 delivered in this branch

`audio::plan_master_monitoring_rate` is a typed rate-boundary planner,
not a resampler. It accepts all six frozen M17/M18 master rates and an
independent physical device rate, distinguishes exact-rate direct audition
from cases requiring dedicated monitoring SRC, and refuses unsupported
master targets. The Focused DSP test covers 6 master rates times 8
representative hardware rates, including 192 -> 48 and 192 -> 44.1.

No playback, export, limiter mathematics, previously approved device
activation or frozen PR #56 code paths are changed in Phase 1.

## Remaining implementation and qualification gates

1. **Final Output SRC:** implement and qualify 30 non-identity combinations
   among 44.1/48/88.2/96/176.4/192 kHz, with finite samples, antialias,
   rational duration, boundaries, mono/stereo, cancellation, and no silent
   fallback. The existing 44.1 <-> 48 kernels remain authoritative.
2. **Master-rate selection:** persist the chosen format with project /
   render settings; connect it to the M17 *master* rendering path rather
   than `TerminalPrivateProductSession::request_private_for_device`.
3. **Audition SRC:** separately handle master-rate -> actual hardware rate
   with a bounded callback, stable source/loop mapping and real sink
   ownership, independent of final-master verification.
4. **M18:** preserve selected master Fs, apply the chosen encoding depth
   and dither, then check final PCM/quantized-file Ceiling behavior.
5. **Acceptance:** PR Focused CI, dedicated 192-master/48-device fake-sink
   integration tests, then PO-driven cold Windows FQ and physical Gates
   M/L. No FQ, merge or PR #56 mutation is authorized by this document.

## Non-claims

- Passing rate-plan tests is **not** evidence that SRC to/from 192 kHz is
  implemented or meets mastering quality, and does not qualify M17/M18.
- FQ #78 remains bound to its original exact SHA. This branch cannot
  change that FQ's evidence or its result.
