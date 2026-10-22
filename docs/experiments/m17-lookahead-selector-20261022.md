# M17 Experimental Lookahead Selector — PO GO / ISOLATED

Date: 2026-10-22. This branch is an **unqualified experiment**, NOT the
frozen M17 DSP implementation and NOT a candidate to merge into main.

## Intent

Build a bounded offline-batch selector of the smallest *sufficient* set of
actual signed M17 detector events (zero or one per controller tick), while
feeding those events into the **existing frozen controller**. No alteration
of controller recurrence, detector FIR, sample rates, Ceiling, output domain,
checkpoint schema or registered DSP module is permitted.

## Bound and limitations

- One batch holds at most **32768** signed events. Cache construction is
  transactional; callers must not mistake this for full-resource bounded
  streaming or a finished offline Prepare scheduler.
- Candidate uses exact binary64 demand and Hann expressions, monotone
  conservative group upper bounds, and an original-order sparse fallback.
- The frozen controller performs all density/hold/release/gain math.
- Per-tick attack-floor bit identity and test-rendered PCM bit identity
  are prerequisites, not evidence of final qualification.
- A production candidate would still need rolling bounded scheduling,
  checkpoint/resume, failure rollback, all six rates, mono/stereo, arbitrary
  chunk partitioning, M16 final verification and end-to-end Prepare latency
  on real audio. No autonomous replacement and no FQ start.

## Exit decisions

This branch remains separated from PR #54. The PO must explicitly approve
any future production integration, following a measured full-Prepare
comparison and test-based falsification. CI green is not such an approval.

## Follow-up experiment (PO-authorized diagnostic, not implementation)

CI #664 passed 24 mono/stereo, six-rate, -1/-12 dBTP cases with bitwise
PCM and frozen-controller field/snapshot checks. The Windows per-tick
microsecond timings in that report may undercount calls shorter than 1 us.
No Windows speedup inference is permitted from those old timings.

Next comparison adds -6 dBTP (36 total cases), nanosecond accumulation
for untruncated timing, and a separate three-batch selector whose cache
is rebuilt for frame ranges [0,47), [47,110), and [110,192).
The controller snapshot is restored at each batch boundary; its output
PCM, control state and clock must match the original IModule exactly.

The batch test is not a completed streaming scheduler: detector events
are collected offline before building each bounded selector. It does
NOT qualify memory consumption or throughput for long tracks, direct
disk rendering, the real Prepare command, or the final M16 verifier.


## Bounded rolling batch prototype — qualification boundary

This next isolated test adds an **unregistered**, separate
`M17RollingBatchSelector` coordinator. It accepts only contiguous
controller-tick batches, requires caller-supplied detector coverage
(or a completed detector drain), checks event order, and rejects
batch inputs larger than 32768 active events before replacing its
cache. A failed load must leave its accepted batch untouched.

The 1536-frame, six-rate, mono/stereo PCM oracle is the real
frozen M17 IModule rendered with uneven input blocks; the isolated
candidate feeds selected original events to a separate original
frozen controller, then bit-compares all emitted PCM. A controller
snapshot is restored at the frame 768 batch boundary. Test reports
the peak event-cache occupancy and asserts 24 successful 64-frame
batch transitions per case. The error scenarios include incomplete
future lookahead, oversized batches, and a non-contiguous tick
origin.

**IMPORTANT LIMITATION:** To isolate scheduler semantics, this
test harness stores the complete detector event stream to form
each batch. Thus, `high_water_cached_events <= 32768` proves only
the memory bound inside this isolated selector; it is not a
source-independent bound on the complete processing pipeline.
Likewise timings comparing the genuine IModule and this test runner
are not directly comparable since they encompass different work.
No production source, module registry, checkpoint schema, actual
Prepare, output-rate contract or M16 verifier was changed.


## Incremental bounded detector input — next isolated falsification

The preceding CI #666 is double-green on 1536 frames, six rates and
mono/stereo, with all 24 rolling batches per case and high-water active
selector events 1465..4292 versus a cap of 32768. However, its test
keeps a *complete* list of signed detector events and therefore does
not prove bounded resource-event memory.

The next test consumes source PCM one frame at a time using the actual
frozen signed ITU4x/FIR16 detector. The new unregistered
`M17BoundedDetectorFeed` retains at most **32768 active events** and
discards only events before the earliest upcoming controller tick.
The separate rolling selector keeps its own independent 32768-event
batch cache. A small, verified input frame advances the detector;
its event append is transactional with respect to invalid PCM and
cap overflow. Incomplete lookahead must be proven by the physical
detector coverage tick or successful final drain before loading a
controller batch. The controller never guesses future event ticks.

