# RGS MasterLab — M19 parallel preproduction contract (2026-11-03)

**Status:** DRAFT / isolated technical candidate; NOT production-integrated; NOT U/G qualified; NO main merge.

## Binding dependency and single-writer sequence
- M17 is still physical-acceptance pending on PR #61; its frozen FQ #91 parent SHA is `995e3f976f752d148847821bd33b56ce656bfbb8`.
- M18 remains Draft PR #63, head frozen for this M19 fork at `345890bdea2c45f80e6256793b51ce2c2523f55a`. M19 is explicitly **stacked on that M18 SHA**; any future accepted M18 changes must be brought forward through a separate reviewed integration, never by touching M18.
- M19 single-writer no-CI staging: `staging/m19-basic-level-preproduction-20261103`.
- M19 Draft PR head: `experimental/m19-basic-level-20261103`; target PR base = `experimental/m18-dither-20261102`, NOT `main`.
- Develop whole coherent lots on staging, CAS fast-forward PR head once per lot, run independent M19-only dual-OS tests; do not alter M17/M18 PR heads, `main`, original 16 FIR files, M17/M18 UI, AUDIO DEVICE or export.
- **HARD RELEASE HOLD**: M19 must NOT integrate until (1) M17 physically and formally qualified, (2) mandatory post-M17 CI audit/reset completed, and (3) M18 merged/integrated and qualified. Later M19 requires its own U+G, integration with actual full-app Windows binary, and explicit PO merge authorization.

## Frozen authority — RGS ContentBox
- `RGS_MasterLab_L1-M19_Basic_Level_Analyzer_Targeted_Materialization_Delta_ACCEPTED_FROZEN_v1.0.md` (2026-09-12): EXACT five static whole-scope metrics: Integrated LUFS, full-scope RMS dBFS, Sample Peak dBFS, **L1-M16** True Peak dBTP, Sample-Peak Crest dB.
- Frozen PO decision 2026-10-04: `ZERO_STATE_AT_LOGICAL_ANALYSIS_SCOPE_START` for BOTH K-weighting stages and all channels; carry state through chunks/blocks, no preroll/outside-scope PCM.
- Bounded independent mathematical re-signoff: F01 PASS; broader result PASS_WITH_REQUIRED_CORRECTIONS, NOT Gate G.
- No live LUFS/RMS/Sample Peak/Crest, Momentary/Short-term/LRA, PLR, TP-RMS, stereo/spectral metrics, DC removal, clipping diagnostics or alternative TP detector.
- No new FeatureDefinitionRegistry, persisted analyzer schema, QML mathematics or altered RAW/PREPARED/PROCESSED/GOLD tokens.

## Baseline reconnaissance / reconciliation, NOT silent fabrication
- Current repository contains `rgsml_analysis` with spectrum and qualified-candidate M16 internal sources, but no materialized publicly callable canonical M19 feature/registry in inspected surfaces.
- M16 currently exposes internal `measure_unqualified_true_peak_full_resource(const audio::AudioBuffer&)`, which explicitly accepts ONLY full owning resources and is **unqualified**. A narrowed AnalysisScope adapter and accepted result/status/identity mapping are NOT yet present. This candidate therefore does NOT calculate or fill the True-Peak DTO. The real M16 result MUST later be bound unchanged, including its status, support, algorithm and provenance, not recalculated in M19.
- Current `AudioBuffer` is planar binary64 mono/stereo; full-scope vs nonzero analysis-window/target composition still must be mapped to actual APIs. The prototype accepts independent chronological spans + explicit absolute origin, without reading any PCM outside the caller-selected logical scope.
- ErrorCode/status precedence, QTP precision/rounding/summation logarithm policy, full Phase-6 registry and fixed other-rate K banks are NOT yet reconciled. A candidate-only state enum is a private diagnostic, NOT a new canonical status schema.
- One fixed printed-coefficient 48-kHz K bank is reproducible from BS.1770-5 M19 preaudit; the prototype deliberately reports other five MASTER sample rates as **K bank not authority-qualified**, while permitting valid RMS/Sample Peak/Crest for their PCM. Do not run 48-kHz coefficients at any other rate.
- The prototype processes all in-scope samples (including incomplete trailing 100ms fragments), emits only complete 400-ms / 100-ms-grid blocks, strict -70/-10 gate and zero-state K recurrences. The C++ arithmetic path is diagnostic, NOT an audited production FP/QTP schedule.

## Files and no-product-change guarantee
- `analysis/src/internal/m19_basic_level_candidate.hpp`: private, header-only zero-state K, chronological full-scope level candidate; no M16 FIR.
- `tools/qualification/m19/m19_portable_selftest.cpp`: C++20 mono/stereo, scope offset, chunk partitions, silence, impulse, +6dBFS level, non-finite, support lengths and other-rate guards.
- `tools/qualification/m19/test_m19_independent_reference.py`: external frozen anchor / separate mathematical oracle.
- `.github/workflows/m19-preproduction.yml`: M19-only Windows MSVC / Linux GCC Debug+Release, strict-FP, no Qt, MPFR, FQ, 16 FIR or DAC. The short lane is a **compiler/oracle smoke**, NOT real product qualification, not 5-metric completion.
- Existing app, `analysis/CMakeLists.txt`, M17/M18 sources, `main`, shared registered tests and installed Windows artifacts intentionally unchanged.

## Required NEXT gates after dependency release
1. Reconcile Phase-6 FeatureDefinitionRegistry owner, true 48k+other-rate K profiles, scope/target/source identity, canonical status/error and actual M16 result/qualification.
2. Reuse already frozen 35-entry independent M19 qualification ZIP and corrected F01 re-signoff instead of inventing a new formula; run official EBU/ITU test files, strict gate edge cases, extreme-range sum/log adversaries and exact QTP (Gate G + U).
3. Bind the five static metrics through `rgsml_analysis` and the real app safely (without touching DSP/audio); M16 only for TP; no new persisted schema. Verify original PCM immutability, chunk invariance and true Windows app via full CI/FQ.
4. Only after M17/M18 formally qualified and approved for integration, rebase/forward-integrate M19 against the accepted chain, run actual Windows/Linux product tests and obtain explicit PO merge approval.

Do not mistake a green 2-minute preproduction CI for a qualified Qt/Windows build or for a release-ready M19.
