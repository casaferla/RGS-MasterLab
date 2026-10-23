# RGS MasterLab — M17 Prepare Throughput and Progressive-Playback Research Gates

Date: 2026-10-23 · Product Owner new direction · PRELIMINARY / NON-AUTHORITATIVE.
REAL_TIME_FIRST remains a separately authorised research ambition; no architectural replacement is approved.

## Product architecture preserved

RGS Product Owner direction 2026-10-22: M16 True Peak Meter is a separate LIVE analysis item; M17 is in anchored FINALIZATION · OFFLINE underneath live DSPs, before future Dither. Prepare, Cancel and Preview Prepared are explicit. NEEDS PREPARATION → PREPARING → READY → PREVIEW/RENDER. No implicit Play-triggered rendering, no stale audio, no fake meter, no false ERROR on ordinary invalidation.

Official PO source: https://docs.google.com/document/d/18uFEzMGeK3KPmotcYb5yzocPomtz3AwcH1ti5rDMGnk/edit

## Mandated ordering

**G0 — Measure.** In test-only, measure REAL production checked terminal candidate with full M17 DSP, mandatory primary M16 true-peak verification, possible global verification trim and second M16 pass. Separate SOURCE_RATE/OUTPUT_RATE SRC, queue/worker and UI/device publication as distinct unmeasured dimensions. Source PCM in first CI is already OUTPUT_RATE, so src_us=-1, NOT zero. Use two Fs (48 and 192 kHz), two ceilings (-1 and -12 dBTP), and two bounded stereo frame counts (512/1536). Measure wall and stage times on Windows/Linux, ensuring verified accepted output. The test is a bounded production stage PROXY, not whole-song Prepare duration. No extrapolated promise or FQ from these tiny buffers.

**G1 — Measure achieved exact speedup in the actual Prepare pipeline.** After #698 baseline, compare frozen production renderer against PR55 exact selector/cursor/lazy optimisations under identical audio inputs, rate, Ceiling, compiler, host and measurement order. Do NOT extrapolate a 64-frame quantum improvement into full-resource gain; report M17 core, M16 verification, trim and total, PCM bit-identity, peak safety, EOS, checkpoints, memory and cancellation. Import no experimental component into production until a separate equivalence gate/PO decision.

**G2 — Primary path: accelerate full Prepare.** Investigate low-overhead bulk/output-rate preparation, reuse exact math/data, avoiding per-frame or per-chunk cost, with bounds and no change to frozen output. Measure 20-second and representative full-track actual Windows Prepare including SRC, worker queue, progress, full M16, optional trim, cache and handoff. Include 192 kHz as priority. Only quantify '5-10 seconds' once this full path has been physically measured.

**G3 — Secondary path: prebuffer while continuing preparation under Play.** Research only; not an authorised active product mode. Determine a true causal certified PCM prefix, exact continuity, per-generation ownership, seek/loop/edit stale cancellation, bounded memory and underrun policy. A buffer solves burst/jitter only if measured producer throughput exceeds consumer throughput. Full-program M16 verification + global trim can retrospectively change a previously emitted prefix, so no preview-before-complete is allowed under the current frozen safety contract unless a new mathematically sufficient prefix-commit proof and authority gate exists. Otherwise startup Play must await full verified Prepare. This is the primary falsification condition for progressive playback.

Decision checkpoints: G0 baseline first; G1 quantitative same-host A/B; G2 runtime improvement with exact PCM and complete M16; G3 independent mathematical feasibility/counterexample and buffering model. No speculative production prefetch, no immediate rollout, no fake realtime claim.

CI classification for #698: [m17-extended] is necessary to run production checked-candidate verification (FAST mode excludes it). Changes are test/workflow/docs ONLY. No M17 frozen math, PR54 product branch, main, M16 DSP, UI or C4/R1. PR55 remains DRAFT and unmerged. Full Qualification and Gate M/L remain PO-owned.