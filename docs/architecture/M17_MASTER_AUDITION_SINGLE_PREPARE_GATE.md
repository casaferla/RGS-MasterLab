# RGS MasterLab — P0 M17 → MASTER AUDITION SINGLE-PREPARE / MASTER RATE GATE

**Tracker (OPEN BLOCKER):** https://github.com/casaferla/RGS-MasterLab/issues/62
**Authority:** Product Owner decisions 2026-10-23 and 2026-10-26 (already agreed), recovered and elevated to mandatory OPEN integration gate 2026-10-27 after physical FQ #83 test.
**Status:** PO APPROVED BEHAVIOR / PRODUCT NOT YET IMPLEMENTED OR PHYSICALLY QUALIFIED. This is a recovery of existing decisions, not permission to change frozen DSP algorithms or merge to main.

## Normative unchanged sources (Drive RGS ContentBox)
- Master Output Rate / Final SRC / M17 consolidated contract 2026-10-23: https://docs.google.com/document/d/13fIk06CgiPLWcaW3KAZPCUoNFN5ffh6NHlVXK1sfA_8/edit
- PO Memory-Aware Master Rate Advisor 2026-10-24: https://docs.google.com/document/d/1vcYPUAF5rBqxPlQMM9PUkehnCGDvi9ikA7HuDXUSDu8/edit
- M17 Explicit Preparation PO 2026-10-22: https://docs.google.com/document/d/18uFEzMGeK3KPmotcYb5yzocPomtz3AwcH1ti5rDMGnk/edit
- Operational continuity 2026-10-24: https://docs.google.com/document/d/1dM8VuadXTAVVZfNDYOPll7qnOKL1eEMnk8T7p4VE9cU/edit

## One master, two rate domains for output
1. SOURCE_PROCESSING_RATE remains the actual sample rate of the input/manual DSP processing.
2. TARGET_MASTER_OUTPUT_RATE is a persistent **Project/Render** authority (initially SOURCE_RATE if supported) with a synchronized edit control **already in M17 before Prepare**. Its six selectable values are 44100, 48000, 88200, 96000, 176400, 192000 Hz. Never treat it as a Limiter-local temporary setting.
3. AUDITION_DEVICE_RATE comes exclusively from the actual Windows playback endpoint, is displayed in Master Audition, and is NOT a master target or render rate. No device-rate selection is allowed to silently change the master rate.
4. For all ordered 6x6 SOURCE→MASTER mappings: SOURCE/manual DSP -> authentic Final Output SRC (or bit-transparent identity) -> M17 frozen limiter -> whole-resource M16 verification -> authoritative immutable MASTER PCM at TARGET_MASTER_OUTPUT_RATE. Only then branch to output encoding at MASTER rate and separately to DEVICE SRC for audition. No post-M17 SRC of authoritative WAV, and never silently substitute the restricted 44.1↔48k playback SRC for unsupported final SRC.
5. **Exactly one authority-bearing full-resource Prepare per unchanged content/chain/M17 parameters/master rate.** Master Audition must BORROW/REUSE the exact prepared verified M17 MASTER candidate; no second M17 DSP/M16 global verification or independent `Prepare MASTER` render for a still-valid candidate. Device installation may independently derive DEVICE_RATE monitoring PCM and verify memory, but that is NOT another master prepare or WAV certificate.
6. Switching to Master Audition, selecting listen, switching/renegotiating the device, A/B Manual↔Prepared, or returning from a floating view must not erase the master or rerun full-resource M17 when all authoritative revisions remain equal. Genuine source/manual DSP change, M17 Ceiling/parameters, target master rate, invalidation, cancel/stale or failed ownership may invalidate according to established fail-closed contracts. No global no-op stale invalidation.
7. A verified master and a measurable nonconforming master must remain distinguishable; do not certify or silently expose measured nonconforming PCM as compliant. Real error code/state and overshoot telemetry must survive a refused monitoring/device stage.
8. UI gating: user-facing M17 Prepare -> READY -> Preview Prepared and Master Audition listening should share authority and true provenance; eliminate the confusing SECOND `Prepare MASTER` request only when its replacement actually consumes the M17 owner; never cosmetically hide the defect first.
9. Default master rate is SOURCE (when supported), not hardcoded 48000 from Device/UI. Render rate choice must precede M17 Prepare and be persisted. Consistent memory advisor/availability labels; do not claim 30 SRC conversions materialized just because six menu options exist.

