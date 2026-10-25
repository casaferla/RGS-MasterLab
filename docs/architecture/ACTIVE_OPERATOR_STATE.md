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

## CI #820 / #821 — recovery update 2026-10-25
- [#820](https://github.com/casaferla/RGS-MasterLab/actions/runs/40211524633) FAIL Windows + Linux, exclusively `Minor Fix G3 UI static guard`, which still required the abolished 28px BYP badge and old Finalization topology. Fixed guard on staging, promoted one package.
- [#821](https://github.com/casaferla/RGS-MasterLab/actions/runs/40211765363) triggered from `6b2704fb69fe7104ecd68be8fd87cb96f2be0b46`. Linux fails `Global stationary tooltip policy` due to MONITOR memory checkbox using immediate hover. Staging fix `48af34ae76a009bca8c693eaa1030df7043d933e` now uses the frozen common StudioTooltipDwell 1000ms with matching delay. Windows #821 outcome still in flight during this update. **Do not promote another candidate until Windows step summary is known; batch all related fixes.**
- Main implementation candidate #820 remains not physically PO-qualified; source code includes no FINALIZATION, no BYP badge; MONITOR remains horizontally side-by-side at ALL widths, no adaptive stacking. General docking is still not implemented.

## Latest staging publication (after #819 green)
- [#820](https://github.com/casaferla/RGS-MasterLab/actions/runs/40211524633) PR head `18992c7632478fa69e73a8954a9282bd068ef929`: submitted candidate of **unified DSP Chain, MONITOR QML service editor, no Finalization dock/no BYP spacer, stable-width scrollbar, fixed side-by-side MASTER/DEVICE**, plus adapter semantics and smoke-test updates. CI was queued when dispatched; verify live status.
- User explicitly rejects any responsive rearrangement at minimum width/height. In MONITOR, MASTER/DEVICE remain in two columns for all sizes; scroll content vertically if needed. Chain title/LED/status/error coordinates stay fixed; no width jump when scrollbar appears.
- **Remaining work before FQ #80**: P0 M17 actual audio activation/prepare independent of unsuitable Windows output, MONITOR device installation/return tests, generalized dock/undock framework for every editor, and Main physical 1184×688 150% DPI checks. M18 deferred.
- [FQ80 scope and fixed-geometry PO amendment](FQ80_SCOPE_CHAIN_DOCKING_2026-10-25.md).

## UI checkpoint — MONITOR and unified DSP Chain, 25 October
- Product Owner explicitly rejected responsive two-to-one-column rearrangement. MASTER and DEVICE cards stay side-by-side at every supported Main-window size; controls may become accessible via an intentional expandable SEEK/LOOP section, not a screen-size rearrangement. At minimum height show primary PREPARE / DEVICE / STATUS / PLAY / RETURN as early as possible, with vertical overflow if required.
- Real QML `MonitorEditor.qml` is a separate, selectable service editor within the persistent DspEditorHost, sharing `TerminalMonitorViewModel` with the previous popup; it adds no DSP processing node and fabricates no live metering.
- The DSP Chain now has one scrollable module list with M17 and MONITOR, fixed label width and no FINALIZATION fixed box or 28-px BYP badge slot. Bypass is shown via row tone plus readable text, without disturbing config LED and errors.
- Local visual design reference: `RGS_MONITOR_schema_layout_fisso_2026-10-25.png` was created as a downloadable conversation mockup only; GitHub QML, not that image, is runtime authority.
- [Focused CI #822](https://github.com/casaferla/RGS-MasterLab/actions/runs/40213587135) triggered by ONE PR fast-forward to `a71fec955f6ca4f4cc461c59d30de9cee4e9f792`; status QUEUED when recorded, verify live. Previous #821 Windows PASS, Linux FAIL on stationary tooltip; source-level policy correction is included in #822.
- Next: treat CI #822 red by scoped artifact/job step only, correct on no-CI staging, promote a coherent retry. On green, move to generalized per-module docking and P0 M17/MONITOR offline DEVICE decoupling; FQ #80 remains PO-only.

## Latest UI delivery, 2026-10-25 — CI #823
- PO APPROVED visible name **Master Audition**. Never display plain MONITOR/Monitor as the module's user-facing name; `MASTER_MONITOR` and `TerminalMonitorViewModel` remain stable internal identifiers.
- Each of the seven visible chain entries now has a distinct fixed **16px** QML Canvas pictogram. Hue denotes frozen family, shape denotes actual module. Master Audition uses headphones in Slate `#718092`. M16 bars use Steel Blue `#6683A1`, M17 ceiling Copper `#C4774A`; internal activation colors unchanged. Separate 8px LED and red error indicator remain.
- [Focused CI #822](https://github.com/casaferla/RGS-MasterLab/actions/runs/40213587135) PASS Windows/Linux on `a71fec955f6ca4f4cc461c59d30de9cee4e9f792`.
- [Focused CI #823](https://github.com/casaferla/RGS-MasterLab/actions/runs/40216767401) IN PROGRESS as this note was prepared; PR candidate `b218558a736fd82d000072cfea782dab876b9b6e`; verify live. One PR promotion bundled code + tests, not CI per icon.
- Staging advances documentation past PR head intentionally, avoiding unnecessary CI. Re-audit refs on next chat.
- Remaining P0: M17 offline/DEVICE dependency and physical audio playback 44.1/48/192; generalized cross-module docking; real Windows 1184×688/150% visual no-reflow Gate. No FQ until authorized by PO.

## CI #824/#825 and PO family-icon decision — October 25
- Product Owner approved REPLACING, not supplementing, the 8px Default/Manual LED with the one 16px per-module family-colored pictogram already introduced in #823. Dim family glyph (opacity 0.37) = Default; full family glyph (opacity 1.0) = Manual. BYPASS is an orthogonal row mute multiplier (0.7 for pictogram), error indicator 8px remains independent. A separate green dot or green icon is NOT used. No extra horizontal slot; title gains 8px.
- [CI #824](https://github.com/casaferla/RGS-MasterLab/actions/runs/40223547727) closed FAIL, Windows PASS and Linux stationary tooltip static policy FAIL due to split-line `ToolTip.visible`; corrected on staging.
- [CI #825](https://github.com/casaferla/RGS-MasterLab/actions/runs/40224460594) started from PR fast-forward to `712634b7797ca3a27109dfd5ff60e739803f559d`; verify status live; its code and tests include tooltip fix AND a separate offline MASTER-only source/DEVICE guard refactor.
- In #825, Master Audition can render a private MASTER after a *previously selected PROCESSED* manual source loses its Windows sink (clean NO_SOURCE zero-cue/no-duration/no-loop/no-id), without any device PCM install. Ordinary M17 Active/Play STILL requires the exact installed audible MANUAL and returns errors otherwise. Unknown/alien sink must never enable the offline path. Native fake audio test exercises lost device, private candidate, denied install and later alien takeover. This is **partial P0** only: it does NOT yet make M17 work when the initial 44.1/48/192 SOURCE cannot be selected due to an unsupported Windows device, nor does it provide audible MONITOR without safe return to MANUAL.
- Next cohesive task after #825: actual source-independent offline provenance vs Windows endpoint; safe MONITOR device PCM install and return; generalized per-module docking. Do not claim FQ readiness based solely on CI #825.

## Latest verified state: CI #825 green, CI #826 docking candidate (2026-10-25)
- [Focused CI #825](https://github.com/casaferla/RGS-MasterLab/actions/runs/40224460594) **PASS Windows and Linux** on PR SHA `712634b7797ca3a27109dfd5ff60e739803f559d`. This qualified the first private MASTER-only/no-sink guard and the PO icon-replaces-LED behavior at the focused-test level ONLY. Windows physical FQ is still NOT qualified.
- [Focused CI #826](https://github.com/casaferla/RGS-MasterLab/actions/runs/40225427197) started, PR HEAD `696c9668700200537d99a3bcec3836f5fad07ac1`. Verify live status. DO NOT publish a further PR candidate until this CI's Windows/Linux result is known.
- Generic docking test kernel `DspFloatingModuleWindow.qml` was wired to the REAL Main window using one persistent DspEditorHost QQuickItem, Qt LayoutItemProxy with mutually exclusive docked/floating displays. The selected adapter's exact instanceId is pinned for each detached window; clicking its row focuses it, switching another module redocks the old. The Main View menu has a Dock/Undock action. Compact/standard editor spaces remain fixed Item placeholders while detached, so window geometry does NOT reflow. One floating editor at a time at this milestone; multi-window per-instance simultaneous float is still not qualified.
- CRITICAL: floatingWindow.returnToDock() must emit redockRequested WITHOUT assigning to its own `detached` property, or Qt breaks the binding and subsequent undock never works. Native UI smoke now tests two detach/redock cycles, one in a standalone Qt cross-window fixture and one in the real Main, while preserving pointer identity.
- Remaining P0 for FQ80: initial SOURCE/PROCESSED audition playback can still fail on unsupported exact or paired endpoint rates, leaving no MANUAL owner at all. This case is NOT fixed by #825's clean NO_SOURCE offline render (which still requires a preexisting accepted PROCESSED/manual identity). Need explicit and fully reviewed offline owner/provenance pipeline. MONITOR/Audition DEVICE playback and generalized multi-window semantics need physical qualification, not only green PR CI.
- Workflow contract: staging commits first, batch, promote one candidate, let CI run; during CI update docs and investigate next task, never launch Full Qualification without PO.

## PO-required draggable docking — CI #827, October 25
- Product Owner approved native-window gesture docking as mandatory for FQ #80. Static menu-based Dock/Undock continues as fallback; separate Source-WAV file drag/drop is postponed, as is M18.
- [#826](https://github.com/casaferla/RGS-MasterLab/actions/runs/40225427197) **FAIL Windows UI smoke, Linux PASS**. Scoped error: Main View menu `count` changed from `instantiator + 5` to `instantiator + 7` (two separators and Dock/Undock); the check was updated while preserving exact count and requiring the named command.
- [#827](https://github.com/casaferla/RGS-MasterLab/actions/runs/40226876576) launched from PR `6a56dcb1889c3166e85df4ab83b6d59171a33e9c`; status QUEUED when recorded, verify live. Includes the #826 correction, and:
  1. Fixed 22px grip in DspEditorHost header. A user drag beyond 32 logical px releasing outside the editor requests the unique floating window; Esc cancels.
  2. Fixed 28px grip in DspFloatingModuleWindow header to move Qt Window and signal native-window global release. Esc returns the window to its starting coordinates and cancels drop.
  3. Two non-reflow placeholders in Main standard/compact editor slots; release over actual logical screen-mapped dock highlights/rehomes the SAME editor object via mutually exclusive LayoutItemProxy, leaving source/chain untouched.
  4. Native smoke asserts both grips, drop target, real Main editor QObject, re-dock over target, and drop outside preserving floated state.
- **Known qualification limit:** QML `DragHandler` movement across a native Qt Window and mixed-DPI desktop remains unproven physically despite CI. PO FQ must test one/two monitors and 150% Windows scaling. One floating editor at a time; multi-window simultaneous float is NOT implemented.
- Next on green: continue P0 INITIAL SOURCE/PROCESSED device-independent render authority and safe actual Device audition, then close remaining family geometry and FQ candidate. On red: bounded Windows/Linux failed-step artifact, stage one cohesive fix, then next CI; never inspect whole Actions logs.

## NEXT: cohesive package then PR promotion
- CI #820 is launched and owns PR SHA `18992c76`. Inspect failed-step summaries / small artifacts only. If red, fix all related errors on staging and promote one candidate. If green, continue the already approved M17 P0 audio and generalized docking packages in staging without asking for another 'Vai'. Update this file on staging; never send one CI per documentation edit.
- No FQ #80 before actual M17 44.1/48/192, MONITOR audition, fixed M16/M17 UI and common docking are plausibly functioning on the candidate. PO alone triggers FQ.
- Every operational reply provides exact clickable CI URL.
