# RGS MasterLab — FQ #80 scope, DSP Chain and generalized docking
**PO decision:** 2026-10-25 · APPROVED SCOPE REVISION, not acceptance of implementation.  
**Parent authorities:** AGENTS.md; OPERATIONAL_HANDOFF_CONTRACT.md; ACTIVE_OPERATOR_STATE.md; Product Owner frozen UI/architecture decisions.  
**FQ dispatch:** Product Owner ONLY, after PR Focused CI & evidence.

## 1. FQ #80 SHALL INCLUDE
1. **P0 restoration:** M17 offline render PREPARE functional at SOURCE 44.1/48/192 kHz and all six frozen rates; device endpoint and MASTER clocks independently owned; MONITOR can materialize and audition compatible device-rate PCM with memory guard and safe return MANUAL. Never synthesize real-time LIVE provenance.
2. **M16/M17 geometry:** same 34px header action slot (M16 unpopulated), same L/R meter tracks, 3 cards, right rail, zero dBTP; PREPARATION and authentic GR HISTORY share fixed lower band; no overlaps at 1184×688 / Windows 150%; physical Main-window verification.
3. **One unified DSP Chain:** remove the anchored FINALIZATION dock, use one vertical scroll; M17 in same scroll list after analysis M16, MONITOR service row adjacent to M17; M18 does not yet exist. Dynamic scrollbar overlay does not change row text width. Selection/focus auto-scrolls into view; wheel, keys, touch, no horizontal scrollbar.
4. **BYPASSED visual semantics:** remove BYP badge AND its permanent 28px spacer, reclaim 28px; darker/muted bypass-capable rows, explicitly textual status + full tooltip; no smaller fonts. Config LED independently remains Default vs Manual Non-Default; visible error remains red; M17 Meter is still alive when limiter bypassed. Utilities and M16 are NOT fake-bypassed.
5. **MONITOR as genuine selectable workflow/service adapter:** after M17 in chain; Slate `#718092`, not a mastering DSP. Its editor in central Adaptive Context Workspace, not an M17-only popup. Separate MASTER/DEVICE source timebases and explicit consent. M18 reserved only conceptually.
6. **General dock/undock for all existing editor types:** single reusable instance controller per module/instanceId. Docked selection routes one editor into central workspace. Detached selection raises/restores ONE existing floating window (no duplicate docked editor). User can resize/maximize window and return to dock. Retain DSP state, histories, telemetry, live transport and instance identities. Persist geometry as a subsequent enhancement if platform storage cannot safely be established in this gate, but preserve during session. Never re-render merely from docking.
7. **Real Windows audition smoke:** zero/44.1/48/192, bypass/active/preview/monitor, closing an undocked window while playing, selection switching, re-dock, minimum size, 150% DPI. UI physical verification remains PO authority.

## 2. Explicit NOT in FQ #80
- M18 processing, PCM24/Float32 export, dither and final delivery.
- Full Reference Match, unapproved unrelated DSP, SkinPack R1, Android UI completion.
- Unrelated redesign or architectural shortcuts to pass a test.

## 3. Architecture & safe implementation order
**Work package A — P0 audio boundary + existing M16/M17 geometry correction.** First make M17 render full immutable private MASTER without presupposing an installed suitable Windows sink. Capture must use approved PREPARED/MANUAL owner, chain revision, SOURCE range/loop and immutable identity; install/preview and LIVE certification stay independently fail-closed. Cover no-device and unsupported-device tests and real six-rate sample policy. Fix host parity.
**Work package B — single Chain + state styling.** Change `DspChainSelector.qml` once; unify row repeater/original adapter index; support scroll/keyboard/selection, scrollbar overlay, remove BYP and fixed finalization. Tests for 164px width, variable vertical height, both scrollbar states, M16/M17 selection and error/config LED.
**Work package C — MONITOR service adapter/editor.** New selectable non-processing adapter, not appended to DSP ProcessingChain; own Slate accent and status, reused `TerminalMonitorViewModel`, remove old M17-only popup entry after wiring. Tests prevent audio and terminal processing topology changes from utility selection.
**Work package D — generalized docking manager.** Introduce one window controller keyed by module instance ID/service identity; share one existing `DspEditorHost` instance using Qt Quick `LayoutItemProxy` for mutually exclusive docked/undocked presentation if supported by target Qt6.8 Windows; otherwise use one parent-reparent route that preserves object identity. All existing types, not MONITOR-only. Switch/re-dock/close/minimize/maximize interactions and no audio resets test.
**Work package E — stabilization and final candidate gate.** Cross-compose A–D, Windows/Linux focused suites, full Main screenshots and physical evidence; only then PO Full Qualification #80.

