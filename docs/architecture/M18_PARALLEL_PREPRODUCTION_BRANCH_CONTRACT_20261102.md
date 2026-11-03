# RGS MasterLab — M18 isolated parallel preproduction · 2026-11-02

**Status:** PARALLEL DEVELOPMENT ONLY · NOT QUALIFIED · NOT MERGEABLE UNTIL M17 PO GATE + CI AUDIT.

## Frozen base and branch ownership

- Parent **EXACT M17 FQ #91 SHA**: `995e3f976f752d148847821bd33b56ce656bfbb8` (Windows FQ #91 success and PR Focused #975 double green).
- M17 parent PR #61: `experimental/m17-monitoring-pcm-isolation-20261024`. Leave the parent SHA and its physical Windows collation completely unchanged.
- Separate M18 no-CI development staging: `staging/m18-dither-preproduction-20261102`.
- Separate M18 Draft PR head: `experimental/m18-dither-20261102`; PR base should be the **M17 PR branch**, not `main`, so the PR diff contains only new M18 changes (stacked review).
- Single-Writer Epoch applies **per branch**. The M18 operator alone writes M18 staging and CAS promotes a finished batch to the M18 PR head; the M17 operator alone writes M17 staging/PR. Neither may touch `main` before a passed gate.
- Once the PO physically accepts M17 and the mandatory post-M17 CI architecture/reset review is closed, merge the M17 dependency into `main` **only with separate PO authorization**; then update/rebase the M18 branch and retarget the M18 PR to `main`. This is an integration step, never an excuse to discard M18 commits or invalidate M17 FQ evidence. Until then M18 work is not release authority and must not land in M17 or main.
- If M17 physical FQ reveals defects, correct them **solely on M17 work branches** with their own new CI/FQ. Once accepted, merge parent changes forward into M18 separately. Maintain immutable original 16 FIR binaries.

## Accepted authority and mathematical falsifications

- Authoritative frozen spec: RGS_MasterLab_L1-M18_Dither_PCM24_Float32_Targeted_Forward_Delta_ACCEPTED_FROZEN_v1.0.md (2026-09-12) in RGS ContentBox; qualification plan v0.2.
- Independent Work pre-audit (2026-10-04) verdict **PASS_WITH_REQUIRED_CORRECTIONS**. Its corrected Philox4x32-10 vectors replace the FOUR falsified preliminary development tuples, not by altering product semantics.
- Terminal sequence: Final Output SRC → M17 True Peak Limiter → M18 Dither → PCM Quantization → Encoding, preserving selected MASTER Fs, independent of DEVICE Fs.
- PCM24: mandatory unshaped non-subtractive 2-LSB p-p TPDF, `D=u0-u1`; seed is explicit `uint64_t`, Philox4x32-10 counter keyed by `absoluteOutputFrame`, `channelIndex`, fixed suffix `0x52475344`. Exact 53-bit uniform grid, target `S=2^23` and ties-to-even binary64 staging, **no clipping/saturation, no hidden reserve/gain**, little-endian 3-byte signed interleaved codes.
- Float32: strictly no dither; correctly rounded binary64→binary32 ties-even, preserve negative zero and subnormals, finite out-of-unity values allowed when finite in binary32; NaN/Inf/overflow must fail, no ambient rounding-mode dependence.
- No noise shaping in v1. After quantization, decoded file must be independently measurable with true-peak M16; no silent correction to M17 output.
- Integration/qualification concerns to reconcile before product code: exact terminal Master owner, seed persistence/render identity and absolute-frame checkpoints, writer/transactional export API, error behavior and no partial file, binary64 stage rounding mode semantics, adopted statistical criteria and Gate G. Never claim Gate G passed solely because the preproduction vectors pass.

## Current *isolated* first M18 code batch

- `audio/src/internal/m18_counter_dither_philox.hpp`: stateless, constexpr, private (not public API) Philox4x32-10, frozen RGS key/frame/channel mapping and exact two 53-bit uniforms/TPDF. Does not touch actual M17 worker, OUTPUT_RATE buffers or DAC.
- `tests/unit/audio/test_m18_counter_dither_philox.cpp`: corrected eleven independent RGS address vectors, three generic Random123-style anchor vectors (one compile-time), fixed uniform bounds and ragged/2^32 carry tests.
- `tests/CMakeLists.txt`: standalone isolated Qt native target `rgsml_m18_counter_dither_philox_tests`; existing M17 sources, Qt UI, original FIR and other DSP tests unchanged.
- Separate PR remains **DRAFT**. Focused Windows/Linux and future rigorous independent mathematical Gate-G evidence must be collected before any integration. This first code is not PCM24 quantization, no Float32 conversion, WAV export or usable M18 UI yet.

## Integration strategy

