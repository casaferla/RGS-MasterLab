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