Packages are *coherent scopes*, not a compulsory one-CI-per-package sequence. Stage each package's interconnected code/tests/docs and promote the largest bounded, reviewable candidate; red CI corrections remain only on staging until a coherent retry is ready. After green, continue during the active chat automatically. Run the FQ ONLY on PO authorization.

## 4. Revised CI forecast
Previous expectation of 3–5 further Focused CIs assumed **no Chain/docking**; superseded. A reasonable planning envelope is **5–8 further complete-candidate Focused CI promotions after #819** if the audio and Qt reparenting investigations go well, with additional retries possible; not a promise and not a quota. Minimize CI count by staging/batching, NOT by skipping independent functional/physical checks.

## 5. Docking QML proof obligation (Qt 6.8)
Current `DspWorkspace.qml` owns a single persistent DspEditorHost rehosted via `LayoutItemProxy`; `Main.qml` already has standard and compact mutually exclusive proxies. Qt documentation: only ONE visible proxy may control a target at a time. A third floating-window proxy must therefore exclude BOTH embedded proxies; test cross-window transfer under actual Qt 6.8 prior to adopting it. Never instantiate a second DspEditorHost or secretly duplicate a DSP editor.
For module switching while detached, distinguish **current window identity** from **selected module identity**. If multiple windows are offered, each tracks original module instance; selection raises matching window; it must not substitute currently selected editor's content into the wrong window.

## 6. Detailed acceptance evidence
- Repeated selection and navigation in 164px Chain does not shift text widths when scrollbar appears/disappears, no horizontal scrolling; M17 visible in same list; MONITOR selectable and not bypassable.
- BYP badge absent as a layout item, LED and error semantics preserved. Child DSP Chain modules retain original IDs and processing ordering; utility row never becomes an audio DSP stage.
- Detach/reattach all existing editors and MONITOR: exact same object or persistent editor state, no telemetry reset, no duplicate window; closing/minimizing/maximizing semantics sound. Editor instanceId authority is retained.
- M16/M17 pixel geometry at minimum Windows 150%; PREPARATION GR lower bay; no clipped text.
- M17 full immutable OUTPUT_RATE master and MONITOR device PCM prove separate rates and clocks; explicit nonconforming authorization; loss of endpoint, stale Source or new parameter/loop/owner invalidates correctly.
- No new FQ if core functionality remains blocked, even when Focused CI is green.

## 7. MONITOR editor visual concept — authoring candidate for PO review
- Entry in the **same shared vertical DSP Chain** as other modules, immediately after M17; label `MONITOR`, muted Slate `#718092`, no fake bypass/configuration action. The UI calls all selectable editors *modules* although MONITOR is not a signal-processing DSP.
- One independent `MonitorEditor.qml` instance in the same `DspEditorHost` and future common floating-window manager. The old `MasterMonitorPopup` is transitional and must not remain a competing user entry once the new route is qualified.
- Reuse existing typography, 6px dark-well radius, subtle 1px borders and restrained family-accent strip; avoid full-Slate fills, oversized titles or unrelated decoration.
- **Adaptive two-card mid band:** left `01 FINAL MASTER` (explicit six-rate Hz choice, Prepare MASTER button, immutable MASTER provenance); right `02 DEVICE AUDITION` (REAL device rate when known; independent Allocate DEVICE / Monitor, memory caution checkbox). Keep the SAME two-column geometry at minimum width. Never switch to stacked cards; vertical scrolling may expose overflow without rearranging the cards. Never infer an unsupported rate from the Windows preferred setting or imply that selected output equals MASTER.
- **State/transport:** one stationary status well (IDLE / PREPARING / VERIFIED MASTER / DEVICE AUDITION / recovery/error), truthful MASTER frame counter when installed, Play/Pause/Stop/Return MANUAL; seek and LOOP subordinate. The global error bar remains the canonical app-wide alert route.
- **Do not fabricate:** no fake waveform, no invented GR/True Peak/LUFS, no LUFS or DAC headroom value without an authoritative measurement. No controls that change MASTER or insert a DSP transform.
- FQ #80 proof: the shared host selects MONITOR as an ordinary editor; the service does not enter ProcessingChain, undo/history/bypass or audio stage counts; never produces a second QML editor or a hidden MONITOR-specific docking implementation.

