## CHAT 57 — #891 active; 14-shape frozen geometry added to SAME staging batch · 2026-10-29

- New test-only `audio/include/rgsml/audio/internal/phase11_frozen_rgsmsr1_geometry.hpp` transcribes exactly the 14 Q0-rational/pi-enclosed frozen reduced L/M→T,N pairs; test asserts 36 rate mappings cover all 14 shapes and sum to **11,440,952 bytes** with the 52-byte RGSMSR1 header per asset. The pinned C++ strict scalar test bridge refuses a caller-supplied T that differs from the frozen shape, even if its candidate SHA matches. This adds NO 14-real-asset certification and NO product Final SRC support.
- These are additions to the **existing next cohesive STAGING lot**, not yet tested by [Focused #891](https://github.com/casaferla/RGS-MasterLab/actions/runs/40829398147), whose head is immutable `bf200e0d2b55d9b451244665f1490cb12b557ce9`. Its eventual Windows/Linux PASS must NOT be attributed to the new staged T/N table or 36-path hash-pinned tests. Promote only after #891 terminal with fresh ancestry/expected-SHA checks, one no-force fast-forward; no other Focused while #891 is running.
- Immediate blocker remains the genuine independent coefficient/DTFT proof for all 14 assets and acceptance of real C++ 30-path product Final SRC. Do not reopen frozen DSP or count experimental scalar infrastructure as product release.

## CHAT 57 — #891 active overlap / staging next pinned-SHA scalar reference (2026-10-29)

- [Focused #891](https://github.com/casaferla/RGS-MasterLab/actions/runs/40829398147) tests immutable PR SHA `bf200e0d2b55d9b451244665f1490cb12b557ce9` (test-only RGSMSR1 structural parser); Windows/Linux CI may be running while this successor staging work is prepared. **Do not credit the following to #891.**
- Subsequent **STAGING ONLY** coherent work: extend `phase11_rgsmsr1_candidate_asset.hpp` with strict 64-character lowercase externally provided SHA-256 pin via existing Qt-free `rgsml::core::sha256_hex`; fail closed on an altered but still structurally valid RGSMSR1 candidate. New `phase11_pinned_scalar_candidate_reference.hpp` ties independently pinned binary64 taps to the test-facing n-ascending strict scalar reference at the six approved source/target rates, including bit-transparent six identity paths, no extra product renderer. Unit tests check synthetic center-delta wire-format assets over all **36 paths**, pinned data tampering, absolute crop/partition invariance and identity semantics. Signed zero is preserved as encoded.
- All fixture taps/hashes are **SYNTHETIC SELF-PINNED TEST MATERIAL**: test hash exercises code but is NOT the independently approved frozen 14-kernel release manifest. NO runtime asset discovery, NO released kernel, NO product Final SRC admission, NO full M17 FQ. Product still allows only six same-rate identity and two historical 44.1/48 candidate paths; the other 28 remain explicitly UNQUALIFIED.
- Next critical real gates: run and independently certify all 14 MPFR/Ziv genuine kernel asset sets with continuous real DTFT and Q1–Q6, then use approved kernel hashes in a REAL C++ product conversion entry, integrate it before M17, verify one master per revision and Master Audition DEVICE-only SRC; finalize six-rate per-host truthful ETA and PO physical Gate M/L. Preserve stable frozen DSP. The staged files require their **own** subsequent Focused CI after the #891 run is terminal and the lot passes review.

## CHAT 57 — #890 double-green; test-only C++ RGSMSR1 binary candidate parser · 2026-10-29

- [Focused #890](https://github.com/casaferla/RGS-MasterLab/actions/runs/40825974102) terminal Windows + Linux SUCCESS at PR #61 SHA `c2681b967213be566c809959d53ad1eb961d7611`. Staging and PR started identical. No FQ or release admission.
- Next coherent batch: `audio/include/rgsml/audio/internal/phase11_rgsmsr1_candidate_asset.hpp` decodes the exact 52-byte little-endian RGSMSR1 header and all IEEE binary64 tap bits at a caller-specified reduced ratio and frozen shape, fail-closed on wrong version/encoding/geometry/size/symmetry/nonfinite. A dedicated Qt unit fixture in the EXISTING `test_playback_sample_rate_adapter.cpp` exercises this parser, preserves signed zeros, feeds strict-scalar convolution and proves malformed bytes are refused. All taps are SYNTHETIC.
- **Important boundary:** header has `PHASE11_TEST_ASSET_PARSER_ONLY` provenance; no SHA-256 authentication, no independently frozen coefficient manifest, no product runtime discovery or loader, no qualified 14-real-kernel/30-product-SRC claim. Do not expose this as `FinalOutputSrc` product support; Q1–Q6 and continuous REAL DTFT are OPEN.
- This package is prepared on STAGING only. Its own Windows/Linux Focused must run after one exact-SHA CAS promotion. Keep all 30 experimental paths unqualified, maintain one M17 MASTER PCM and independent DEVICE branch, no PO FQ/main merge/M18 without Gate M/L.

## CHAT 56 — #889 double green; opt-in real 14-ratio DTFT candidate screening staged (2026-10-29)

- [Focused #889](https://github.com/casaferla/RGS-MasterLab/actions/runs/40782090919) terminal **Linux+Windows SUCCESS** at exact PR SHA `8a2a08f453c1cc42dc8055a3015fcc16989afe0d`. Existing conservative continuous-box MPFR DTFT preflight test is green; this is synthetic non-release scope, not a real full-family proof.
- NEW on no-PR staging: `screen_real_candidate_family_dtft.py` consumes an already-generated candidate folder, first performs strict 14-ratio SHA+geometry+folder-integrity audit, then executes the directed-MPFR continuous-DTFT proof ATTEMPT for each REAL ratio with bounded adaptive boxes and truthful per-asset outcomes. A supplied candidate digest is checked for file integrity; it is **not independently frozen release provenance**. If a box remains unresolved, overall status is `CANDIDATE_DTFT_CONTINUUM_SCREEN_UNRESOLVED`; no Q1–Q6 or product-support flag is ever issued, even when all boxes prove sufficient bounds. CLI can write a new atomic JSON report OUTSIDE the candidate folder; it exits nonzero if full coverage is unresolved. No real 14-asset campaign is claimed run.
- Fast **synthetic only** `test_screen_real_candidate_family_dtft.py` verifies 14-ratio requirement, corruption/hash fail-closed, refusal of forged certification/continuum statuses, and bounded UNRESOLVED, under the EXISTING Linux Focused DTFT test step. No additional workflow, no claimed real spectral completion. This lot is STAGING ONLY until its own future Windows/Linux Focused success.
- Still blocking M17 completion: genuine complete independently certified 14 assets and DTFT Q0–Q6, real 30-conversion product C++ Final SRC, one prepared MASTER reused by Master Audition, six-rate calibrated ETA, PO physical Windows Gate M/L. Keep all unqualified conversions blocked; no FQ, main merge or M18 without PO approval.

## CHAT 56 — directed MPFR continuum-box DTFT preflight on staging (2026-10-28)

- New `tools/qualification/final_src/mpfr_dtft_interval_preflight.py` computes an **outward** MPFR enclosure of the **zero-phase** DTFT of immutable exact binary64 symmetric candidate taps, independently of the MPFR coefficient generation recipe. It reduces rational cycles before trigonometry, uses exact dyadics (including subnormals), directed rounding and a rational `pi` upper bound for the first-derivative Lipschitz remainder over **every frequency within each box**. An explicit finite adaptive subdivision budget never turns an unresolved box into success.
- Conservative sufficient rational thresholds `abs(H-1) <= 1/30000` on the passband and `abs(H) <= 1/10000000` on the stopband cannot create a false spectral pass: the passband bound implies less than 0.001 dB absolute/ripple, and the stopband bound implies at least 140 dB attenuation. These tighter conditions **may reject valid frozen-recipe kernels**, so an `UNRESOLVED` result requires independent proof refinement, never beta/T/cutoff retuning.
- Small synthetic symmetry/delta/endpoint/subnormal and finite-budget fail-closed tests run only in the **existing Linux Focused** Phase11 MPFR section. No run generates or qualifies the 14 real assets; no claim that a full continuum sweep has closed on any actual asset. The proof tool requires a separately specified expected candidate SHA to read assets and remains `NOT_RELEASE_QUALIFIED` even if both frequency ranges close. Independent coefficient provenance, full actual 14-ratio MPFR/DTFT Q1–Q6, product C++ Final SRC, all 30 nonidentity paths, M17 single MASTER PCM and per-host ETA are still OPEN.
- Coherent candidate is **STAGING ONLY until its own PR Focused Linux/Windows success**. Do not infer qualification from the earlier immutable PR #61 CI #888. Full FQ and Gate M/L remain PO-only; no merge main or start M18.

## CHAT 55 — sampled spectral and immutable candidate-folder integrity screens, staging-only (2026-10-28)

- Added opt-in `build_candidate_manifest.py --screen-sampled-fft`, using the existing separately provided sampled FFT screen for EACH generated real RGSMSR1 candidate before atomic folder publication. A ratio that fails `sampled_pass` or misstates the exact noncertifying caveat aborts the entire family; the manifest records bounded per-ratio grid metrics and `sampled_fft_screen_complete` only for a genuine all-14 screened family. NumPy remains optional offline-only and is NOT a product dependency.
- Added `verify_candidate_family.py` independent post-assembly filesystem/content audit: canonical L/M→fin/fout mapping, frozen T/N/bytes, exact filenames, structure and SHA-256, no extra files or missing assets, truthful 14-family and three per-asset screen flags. Default requires exactly all fourteen; explicit subset mode is **test-only**. Refuses any manifest claiming MPFR DTFT/Q1–Q6/product admission and NEVER upgrades a candidate into a released kernel.
- Extended the existing Focused `test_build_candidate_manifest.py` with synthetic two-ratio callback success/failure, no-partial-publication, swapped bits, extra asset file, falsely claimed complete family, falsely claimed screen and falsely claimed release. All still **NONRELEASE Q0**. True 14-real-asset coefficient, MPFR continuous DTFT and production admission Gates remain outstanding; do NOT conflate sampled FFT with certified frequency response.
- All changes under no-CI staging while [CI #887](https://github.com/casaferla/RGS-MasterLab/actions/runs/40773887014) qualifies preceding PR SHA `8683af9adb0de9e46529b7342b107b4adea33123`. They require a later single-CAS PR promotion and their own Focused Linux/Windows success.

## CHAT 55 — first test-facing C++ strict-scalar Final SRC candidate across 36 paths (STAGING ONLY) · 2026-10-28

- Created `audio/include/rgsml/audio/internal/phase11_strict_scalar_reference.hpp`: isolated, inline, **test-facing only** C++ n-ascending polyphase convolution. It accepts complete read-only SOURCE samples and immutable caller-provided IEEE binary64 kernel bits; checks finite PCM, bit-symmetric finite FIR, rational geometry, signed ceil, 128-bit-risk-aware 64-bit output bounds, and endpoint-duplicating even reflection. Output uses separate `double product=sample*coefficient` and `sum=sum+product`; for `L=M=1`, identity copies source bits including signed zeros. This is NOT connected to product SRC selection, output or runtime asset loader and does not claim release status.
- Added `phase11StrictScalarReferenceSixBySixAgainstConceptualGrid` to the EXISTING `rgsml_playback_src_tests`: all 36 6×6 paths, 30 nonidentity rate-pair geometries, 6 bit-transparent identities; synthetic dyadic symmetric taps compared to a separately indexed literal high-grid **j-ascending** conceptual sum, absolute partition invariance, and invalid-input/crop fail closed. **Kernel samples are SYNTHETIC and not the 14 frozen MPFR assets**, so no product fidelity claim.
- The existing CMake test target now enforces Windows MSVC `/fp:strict` or portable `-fno-fast-math -ffp-contract=off` in the test TU; frozen production DSP and playback SRC build flags are unchanged. This test must first pass a NEW Focused CI after current [CI #886](https://github.com/casaferla/RGS-MasterLab/actions/runs/40771404800) is terminal green.
- Still OPEN before M17 FQ: independently certified 14 MPFR/DTFT coefficient and PCM assets, strict product Final SRC with frozen SHA, 30 nonidentity SOURCE→MASTER conversions and M16 at master rate, exact single M17 Prepare shared with Master Audition, 6-rate honest calibrated ETA, physical Windows Gate M/L. Do not mislabel this test harness as product integration.

## CHAT 55 — Phase11 strict-scalar polyphase reference, staging-only (2026-10-28)

- Introduced `tools/qualification/final_src/strict_scalar_polyphase_reference.py` as **offline-only** n-ascending binary64 reference for the exact frozen `q=D+m*M`, Euclidean signed `nLo/nHi`, `j=q-nL`, endpoint-duplicated even reflection and absolute source/output crop. Each Python multiply and add is a distinct binary64 operation, with no FMA. Identity uses direct original sample bits including signed zero and subnormals.
- `test_strict_scalar_polyphase_reference.py` compares the float n-ascending result with independent `Fraction` exact conceptual j-ascending zero-stuffed oracle, under a standard bounded sum-of-products rounding diagnostic for short finite synthetic tracks; verifies 48→96, 96→48, 44.1→48, absolute crop/whole equality, one-frame reflection, nonfinite rejection and bit-transparent identity.
- Linux/Windows Focused CI reuse the already-existing Phase11 zero-stuffed-oracle steps and add this small independent regression; no new CI workflow. These are **synthetic Q0 candidate references**, not a C++ product implementation, MPFR full convolution oracle, locked real all-14 binary asset manifest, continuous DTFT response proof, Q1–Q6 qualification or product SRC admission.
- While [CI #885](https://github.com/casaferla/RGS-MasterLab/actions/runs/40768552809) checks previous PR SHA `8f93e0e8db626a809713fdfac68f9db38db0447e`, these new changes are staging-only and untested by that run. Subsequent candidate requires its own Windows/Linux CI after #885 terminal green and one verified CAS.

## CHAT 55 — next exact-dyadic 14-asset optional regression lot (STAGING ONLY) · 2026-10-28

- `exact_dyadic_zero_stuff_oracle.py` now iterates only prototype positions where the *conceptual zero-stuffed source grid* is nonzero: `j = (q mod L) + kL`, in increasing `j` order. This does not call/imitate product n-ascending binary64 summation; arithmetic remains exact `Fraction`. Added three-ratio literal all-tap comparison incl. endpoint reflection and crop.
- New `screen_real_candidate_dyadic(asset_bytes,fin,fout)` applies independent conceptual exact dyadic convolution to an 8-frame mixed-sign full source and absolute crop [1,7), verifies full and crop frame count/invariance and reports a *sampled DC deviation* for constant 0.25. This is not continuous MPFR DTFT or an independent locked coefficient/PCM release oracle.
- `build_candidate_manifest.py --screen-exact-dyadic` now optionally screens each of 14 real generated candidate assets before final atomic publication, alongside optional `--screen-independent-mpmath`. Failed later ratio aborts the whole candidate, tested against synthetic shapes; per-asset and family flags explicitly remain **NOT_RELEASE_QUALIFIED** and do not authorize 30 conversion paths.
- Remains staged without PR CI or Q1–Q6 qualification. Follow up with certified independent MPFR coefficients/DTFT and product strict-scalar SRC; no beta/T redesign, no fake SHA or runtime product admission.

## CHAT 55 — optional full-family independent numerical screen (STAGING ONLY, no CI yet) · 2026-10-28

- Extended `build_candidate_manifest.py` with opt-in `--screen-independent-mpmath` for all 14 **real** MPFR candidate assets. For each ratio it calls the separate mpmath 120/160-dps reconstruction, requiring byte-identical candidate output at both precisions; a mismatch aborts the entire atomic candidate destination (no partial release/publish). The check runs **only if explicitly requested**: default candidate builder and Focused CI do not silently run expensive 14-asset work.
- Two synthetic shape-only unit tests exercise optional screen invocation and the second-ratio failure/atomic cleanup; they **do not** constitute actual all-14 coefficient crosscheck. A complete future run sets only `independent_mpmath_screen_complete=true`; `independent_generator_verified=false`, `mpfr_dtft_oracle_passed=false`, `q1_q6_passed=false`, `product_admission_authorized=false` remain immutable.
- Requires `mpmath` only when the optional numerical screen is used, separate from product runtime. Numerical two-precision agreement is **NOT** certified final-rounding interval proof or continuous DTFT response. No kernel release checksum is authorized, no 28 new pair admission. Staging branch only, pending next PR CAS and its own Focused outcome.

# PHASE 11 FINAL SRC — Q0 mathematical preflight and pending MPFR/Ziv work

Sampled FFT screen: `tools/qualification/final_src/screen_candidate_spectral_response.py` accepts exact candidate-manifest SHA-256 and checks 0.001 dB passband and 140 dB stopband on a sampled FFT grid. This is NOT the independent certified MPFR/DTFT response oracle and cannot authorize release.

## Next after #883 — genuine-kernel exact-dyadic convolution smoke (STAGING ONLY)

- The extended 48→96-kHz Linux test now feeds the **real MPFR-generated RGSMSR1 candidate bytes** into the independent conceptual zero-stuffed exact-dyadic convolution oracle. A generated candidate SHA-256 guards the fixture load; it is explicitly not the separately frozen release manifest checksum.
- Checks canonical eight-frame output for a four-frame impulse, exact whole-resource versus absolute crop invariance for [1,3), and a constant 0.25 signal through the actual FIR kernel against 1e-8 output tolerance (bound is a Q0/Q1 functional smoke threshold, NOT the frozen rigorous spectral 0.001dB/140dB release criterion).
- **No product-admission, no complete 14-ratio asset release, no Q1-Q6 final verdict**. This stays on no-CI staging until the prior [Focused #883](https://github.com/casaferla/RGS-MasterLab/actions/runs/40761148791) is terminal green; only then promote a complete coherent package with CAS.

## Next staging — independent high-precision coefficient comparison (UNQUALIFIED)

- `tools/qualification/final_src/independent_mpmath_crosscheck.py` independently rebuilds frozen Kaiser/I0/sinc coefficients with mpmath at 120 and 160 decimal digits. The sample 48→96-kHz candidate is compared with both computed byte streams; any mismatch fails instead of publishing a checksum. Algebraic sinc zeros are exact. This is a separate numerical algorithm from the MPFR interval engine, but the two mpmath precisions are **not** rounding-cell interval proofs.
- Extended Linux Focused canary adds the mpmath comparison on the already-generated genuine 1,025-tap MPFR asset. Only offline Python tests consume pinned mpmath; no Windows build or product DSP dependency is added.
- Status: **staging-only, not green at the preceding PR SHA** until its own Focused CI. No all-14 MPFR asset proof, no MPFR DTFT continuum oracle, no production manifest/release admission, no physical Windows Gate M/L.

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