1. Hold the physical Windows M17 assessment and FQ #91 artifact as a frozen parent.
2. Develop M18 in coherent no-CI staging batches while parent remains pending; send each ready batch by fast-forward **CAS** to isolated M18 Draft PR.
3. Prevent unrelated CI explosions: coordinate M18-specific fast tests in the planned CI reset, but do not tamper with M17 FQ #91 workflow/certificate.
4. Keep M18 code entirely independent of AUDIO DEVICE, Master Audition and the outdated experimental SRC; preserve zero PCM changes in M17.
5. Once parent gate and CI reset are approved, forward-integrate M17 changes, run Windows/Linux focused tests, independent mathematical Gate G, cold FQ and physical PCM24/Float32 tests as needed. Then PO alone authorizes any merge.

**No M18 readiness, product release or M17 acceptance is implied by branch creation.**


## Independent M18 CI tier — incremental 2026-11-03

- Added `.github/workflows/m18-preproduction.yml` with its own workflow named **M18 Independent Preproduction CI**, triggered only by pushes to `experimental/m18-dither-20261102` touching M18 paths or explicit manual dispatch. **M17 PR #61 does not trigger it**. The legacy M17 `pr-focused.yml` is triggered only by `pull_request` with base `main`, not by PR #63 (which targets the M17 experimental branch). The full Windows FQ remains manual-only and untouched.
- Jobs: Windows 2022 MSVC C++20 Debug+Release, Linux C++20 Debug+Release. Standalone translation unit (no Qt, Fir, MPFR, M17 tests, performance benchmarks, or DAC). Existing Qt-native M18 test remains registered in the main CTest suite for later *full integration qualification*; the fast lane intentionally does not claim that Qt test has been run.
- Added `audio/src/internal/m18_pcm24_pack.hpp` for **checked PCM24 integer-code byte packing only**, not yet for conversion or statistical TPDF quantization. Rejects overload, little-endian three-byte two's-complement, exact sign extension in test function, static assertions on rails. Never writes files or changes any M17 sample.
- Added `tools/qualification/m18/m18_portable_selftest.cpp` with exact mathematical word-vector oracle, 53-bit grid, counter carry, chunk identity and packing overflow/boundary checks; compiled against the *same* internal C++ headers. This allows immediate cheap iteration on GitHub Actions separate from M17.
- The CI must actually run and report PASS on the new M18 SHA before it may be called green. It is **preproduction compiler+fixed-oracle smoke**, NOT M18 mathematical Gate G, CTest/Qt product, PCM24 sample quantizer, Float32 writer, M16 post-delivery TP or actual listening qualification.
- When continuing M18 in parallel, write one coherent macro-lot to the M18 staging branch. Advance M18 PR head using nonforce expected-SHA CAS only once per ready lot, creating a dedicated M18 push run. Do not touch M17 PR #61. Never merge M18 to main before M17 physical acceptance, explicit post-M17 CI audit/reset, forward integration, full validation and PO approval.


## Large M18 isolated macro-lot #2 — exact PCM24, Float32 and whole-block byte ownership

At this checkpoint all code remains on M18-only staging until CAS promotion. No M17 PR/FQ/main edits.

1. `audio/src/internal/m18_terminal_sample_codec.hpp` now implements full **PCM24 TPDF quantization** (not just packing): accepts immutable binary64 `double`, seed, absolute output frame and channel address; uses the frozen Philox4x32-10 and exact 53-bit z construction; computes the exact signed dyadic `x*2^23 + (z0-z1)*2^-53`, applies the independently audited **RN64 staging** via fixed 1152-bit integer arithmetic with ties-to-even and then integer ties-to-even. No ambient hardware rounding-mode dependence; no `double` arithmetic or hidden gain/saturation. Categorical nonfinite/overload failure, original sample untouched. The 1152-bit format is deliberately bounded by `|x|<2`; larger finite input necessarily overflows signed PCM24 even with dither and is correctly rejected. Private internal helper, not public API or M18 release.
2. The same private header implements exact **binary64→binary32 RNE** using integer bit construction, including signed zero, normal/subnormal ties, minimum normal, finite outside unity, and categorical NaN/Inf or Float32-overflow rejection. No terminal dither is applied to Float32.
3. Two frame-major interleaved mono/stereo block adapters validate layout, output byte length and unsigned absolute-frame overflow; they **prevalidate the entire block before changing any output byte**. No allocation, no partial output on error, no changes to upstream immutable Master PCM. PCM24 byte layout is signed little-endian three-byte two's complement. Float32 little endian four bytes.
4. `tools/qualification/m18/m18_portable_selftest.cpp` expanded with 16 independent PCM24 literal-bit sample fixtures, fifteen Float32 numerical boundary fixtures, mandatory synthetic **RN64 rail overload** adversary at +8388607.5 RNE→8388608, failed-block output nonmutation, selected frame carry and chunk invariance and supported FE rounding-mode comparisons; still invokes corrected 11 Philox pinned address oracles and separate generic KAT.
5. `tools/qualification/m18/test_m18_independent_reference.py` reconstructs Philox and PCM24 using Python integer/Fraction arithmetic, with **independent** fixed expected byte-code oracles, difficult binary64 rounding and Float32 bit patterns. It is executed before Linux native compile by the M18 independent CI and does not import/use the C++ implementation.
6. Existing `.github/workflows/m18-preproduction.yml` still triggers ONLY M18 PR-head pushes, not M17 focused CI/FQ. Windows MSVC Debug+Release and Linux GCC Debug+Release build and run the exact newly shared C++ codec. The Python oracle is a separate Linux-only independent fast preflight. This is a **preproduction gate**, not formal mathematical Gate G, Qt/native product/FQ or physical decoded-file verification.

