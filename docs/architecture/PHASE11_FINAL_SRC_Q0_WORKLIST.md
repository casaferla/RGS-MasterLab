# PHASE 11 FINAL SRC — Q0 mathematical preflight and pending MPFR/Ziv work

**2026-10-28 · L1-M17 prerequisite · Status: STAGING CANDIDATE / Q0 ONLY / NOT RELEASE QUALIFIED**

Authority (read full originals, not the older abbreviated filename for source A):
- [Phase-11 Final Pipeline and SRC Deltas](https://drive.google.com/file/d/1uJdr2WLzSOtrxx9JqY0JwWduSPNfu5oJ/view)
- [Phase-11 Final SRC Integrity Micro-Delta](https://drive.google.com/file/d/1ybdzxVJ1P8Mft82dADv_47sFO_pKg7hL/view)
- [M17 Master Output contract](https://docs.google.com/document/d/13fIk06CgiPLWcaW3KAZPCUoNFN5ffh6NHlVXK1sfA_8/edit)
- [M17 P0 owner/physical gate](https://github.com/casaferla/RGS-MasterLab/issues/62)

## Implemented in THIS Q0 staging lot

`tools/qualification/final_src/phase11_math_preflight.py` is intentionally
a **read-only, stdlib-only mathematical qualification tool**, NOT an alternate
production resampler and NOT a floating coefficient generator.

1. A single canonical six-rate 6x6 planning matrix: 6 identity paths,
   30 non-identity conversions and **14 unique reduced L/M ratios**.
2. **Exact rational** proof of the frozen Kaiser T/N/D shape for each distinct
   ratio, including the entire 256-bit+ pi sensitivity problem for the
   integer `ceil(orderEstimateInput)`: a Machin alternating-series interval
   encloses pi with rigorous rational bounds, and both endpoints must produce
   the same integer ceil before T is emitted. No `std::sin`, platform `libm`,
   binary floating-point approximations or guessed filter lengths.
3. Frozen mapping `ceil_div_signed(sourceOrigin*L,M)`, signed floor/ceil/mod,
   full signed 128-bit product bounds and signed 64-bit source/output bounds;
   half-open intervals and the **endpoint-duplicating** even reflection
   `period=2*Nin` (NOT reflect101 `2*(Nin-1)`).
4. Structural RGSMSR1 verifier: 52-byte LE header, declared payload size,
   exact symmetric binary64 bit patterns, finite values and SHA-256 of exact
   bytes. **Passing this parser says NOTHING about the numerical correctness**
   of the values: a structurally valid zero filter is explicitly tested.
5. Exact `above_new_nyquist` identifiers/frequencies `(fin+fout)/4` for
   all **15** downsampling cases, with independent Nyquist inequality tests.
6. Standalone regression tests wired to **both** Linux and Windows Focused CI.
   The report explicitly says `NOT_RELEASE_QUALIFIED` and leaves all
   `kernel_sha256` fields `null` until an independently certified manifest
   exists. Run `python tools/qualification/final_src/phase11_math_preflight.py
   --json` to inspect the six-rate, ratio and alias-fixture worklist.

Q0 preflight predicts **14 RGSMSR1 files**, **11,440,952 total bytes**
(including 52-byte headers); largest kernel has **327,681 coefficients**.
These are expected *sizes*, not coefficient assets or hashes.

## Mandatory next coherent work — DO NOT silently skip

- Offline **MPFR directed-rounding / certified-interval** source evaluator:
  precision 256 -> 512 -> 1024 -> ...; repeat pi, sin, sqrt, I0 recurrence,
  Kaiser window, raw taps and strictly j-ascending normalization at each
  precision. I0 stop predicate and discarded tail require interval proof;
  no midpoint heuristic or two-precision numeric agreement counts as proof.
- Derive one correctly rounded IEEE binary64 value for each symmetric
  coefficient pair **only after its entire enclosure belongs to one binary64
  rounding cell**. Emit exact RGSMSR1 bytes, frozen recipe/provenance and
  SHA-256 manifest for all 14 distinct reduced ratios. Independent second-run
  byte-for-byte reproduction before publishing asset hashes.
- Independent MPFR zero-stuffed conceptual convolution and DTFT oracle,
  **0.001 dB passband / 140 dB stopband** checks, above-new-Nyquist
  attenuation on all 15 downsample cases; fail **algorithm contract violation**
  rather than silently retuning beta/T/cutoff/window.
- Production C++ portable strict-scalar polyphase convolution with checked
  FrameMap, exact n-ascending accumulation, distinct multiply/add (FMA
  contraction off), entire-track even-reflect, finite support and exact
  crop/seek/checkpoint invariants. Retire the experimental ad-hoc rational
  FIR only when the real coefficient assets and oracles exist.
- Product Final Output SRC before M17 and authoritative M16 at TARGET
  MASTER rate, once-per-authority PCM, monitoring/device-only SRC separate,
  final export rate correctly written, cancellation/stale/no-Personal-Audio
  invariants. Product candidate admission cannot expand from its current
  6 identity + 2 legacy candidates to 36 supported on planning evidence.
- Q1/Q2/Q3/Q4/Q5/Q6 cross-platform, memory, spectral, timing and physical
  Windows gates, with PO-only FQ + Gate M/L before M18. No main merge.

This file preserves the **unqualified** status of every unproven
conversion. It is a mathematical proof harness and executable preparation,
not evidence that real Final SRC has already been integrated.
