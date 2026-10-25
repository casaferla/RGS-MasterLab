# RGS MasterLab — ACTIVE OPERATOR STATE
**Snapshot as of:** 2026-10-25 (state verified before writing; always re-fetch live refs!)
**Runtime source of truth:** GitHub refs and CI; this is a handoff cache, not authority to force-push.
**Permanent workflow:** [OPERATIONAL_HANDOFF_CONTRACT.md](OPERATIONAL_HANDOFF_CONTRACT.md).
**Bootstrap:** Before next session modifies code, read `AGENTS.md`, `docs/architecture/OPERATIONAL_HANDOFF_CONTRACT.md` and this file, then do live Recovery Audit.

## Branch / ownership
- Repo: `casaferla/RGS-MasterLab`
- STAGING (intermediate writes ONLY): `staging/m17-monitor-preallocation-no-ci-20261024`; no open PR for staging.
- PR branch (publish complete candidates ONLY): `experimental/m17-monitoring-pcm-isolation-20261024`.
- PR: [#61 DRAFT](https://github.com/casaferla/RGS-MasterLab/pull/61), base `main`; NEVER merge main without PO gate.
- PR HEAD after fast-forward promotion of compact UI test + governance dossier: `18992c7632478fa69e73a8954a9282bd068ef929` (CI #820). Always live-check before a new promotion.
- Baseline `main`: `46e917bd2adc8ff13e83c0802895afda7daf6b2a` at the time of the audit; always live-check.
- Single-Writer Epoch applies. Before any modification compare staging/PR SHAs and relationship. Use CAS.

## Focused CI / FQ
- Latest earlier green focused CI: [#813](https://github.com/casaferla/RGS-MasterLab/actions/runs/40205911022), Windows/Linux PASS on `33607cc05390cd99d9f4652b58d779060661cfda`.
- [#817](https://github.com/casaferla/RGS-MasterLab/actions/runs/40207585365): PASS on `6c17846d35094a9becfceeff7891a7d25e31681f`.
- [#819](https://github.com/casaferla/RGS-MasterLab/actions/runs/40209270431): Windows/Linux PASS on `7af7c0e4c881476419c71c396f79c276053dc2f0`. The single PR promotion included first attempt to stabilize the M16/M17 host parity fixture, and all three governance docs.\n- [#818](https://github.com/casaferla/RGS-MasterLab/actions/runs/40207725757): Windows UI smoke FAIL; Linux PASS on `5be31593e67387b82b8bc51366b0d134ceaa2ee8`. Small filtered evidence `RGS-Focused-Test-Errors-Windows-818-...`: `truePeakLimiterHostFamilyTheme`, real-host common left-lane measured M16 height 293 vs M17 height 140; runtime QtQuick.Layout timing suspected, not yet proven. Other warning `prepareRateAdvisor` missing in isolated Main fixture exists. NO full logs.
- Staging after #818: QML host-parity test amended to settle QtQuick layout for both selected editors at constrained heights, commit `4aaeffe343d2e0c1a493058f8f43c9298f65568c`, before governance-document commits. Outcome UNKNOWN until next Focused CI.
- Previous [FQ #79](https://github.com/casaferla/RGS-MasterLab/actions/runs/40194240301) automatic PASS on old SHA `cf868117700a48723df56af2aad841dc8a9d3dbe`; Product Owner physical review BLOCKED due M16/M17 UI regressions and M17 non-functioning at 44.1/48/192 kHz. **No new FQ until PO explicitly launches one.**

## Work implemented after FQ #79, still unqualified by new FQ
- M16/M17 same 34px action strip: M16 reserved but controls empty.
- M17 prepare progress moved from top-only row into fixed GR HISTORY lower band (PREPARATION before ready, GR HISTORY afterward).
- Compact True Peak readouts/meter text constrained; host-level geometry comparison added.
- Old M17 ERROR/BYPASS from previous SOURCE cleared on new accepted SOURCE, guarded; Windows default playback format rejection emits exact SOURCE/preferred-rate support facts. Distinguish diagnostic improvement from actual PCM functionality.
- Several intermediate PR pushes mistakenly triggered #808–818 (11 CIs). **Do not repeat.** From now on intermediate writes solely to staging.

## BLOCKING open bugs / priority
1. **P0** Real Windows M17 activation/prepare unusable with 44.1/48/192 kHz SOURCES; MONITOR untestable. Offline master creation is still coupled to active audible MANUAL device ownership; separating render eligibility from playback permission requires careful design and tests (no unsafe bypass). The device default supports only source exact or authorized 44.1/48 pairing. Do not silently retarget MASTER.
2. **P0/UI** M16/M17 same physical geometry to pixel at minimum window and 150% Windows scaling: action strip, meters, three cards; PREPARATION and GR HISTORY share lower area. Verify host fixture as well as Main Windows program. UI #818 test failed, candidate staging fix is unverified.
3. **P1/UI** M17 right cards and NO MEASUREMENT still clipped/overlapping at minimum window; 0 dBTP compact labels and alignment need actual PO check; icon C now PASS.
4. PCM/monitor memory preflight, real device playback, 192k and 44.1/48 SRC and M17 ceiling are pending qualification.
5. New PO-approved UI product roadmap **partially implemented on PR candidate #820, unqualified until its CI and physical FQ**: SINGLE scrollable chain; remove fixed finalization M17 block; bypass shown by subdued card (remove BYP badge); thin vertical overlay scrollbar with stable label width; MONITOR as Slate service row; order M17→MONITOR→M18; M18 Bronze output/dither; general dock/undock, floating window focus without editor duplication. PO has now explicitly included the Chain and generalized docking in FQ #80 scope; do not revert this to post-FQ. M18 remains deferred.

## PO UI invariants from October 25
- All shared M16/M17 physical elements fixed at identical coordinates; M16 upper PREPARE row reserved but empty; M17 progress in same lower band as GR HISTORY.
- M16 analyzes without modifying samples; M17 terminal dynamics limiter; MONITOR non-destructive listening branch; M18 final delivery/dither, and Float32 output without dither.
- Normal processing order visual terminal: M17, MONITOR, M18. No implied processing through MONITOR.
- Family accents WORKFLOW/UTILITY Slate `#718092` for MONITOR; OUTPUT/DELIVERY Bronze `#A78351` for M18; M17 Copper `#C4774A`.
- Docking general for all module types; clicking floating module in chain focuses its ONE window (does not create duplicate or reopen embedded editor).
- Staged changes require automatic next step **during active chat**, not scheduled/background monitoring.

## Latest staging publication (after #819 green)
- [#820](https://github.com/casaferla/RGS-MasterLab/actions/runs/40211524633) PR head `18992c7632478fa69e73a8954a9282bd068ef929`: submitted candidate of **unified DSP Chain, MONITOR QML service editor, no Finalization dock/no BYP spacer, stable-width scrollbar, fixed side-by-side MASTER/DEVICE**, plus adapter semantics and smoke-test updates. CI was queued when dispatched; verify live status.
- User explicitly rejects any responsive rearrangement at minimum width/height. In MONITOR, MASTER/DEVICE remain in two columns for all sizes; scroll content vertically if needed. Chain title/LED/status/error coordinates stay fixed; no width jump when scrollbar appears.
- **Remaining work before FQ #80**: P0 M17 actual audio activation/prepare independent of unsuitable Windows output, MONITOR device installation/return tests, generalized dock/undock framework for every editor, and Main physical 1184×688 150% DPI checks. M18 deferred.
- [FQ80 scope and fixed-geometry PO amendment](FQ80_SCOPE_CHAIN_DOCKING_2026-10-25.md).

## NEXT: cohesive package then PR promotion
- CI #820 is launched and owns PR SHA `18992c76`. Inspect failed-step summaries / small artifacts only. If red, fix all related errors on staging and promote one candidate. If green, continue the already approved M17 P0 audio and generalized docking packages in staging without asking for another 'Vai'. Update this file on staging; never send one CI per documentation edit.
- No FQ #80 before actual M17 44.1/48/192, MONITOR audition, fixed M16/M17 UI and common docking are plausibly functioning on the candidate. PO alone triggers FQ.
- Every operational reply provides exact clickable CI URL.