## Current code audit (FQ #83 source: e6d23614aa549f597a228140c86c3380db0b5734)
- `app/src/terminal_product_activation_coordinator.cpp`: M17 Prepare invokes `session_->request_private_for_device(true)` and probes device format, tying M17's full render to DEVICE rate. This contradicts the independent final-master-rate authority for some source/device/target combinations.
- `ui/qml/MonitorEditor.qml`: independent 6-item ComboBox with `currentIndex: 1` (48000) and its own `Prepare MASTER` action. No proven shared `TARGET_MASTER_OUTPUT_RATE` property/Project binding here.
- `app/src/terminal_monitor_view_model.cpp::requestMaster` calls `session_->request_private_for_final_master(rate)`. `app/src/terminal_private_product_session.cpp::request_private_for_final_master` delegates to `request_private_impl(...masterOnly=true)` and creates a NEW `renderer_->request_private(rate)` job; does not reuse M17 ready PCM.
- `app/src/terminal_monitor_view_model.cpp::can_prepare` requires session status IDLE; after M17 output is installed session is not idle. This can block Master Audition/require dismiss+new render. No assumption about the exact user's error message without a screenshot/status.
- `app/src/main.cpp` wires both to the *same* TerminalPrivateProductSession but not the same reusable master owner: sharing a session object alone does not mean reusing prepared master PCM. Some older comments describing visibility are stale; trust actual QML/ViewModel bindings.
- Source user testing after FQ #83: short WAV 44.1 and 48 kHz M17 Prepare + Preview Prepared OPERATIVE in limited physical tests; transition to Master Audition reported not functioning without additional prepare. Point 1 editor geometry and point 2 multiwindow Dock/Undock already PO ACCEPTED, keep closed.

## Mandatory implementation and qualification acceptance
- Create ONE Project/Render target-master-rate authority and synchronize the M17 widget. Persist it and implement strict invalidation; no duplicated independent MONITOR selector authority. Preserve single DSP writer; no second PCM pipeline.
- Refactor ownership handoff so M17 produces one immutable authoritative master, may retain it across Preview Prepared, and Master Audition derives DEVICE PCM from SAME owner (no new M17 render). Reject wrong source/RealizationId, stale revision, wrong target Fs, aborted render, output loop/seek ownership, and nonconforming certification claims. Explicit monitoring memory guard remains effective.
- Instrument test renderer call counts and immutable owner IDs: M17 Prepare exactly once; then Master Audition READY/install/cancel/return, device rate 48k while MASTER=44.1 or 192k, device reconfiguration and A/B; M17 worker count remains unchanged. Changing TARGET_MASTER_OUTPUT_RATE or M17 parameters triggers proper invalidation and one new Prepare, no silent re-render of same target. CI automated 6x6 master mappings/6 identities and 30 actual Final Output SRC with oracle, not just planner.
- Require genuine Windows FQ run by PO only and physical tests 44.1, 48, 192 sources (incl 44.1→192 MASTER on 48k device), Ceiling -3.9/-12, preview audition and monitor, real WAV output rate, seek/loop/transport/memory/PCM identity. A green Focused or earlier FQ #83 NEVER closes this P0.
- M18 remains blocked until this M17↔Master Audition integration gate is addressed and explicitly accepted. Cosmetic large Dock glyph polish is separate non-blocking backlog; no cross-scope DSP alterations.
- **No main merge or PO Gate M/L without explicit PO decision.**
