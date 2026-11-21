# RGS MasterLab — M24 final PO physical exit: finite acceptance (2026-11-21)

**Status:** approved GUI baseline; final real-hardware audio/Region/tonal-profile observations still OPEN. **Not** a declaration of full M24, FQ, Phase-6 Gate G or release PASS.

## Frozen test baseline and provenance

- Single writer: `staging/l1-m24-p2-integration-writer-no-ci-20261114` (this document is a no-CI staging-only checkpoint).
- Windows research build to test: **Focused #97**, exact SHA `e9acb57f1f8f32c7c0591c076f6677c4f627d399`, [7/7 GREEN](https://github.com/casaferla/RGS-MasterLab/actions/runs/44902078913). Research-only Windows ZIP artifact: [RGS-P2-I2-Win64-RESEARCH-ONLY-97](https://github.com/casaferla/RGS-MasterLab/actions/runs/44902078913/artifacts/14706840898).
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

Use the **exact #97 ZIP**, not an earlier #90/#92/#96 build. Retain the original SOURCE and REFERENCE files unchanged. Record a screenshot only where needed; do not rerun full M19/M20/M21 coverage already accepted.

### A. Nonempty Region save/reopen (missing evidence)

1. Open existing `Prova 10.rgsml` in the #97 research build; confirm the correct SOURCE and REFERENCE are present and P2 typed values remain the accepted values above.
2. Select an **actual nonempty SOURCE Region** (start/end both inside the track) and enable its relevant Loop control. Switch to REFERENCE; define a **different nonempty REFERENCE Region** and its intended loop behavior. Confirm the REFERENCE region edit has not changed SOURCE coordinates.
3. Save As **a new** `.rgsml` (never overwrite the accepted `Prova 10.rgsml` or source audio). Close and reopen the new project. Verify exact SOURCE/REFERENCE locators, their distinct nonempty Region coordinates and selected loop behavior, P2 Ceiling/Drive/Bypass, module order and selected Master output format/rate.
4. A missing, shifted, swapped or silently emptied region after reopen is a **real blocker**. Record the specific before/after values or a screenshot; do not silently mark a missing field accepted.

### B. Final listening and owner/target continuity

1. In the reopened project, use real SOURCE audio and a real REFERENCE recording. Set Master output rate **88.2 kHz** when the actual audio device is at **48 kHz**, if available (the already observed physical combination); do not claim another device rate without checking it.
2. With an audible nonneutral MANUAL chain including genuine P2, render/prepare the Master **once**. Check M17 Final TP Limiter's actual ceiling/readiness without confusing it with P2's upstream streaming ceiling. Observe M16 TP Meter; compare the audible SOURCE/PREPARED, PROCESSED and REFERENCE targets.
3. Switch PROCESSED → REFERENCE → PROCESSED and PROCESSED → PREPARED → PROCESSED; optionally P2 Bypass→Active. Check that navigation alone does **not** discard the immutable verified Master, create a persistent red error, or spuriously demand a new full render when the exact owner/parameters are unchanged.
4. In the cross-rate case, use the genuine **Listen to Master → Return to Source** flow, including a short real headphone/speaker listen to ensure the device-output audio is the expected Master and not silence/stale SOURCE. Verify transport/loop/region behavior, continuity of the source cue and return. A known real parameter edit legitimately invalidates the prior Master and may require a new Render; this is **not** a navigation regression.
5. Listen to the complete intended result, including transient and tail passages, and report concrete issues (dropouts, audible clicks, unexpected level shifts, incorrect source, missing tail, wrong channel orientation, hangs). Do not infer auditory success from a Windows runner screenshot or codec test.

### C. One last genuine data-display check

- With an actually analyzed non-silent source selected, open **Spectrum / Stereo** and inspect the `TONAL PROFILE / 1/12-OCTAVE BAND ENERGY` panel. It should show only measured, fully supported positive-energy bands on the new relative scale, or honestly display `NO MEASURED BAND ENERGY` when data is unavailable. Verify that PREPARED, PROCESSED, REFERENCE still identify the correct owner without changing analysis data unexpectedly. A runner screenshot with no audio is **not** proof of the populated band plot.
- The already accepted M21 nine-metric matrix and M18 PCM24/F32 WAV need no duplicate full test unless this final session reveals a new anomaly.

## M24 exit decision

**PASS only if** A Region Save/Reopen, B real full-chain listening/continuity and C genuine M20 band-energy presentation all pass on the same identified Windows #97 research build, with no unresolved actual defect. Keep source/target/rate and test-build identity in the final PO report.

An unresolved symptom must be reproduced on exact build/owner and repaired narrowly (writer → focused CI → new physical replay of the affected path). Do not generate speculative safety suites or alter approved UI/DSP to chase a guessed regression.

After *physical M24 acceptance*, the next authorized engineering scope is **minimum L2 contracts only** for typed reference/current analysis and bounded render-candidate cache, reusing M19–M21/P2. Then prioritize L3 AutoMatch. Full cold FQ, Q1–Q6 and Phase-6 Gate G remain distinct; no automatic main merge or release claim.