## 8. PO explicit fixed-geometry amendment — 2026-10-25
The user rejected responsive rearrangement: **no side-by-side at large size, stacked at small size** for MONITOR or the Chain. MASTER and DEVICE keep the same horizontal relationship at every window size; do not add adaptive row/column transitions. The Chain keeps fixed 164px width and steady label coordinates whether vertical scrollbar is visible. Only the **scroll position and available empty space** may change. For cramped editor heights use scrolling inside MONITOR; do not move widgets to an alternate hierarchy. This is mandatory FQ #80 physical UI evidence at 1184×688 and Windows 150% scaling.

## 8. PO-approved visible name + semantic module pictograms — 2026-10-25
**Name:** `Master Audition` (Title Case) is the sole end-user-facing module name. The user expressly rejected `MONITOR`/ `Monitor` as a visible label. Existing internal `MASTER_MONITOR`, `TerminalMonitorViewModel`, audio state keys and ownership identifiers remain unchanged for compatibility. Tooltips and visible status messages should use Audition or Master Audition as appropriate.

**Pictogram grammar:** each chain row has its own distinctive monochrome 16×16 Canvas glyph, tinted with its frozen *family* color. The previous BYP-badge slot is NOT reinstated; the 8px configuration LED and 8px error indicator remain separate and fixed. At 164px chain width no title font-size reduction, no label/LED shifting on bypass or scroll. Module pictograms:
- Input Gain: gain fader · LEVEL/GAIN Ice Blue `#6F9FB3`
- Parametric EQ: response curve · TONAL/FILTER Azure `#4E86C6`
- Compressor: threshold/knee curve · DYNAMICS Copper `#C4774A`
- Stereo / M-S: linked channels · STEREO/SPATIAL Sea Green `#4A9A88`
- True Peak Meter: measurement bars · ANALYSIS/METERING Steel Blue `#6683A1`
- True Peak Limiter: ceiling cap · DYNAMICS Copper `#C4774A`
- Master Audition: headphones · WORKFLOW/UTILITY Slate `#718092`

No family or state is identified by color alone. Distinct shape gives module identification; family hue groups related modules; bypass dims the row and glyph rather than moving controls. The pictogram hues for M16/M17 do not replace their independently frozen editor geometry/semantics. Preserve the first 16px slot when adding future modules such as M18 (Delivery/Bronze). Native 1184×688 Windows screenshot/150% gate is still required: CI green is not physical qualification.

