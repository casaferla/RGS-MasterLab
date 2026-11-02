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
