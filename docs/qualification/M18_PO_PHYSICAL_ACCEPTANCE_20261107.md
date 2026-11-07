# RGS MasterLab — M18 Windows Product acceptance (PO, non-FQ)
_Date: 2026-11-07. Isolated staging only; not evidence of PASS._

## Decision boundary

M18 is **not closed** when Focused Windows/Linux is green. The candidate
must also start as a **deployed Windows application**, preserve M17, export
real PCM24/TPDF and IEEE Float32 from an exact **verified M17 full Master**,
verify the delivered WAV, refuse destructive operations, and pass the
Product Owner's physical gate. No main merge or Full Qualification is
authorized by this document.

## Candidate & environment

- Use the **latest** successful Draft PR #84 Focused Windows artifact
  `RGS-M18-Windows-PO-<run>-<sha>`; keep its exact SHA beside test notes.
- Unzip the **entire** installer/staged directory, including
  `plugins/platforms/qwindows.dll` and `bin/resources/fir` with the
  verified 14 Final SRC FIR plus 2 playback FIR. Do not launch a lone EXE.
- Start `bin/RGSMasterLab.exe` normally on Windows 10/11. A green
  `--rgsml-deploy-smoke` alone does not count as normal GUI acceptance.
- Preserve a copy of a known-good M17 installation/project for rollback;
  use separate temporary OUTPUT files, never the selected SOURCE.

## Functional gate — required physical observations

| ID | Exact operation | PASS evidence |
|---|---|---|
| A01 | Launch with no SOURCE selected; inspect File > Export verified Master | Disabled; no phantom Master, no QML error |
| A02 | Load a real 44.1 kHz stereo SOURCE; select 48 kHz MASTER; run M17 Prepare | Export disabled until an exact final M16 checked M17 Master is ready; no playback-device rate substitution |
| A03 | Export 24-bit PCM/TPDF to a NEW `.wav` path | Completion is **VERIFIED DELIVERED**, full 64-character SHA-256 visible, verified nonzero file size; WAV readable in independent editor |
| A04 | Export the same verified Master as 32-bit IEEE Float WAV to a second NEW path | Both files report 48 kHz stereo and correct encoding; Float32 has no dither; original M17 audition remains available |
| A05 | Attempt to export to an already existing WAV filename | Existing bytes unchanged; no success claim; a useful failure and dismiss/ACK path |
| A06 | Attempt to export directly over the selected SOURCE | Refused before writing; SOURCE retains original bytes and selection |
| A07 | After a verified M17 Master, modify Gain, EQ or terminal Ceiling and attempt export without fresh Prepare | Export disabled/refused; no stale Master published. Re-Prepare to re-enable |
| A08 | Replace SOURCE, including same-rate replacement | Old M17 stage cannot be exported; new source is usable and new Prepare succeeds |
| A09 | Load an existing Project and SaveAs while no export runs | Correct SOURCE, rate and chain restored; no broken Source/Project controls from M18 mutation guards |
| A10 | While exporting a long real master, press Cancel | UI remains responsive, settles to CANCELLED or genuine failure, no false verified delivery; no orphan temp files |
| A11 | While encoding/verification is active, use Alt+F4/window X | App does not exit before worker transaction has settled; Cancel/ACK remain usable |
| A12 | After Cancel/FAIL/VERIFIED, acknowledge the result, then export again to a NEW filename | New independent consent accepted; no stuck Export lock or reused previous result |
| A13 | Test one 192 kHz mono or stereo Master and one 44.1 kHz output | No unintended SRC, WAV Fs/channel count match selected MASTER; packaged original FIR assets are available |
| A14 | Listen to the PCM24 and Float32 files in an independent player/editor | No clicks, cut ends, silence, channel reversal or incorrect duration; compare to M17 Master audition at matched level |

## Non-negotiable engineering gate

1. Focused CI on the **exact candidate SHA**: Windows and Linux SUCCESS.
   The Windows job must complete **native app build**, ProjectSession M18
   positive path, M18 Windows transactional test, UI smoke and deployed
   standalone-package verification (not just DSP unit tests).
2. The encoded bytes, decoded samples, post-dither/postquant M16 full-file
   true peak and M17 accepted Ceiling must pass the canonical M18 validator;
   no `VERIFIED DELIVERED` if any evidence is absent or nonconforming.
3. Physical SOURCE/Project/MASTER/chain mutations and file publication
   must share the Product Single-Writer epoch; no stale ticket, partial
   filename overwrite, second concurrent export or Qt worker-object access.
4. Keep 24-bit TPDF with exact `uint64` seed; Float32 **without** dither;
   no noise shaping. No fabricated M18 output-schema fields in existing
   `.rgsml` files without an explicitly approved stable schema decision.
5. Do not change accepted M16 B2i math/FIR evidence or M17 qualified DSP
   solely to make a red test pass.
6. Reserve **one overall Full Qualification after M18–M22 integration**,
   per PO decision. M18 integration needs explicit PO authorization,
   not this checklist alone.

## Recording results

Record: tested SHA, artifact name, Windows version, SOURCE Fs/channels,
selected MASTER Fs, PCM format, destination path class (not private path),
outcome, verified bytes and SHA-256, audible result, screenshots for any
failure, and the exact first failing checklist ID. Never upload full CI
job logs; use bounded CI diagnostics.

**Status: ACCEPTANCE PENDING.** Checking these boxes is future physical
evidence, not an assertion that any of them passed today.
