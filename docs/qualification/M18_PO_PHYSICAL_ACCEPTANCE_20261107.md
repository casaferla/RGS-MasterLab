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
| A15 | Choose Float32, SaveAs, reopen the Project, then select PCM24 and SaveAs again | ComboBox restores Float32 on open; subsequent file uses PCM24 TPDF. Exact uint64 seed is serialized without numeric rounding; old output keys are preserved |

## SOURCE changes and future DSP presets — scope boundary

L1-M18 MUST keep the existing SOURCE-selection transaction consistent:
the current product can reset MANUAL module controls to defaults on a
new SOURCE, so all nested parameter/bypass writes must either complete
under the same SOURCE epoch or fail closed. M18 does NOT introduce a
new DSP preset file format, and cannot claim that the user can already
save/reload independent presets to protect edits before replacing SOURCE.

The previously agreed independent feature is **L2-M08 — Module
Compact/Expanded States + Basic Presets**, after L1-M24:
(1) reusable, named presets of one Mastering DSP module from its
expanded chain card, and (2) reusable presets of the entire Mastering
DSP Chain from its chain-level header. These are separate from
`.rgsml` Project persistence; neither exact file extension nor file
format/name was frozen. This M18 test deliberately protects legacy
SOURCE-reset consistency, not a future decision on retaining DSP
parameters automatically across SOURCE replacement.

## PO decision update — 2026-11-14 (supersedes the prior M18 TP veto)

The additional M18 post-quantization True Peak qualification and the
conditional "Apply Micro Trim & Export" research route were explicitly
reconsidered and rejected for L1. The earlier 31-commit staging experiment
is retained only as historical evidence; no micro-trim, second export
authorization, hidden gain processing, second M16 4x FIR pass, or
post-quant True Peak ceiling veto enters the deliverable Product.

**Division of authority:** M17 verifies the full Master and True Peak
Ceiling after Final SRC. M18 only performs deterministic format conversion,
exact decoded-file codec verification, integrity and safe delivery.
Format quantization may create microscopically different instantaneous
peaks; such codec-correct deviations do not invalidate the M17 Master's
previous certificate. No extra statement of WAV post-quant TP compliance
may be inferred from "VERIFIED DELIVERED".

This change does NOT relax the SOURCE/Project/M17 ownership, immutable
Master proof, final SHA/file readback, no-overwrite, cancellation or
single-writer epoch guarantees. Physical acceptance remains mandatory.

## Non-negotiable engineering gate

1. Focused CI on the **exact candidate SHA**: Windows and Linux SUCCESS.
   The Windows job must complete **native app build**, ProjectSession M18
   positive path, M18 Windows transactional test, UI smoke and deployed
   standalone-package verification (not just DSP unit tests).
2. The M17 owner must supply a currently verified full Master and its
   accepted Ceiling. M18 must prove the exported WAV is the **exact**
   deterministic PCM24/TPDF or Float32 encoding of that unchanged Master,
   including decoded sample/format/rate/frame checks, encoded and decoded
   SHA-256, atomic no-overwrite publication and final filesystem readback.
   M18 must **not** remeasure the delivered True Peak or reject tiny TP
   shifts inherent to correctly quantizing the already verified M17 PCM.
   `VERIFIED DELIVERED` certifies the WAV conversion and delivery, NOT a
   second independent WAV True Peak qualification.
3. Physical SOURCE/Project/MASTER/chain mutations and file publication
   must share the Product Single-Writer epoch; no stale ticket, partial
   filename overwrite, second concurrent export or Qt worker-object access.
4. Keep 24-bit TPDF with exact `uint64` seed; Float32 **without** dither;
   no noise shaping. Staging proposes the final Project OUTPUT keys
   `finalMasterWavEncoding` and `finalMasterPcm24TpdfSeed`
   (see `docs/architecture/M18_FINAL_WAV_OUTPUT_PREFERENCE_V1_20261107.md`).
   Their exact spelling requires PO acceptance before main merge.
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

## Physical replay record — 2026-11-14, PO screenshots

**Narrow regression: PASS** for the previously failing M17 Ceiling
at **-2.70 dBTP**, and for subsequent M18 WAV publication, on the
standalone Windows x64 NON-FQ candidate at commit
`64cb0f5529fef76df3444182b0a7f325885db948`.
Qualification CI: https://github.com/casaferla/RGS-MasterLab/actions/runs/43597820523
(Windows and Linux successful; separate from physical replay).

