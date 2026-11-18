# RGS MasterLab — formal re-audit PREPARED / PROCESSED / REFERENCE
Date: 2026-11-18. Owner: L1 Single-Writer Epoch. Status: **CODE REVIEW AND REGRESSION TESTS ADDED; CI AND PHYSICAL QUALIFICATION STILL REQUIRED**.

## Mandate and authority
Re-audit the complete three-target listening, waveform, loop, M16/M17/M18/M19–M21, source-reference lifecycle and Qt/Product state path after PO-authorized independent REFERENCE Waveform and Region. This is a preventative engineering audit, not authority for M24 PASS, FQ, release, or main merge. Base physical #64: verified M17 retains identical Master PCM across REFERENCE→PREPARED→PROCESSED and Preview without rerender. Initial new UI #65 and focused #66: two M23 source-contract reds due to outdated static assertions that REFERENCE loop must be disabled. Other six? #66 five of seven jobs passed; its M23 Windows/Linux jobs did not execute native Qt tests.

## Covered code and dependency boundaries
- `app/src/audition_source_selector.{hpp,cpp}`: target authority, actual device transitions, Gold aliases, source- vs Gold-cue, M17 exact capture/parking, playback state.
- `app/src/gold_selection_view_model.cpp`: probe-before-commit, selection revision, Project and SOURCE invalidations, retained prior file identity.
- `app/src/playback_transport_view_model.{hpp,cpp}`: source/target seek, loop service calls, M17 terminal owner, anonymous monitor exclusion, device snapshot.
- `app/src/audition_region_view_model.{hpp,cpp}`: separated source and Reference half-open ranges, true audio-loop installation, reset, thread confinement and QML timed editing.
- `app/src/terminal_product_activation_coordinator.{hpp,cpp}`, `terminal_private_product_session.cpp`: READY/INSTALLED/COMPARISON_VERIFIED, immutable M17 cache rearm without render, exact manual identity.
- `platform/windows/src/internal/m18_owner_mutation_fanout.hpp`: Product epoch, M17 verified-epoch continuity for transport-only target changes, mutation admission.
- `app/src/main.cpp`: all owner wiring, independent reference waveform generation/selection, M16 LIVE consumed-prefix gate, M19–M21 owner reconciliation, SOURCE-only M17 loop capture and Qt Product installation.
- `ui/qml/Main.qml`, `AuditionSourceSelector.qml`, `DspEditorHost.qml`, `DspChainSelector.qml`, `StudioIcon.qml`, `StudioButton.qml`: target routing, waveform, Region, monitor return, fixed label, export message.
- Existing `tests/integration/app/test_project_session.cpp` and M23 file-fault/static guards: preservation of Project/Export identity.

## Mandatory invariants re-reviewed
1. SOURCE cannot equal or alias REFERENCE. Failed probe or preflight may not replace an accepted SOURCE/REFERENCE.
2. PREPARED/PROCESSED are exact accepted SOURCE-processing-rate realizations; REFERENCE is a separate file rate, frames, cursor, waveform, region and loop. No Gold PCM may enter the MANUAL DSP path.
3. Changing targets or audition seek/loop cannot publish a new PROCESSED RealizationId, rerender DSP, or mint an M16/M18 proof. M16 LIVE only binds actual consumed PROCESSED sink owner; REFERENCE audio has no M16 LIVE authority.
4. A SOURCE-region loop remains solely SOURCE-owned. ReferenceRegion never writes the SourceRegion or M18 source-region Product epoch. Gold loop must check Gold device owner, actual file duration and absence of anonymous Master monitor/terminal gate.
5. M17 is final after Final SRC, never REFERENCE. Render from REFERENCE may first select accepted PROCESSED, but exact PCM snapshot/owner checks may still refuse safely.
6. A verified M17 Master is parked before leaving PROCESSED; returning can reuse the SAME immutable candidate without invoking the M17 renderer. Automatic preview restore requires exact accepted source id and current verified session; it cannot grant new verification.
7. Master Audition owns anonymous DEVICE exclusively until explicit safe return; a target button must not seize its sink. Explicit recovery is respected.
8. M18 proof and export consent remain exact Product epoch dependent; no new REFERENCE loop creates a verified Master or export Ticket. Gold replacement can change comparison metrics, but does not mutate SOURCE WAV bytes.
9. M19/M20/M21 cache reconciles with actual SOURCE, Gold and PROCESSED owners and versions, with queued Qt events, not synthetic readouts or implicit auto-match.