### Frozen scopes and next integration
- No WavWriter, file transaction, final-output serialization to disk, persisted ditherSeed, target-format project persistence, M18 UI, M16 decoded delivered-PCM postquantization measurement, cumulative render evidence, exact statistical Gate G or physical listening has yet been integrated. The codec's outputs are ready for ownership mapping after M17 PO acceptance and mandatory CI audit/reset.
- Zero changes to the 16 FIR assets, the 30/30 SOURCE/DEVICE route boundaries, M17 True Peak Limit/M16 verified-mono-or-stereo Master, PR #61 or FQ #91. No main merge.
- Important engineering tradeoff: correct integer-only fixed-dyadic RN64 codec favors auditability and mode independence; benchmark/optimize with byte-identical oracle evidence before claiming real-time throughput. Encoding is offline delivery, not DEVICE callback DSP.
- Exact pre-audit statistical thresholds and four corrected preliminary RNG vectors are still independent authority obligations, not optional test adjustments. Existing frozen product representation remains unchanged.


## Parallel M18 macro-lot #3 — file-candidate boundary, container/evidence and decoded-M16 seam

Developed only on M18 no-CI staging; separate M18 PR#63 fast-forward after final review. Parent M17 FQ#91, PR#61, all 16 FIR and main MUST remain untouched.

- `audio/src/internal/m18_wav_candidate.hpp`: canonical RIFF/RF64 header geometry for **PCM24 and IEEE Float32**, six authorized MASTER Fs and mono/stereo, little-endian WAVEFORMAT PCM tag 1/24-bit and IEEE-FLOAT tag 3/32-bit with required `fact` chunk. Strict checked frame, channel, byte-rate and size arithmetic; RIFF→RF64 4-GiB transition and `ds64` counts. A bounded memory-only *complete candidate* assembles header+encoded PCM24/Float32 payload only after whole-program sample codec succeeds; on any invalid input it returns an error and **no output bytes**. Memory cap 16 MiB intentionally prevents claiming this is the real long-track exporter. No filesystem commit, no direct change to old L1-M09 F64 WavWriter or the platform transactional exporter.
- `audio/src/internal/m18_delivery_identity.hpp`: explicit typed per-render identity for encoding, chosen MASTER Fs, channel count, absolute start output frame, frames, and algorithm/container versions; PCM24 includes the **exact user/authority seed** and TPDF version, Float32 includes **no seed** because its bytes are seed invariant. NOT a SHA-256 and NOT a persistence schema yet.
- `audio/src/internal/m18_candidate_decoded_measurement.hpp`: verifies an internally generated candidate's exact header/geometry and returns immutable canonical decoded binary64 samples at the selected MASTER rate. This establishes an isolated future input seam for authentic **post-quantization M16 True Peak**; it does NOT run M16, prove a final-delivery exact Ceiling, accept untrusted WAV files or certify the physical DAC.
- `tools/qualification/m18/m18_portable_selftest.cpp`: native Debug+Release test now covers PCM24 RIFF byte geometry, Float32 `fact` geometry, complete interleaved sample payload equality, deterministic regeneration, Float32 seed invariance vs PCM24 identity dependence, returned-only-complete candidate failure, final UINT64 frame, RF64 huge size/ds64 without actual allocation, malformed rate/channels/frame bound, decoded-candidate PCM24 error magnitude and Float32 roundtrip, tampered-header rejection.
- `tools/qualification/m18/test_m18_wav_reference.py`: independent Python stdlib WAV (wave+struct), byte-for-byte fixed headers for PCM24/Float32 and 64-bit ds64, independent PCM24 quantization reference, verifies sample framing and payload. Wired into M18-only independent Linux native CI alongside previous Fraction oracle; Windows CI compiles/runs the SAME C++ WAV candidate+identity+decode functions.
- **Status:** this is a bounded, standalone preproduction candidate suitable for later integration; NOT the full transactional WAV file exporter, not arbitrary-length streaming/RF64 filesystem output, not seed persistence in .rgsmaster, not encoded SHA-256, not actual M16 post-quantization measurement, and NOT Gate G or M18 qualification. Integration is deferred until M17 physical acceptance and mandatory CI reset. The full M18 application/writer may need different filesystem API and transaction cleanup; preserve the numerical byte semantics and compare against these independent reference fixtures.