**Focus CI:** [#823](https://github.com/casaferla/RGS-MasterLab/actions/runs/40216767401) submitted PR HEAD `b218558a736fd82d000072cfea782dab876b9b6e`; status must be verified live. The promoted candidate includes the exact approved visible name, per-module glyphs, fixed-width Chain geometry and focused unit/QML static/native assertions. This document update is only on no-CI staging, to avoid an extra CI.


## 9. Product Owner additional FQ #80 acceptance — draggable docking (2026-10-25)
**APPROVED/REQUIRED for FQ #80, not just a prototype.** Every currently selectable editor, including Master Audition, must support the same two docking routes:
- Deterministic `View → Dock/Undock` command and floating window Dock button as fallback/accessibility.
- A *small fixed grip in the existing editor header*: deliberate drag outside the dock and RELEASE creates/moves the SAME editor into the one floating native Qt Window; no accidental dragging of curves/sliders/text.
- A corresponding grip in the floating window's header: drag the window until the actual fixed central dock slot highlights; release there to re-dock. A drop outside leaves it floating. Esc during either gesture cancels without changing DSP, transport or unqualified provenance.
- Stable dimensions of all other Main controls and the 164px Chain throughout; geometry does not adapt between side-by-side and stacked layouts. The cyan highlighted target appears only while a detached module's grip is over a qualified editor slot.
- On a multi-monitor Windows desktop, compare QQuickItem.mapToGlobal logical desktop coordinates against floating `Window.x/y` and header-grip coordinates. DIPs/scaling 150% are test obligations; **a standard QML DropArea cannot automatically accept a moved native Qt window**, so the docking coordinator owns this explicit native-window coordinate check. Required physical test covers dual monitors and DPI scaling; tests simulating `finishDockDrag` prove the admission decision, not true native gesture quality.
- Preserve same QObject/editor instance, visible telemetry, Qt state and selected module instanceId on repeated detach/reattach. Starting an undock is never a DSP edit and cannot trigger a render, M17 install or M16 LIVE claim.
- **The Source WAV file drop zone is postponed**, and is entirely separate from native window docking. M18 remains postponed too.
- Current shared-host design is one floating editor at a time. The PO approved generalized docking for every module type; simultaneous multiple detached windows are not yet accepted/proven by this shared-host implementation and must not be misrepresented as supported.

## 10. Later family-indicator supersession
PO subsequently approved one module-family pictogram **in place of**, not alongside, the old configuration LED. The 16px pictogram is dim at Default and luminous in the same family hue at Manual Non-Default. Bypass remains independent, no additional horizontal slot; 8px red error indicator remains. This supersedes earlier prose in §§1,6,8 mentioning a separate configuration LED. The pictograms and this behavior were in the #825 green candidate.


## 11. PO original seven-icon visual reference — 2026-10-25, superseding #823 shapes
The Product Owner supplied two visual references in the operating chat. **FIRST reference is definitive for the seven existing modules**; the already-green #832 icon shapes were explicitly rejected as visually different. Implementation must follow their distinctive silhouettes, not merely approximate their accent colors. The SECOND reference is a provisional candidate gallery for future modules: no implementation or milestone authorization is implied.

Frozen existing-module glyph map in the ONE `StudioIcon.qml` shared QML Canvas (18×18 optical geometry rendered in a stable 16×16 Chain slot):
| Module | Approved first-reference silhouette | Existing icon kind |
| --- | --- | --- |
| Input Gain | Three **horizontal** offset mixer-slider tracks | `module-gain` |
| Parametric EQ | Three **vertical** offset mixer-slider tracks | `module-eq` |
| Compressor | Five rounded, graduated **audio amplitude bars** | `module-compressor` |
| Stereo / M-S | Upward **Y-branch to two opposing arrows** | `module-stereo` |
| True Peak Meter | Three **filled-height measurement bars** | `module-peak-meter` |
| True Peak Limiter | **Shield with check mark** | `module-limiter` |
| Master Audition | Rounded **headphones with separate earcups** | `module-headphones` |

The previous geometry (single fader, EQ response curve, compressor transfer curve, two overlapping stereo rings, limiter ceiling curve) is superseded, not a second optional style. Preserve 7 existing family colors, module identities, 16px slot, configuration-based Default opacity 0.37 versus Manual 1.0, independent BYPASS dim multiplier 0.7, accessibility tooltip, red error indicator and no separate config LED. All UI drawing stays presentation-only; no DSP rate/process/owner code may change for an icon update.

**PROVISIONAL future-icon triage** (not DSP roadmap nor implementation approval): keep DC Offset, Dehum, Gate, Expander, Imager, Mono Bass, Width, Tilt EQ, Low/High Shelf, High Pass, Low Pass, Notch, Phase, Ducker, Metering; refine Declip, Clipper, Multiband Compressor, Transient Shaper, Delay, Reverb, Limiter (legacy); redesign Saturator, Exciter, Chorus, Flanger, Vibrato. Keep names stable until the PO approves an actual future module.

Evidence obligations: real Windows Main screenshot at 1184×688 with at least existing modules and exact 16px glyphs, plus 7-icon Qt Canvas real-rendered gallery at 16px and magnified scale; gallery is **not** proof that seven modules are present in the production Main. Windows physical DPI 150% and original 7-row product screenshot remain FQ gate. Never use image-generated mockups as code/evidence.


## 12. FQ #80 concise audio/Windows acceptance sequence — PO authorizes dispatch separately
This is a **readiness/test plan** and does not launch an FQ, merge, declare accepted audio, or expand scope.
1. **Automatic code proof, independent of physical DEVICE:** focused Windows matrix has 36 SOURCE→MASTER offline M17 render paths, 36 actual IEEE-F64 WAV writer encodes and 36 MASTER→DEVICE PCM conversions; verify bounded marker `RATE_GRID_36_MASTER_36_WAV_36_DEVICE=CONFIRMED` at exact SHA. Source initial DEVICE refusal must be tested at six frozen Fs × mono/stereo, and the return-to-NO_SOURCE state must not retain a ghost transport. No test on a fake sink is physical audio evidence.
2. **Physical Windows audible MANUAL (eligible SOURCE)**: load real WAV at 44.1/48 kHz; verify selected SOURCE and PROCESSED each have an actually accepted Windows sink, recorded RealizationId, Play/Pause/Stop and cue; exercise M17 Active, explicit Prepare, actual monitor playback, ceiling and bypass A/B with provenance/GR/meter truthful. A UI button pressed without sink ownership is NOT a PASS.
3. **Physical Windows initial SOURCE-format refusal (P0)**: with genuine 88.2/96/176.4/192 kHz (and 44.1/48 where mismatched device reproduces), preserve accepted SOURCE and independently rendered PROCESSED even when default endpoint rejects playback; assert `NO_SOURCE / UNAVAILABLE / no audible RealizationId` rather than inventing MANUAL. In **Master Audition**, explicitly choose an independent frozen MASTER rate (including cross-rate e.g. source 192k → MASTER 48k or 192k), Prepare MASTER; inspect immutable output-rate/master count; choose Start Device Audition only after OS device-rate and memory preflight, user opt-in for CAUTION; hear PCM and exercise Play/Pause/Stop, MASTER seek; Close DEVICE must return exact MANUAL when it really existed or the original clean NO_SOURCE otherwise. Check no ghost Playing/device Hz after return. Fail closed for wrong/foreign owner.
4. **Real WAV output:** verify representative mono/stereo and cross-rate WAV files with correct sample rate/channel count, duration and unmodified immutable master source. The IEEE-F64 WAV evidence is a test-internal writer; it is **not** an approved M18 final PCM24+dither delivery. M18 remains deferred.
5. **UI physical pass:** 1184×688 minimum and Windows 150% scaling; seven original family pictograms Default/Manual/BYPASS, MASTER/DEVICE always side-by-side, full seven-row Chain; dock/undock/dock by both action and native drag grip for every editor; physical dual-monitor/mixed-DPI target hit test and Escape cancel; screenshots of actual Main and real undocked window. Never substitute isolated Qt fixture screenshots for this physical PASS.
6. **Final strict gate:** only the PO may dispatch/accept FQ #80 on a specific verified PR candidate SHA after both focused jobs pass, and authorize any merge/Gate M/L. The cold FQ build may create a packaged staged Windows `RGSMasterLab.exe`; do not call the package usable without actual GUI/DEVICE acceptance.


## 13. Historical real-song limiter P0 regression — mandatory FQ #80 listening evidence
Beyond the quick 36-rate software fixtures, repeat the **real-duration** historical user failure scenarios: SOURCE 44.1 kHz, Ceiling −3.9 dBTP and particularly Ceiling −12.0 dBTP (previously produced a nonconforming result after a long Prepare). At minimum use representative real multi-minute mono/stereo WAV input and repeat the same ceiling to rule out intermittent or stale-identity behavior. Independently verify measured true peak against the chosen ceiling, show a truthful out-of-ceiling magnitude and occurrence count/duration if it exists, and do not call nonconforming output a conforming master. A rejected full-song limit is an FQ P0 failure requiring a focused audio correction, not a user-config workaround or UI-only green badge. Ensure PREPARE progresses without freezing the app, Preview retains the correct source/target identity, and Master Audition hears only installed/owned DEVICE PCM after rate conversion. FQ cold release and real Windows physical audition are both needed; tiny test renders alone do not close this gate.
