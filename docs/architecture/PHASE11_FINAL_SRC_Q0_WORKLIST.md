# PHASE 11 FINAL SRC — Q0 mathematical preflight and pending MPFR/Ziv work

## Subsequent staging — atomic 14-asset MPFR candidate assembly (NOT YET FOCUSED-QUALIFIED)

- `tools/qualification/final_src/build_candidate_manifest.py` enumerates the 14 canonical reduced ratios from the frozen 6x6 matrix, reuses reduced-ratio kernels and invokes the offline MPFR/Ziv generator twice for each ratio. It verifies full RGSMSR1 structure, exact same-implementation binary bytes and SHA-256, then atomically publishes one **CANDIDATE_NOT_RELEASE_QUALIFIED** folder. No partial asset files are published after any failed proof/byte mismatch, and pre-existing destinations are never intentionally overwritten.
- `test_build_candidate_manifest.py` exercises a **synthetic center-delta asset**, not a real Kaiser filter: candidate marking, 14-ratio metadata, two-asset subset, divergent repeat fail-closed, destination integrity and wrong-ratio rejection. Linux Focused gains this quick life-cycle check, not the costly all-14 real generator work.
- These files are explicitly **STAGING-ONLY until promoted after the preceding Focused run terminates**. A same-generator repeated byte match is not independent coefficient regeneration, not MPFR DTFT verification and not release authority. The candidate manifest can never be imported as a product KernelManifest without independent coefficient/response Q0–Q6 gates and PO authorization.

## 2026-10-28 exact-sinc and reproducibility follow-up (STAGING ONLY)

- The offline-only MPFR interval/Ziv candidate now recognizes mathematically exact nonzero integer sinc arguments: for `u=j-D`, `z=49*u/(50*max(L,M))`; `u=0` gives sinc=1, nonzero integral `z` gives sinc=+0 exactly, all other cases use the original certified interval `sin(pi*z)/(pi*z)`. This preserves the frozen formula; it avoids permanent interval ambiguity and tiny spurious sine residuals at analytic zeros.
- Small 256-bit MPFR tests cover all 14 distinct reduced ratios (28 positive/negative zeros), nearby nonzeros, and canonical `u=0`.
- New opt-in full-Q0 canary now enabled **only on Linux Focused**: independently execute `generate(48000,96000)` twice for the genuine 1,025-tap `RGSMSR1` candidate, require identical precision, asset bytes, structural checksum and exact +0.0 tap bits at `u=±100,±500`. It is NOT a second independent generator; therefore a PASS proves repeatability of this same implementation, NOT cross-implementation correctness or Q1-Q6 readiness.
- Before this candidate's own CI, all new edits are STAGING-ONLY and UNQUALIFIED. Preliminary local 14-kernel FFT spectral screening and 120/160-decimal high-precision comparison for four sample-rate directions are supporting evidence only (not certified DTFT MPFR intervals, proof of final rounding or frozen asset manifests).
- Full 14-asset generation/second independent generator, MPFR spectral response plus conceptual zero-stuffed oracle, true cross-rate runtime, physical Windows and PO Gate M/L remain OPEN. Never widen 6+2 candidate admission or issue a release checksum on this Q0 canary alone.


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

## Offline MPFR/Ziv coefficient generator candidate (next same staging batch)

`tools/qualification/final_src/generate_rgsmsr1_mpfr.py` now contains a
**separate offline-only, dynamically loaded MPFR directed-rounding interval
implementation**, with: a full interval `sin` bound via the global 1-Lipschitz
inequality; certified `I0` frozen first-stop predicate and omitted-tail bound;
reconstruction of the exact Kaiser windowed-sinc recipe and strictly ascending
normalization on each precision pass; vector-wide IEEE binary64 round-to-nearest
cell agreement; shared symmetric bit patterns; exact 52-byte little-endian
RGSMSR1 serialization; fail-closed 256→512→1024 precision retry, and no
partial asset output on exhaustion. The only output is offline frozen bytes
and an informational SHA-256 record; **no source code in the product selects
this generator or publishes its candidate files at runtime**.

`tools/qualification/final_src/test_phase11_mpfr_ziv.py` performs a small
MPFR directed-interval/rounding smoke on Linux. A complete 1025-tap
48→96-kHz asset generation test is **opt-in**, not a test claimed to have
passed. No kernel SHA was inserted into a release manifest and no coefficient
or acoustic response is asserted correct before independent reproduction.
This candidate must pass the independent reference and multi-platform
qualification before its exact bytes can be frozen and used for production.

## Supplemental independently ordered exact-dyadic reference (next staging)

`tools/qualification/final_src/exact_dyadic_zero_stuff_oracle.py`
provides a separate, conceptual zero-stuffed-stream convolution cross-check:
it consumes **exact binary64 coefficient bits**, independent externally supplied
SHA-256 and a complete source, iterates prototype tap j in ascending order
and performs **exact Fraction dyadic arithmetic** rather than calling the
production polyphase renderer or reusing its binary64 accumulator. Full and
absolute-crop results are compared for exact equality in dedicated synthetic
delta-kernel tests; a checksum mismatch is rejected. This is an additional
oracle: it is **NOT the frozen MPFR 256+ standalone zero-stuffed oracle** and
must not be used to issue the locked production coefficient or PCM expected
without subsequent independent MPFR/QTP comparison. Both Windows/Linux
Focused CI can exercise its fast synthetic regression tests.

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
