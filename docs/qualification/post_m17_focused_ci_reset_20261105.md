# Post-M17 Focused CI audit/reset — gate #82 (2026-11-05)

**Status: IN PROGRESS, unqualified.** Based on the formally merged M17 parent
`f36446470bda10acf2ec2dc45787b47acf5e3815`.
No main merge is authorized by this document; M18 integration remains blocked.

## Verified baseline (exact M17 SHA d166706e36394c163653438a8254ed29898cda1c)

- PR Focused CI #1023, run 42027987492, Windows + Linux SUCCESS.
- Windows job (127246915900): 1118 s. Isolated Debug compile 299 s,
  isolated Debug Qt smoke 115 s, Debug configure 18 s, Release DSP CTest 288 s,
  Release build 133 s, standalone Release Qt smoke 69 s.
- Linux job (127246916483): 864 s. DSP CTest 296 s,
  mpmath/numpy setup 217 s, build 166 s, REAL fourfold MPFR canary 44 s.
- Official cold Windows FQ #97, run 42031515839, job 127258874881:
  3349 s SUCCESS. Includes canonical Visual Studio Debug clean build (1070 s),
  Debug Qt smoke (101 s), full Debug CTest (775 s), clean Release build (570 s),
  full Release CTest (464 s), Tests-OFF build (167 s), staged authentic 16-FIR
  packaging, missing/corrupted-FIR negatives and deployment evidence.
- Timings are GitHub Actions step timestamps, **not** estimates or full logs.

## Macro-lot 1: Windows duplicated gate reduction

The Focused Windows workflow previously:
1. ran `rgsml_source_metadata_panel_smoke.exe` as a standalone Release UI
   diagnostic and **again** as part of its inclusive Release CTest regex;
2. configured and compiled an independent MSVC/Visual Studio Debug tree, ran
   the same Qt smoke in Debug, plus two CTest Debug guards, *in addition to*
   the official cold FQ at the exact candidate SHA.

Now:
- **Keep per-PR:** native C++ Release build, Release DSP CTest including
  M17 true peak, SRC/FIR authenticity and fail-closed, all B2f/B2i/B3/FP,
  Qt Release smoke (standalone), 150% actual Main raster capture,
  bounded diagnostics, structured M17 probes.
- **Move duplicate Debug qualification to existing mandatory cold FQ:**
  canonical Debug clean build, Debug Qt smoke, Debug CTest containing
  `rgsml_external_playback_fir_assets_tests` and
  `audio_forbidden_include_contract`; identical code must pass before merge.
- No changes at all to `windows-full-qualification.yml`, product DSP,
  original FIR payloads, or existing source-level/static/fail-closed tests.
- The Release CTest regex no longer executes a second Qt smoke. The dedicated
  Qt Release smoke remains an immediate Focused failure on regression.

**Important:** Focused SUCCESS is never physical qualification, cold FQ,
or authority to integrate. Before M18, the exact reset SHA must pass Windows
and Linux Focused CI, required FQ and explicit PO approval. A second CI reset
macro-lot must wait for this lot's CI to finish.

## Later candidates — not yet implemented

- Reduce Linux 217 s numerical dependency setup / separate historical Phase-11
  MPFR canaries into a genuine, explicit extended gate when affected paths
  change. Do not turn real canaries into mocks or mark skipped cases as PASS.
- Review safe CTest parallelism only after shared-artifact/probe audit.
- Review cost of long historical B2h screen while retaining real six-rate
  and strict-FP proof.
- Keep PR Draft preproduction M18–M22 isolated, with their own focused tests
  and provenance. No automatic downstream merge.

## Safety contracts

Single-Writer Epoch; exact-head checkout and CAS on ref writes; no overlapping
macro-lot while preceding CI is pending; `main` untouched until PO approval;
never fetch whole Actions logs; use job/step duration metadata and bounded
error-specific artifacts only.

## Macro-lot 2 — staging-only, NOT covered by Focused #1024

Path `staging/post-m17-ci-reset-no-ci-20261105`; its commits must remain
off the PR head until the preceding run is terminal.

- Linux Python numerical-screening interpreter is fixed at CPython 3.13
  rather than a moving `3.x` selector.
- Exact original dependency versions remain `mpmath==1.3.0` and
  `numpy==2.3.5` in `tools/ci/requirements-src-numerical.txt`;
  `--only-binary=:all:` rejects slow/uncontrolled source builds,
  failing the CI instead of silently changing math or native wheels.
- `actions/setup-python` caches pip distributions using that locked file.
  The 217 s baseline install is the measured cost, **not proof** of its
  root cause; a new real CI must establish whether this saves time.
- Both jobs suppress automatic expensive CI for dormant
  `experimental/*` Draft PRs. Staging Draft PRs still run; non-Draft PRs
  run; `workflow_dispatch` runs explicitly. The
  `ready_for_review` PR event is included so changing Draft→Ready
  triggers the required real dual-OS gate.
- No DSP test suites, authentic FIRs, independent oracles or full FQ logic
  are removed in this lot. Skipped Draft CI is NOT a successful test gate.

**Gate:** the staging changes remain UNQUALIFIED until the prior candidate
Focused #1024 concludes and this coherent lot receives an exact-SHA PR
promotion and its own Windows+Linux SUCCESS. PO-only cold FQ still required
before main integration.