The 4096-frame test on all six Fs in mono/stereo at -6 dBTP creates
64 contiguous 64-frame batches, restores controller state halfway
through and bit-compares every PCM sample to the frozen M17 IModule,
including startup, signed intersample events, long silence, EOS and
discontinuous-input rejection. Neither an entire detector-event list
nor a full-resource peak cache exists in this **candidate test path**.

**Scope limits:** The test still owns a 4096-frame PCM input buffer and
an independent reference PCM buffer; it is NOT yet a memory bound for
the complete DAW offline Prepare pipeline, a 20-second WAV test, an
end-to-end wall-time benchmark, or checkpoint schema qualification.
The paired test uses real IModule time versus a different experimental
harness; no whole-Prepare speedup inference is permitted. Passing CI
does not authorize production integration or merging this PR.

## Longer resource / end-to-end experimental output comparison

CI #667 Windows+Linux SUCCESS: 12/12 PCM bitwise comparisons, 64
certified 64-frame batches per 4096-frame input, real signed detector
feeding candidate incrementally without a global detector event vector.
Peak detector and selector retained events are 1396..4841 per cache,
below the independent 32768 limits. This is a cache-only boundedness
claim, not a full application memory qualification.

Next isolated benchmark renders complete PCM *buffers* from the same
generated source via two alternative processing paths:
- Frozen real M17 IModule renderer (nonuniform physical streaming blocks,
  actual D-frame algorithmic delay and drain).
- Unregistered streaming detector + bounded rolling selector + frozen
  controller, with nonuniform experimental batches, controller snapshot
  reconstitution every third batch and a full output buffer.

The measured interval includes each implementation's processing and
output-buffer production. It excludes source generation, post-render
bitwise comparison and final M16 verification. All output samples must
match the frozen IModule bit for bit. Report 32768 frames at 48k and
8192 frames at 192k, both mono/stereo at -6dBTP. The 192k sample is
intentionally short to avoid overloading Focused CI.

**This is not the product Prepare benchmark.** The candidate is an
internal test-only renderer and has no disk WAV IO, project state,
UI, M16 final verification, user cancel, production checkpoint schema,
or real Prepare transaction. Timings are preliminary, single-run and
platform-specific. They must not be projected linearly to music-length
resource latency or used to authorize replacing M17. Stage 2 will
require a reproducible same-workload offline Prepare qualification,
including post-render M16 checks, memory and disk IO.

## CI #669 result and consolidated regression matrix

CI #669 succeeded Windows+Linux. Four longer synthetic render cases per
platform matched all original M17 IModule PCM bits (48k mono/stereo
32768 frames and 192k mono/stereo 8192 frames, -6 dBTP). Each bounded
cache remained within 32768 events, completed detector EOS drain, and
restored the original controller snapshot at multiple batch boundaries.

Measured ratios of baseline frozen IModule buffer production over the
unregistered experimental alternate renderer were Windows 32.8..39.6x
(48k) and 71.2..79.1x (192k), Linux 13.8..15.3x (48k) and 28.3..30.0x
(192k). These are *single-run preliminary synthetic buffer-render*
ratios across different implementations, not qualified realtime speedups,
not real Prepare performance and not extrapolatable to long music tracks.

To reduce unnecessary CI churn, the next single consolidated focused
lot combines the six sample rates (44.1/48/88.2/96/176.4/192k),
mono/stereo and four Ceiling settings (0/-1/-6/-12 dBTP) in one 48-case
matrix. The 48k/-6 cases retain the 32768-frame stress length;
192k remains at 8192 frames, all others at 4096 to bound CI runtime.

The test compares original and candidate PCM bit-for-bit both *before*
and *after* invoking the real M16 test verifier, including a guarded
single global trim if M16 deems it necessary. A matching rejection
from M16 is recorded explicitly rather than silently mislabelled a
successful final verified resource. Controller snapshots, physical
end-of-stream, event cache caps and exact output size remain oracles.
M16 durations are recorded separately from M17 output-buffer timing.

This completes the current isolated synthetic proof matrix only.
Production integration, FQ, gate M/L, export/publication and altering
the frozen M17 code remain explicitly not authorized. The actual
product offline Prepare, deterministic repeated/paired timings,
representative multi-second WAV programmes, SRC, cancellation, disk
IO and full pipeline RAM still require a separate approval/gate.

## CI #670 exact-artifact review: 36, not 48 (correction)

