# RGS MasterLab — M24 final PO physical exit: finite acceptance (2026-11-21)

**Status:** #98 focused Windows/Linux 7/7 GREEN for dual Region/Loop project persistence; final real-hardware listening, Region/Loop restore and tonal-profile checks still OPEN. **Not** a declaration of full M24, FQ, Phase-6 Gate G or release PASS.

## Frozen test baseline and provenance

- Single writer: `staging/l1-m24-p2-integration-writer-no-ci-20261114` (this document is a no-CI staging-only checkpoint).
- **Current physical test build: Focused #98**, exact SHA `ff2f81586cadf467db447b6958232a36e0d1bc3c`, [7/7 GREEN](https://github.com/casaferla/RGS-MasterLab/actions/runs/44905534616). Authentic Windows research-only ZIP: [RGS-P2-I2-Win64-RESEARCH-ONLY-98](https://github.com/casaferla/RGS-MasterLab/actions/runs/44905534616/artifacts/14707409493). Prior #97 remains historical approved-GUI baseline only; its executable does **not** persist independent REFERENCE Region/Loops.
- PO-approved GUI: all DSP Chain glyphs now share `StudioIcon.qml` Canvas 2D, with their 16px silhouettes/family accents and existing glow/bypass semantics. **Do not reopen icons** in M24.
- The suite is a **scoped** focused build: the previously identified long legacy M17 native combined fixture remains excluded from the physical-artifact lane under the explicit PO scope reset, not covertly counted as PASS. Formal cold FQ and other distinct development/release gates are independent.

## Already accepted from real Windows PO evidence — DO NOT REPEAT WITHOUT A NEW SYMPTOM

- Cross-rate verified Master retention/monitoring repair: physical #87 source 44.1 kHz, selected Master 88.2 kHz, device 48 kHz; audio target transitions preserve installed verified Master without unrelated new whole-master rendering. Subsequent scoped CI green applies to later fixes, not proof of additional PC listening.
- M18: physical stereo 88.2-kHz PCM24 and Float32 WAV export/readback files were independently inspected (17,567,998 frames/channel, no nonfinite samples), with high PCM24/F32 agreement; no claim of independently measured true intersample peaks solely from these files.
- M19 and M20: physical PREPARED/PROCESSED/REFERENCE measurements/screens compared and accepted as genuinely source-dependent.
- M21: PO accepted the genuine nine-metric, three-source matrix and signed REFERENCE-minus-current deltas.
- Project `Prova 10.rgsml`: PO reopened it, retained SOURCE + REFERENCE, 88.2-kHz Float32 output and fifth true P2 module (Ceiling -6 dBTP, Drive +8 dB, marginPercent 2, not bypassed). The analyzed project had **no nonempty Audition Region**; it does not qualify nonempty Region persistence.
- Public analysis labels `Levels`, `Spectrum / Stereo`, `Metrics / Delta`, `ANALYSIS`, `MEASUREMENTS AVAILABLE` and `NO MEASUREMENTS`, as well as unified Chain icons, passed focused #97; PO approved the icon visual result. The actual M20 1/12-octave **band-energy graph** still needs a genuine PCM physical display check: existing `measure_tonal_stereo_scope` supplies measured linear energies; Qt only projects eligible rows into a relative 48-dB graphic.

## Remaining physical M24 test — one coherent Windows session

Use the **exact #98 ZIP**, not an earlier #90/#92/#96/#97 build. Retain the original SOURCE and REFERENCE files unchanged. Record a screenshot only where needed; do not rerun full M19/M20/M21 coverage already accepted.

### A. Both nonempty Regions and both independent Loops — new #98 physical acceptance