## Findings corrected in the writer (regression-prevention)
- **A01 STALE TEST**: #65/#66 M23 Linux/Windows source gate assumed Reference Loop permanently disabled. New source assertions require an independent region, exact device-checker and regression tests; no M23 qualification assertion is removed.
- **A02 PREVIEW REARM RACE**: synchronous `auditionSelector.changed` can rearm `READY_TO_INSTALL` before QML calls the new resume method; old candidate returned false on `intent_`. Now both ALREADY_REARMED and PARKED states are handled with exact same owner checks and no new rendering.
- **A03 REFERENCE REPLACEMENT FAIL**: a probed replacement could fail `prepare_file` AFTER mutating the selector; GoldSelection would retain prior resource while selector held new resource. New fail-closed rollback restores old reference identity, with no claim of continued audio after device retirement.
- **A04 REFERENCE THREAD SAFETY**: unlike SOURCE with installed M18 admission, independent ReferenceRegion had no thread gate. All Region mutations now require Qt owner thread regardless of M18; explicit foreign-thread regression included.
- **A05 ANONYMOUS DEVICE HIJACK**: `!sourceDerivedActive_` alone was insufficient to authorize Reference Loop while an anonymous Master monitor exists. The Product now supplies an exact `gold_active && !monitor_exclusive` checker; SERVICE snapshot, track duration, loop bounds and terminal ownership are rechecked before set_loop.
- **A06 SOURCE LOOP CROSS-TARGET**: Reference loop reinstalls on return to GOLD only, and original Source loop reinstalls on return to PREPARED/PROCESSED. Separate test covers REF→SOURCE→REF and source region preservation.
- **A07 M17 RENDER ENTRY**: target REFERENCE treated as a normal listening state; explicit Render Master selects accepted PROCESSED before capturing terminal intent, never bypassing the native authoritative guard.
- **A08 MASTER A/B UX**: from independent targets, reinstallation of a retained verified Preview is explicit, source cue preserved after STOP and same accepted owner checked. Listen to Master target switching first returns through the monitor's genuine retirement.
- **A09 M18 STATUS DISPLAY**: bounded 120px status uses full readable short labels, while the full native failure reason remains available in tooltip.
- **A10 UI ROW BASELINES**: all DSP titles anchored independently from two-line statuses, not moved by M17 state wrapping.

## Added / adjusted verification
- `tests/integration/app/test_audition_region_view_model.cpp`: native Reference loop isolation, SOURCE loop noninterference, foreign-thread denial, anonymous-monitor rejection, clear/reset semantics.
- `tests/unit/app/test_audition_source_selector.cpp`: active Reference replacement, rollback on failed device install, repeated SOURCE vs Gold loop reinstallation.
- `tests/unit/app/test_terminal_product_activation.cpp`: real M17 verified candidate parked across Gold+Prepared+Processed and automatically restored via the native API, without a new renderer invocation.
- `tests/ui_smoke/test_source_metadata_panel.cpp`: both independent load actions and ReferenceRegion object provided to real Main fixture.
- `tools/qualification/m19_m21_gui/test_product_navigation_guards.py`: source architecture guard now enforces independent reference owner, loop timebase, Qt owner and cached M17 return.
- `.github/workflows/r1d-p2-i2-product-gui-smoke.yml`: Windows CTest runs `rgsml_audition_region_tests` in addition to selector / transport.

## CI and physical qualification are separate gates
This audit is **not** a claim that any newly changed C++ code compiled or passed native tests. Promote a single exact writer SHA to the focused candidate after #66 reaches terminal, run Windows+Linux M23, M17/M18, P2 and real Qt startup. Read bounded failure evidence, correct actual defects without bypassing the gate, then re-run. No forced CAS, no automatic FQ.

Real Windows PO validation still needed: load Source/Reference of different rates and durations; Shift-select Reference region; toggle Loop; switch repeatedly through all targets while PLAYING and STOPPED; replace Reference while selected; induce/observe failed file transaction without modifying originals; render M17 from Reference; Preview A/B; Listen to Master exit via all three buttons; export/read back PCM24/F32; verify unchanged SOURCE/REFERENCE; reopen project. All actual audio observations are separate from static/source checks.

## Residual boundaries for M24
- Formal Gate G/M/L and M18 physical A01–A15 remain independent. Full 17-step real remaster and PO listening verdict cannot be self-certified.
- Gold audition region is intentionally session-local; canonical Project persistence concerns SOURCE region and accepted chain; if Gold-region persistence becomes a requirement, authorize a separate versioned model change.
- P2 opt-in/persistence in ordinary saved Product projects remains a separate L1 integration issue and must not be smuggled into the three-target audit.
- No `main` merge, production claim or phase-6 numerical release has been authorized.

**Verdict:** audit findings addressed in code; readiness **PENDING EXACT-SHA GREEN CI AND PHYSICAL REPLAY**, never blanket PASS.