CI #670 Windows artifact contains **36** long-render scenarios:
six Fs, mono/stereo, Ceiling -1/-6/-12 dBTP. All 36 are
PCM-bitwise PASS before/after the M16 verifier, with M16 outcome PASS.
The planned 0 dBTP boundary did not execute because an earlier test
function, not the long-render matrix, received the loop edit.

A dedicated correction changes the *long-render* loop explicitly to
0/-1/-6/-12 dBTP and asserts exactly **48** executed matrix scenarios.
This catches future omissions even if the workflow reports SUCCESS.
This is test-only work; the frozen M17 DSP and PR #54 remain unchanged.

## Verified full matrix and next 64-frame quantum falsification

CI #671 succeeded Windows/Linux at commit 10cc93d4, and the actual
LongRender CSV artifacts each contain **48 records**: 6 rates,
mono/stereo and four ceilings 0/-1/-6/-12 dBTP. Every record reports
exact untrimmed PCM, actual M16 final check, matched M16 outcome and
bit-identical PCM after verification (48/48 PASS on each OS).
Detector EOS, cache caps and checkpoint-restored control all passed.
Highest retained detector/selector cache occupancy was 7644 events
of 32768 each. This closes the initial cross-rate synthetic
functional equivalence matrix, NOT production qualification.

The apparent acceleration is not a realtime safety bound:
at 192 kHz, 8192 source frames correspond to only ~42.7ms of audio.
The synthetic candidate's Linux output-buffer time on a -6dBTP case
was ~0.41-0.43 seconds, greater than input audio duration, even
though it far outperformed the original frozen M17 IModule.

The next *separate test-only* falsification uses exact 64-frame
quanta at 48/192kHz, mono/stereo, -6/-12dBTP (8 cases).
Per-quantum clock intervals include detector prefeed, lookahead cache
rebuild, frozen controller and final PCM writes, with true original
IModule PCM bit identity required. Report mean, p99, worst case,
cold first quantum, over-budget count and a steady-state subset
excluding 4 initial and final quanta; 64/Fs is the provisional
quantum budget (1.333ms at 48k, 0.333ms at 192k).
A deadline miss is **reported, not hidden**, and is not a CI test
failure: the test measures feasibility rather than requiring success.

**NOT realtime certification:** GitHub hosted runner scheduling,
thermal changes, Qt test harness, bulk source buffers, no actual
audio-device callback or full product M17/Prepare stack prevent any
hard latency guarantee. The reported windows are experimental
feasibility evidence only. No production backend is modified.


## Fast feedback vs extended Focused CI — experimental branch only

The #672 Linux logs show **55 CTest entries, 811.02 seconds** total
test time; verification alone took 450.28 s and checkpoint 169.72 s.
Windows ran 59 tests in 382.04 s, with verification 138.73 s and
checkpoint 53.62 s. Ninja/sccache accelerates compilation, not CTest.

The PR #55 experimental branch now distinguishes two clearly labelled
profiles under the unchanged `PR Focused CI` workflow:

* **FAST** is the automatic profile for only the exact GitHub head ref
  `experimental/m17-lookahead-selector-isolated-20261022`.
  It runs every normal selected focused CTest EXCEPT the full M17
  verification and checkpoint executables, with at most two parallel
  workers for non-extended tests. It then invokes seven named smoke
  functions from the genuine verification test binary, covering
  exact binary64 guard, M16 measurement/error handling, common
  stereo verification, basic end-of-stream and bypass semantics.
  Full M17 diagnostic CSV artifacts are not uploaded in this profile.
  Its green means **fast experimental feedback only**, not full
  tested/qualified M17.
* **EXTENDED** is the original, serial, exact full CTest command,
  retaining M17 verification and checkpoint and every CSV artifact.
  It runs for every other PR branch, including the production PR #54,
  without any regression coverage change. On experimental PR #55,
  explicitly include the string `[m17-extended]` in the HEAD commit
  message to request a fully comprehensive CI when the experiment
  warrants it. This is an engineering trigger, not a PO gate.

The original M17 detector/controller, product renderer, factory,
checkpoint and all production/UI code remain untouched. Changes are
limited to test registration, the experimental branch's CI workflow
and this document, all on Draft PR #55. This opt-in fast route must
never be mistaken for a merge qualification. PR #55 remains DO NOT
MERGE, and no Gate M/L or Full Qualification has been performed.

**Critical observation:** This refactor reduces repeated costly
experimental diagnostic loops only for the isolated branch. It does
not shorten the product PR #54, which retains its full prescribed
regression tests. Any future proposal to change the real product
CI matrix requires a separate reviewed decision and proof that
mandatory safety gates are not lost.