1. Open existing `Prova 10.rgsml` in the **#98 research build**; confirm SOURCE, REFERENCE, typed fifth P2 (Ceiling -6 dBTP / Drive +8 dB) and 88.2-kHz Float32 master output. This old project has no Audition Regions: both Loops should start OFF.
2. Select a genuine nonempty SOURCE Region and enable Loop. Switch to REFERENCE; define a **different nonempty REFERENCE Region** and enable its independent Loop. Verify SOURCE coordinates/loop remain unchanged, and REFERENCE Loop applies to the REFERENCE original at its own sample rate, not to SOURCE or the M17 Master.
3. **Save As a new distinct .rgsml**, leaving `Prova 10.rgsml` and both WAV originals untouched. Close and reopen the new file in #98.
4. Verify both **SOURCE** and **REFERENCE** Region Start/End and both saved Loop ON flags, plus P2 typed parameters, all source/reference locators, chain order and output rate/format. Reopen must **not** start audio.
5. Switch audibly SOURCE/PREPARED → REFERENCE → SOURCE/PREPARED; each Loop should apply only to its respective selected timeline. A missing/swapped/out-of-range Region or Loop after reopening, an unexpected autoplay, or Project Open changing the wrong audio owner is a genuine M24 blocker.
6. Optional quick negative check: turn OFF only REFERENCE Loop, save a second separate project and reopen. SOURCE Loop must remain ON while REFERENCE Loop remains OFF. Do not overwrite earlier acceptance evidence.

This #98 schema accepts up to two role-tagged entries (`SOURCE`, `REFERENCE`) and optional strict boolean `loopEnabled`; absent in legacy projects means OFF. Its C++/JSON tests have passed the scoped Focused CI, but real Windows PO acceptance still needs the above observation.

### B. Final listening and owner/target continuity

1. In the reopened project, use real SOURCE audio and a real REFERENCE recording. Set Master output rate **88.2 kHz** when the actual audio device is at **48 kHz**, if available (the already observed physical combination); do not claim another device rate without checking it.
2. With an audible nonneutral MANUAL chain including genuine P2, render/prepare the Master **once**. Check M17 Final TP Limiter's actual ceiling/readiness without confusing it with P2's upstream streaming ceiling. Observe M16 TP Meter; compare the audible SOURCE/PREPARED, PROCESSED and REFERENCE targets.
3. Switch PROCESSED → REFERENCE → PROCESSED and PROCESSED → PREPARED → PROCESSED; optionally P2 Bypass→Active. Check that navigation alone does **not** discard the immutable verified Master, create a persistent red error, or spuriously demand a new full render when the exact owner/parameters are unchanged.
4. In the cross-rate case, use the genuine **Listen to Master → Return to Source** flow, including a short real headphone/speaker listen to ensure the device-output audio is the expected Master and not silence/stale SOURCE. Verify transport/loop/region behavior, continuity of the source cue and return. A known real parameter edit legitimately invalidates the prior Master and may require a new Render; this is **not** a navigation regression.
5. Listen to the complete intended result, including transient and tail passages, and report concrete issues (dropouts, audible clicks, unexpected level shifts, incorrect source, missing tail, wrong channel orientation, hangs). Do not infer auditory success from a Windows runner screenshot or codec test.

### C. One last genuine data-display check

- With an actually analyzed non-silent source selected, open **Spectrum / Stereo** and inspect the `TONAL PROFILE / 1/12-OCTAVE BAND ENERGY` panel. It should show only measured, fully supported positive-energy bands on the new relative scale, or honestly display `NO MEASURED BAND ENERGY` when data is unavailable. Verify that PREPARED, PROCESSED, REFERENCE still identify the correct owner without changing analysis data unexpectedly. A runner screenshot with no audio is **not** proof of the populated band plot.
- The already accepted M21 nine-metric matrix and M18 PCM24/F32 WAV need no duplicate full test unless this final session reveals a new anomaly.

## 2026-11-21 — PO-authorized M24 persistence extension (Focused #98 GREEN, physical OPEN)

PO has explicitly chosen to persist both SOURCE and REFERENCE Regions and each independent Loop preference in the `.rgsml` file **before** M24 physical acceptance. The former SOURCE-only persistence contract applies to the historical #97 baseline, not to the current tested #98 candidate.

