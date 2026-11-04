# M17 Windows measured NON-CONFORMING DEVICE audition — recovery / release gates

Status: **OPEN, unqualified preproduction** · Product Owner-controlled.

## Immutable Focused CI baseline and staging policy

- Focused #1006 **PASS Windows + Linux**, exact source `6969e5b17e07c8adbd20714eb133a0ee3d73a943`.
- Draft [PR #78](https://github.com/casaferla/RGS-MasterLab/pull/78) first coherent multi-commit Windows integration candidate `cef747a8c9323c3443616466c9316d7210c5648a`, [Focused #1007](https://github.com/casaferla/RGS-MasterLab/actions/runs/41836665116). **Linux RED** during build on `-Werror=missing-field-initializers` (two Action PLAY/PAUSE requests at lines 494/496); Windows status must be independently checked before promoting repairs. No complete job logs were fetched.
- Post-#1007 corrections are isolated on no-PR/no-CI branch `staging/m17-post-ci1007-return-epoch-no-ci-20261104`. This branch is **NOT** qualified by #1007. Wait for #1007 terminal, then one expected-SHA fast-forward promotion of the coherent correction lot. Never write `main`.

## Typed product implementation (not yet physical approval)

1. Keep the one immutable M17/M16-measured MASTER at selected Project rate, retain original measured peak/Ceiling/miss and source owner.
2. Only explicit Preview Anyway can trigger a cancellable off-thread FIR into a separately owned DEVICE PCM. Qt owner thread does both current Windows endpoint/memory probes and stage/consent rechecks.
3. The anonymous Windows monitoring sink gets no SOURCE/MASTER `RealizationId`, verified peak, gain-reduction history, or M16 LIVE certificate.
4. The original MANUAL playback state, Loop and playback cursor are independently mapped SOURCE → MASTER → DEVICE, with exact integer frame arithmetic; source `seekSerial`, `traversalSerial` and `loopWrapCount` require fresh-handoff revalidation.
5. Play, Pause, Stop, Seek and Master Loop must check the exact anonymous DEVICE candidate on the Windows worker and verify postconditions. Compare seek in **DEVICE** frames rather than false +/-1 MASTER-frame tolerances at 192 kHz → 48 kHz.
6. Return to Source preflights original MANUAL before exact anonymous lease retirement. After retirement, failed MANUAL reinstall retains a guarded SOURCE recovery ticket; a fully reopened exact MANUAL must be recognized without installing over it again. Unknown/foreign DEVICE owner is never cleared.

## Mandatory adversarial tests not yet proven

- 6 MASTER rates (44.1/48/88.2/96/176.4/192 kHz) to a real 48 kHz PCM16 Windows device, stereo and mono where supported.
- Source rate ≠ MASTER rate ≠ DEVICE rate, including 44.1 → 192 → 48, 192 → 44.1 → 48, 48 → 88.2 → 48; one M17 Prepare only.
- Fake Windows source/manual and anonymous candidate ownership: initial Playing, Paused, Stopped, nonzero seek, active Loop and loop wrap; no double RealizationId.
- Same-rate endpoint hotplug, endpoint ID substitution, device format change, RAM caution/refusal, true mid-FIR cancellation and same-owner retry.
- Windows sink install refusal before and after MANUAL replacement, ownership query exception, Play/Pause/Seek/Loop postcondition failures, failure after monitor retirement, successful Return and explicit retry.
- Distinct QML direct versus DEVICE preview consent; no false LIVE true-peak/GR history after unqualified measured playback or failed restoration.
- Cold official exact-SHA Windows Full Qualification; verify all 16 authenticated FIRs packaged and auto-discovered; fresh Windows 10/11 x64 install; prolonged physical 48-kHz PCM16 listening by PO.

## Mandatory sequencing

Do not merge PR #78 or PR #77 to `main`, do not close [M17 blocker #76](https://github.com/casaferla/RGS-MasterLab/issues/76), #73 or #75 on a Focused green alone. Product Owner alone dispatches cold Windows FQ and approves physical acceptance. After M17 acceptance perform mandatory structural CI reset/audit **before M18**. No Android and no other milestones integrated in this workstream.

## Corrective no-CI candidate updates (post #1007 initial candidate)

- Linux strict C++ aggregate initializer error corrected for PLAY/PAUSE requests (full `MonitoringTransportRequest` initialization); no full run logs fetched.
- Fresh SOURCE `seekSerial`, `traversalSerial`, `loopWrapCount`, plus true DEVICE-frame exact SEEK postcondition (192-kHz MASTER frame 1 projects to DEVICE frame 1 and round-trips to displayed MASTER frame 4 at 48 kHz). Added pure rational regression.
- Measured Windows audio lease now catches preflight/owner/retire/restore exceptions, preserves an exact-owner recovery ticket, recognizes a partially reopened genuine MANUAL, never clears foreign/successor sinks, and requires explicit Return when postconditions fail.
- **Critical cancellation UI corrected:** the old Cancel button and `cancelPreparation()` only recognized the full-resource `Preparing` label. The async measured DEVICE FIR now has its own read-only `nonconformingDevicePreparing` state, enables the shared Cancel affordance, leaves the honest NON-CONFORMING label intact, and revokes the Cancel signal when the worker exits. Qt native regression distinguishes full-M17 and anonymous DEVICE cancellation.
- Main close protection now recognizes measured-device `RECOVERY REQUIRED`; once true MANUAL return succeeds, Main clears abandoned DEVICE time, rate and Loop metadata.

Do not attribute any of these post-#1007 corrections to the original #1007 CI. Promote only when Windows #1007 is **terminal** and the correction lot has been consolidated; then run one dual-OS Focused test on an exact new SHA. The actual physical Windows playback gate remains OPEN.

## Focused #1008 PASS / next six-rate control-plane regression lot

- [Focused CI #1008](https://github.com/casaferla/RGS-MasterLab/actions/runs/41841644401) **PASS Windows + Linux** at exact SHA `eabc34ae81e869462955b89e5afe075a4fc9182e`. This is the previous immutable PR #78 baseline, not a release certificate.
- **New nontrivial bug identified by code review:** `MeasuredDeviceAuditionTicket::admit` equated original SOURCE `programme_frames` with OUTPUT_RATE M17 MASTER PCM frames. This silently prohibits genuine SOURCE != MASTER, even when Final SRC is authenticated and the 48-kHz DEVICE PCM is valid. Previous six synthetic rate cases incorrectly gave every MASTER 513 frames (SOURCE192k/513), so they could not detect the defect. New checked admission instead validates independent SOURCE and MASTER origins, owner/frame domains/layout, authenticated SRC marker and exactly the same rational source/master duration contract as the Windows monitoring map. Existing original M16-measured Ceiling values remain immutable.
- **Second cross-rate bug:** the shared `MonitoringFrameMap` rejected 192k SOURCE 513 frames → 48k MASTER 129 frames because reversing rounded 129 to SOURCE projects 516, three SOURCE frames away. The validation now compares the expected rounded **MASTER** frame count (within one genuine MASTER frame), independently for forward SOURCE cue and reverse MASTER Loop. The exact full MASTER terminal Loop end snaps to original SOURCE end; never outputs a fabricated SOURCE tail.
- Corrected the synthetic structural fixture to model realistic rational MASTER frame counts, appropriately sized gain witness vectors, separate SOURCE/Master SRC metadata and 6 rate-specific SOURCE→MASTER→DEVICE cue/Loop/rebase plans; a forged cross-rate stage with no SOURCE SRC provenance is rejected.
- Extended existing *genuine* 36 SOURCE×MASTER Final SRC functional renders and 216 stereo +216 mono DEVICE conversions to verify rational SOURCE cue, SOURCE→MASTER Loop, MASTER→DEVICE Loop, and full MASTER Loop returning to original exact SOURCE end for **432 triplets**. They remain native code tests, not evidence of physical Windows sound.
- Added fail-closed measured Windows command handling: if an exact-owned Play/Pause/Seek command returns an error, throws, or loses its owner after possible side effects, the lease transitions to `MEASURED_DEVICE_RECOVERY_REQUIRED` and no longer advertises active transport. Keep Return guarded by exact-owner conditional retirement.

These new commits are on isolated no-PR/no-CI `staging/m17-ci1008-followon-anonymous-device-adversarial-no-ci-20261104`; they are **NOT covered by #1008** until promoted as a full multi-commit macro-lot for a new dual-OS Focused gate. PO alone authorizes official cold FQ and main merge; M17 #76/#73/#75 remain open.
