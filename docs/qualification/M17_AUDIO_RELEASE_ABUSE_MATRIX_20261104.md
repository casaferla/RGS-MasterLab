# M17 — AUDIO RELIABILITY RELEASE / ABUSE MATRIX
Owner: Product Owner. Date: 2026-11-04. **M17 NOT CLOSED. No main merge.**
Terminology: **Focused CI** is GitHub `PR Focused CI` (real workflow run number #991, #992, ...); **FQ** is the official manual-only `Windows Full Qualification`, exact SHA, `cold`. A physical Windows listening gate is separate. Historical FC97–100/FQ97–100 labels are not workflow run numbers or formal FQs.

## Evidence hierarchy (not interchangeable)
1. Unit/property/memory test (emulated fake device) -> C++ local safety only.
2. Focused CI (Windows+Linux), no skipped required native cases -> same SHA build/test proof, **not** real DAC.
3. Official cold FQ (exact 40-character SHA), Debug/Release, strict FP, source provenance, full CTest and deployed executable -> release candidate software proof, **not** physically audible DAC.
4. Physical Windows hardware matrix, accepted audio files/telemetry and failure recovery -> output-device proof.
5. Fresh Windows installation/distribution (no hand-copied FIR) + extended burn-in -> consumer release proof.

## Source/MASTER/DEVICE matrices: each axis independent
- Six rates: 44,100 / 48,000 / 88,200 / 96,000 / 176,400 / 192,000 Hz.
- All **36 SOURCE→MASTER** render choices (including same-rate); all **36 MASTER→DEVICE** offline SRC plans; the combined path space is **216 SOURCE/Master/DEVICE triples** for a fixed device rate choice. Then mono/stereo and IEEE-F32/PCM16 endpoint formats, plus actual supported/unsupported Windows masks.
- Existing isolated source/rate and monitoring tests cover portions of these matrices, but **no existing PASS proves all 216 paths audibly on Windows**. Mandatory build test tracks coverage explicitly and does not count 36+36 as 216.
- For each sample rate, test exact hardware F32 priority, 16-bit fallback, preferred-rate fallback, all-device-rates-rejected, endpoint change between probe/conversion/install, unsupported non-six-rate SOURCE exact-playback preserved, missing/corrupt SHA-pinned FIR fail closed.
- Duration/geometry: zero, one frame, 7, 193, 4096, exact one second, incomplete final block, 180 and 300 seconds; actual PCM content and bitwise block invariance versus frozen CPU golden. Long tracks may use 180s projection; that is NOT a physical 180s sound test.

## PCM and DSP assertions
- Independent mono and stereo; left/right polarity-separated impulse, silence, DC, nearly 0 dBFS sinusoid, intersample overs, subnormal/finite boundary, abrupt transient, nonintegral frame-ratio length, no NaN/Inf, no unwanted channel crossfeed.
- The MASTER remains byte-for-byte unchanged across listening, source/DEVICE rate changes, M16 evidence and Loop/Seek; no borrow of SOURCE-epoch bytes or fake M16 LIVE/GR witness.
- M17 nonconforming: independently measured overshoot retains exact original MASTER-domain Ceiling, peak, reason, epoch and a distinct unqualified PCM owner. A change in DEVICE Fs does not turn it into VERIFIED. No auto-play without explicit Preview Anyway consent.

## State-machine negative/failure matrix (each failure can occur BEFORE and AFTER monitor retirement)
- Prepare, M17_CORE, M16_PRIMARY, optional TRIM/M16_SECONDARY, monitor SRC, DEVICE install, start, pause, stop, seek, Loop, Return.
- Inject cancellation at each stage; old worker callback, duplicate completion, Qt change signal reentrance, OOM, exceptional callback, source-selection/Loop/MASTER-rate change, Quit while pending, second Prepare, device hotplug/default-device switch, no physical sink, zero-byte write, short write, underrun and halted sink.
- After ANY fail: must prove exact owner, terminal status and origin, no false Play/READY/GR, no silently consumed audio, no lost prepared MASTER because Windows SOURCE refused.
- **Issue #73 post-retirement recovery**: preflight PASS then SOURCE restore FAIL; preflight retry FAIL; forced exception; Windows claims install success but wrong ID; alien sink occupies device; true exact owner restored; paused/playing/looped transport recovery; original SOURCE becomes stale; Qt Return button and Main menu call same command. Never erase a foreign sink. Never automatically retry as side-effect of SOURCE Loop change, Cancel or Quit.
- Original user finding `MONITOR_RETURN_MANUAL_SINK_NOT_EXACT` needs hardware diagnostics identifying the mismatched member (state, phase, ID, cue, duration, Loop) rather than a generic error. Do not relax identity proofs to hide it.

## Performance/underrun gates
- Reproduce 192→48 and 44.1→48, 48→48, 48→88.2 on physical 48-kHz/16-bit endpoint. Test first second, full 3+ minute song, loop boundary, multiple successive A/B returns, seek to start/mid/end, 25x repeated Return, stop/start during conversion.
- No crackle, slowdown, stalled playback, ghost meters or missing loop control; output device's active PCM frame clock vs wall time must remain coherent within a separately specified clock tolerance. A fast CPU SRC unit benchmark alone is NOT proof of Qt QAudioSink scheduling.
- Memory: 192-kHz 5-min stereo MASTER/48-kHz DEVICE has a computed two-buffer floor of 1.152 GB; check peak RSS, repeated allocation, cancellation and failed preflight/stop; prevent UI freeze, unbounded allocations or bad_alloc.
- Windows 10/11 x64, monitor 4K+FHD/mixed DPI, 16/24/float32 backend formats as physically supported; do not invent support for devices which do not advertise it.

## Consumer-distribution gate
- Fresh Windows machine/VM, clean package or installer, no local developer Q0 FIR directories, confirm all 14 pinned kernels and historic playback assets included or intended combined distribution. Test reinstallation/upgrade, missing asset, read-only install path, Unicode and long file names, mute/volume, alternate device default and project reopen with SOURCE/MASTER provenance preserved.
- Save and reopen .rgsmaster, produce final offline audio only after full true peak and dither/export gate, confirm output sample rate, sample format, duration, stereo channels and measured TP with an independent reader. No fake WAVE export qualification from M17-only UI.
- If DEVICE is gone during SOURCE recovery, GUI must remain responsive and provide safe explicit recovery / close handling; do not trap the user indefinitely or silently discard data.

## Release gates — ALL required, no optimistic status
- [ ] Targeted tests for issue #73 PASS on exact candidate SHA, both OS.
- [ ] Fully implemented and tested unqualified cross-rate Preview Anyway, with source-MASTER-DEVICE owner isolation.
- [ ] All expected native cases execute (inspect skips/ctest inventory/artifacts; review absent assets explicitly).
- [ ] No P0/P1 audio/PCM-owner issues open; documented fault-injection log and linked code/test evidence.
- [ ] Official Windows FQ **cold** PASS on frozen candidate SHA; no new commits after qualification.
- [ ] Physical Windows PCM16/48k scenario, 3+ minute listening/return/Loop and other present devices PASS.
- [ ] Packaged Windows installation/distribution burn-in PASS. Product Owner gives explicit merge GO only after report.

Do not equate successful CI status with "no more audio bugs": all hardware timing and long-term reliability are empirical release evidence, not a theorem.
