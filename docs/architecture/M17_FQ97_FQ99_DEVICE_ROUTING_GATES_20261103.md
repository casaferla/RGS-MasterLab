# M17 Windows DEVICE Routing — FQ97/FQ98/FQ99 Gate Contract
Date: 2026-11-03. No merge into main. Product Owner authorization is mandatory.

## Source of truth
- SOURCE/PREPARED/PROCESSED PCM and immutable MASTER PCM retain distinct rate and frame domains.
- Six eligible Fs: 44,100; 48,000; 88,200; 96,000; 176,400; 192,000 Hz.
- 36 SOURCE->MASTER rate pairs (6 identity + 30 nonidentity). Playback to physical DEVICE is an **independent** rate choice, and must not silently change the selected MASTER.
- The two historical playback FIR and original fourteen Q0 FIR are the only authenticated playback kernels. Never synthesize filters or bypass SHA, owner/epoch, memory or Windows endpoint checks.
- A physical sink format is reported by Qt; it is not proof of the DAC clock. Device opens, underruns and hotplug still require physical tests.
- M16 True Peak Gate G remains open; all hardware/live claims remain unqualified until their own proofs are complete. Never change protected M16 arithmetic or audit hashes to pass a UI gate.

## FQ97: SOURCE/PROCESSED default DEVICE negotiation
Scope: `platform/windows/src/internal/playback_support.*`, `windows_audio_playback_service.cpp`, native Windows playback support regression.
Admission:
1. Query six rate options, stereo/mono each as appropriate, IEEE F32 and PCM16, on one current default endpoint.
2. Prefer exact SOURCE format if actually supported; otherwise observed Windows preferred format if supported; otherwise deterministic known six-rate fallback from actually supported candidates.
3. Nonidentity requires a versioned, SHA-verified installed FIR *before* candidate sink installation. A missing/tampered FIR or endpoint switch remains an explicit error.
4. Do not mutate selected MASTER, SOURCE owner, cue/Loop or True Peak evidence.
5. Native test: 144 source/device/encoding/layout combinations; exact-preference precedence, unavailable-format and malformed-query guards. Test Windows build and Linux core.

## FQ98: Return to Source must be recoverable
The current monitor must not be retired solely to discover that original manual PCM can no longer be installed. Preflight the exact original immutable MANUAL view **on the actual Windows audio worker**, using the same FQ97 selector, installed FIR and current endpoint. The preflight may only allocate a temporary candidate queue, not create/start/stop a QAudioSink. Check original monitor owner again before conditional retirement. If preflight fails, report the category; leave monitor and exact retained MASTER installed for retry, never discard the source epoch.
- New native fake-sink regression: refused preflight leaves same monitor pointer/position, zero retire calls, zero manual restore calls; subsequent successful preflight permits exact Return.
- The final manual-owner/clock/loop verification after retirement is STILL mandatory; a hotplug between probe and actual restore can fail, and must be diagnosed rather than hidden. Do not claim unconditional atomicity across Windows endpoint changes.

## FQ99: measured NON-CONFORMING cross-rate audition — NOT YET IMPLEMENTED
A device-converted nonconforming MASTER must have a distinct **typed unqualified** owner. Do not reuse VERIFIED `TerminalCandidateGate::StagedCandidate` or M16 LIVE/trim certificates. The measured peak/ceiling evidence refers only to the original MASTER-rate PCM; a DEVICE-rate SRC version is for audition only. Required before enabling cross-rate Preview Anyway:
1. Captured `StagedMeasuredCandidate` and exact immutable master/device PCM epoch owners, separate from verified stage.
2. Six-rate DEVICE preflight and allocation, RAM limits and cancellation, endpoint identity, install and return/Loop/seek map.
3. Explicit unqualified button; verified Master Audition must never be enabled by measured PCM. No fake GR/M16 LIVE identity.
4. Regression full 6x6 source/master and six device rates, malformed rates, absent FIR, cancellation during SRC, selected MASTER change, old SOURCE/LOOP epoch, device change before install/after admission, zero/no sink, rollback and explicit Return.
5. Physical Windows 48 kHz / PCM16 end-to-end testing against a 3:03 stereo source at 48->48 and 48->88.2; verify that false Ceiling misses remain clearly nonconforming after any device SRC.

## Qualification sequence and merge guard
- FQ96 #41721017928 dual-OS PASS, head 250f41b.
- FQ97 PR #61 is Draft, code first tested at head b7c0459 in Focused #41725490240. Do not promote newer staging changes while an earlier run is in flight; get terminal conclusion first.
- FQ98 staged separately, not considered implemented until its own Windows+Linux CI and physical failure/recovery check.
- FQ99 must not be rolled into FQ97/FQ98 to mask gaps. No M17 full qualification, M18/M19/M20/M21 integration, CI reset, or main merge before a written owner/transport/matrix acceptance review and PO GO.

## Gate checklist
- [ ] FQ97: exact head, dual-OS C++ build, 144-case device matrix, SHA-pinned FIR proof, physical Windows SOURCE playback
- [ ] FQ98: dual-OS native fake-sink recovery, actual Windows Return, user cue/Loop, no loss of monitor on rejected SOURCE
- [ ] FQ99: typed measured cross-rate device audition, isolated consent, all negative/invalid/stale cases
- [ ] Full FQ: six rates, all three project/device/monitor domains, playback/Return/Stop/Pause/Loop, authenticated gain history without fake curves
- [ ] PO explicitly authorizes main merge only after completed Gate reports
