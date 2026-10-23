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

## CI #699 — Product checked Prepare vs accelerated candidate, same-host A/B

CI #698 EXTENDED passed Windows/Linux. The *actual product checked
renderer* short INPUT=OUTPUT_RATE fixtures (512/1536 stereo frames)
verified M16 and recorded M17 DSP core, M16 primary, optional trim
and optional recheck. At 192kHz/1536frames/-12dBTP, Windows real
checked wall=1.297s (M17 core=1.296s); Linux=3.211s (M17 core=3.209s).
These are *8ms of audio*, not 20s or a full song. No SRC or UI/queue
was included. On the separate same-source experimental 8192-frame
tests, Windows 192kHz stereo -12dBTP reference-vs-accelerated
core=131.547x and Linux=50.010x; both had PCM bitwise PASS and
M16 outcome PASS. These isolated core ratios are NOT the user's
Prepare speedup and must not be marketed as such.

#699 adds eight identical-waveform input/ceiling/rate cases to the
existing M17 long-resource A/B function: stereo 48/192kHz, -1/-12dBTP,
512/1536 OUTPUT_RATE frames, plus the existing 48 legacy controls.
Each short case executes on the SAME CI host:
1. The frozen original IModule as the PCM reference;
2. The bounded experimental detector/selector/controller candidate;
3. The ACTUAL product render_true_peak_limiter_checked_candidate,
   including production M16 true-peak measurement and possible
   verification trim/recheck.
4. The independent full-resource M16 verifier for original/experiment.

New artifact columns record product wall, product per-stage timings,
a carefully labelled ratio product verified wall /
(experimental *core* + independently timed M16 verification),
and the exact product PCM-vs-IModule comparison whenever production
trim is absent. This ratio is a **research potential**, not an installed
Prepare acceleration; candidate still lacks production checked
renderer, SRC, queue/worker, ownership and FQ. If a global trim
actually occurs, the product checked result is labelled TRIMMED,
not falsely claimed bitwise unchanged.

This test/docs-only commit must use EXTENDED so that both real
verification suites and entire experiment execute. Maintain
Single Writer/SHA-CAS, PR54 and main unchanged, frozen math unchanged,
no UI, no new FQ, no rollout, no C4/R1.


## CI #700 — Provenance and measured paired Prepare potential (EXTENDED PASS)

Run: https://github.com/casaferla/RGS-MasterLab/actions/runs/39968935196
SHA: 95c4a89501a4d02320c664bd972cb19eb8a7f235.
Windows and Linux completed SUCCESS; original product and
experimental M17 PCM matched bit-for-bit on the paired cases
where no global trim occurred. Both independent M16 outcomes
matched and returned PASS in all eight paired rows per platform.

**Measured same-host, same-source ratios of the production checked
wall time to (experimental core + separately measured M16)**,
NOT measured user-facing Prepare speedups:

| Fs / Ceiling | Windows 512 | Windows 1536 | Linux 512 | Linux 1536 |
|---|---:|---:|---:|---:|
| 48 kHz / -1 dBTP | 9.06x | 11.32x | 5.61x | 6.74x |
| 48 kHz / -12 dBTP | 30.39x | **37.16x** | 17.80x | 21.52x |
| 192 kHz / -1 dBTP | 14.22x | 22.97x | 8.60x | 12.45x |
| 192 kHz / -12 dBTP | 37.50x | **81.95x** | 21.75x | **41.92x** |

Absolute Windows checked renderer wall for the paired 1536
sample source: 48 kHz -12 = 314.287 ms; 192 kHz -12 = 888.257 ms.
Linux: 48k -12=928.321 ms; 192k -12=2634.428 ms.

M17 DSP dominates product verified wall at severe -12 ceiling,
whereas M16 is at most a few milliseconds on these short inputs.
Other real Prepare costs SRC, worker queue, CPU scheduling,
I/O/Qt publication and the full-duration verification remain excluded.
Since 1536 frames are just 32ms at 48k or 8ms at 192k,
ratios cannot be extrapolated to three minutes of source audio.
A naive 180s/132=1.36s scenario is purely conditional, not measured.

## CI #701 — Three-rate scale-up / 44.1k failure-risk boundary

Preserve the 48 legacy full-rate short experiments and 8 initial
product-checked Prepare-proxy cases (#700). Add 12 higher-workload
same-source, same-host checked product/experimental pairs:
Fs {44.1k,48k,192k}, stereo, Ceiling {-1,-12},
frame counts {4096,8192}. All use the genuine production
M17+M16 complete verify path, exact frozen IModule PCM,
independent candidate M16 and original binary64 oracle. Total
68 matrix cases and existing EXTENDED tests, with bounded
GitHub Actions CSV evidence both hosts.

WHY 44.1k: actual Windows FQ #75 observed ~3 minutes and then
ERROR-BYPASS at Ceiling -12 on ~20s WAV. A successful 8192-frame
fixture cannot clear that physical bug; a failure isolates a
reproducible production verification case for diagnosis. Do not
silently interpret this CI as a 20-second FQ.

This is NOT 20 seconds of audio; it is a scale sensitivity test
only. No PR54 or frozen M17 production code is modified.
Do not promise a few seconds of whole-song preparation without
a bounded full-resource worker+SRC+M16 benchmark on physical Windows.
#701 is EXTENDED by design and retains all previous exact checks.


## CI #701 — Scope correction required, algorithm unaffected

Run #701: https://github.com/casaferla/RGS-MasterLab/actions/runs/39972021506.
Windows SUCCESS, full 68-case CSV with bitwise PCM PASS and independent
M16 outcome PASS in all 12 new long-input Prepare-proxy cases. At 8192
frames, Windows ratios of production checked wall /
(experimental core + independently timed M16):
44.1k -12dBTP = **34.37x**; 48k -12 = **40.64x**;
192k -12 = **111.36x**. All are potential speedups for
experimental OUTPUT_RATE-only short proxies, not product Prepare
full-song completion times. No verified physical 20-second WAV FQ.

Linux failed the **QtTest 300,000ms per-function timeout** inside
experimentalLongResourceRenderBenchmark(), not due to PCM mismatch,
M16 ceiling failure or build error. The 12 long additional cases
were bundled with the 56 previously passing tests in one QtTest function,
so their cumulative cost exceeded the runner test budget.

CI #702 restores the verified 48 legacy + 8 original short proxy
cases and retains ONLY four extra severe-ceiling boundary probes:
44.1 and 192kHz stereo, -12dBTP, at 4096/8192 frames.
Total 60 cases, no changes to core, detector, controller, input
waveform, M16 verifier, output, CI timeout or floating point.
If this still exceeds 300s, split or separately gate diagnostic
matrices rather than raise timeouts or weaken DSP oracles. Upon
#702 green the next phase is Product Owner authorised, isolated
production integration of the exact accelerated maths followed by
Focused regression and manual Windows FQ. No more speculative
algorithm micro-optimisation first.