Evidence provided directly by the PO: screenshots timestamped
`20261114-133242`, `20261114-133252`, `20261114-133320`,
`20261114-133338`. SOURCE is `Reati universali (Remastered) (Edit) 44.wav`,
44,100 Hz, PCM32, stereo, duration 03:19.184. The screenshot exposes
44,100 Hz status but does not independently prove a cross-rate 48-kHz
MASTER: do not claim the A02 cross-rate gate from this evidence.

- M17 enabled at requested -2.70 dBTP; status: `MASTER VERIFIED ·
  LISTEN TO MASTER`; `VERIFICATION TRIM <0.01 dB · M16 CHECK`.
  Previously the identical target presented measured `NON-CONFORMING`
  / over <0.01 dB and withheld export.
- 24-bit PCM/TPDF WAV: `VERIFIED DELIVERED`, 52,704,038 bytes,
  displayed post-delivery SHA-256:
  `9309155d39365fbe9cd8ddfe26dd7331e963d29ce1e822edce659ab9667fe2ae`.
- 32-bit Float WAV: `VERIFIED DELIVERED`, 70,272,050 bytes,
  displayed post-delivery SHA-256:
  `87f400eb6b45be19ac75cd51b16c38ecdf25c57479a9519501071fc8b3aa0669`.
- Both delivery receipts explicitly state filesystem readback. No
  M18 micro-trim path was used. M17's existing single verified trim
  remains distinct from M18 conversion.
- This record is **screenshot-based**; the new exported WAV bytes were
  not attached in this turn. No independent playback/editing, external
  WAV inspection, A02 cross-rate gate, SOURCE/Project mutation, cancel
  or no-overwrite physical tests are claimed.

**Disposition:** M17 -2.70 dBTP field regression CLOSED ON THIS
SOURCE/CONFIGURATION; M18 PCM24 + Float32 delivery field replay PASS.
The larger A01–A15 physical acceptance matrix remains PARTIAL;
overall L1 Full Qualification and `main` merge remain NOT AUTHORIZED.

**Status: PARTIAL PHYSICAL ACCEPTANCE.** Remaining checklist items
are not evidenced by the screenshots and must not be marked PASS.


## 15 November 2026 — M17 Rate & Time direct WAV export controls (non-FQ candidate)

Before the next physical M24 replay, the PO's chosen UX is a compact
`WAV OUTPUT` footer in the existing M17 `Rate & Time` popup rather than
a new DSP/editor window or an additional format wizard. The view offers
two explicit actions: `EXPORT PCM24` (TPDF dither) and `EXPORT FLOAT32`
(IEEE floating point, no dither). They share the existing single Product
`M18ProductExportViewModel::beginExport(destination, formatIndex)`,
one SaveFile dialog, worker progress/Cancel/ACK, readback SHA receipt,
no-overwrite, SOURCE/Project/MASTER epoch guards and the existing M17
certificate. No M18 post-quantization TP veto is reintroduced.

The buttons are disabled until `canExport` is genuinely true for an
exact M17 verified full Master. Only the format selected by the button
is handed to M18; cancelling the destination dialog does **not** change
the persisted Project output preference. On successful choice, M18
writes the canonical preference itself through `chooseM18WavEncoding`;
no QML code handles or rounds the uint64 dither seed. The File menu's
pre-existing export modal remains available as a backward-compatible
secondary route.

Physical checks in addition to the above A01–A15, on the **new exact SHA**:

- B01: At no SOURCE and after M17 becomes stale, both popup buttons are
  disabled, without changing `canExport` or the old M18 model.
- B02: With M17 verified, `EXPORT PCM24` directly opens SaveFile; 
  verified receipt is 24-bit PCM with TPDF, no prior encoding modal.
- B03: `EXPORT FLOAT32` does the same for Float32 with no dither.
  Each format requires a NEW destination, preserving the other WAV.
- B04: Cancel SaveFile: no worker begins, no WAV delivered, the
  Project's previously persisted WAV choice is unchanged.
- B05: Docked and floating M17 windows both invoke the same Main-owned
  M18 transaction. At the 1184x688 Windows minimum, no scrollbar, clipped
  action, overlapping footer or offscreen popup is allowed. All six
  MASTER rate choices, TIME, memory, CALIBRATE and both WAV actions must
  remain visible simultaneously.
- B06: While a delivery is running, second export is disabled, existing
  files are not overwritten, and Cancel/ACK/Source invalidation behave
  exactly as in A05–A12.