- Project JSON accepts exactly two distinct `timeSelections.auditionRegions` roles at most: `SOURCE` references the selected SOURCE resource, `REFERENCE` the selected active REFERENCE resource. Each entry can carry optional strict-boolean `loopEnabled`; absent means false and is omitted from legacy reserialization.
- Independent original WAV frame counts validate both Regions **before** Project Open mutates SOURCE, REFERENCE, DSP or Product epoch. Duplicate roles, wrong resource, extra entries, invalid frames and invalid JSON flags fail closed.
- `ProjectSessionViewModel` binds the existing independent `referenceRegion` in real Main, saves/restores both frame domains and Loop preferences without playback start. SOURCE loop is installed on the current stopped SOURCE audio owner when possible; REFERENCE loop is installed only when REFERENCE itself is selected, using the existing selector provider.
- **Focused #98 7/7 GREEN at `ff2f8158`** covers the Windows native Project SaveAs/Open with two independently timed WAVs, Loop reattachment and invalid REFERENCE bounds, alongside strict JSON legacy compatibility; scoped CI success does **not** imply the physical M24 or Full Qualification gate.
- Final PO physical M24 Save/Reopen gate must now test **both** saved Regions and both Loop toggles on exact #98; no need to rerun the accepted GUI icon work.

## Persistence contract verified in #98 source

- `ProjectSessionViewModel::current_snapshot` saves an optional SOURCE-tagged region and a distinct REFERENCE-tagged region associated with its own original resource, each with an independent saved Loop preference.
- `ProjectSessionViewModel::openProject` preflights the SOURCE and active REFERENCE resource and the corresponding WAV frame bounds before mutation; then restores both Region view models and their Loop preferences. No autoplay; actual Loop installation is owner-specific.
- `ProjectSnapshot::create` rejects duplicate roles, cross-resource Regions, inactive REFERENCE, invalid frame ranges and more than two entries. Canonical JSON requires exact keys/types and treats missing legacy `loopEnabled` as OFF without changing reserialized original bytes.
- `rgsml_project_session_tests` and `rgsml_project_json_tests` executed within Focused #98. This evidence is a software gate only; actual user Windows SaveAs/Open remains open.

## M24 exit decision

**PASS only if** A both Region/Loop SaveReopen and independent ownership, B real full-chain listening/continuity and C genuine M20 band-energy presentation all pass on the same identified Windows #98 research build, with no unresolved actual defect. Keep source/target/rate and test-build identity in the final PO report.

An unresolved symptom must be reproduced on exact build/owner and repaired narrowly (writer → focused CI → new physical replay of the affected path). Do not generate speculative safety suites or alter approved UI/DSP to chase a guessed regression.

After *physical M24 acceptance*, the next authorized engineering scope is **minimum L2 contracts only** for typed reference/current analysis and bounded render-candidate cache, reusing M19–M21/P2. Then prioritize L3 AutoMatch. Full cold FQ, Q1–Q6 and Phase-6 Gate G remain distinct; no automatic main merge or release claim.

## 2026-11-21 — PO physical #98: SOURCE Loop cursor mismatch and targeted repair

The user-provided `Prova 13.rgsml` (verified as an intact ZIP) contains **two persisted nonempty, enabled loops**: SOURCE [3,783,079,5,622,796) and REFERENCE [1,210,966,2,568,083). Both region resources are distinct and correctly serialized. On reopening build #98, REFERENCE correctly sought the REFERENCE region start when selected; SOURCE instead left its stopped playhead at frame zero even though SOURCE Loop was armed. The accepted PO contract is symmetric: when an enabled saved loop is restored, a cursor outside it is moved to its beginning **without starting playback**. When Loop is OFF, no such seek is performed.

The fix only extends `AuditionRegionViewModel::restore_project_loop_preference` to reuse the already qualified `reposition_if_outside` action after successfully reinstalling the current SOURCE loop under the existing Project Open epoch. Independent REFERENCE behavior remains unchanged. The genuine native Windows `reference_region_and_independent_loop_project_roundtrip` now requires STOPPED + SOURCE start frame immediately after Open, then correct REFERENCE and SOURCE start frames across switches, and zero `play()` calls. The legacy unarmed SOURCE Region test asserts its cursor remains at zero. **No project schema/codec, DSP, PCM, M17, P2, SRC, or GUI modification.** This writer candidate requires a new exact-SHA Focused Windows/Linux CI and PO confirmation on the packaged build; #98 remains historically green but does not qualify this correction.