- B07: During M18 encoding/verification the EXISTING copper ProgressBar
  in the M17 GR HISTORY lower bay shows the actual encoded fraction
  with an explicit readback-pending label, not an invented delivered WAV
  percentage. No second progress window, no additional permanent row,
  no new progress timer. The existing M17 control row exposes Cancel
  Export. On terminal VERIFIED/FAILED/CANCELLED, the lower bay restores
  authentic GR HISTORY (or a genuinely concurrently owned preparation
  if one exists); the SAME fixed WAV OUTPUT row shows inline status,
  full-sha tooltip/copy affordance (only for readback-verified results)
  and a real native M18 ACK button. No independent result dialog may open.
  The M16 independent meter is unchanged.
- B08: The Rate & Time window is strictly NONSCROLLING, including at
  physical minimum size and while M17 is floating. No modification of
  FQ99 geometry, app min 1184x688, M19–M21 layout or frozen M16/M17 DSP
  output. The existing M19–M21 scrolling is a separately tracked future
  GUI closeout item, not a reason to rewrite analyzer code now.
- B09: Reopen Rate & Time after File dialog Cancel and after normal
  completion; do not show a synthetic result or commit an output format
  without accepted WAV. After an actual encoding run, reopen Rate & Time
  automatically only once on terminal receipt; keep it accessible there
  until explicit OK, and keep the original error details accessible from
  the compact failure status. A second export is blocked while ACK is owed.
- B10: The previous separate M18 result dialog is absent. At 1184x688,
  inline result controls do not move the two format buttons, increase
  the popup height, scroll or obscure six rate rows, CALIBRATE or memory
  summary. Native clipboard copy must expose only the independently
  SHA-verified 64-character result, never a guessed output path.

These additions require Qt native UI inspection, not a CI-only PASS.


## Physical PO replay — 2026-11-15, CI #18 / SHA 8b2197e9

The PO physically launched `RGS-P2-I2-Win64-RESEARCH-ONLY-18`,
commit `8b2197e961ac98f427b126da022c8d554dbbcfdc`,
[7/7 focused CI #43705272715](https://github.com/casaferla/RGS-MasterLab/actions/runs/43705272715),
and supplied Windows screenshots of the ACTUAL Product WAV progress at
60%/100%, return to authentic GR HISTORY, inline PCM24 and Float32
receipts, SHA and ACK hover/tooltips and the unchanged non-scrolling
Rate & Time panel. SOURCE displayed: `Reati universali (Remastered)
(Edit) 44.wav` (44.1 kHz, stereo); MASTER selected 48 kHz; M17
Ceiling -2.70 dBTP and Verification Trim 0.00 dB / NO TRIM. P2 row
visible/bypassed in this case. These screenshots are a genuine physical
replay, NOT merely Qt CI mockups.

The two **actual WAV bytes were provided by the PO** and independently
inspected (not merely trusting UI receipts):

| Output | Exact verified SHA-256 | WAV encoding | Rate/channels | Frames | Duration | Size |
|---|---|---|---|---:|---:|---:|
| `24(1).wav` | `eee19b50715160eec33ef5eabb461d16ce8a0688dd0b1bd642f819afceb25da7` | PCM24 | 48000 Hz stereo | 9,560,816 | 03:19.184 | 57,364,940 bytes |
| `32(1).wav` | `f5b5dbacacab8c2d1278a9ad61f94d7c737a7dcd5401363834927842c2a7de75` | IEEE Float32 | 48000 Hz stereo | 9,560,816 | 03:19.184 | 76,486,586 bytes |

Both SHA-256 values matched those independently copied from the app
through its inline SHA control. The WAV headers were valid, samples
checked from multiple positions were finite, levels matched, and
sampled PCM24-vs-Float32 maximum absolute delta was approximately
1.79e-7 (quantization/dither-scale). No claim is made of independent
full-song audition, bit-exact reconstruction from private M17 PCM,
standalone True Peak after quantization, or completion of the full
A01–A15/B01–B10 test matrix.

**Field findings:** The SHA and OK receipt controls were using native
Qt Basic `Button` chrome instead of RGS `StudioButton`, and their
tooltips were immediate rather than the established stationary
`StudioTooltipDwell` 1000-ms policy. This is a local GUI consistency
defect, NOT an M18 codec/sha/ceiling failure. Corrected on the active
writer with exactly the existing RGS control component and hover policy;
re-qualification of that updated UI still requires new exact-SHA CI
and subsequent physical visual confirmation.

**Disposition:** M18 PCM24 and Float32 physical cross-rate encoding,
SHA match and core in-place progress/receipt interaction are supported.
Remaining destructive, cancellation, stale-epoch, Project SaveAs and
whole-M24 physical gates remain open. No FQ or release claim.
