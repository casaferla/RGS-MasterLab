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
  **subject line**, not merely its body to request a fully comprehensive CI when the experiment
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

## Profile-trigger correction (2026-10-22)

The first profile-switch commit described the extended marker inside
its own commit body, inadvertently matching the original whole-message
search and causing CI #673 to exercise the full suite. The correction
matches the HEAD commit **subject only** (Git `%s`), ensuring that
documentation or body references cannot accidentally enable the
extended path. Subsequent runs show the mode in the select-profile
step; treat that visible mode as the authoritative test-scope record.


## CI #674 FAST verification and targeted stage attribution

CI #674 (commit d9cce578) is double-green, with the log explicitly
confirming FAST profile selection. Linux CTest fell from 811.02 seconds
(CI #672, complete 55 tests) to 91.61 + 0.19 seconds
(53 ordinary tests plus one selected M17 smoke). Windows fell from
382.04 seconds (59 complete tests) to 144.51 + 0.13 seconds
(57 ordinary tests plus one M17 smoke). These are *test-step*
durations, not whole job durations; runner variance applies.
Original full M17 checkpoint/verification remains required when a
complete diagnostic/FQ-equivalent signal is actually sought.

The next extended experiment does not change the frozen M17 math.
It decomposes each 64-frame quantum into:
1. incremental signed detector feeding and stale event discard,
2. rolling cache selection/preparation,
3. unchanged frozen controller tick advances,
4. final PCM lane writes.

Report per-case mean, p99 and maximum of each stage separately while
retaining the original outer quantum mean, p99, worst case and budget
miss counts. Per-frame clocks introduce instrumentation overhead;
component sums are diagnostic hypotheses rather than hard realtime
latency certificates. Preserve the exact PCM oracle, both cache caps
and the all-rate functional verification regression. The commit
subject requests [m17-extended] once so Linux and Windows upload the
complete M17 quantum artifacts. Return to FAST for later routine edits.


## CI #675 stage decomposition and targeted exact cache reuse

CI #675 at b8f065fd succeeded on both Windows and Linux in EXTENDED
mode. All eight 48k/192k mono/stereo, -6/-12 dBTP synthetic cases
passed bitwise PCM, finite detector drain and cache caps.

At 192k, quantum 64-frame budget = 333333 ns. The eight-case
cross-platform diagnostic shows that the most expensive stages are
the frozen controller tick recurrence and rolling lookahead cache
preparation. Windows mean controller costs 331..401 us per quantum,
cache 205..260 us; Linux controller 2584..2968 us, cache 711..906 us.
The total stage clocks are diagnostic, not additive realtime
certificates. Every Linux 192k quantum missed the budget; Windows
missed 122/128 or 121/128 per case.

**Falsifiable experiment**: the exact Hann weights and suffix upper
bounds depend ONLY on the frozen sample-rate-derived lookahead length.
The existing selector rebuilt them and evaluated many cosines on
every single 64-frame rolling cache load. Retain those *identical*
binary64 weight values within the private experimental selector
instance, computing once on its first dense batch. The rolling
scheduler previously copied the entire accepted selector before
calling its already-transactional `prepare`; avoid that redundant
copy, still validating all incoming events and staging new vectors
before committing the cache. The transactional invalid-load tests
remain active. Add an assertion that 64/128 successive quanta each
reuse the same single Hann table construction count = 1.

These changes do NOT modify the frozen detector, controller (including
exp/pow release and gain), FIR, user-visible processing chain, factory,
output rate, checkpoint or source PCM. They must produce the exact
same binary64 PCM on the full original 48-scenario M17/M16 suite,
longer resource and quantum oracles. Use one EXTENDED focused CI
to compare stage costs after reuse; if any bits differ the experiment
is falsified, not accepted. Shared hosted runner times can vary.


## CI #676 result; isolated lazy scalar-controller falsification

CI #676 is Windows/Linux SUCCESS on aa4206fc, with 48/48
long-render and real M16 post-resource bitwise PCM oracles PASS on
both systems. At 192kHz, exact Hann precompute and removal of a
redundant selector deep copy reduced cache cost by about 51–62%
(Windows) and 38–43% (Linux); end-to-end 64-frame quantum means fell
by about 16–22% (Windows) and 10–12% (Linux). The one-time table
construction assertion passed all 8 quantum cases on each platform.

The frozen reference controller's per-tick transcendental math is now
the dominant isolated budget cost. New private test-only
`M17LazyController` reproduces the signed-event attack lookahead,
required level, density, tau, attack/hold/release branch, reduction
and binary64 gain **with the exact original expression order**.
It experiments with *only* two control-flow redundancies:

- Original always evaluates the release `std::exp` on attack/hold
  paths even though no release candidate is read. The alternative
  computes the identical exponential only when the release branch
  is taken, preserving the release coefficient formula bit for bit.
- Original evaluates `std::pow(10,-applied/20)` for each nonzero
  applied reduction. The alternative reuses the previously computed
  binary64 gain if and only if the current `applied` bit-level
  reduction is identical to the previous one. A controller checkpoint
  restore invalidates this private memo and forces recomputation.

The 48-scenario bitwise-oracle test includes all six sample rates,
mono/stereo, Ceiling 0/-1/-6/-12 dBTP, selected original signed
detector events, both controllers stepped identically at 8 Fs, every
numeric output field compared by binary64 bits, actual frozen IModule
PCM for every emitted frame, and a midway controller restore.
It includes a 2560-frame 192k stereo case to exercise more of the
hold/release history. Collect raw controller timing and counts of
release `exp` evaluations avoided / gain `pow` evaluations reused.
Do not turn timing measurements from interleaved paired execution into
a hard real-time claim.

This is a **different isolated controller implementation**, not a
patch to the production controller. If it differs by a single bit,
the experiment is falsified; production M17 remains ACCEPTED_FROZEN.
Even a green 48-scenario result only supports the tested conditions,
not equivalence for every possible binary64 input. Full future
integration would require a separate PO decision, adversarial states,
checkpoint ABI coverage, no-alloc real-time proof, native live-path
metrics and hard-gated complete qualification.


## CI #677: confirmed exact lazy controller, integrated next step

CI #677 Windows/Linux SUCCESS at 4a116e37. Both Lazy CSV artifacts
contain 48/48 successful cases for six supported Fs, mono/stereo,
Ceiling 0/-1/-6/-12. Every controller field, checkpoint/restart state
and actual frozen IModule PCM sample matches bit-for-bit. The private
controller skips some unused release exp and reuses exact equal-gain
pow results; **this is not yet proof of realtime**.

The median unpaired per-scenario isolated controller speedup is about
1.28x Windows, 1.04x Linux; at 192 kHz median is 1.30x Windows and
1.05x Linux. The brevity of many tests (384 frames, one case 2560)
overrepresents startup/hold: on many higher-rate cases 100% of release
exponentials are skipped. Full-track release pressure and scheduler
effects remain unproved. The raw timing is from one paired hosted
runner pass, not a deterministic benchmark.

The next single [m17-extended] experiment directly compares complete
64-frame output pipeline paths over the same synthetic full input:
- Real signed bounded detector feed and validated rolling batch cache,
  with the original frozen controller;
- The **same independent signed detector/cache** and test-only lazy
  controller, retaining the original output domain/PCM and timing.

The two paths are executed sequentially on each runner with
sample-rate-alternated order, never concurrently. All final PCM bits
must match the genuine frozen IModule, independent of the other path;
detector EOS, output size, event cache caps, and exactly one immutable
Hann table build remain mandatory. Each path emits per-quantum mean,
p99, worst and missed deadlines against 64/Fs, plus full elapsed
nanoseconds and lazy exp/pow skip counters. Four synthetic configurations
at 48k and four at 192k, mono/stereo, -6/-12dBTP, 4096/8192 frames.
This intentionally excludes whole product Prepare, SRC, disk IO, full
length music, repeated timings and genuine host callback scheduling.
It is a comparative research diagnostic, not an RT certification.
The product M17 controller/PR54/main remain untouched.


## #678 verdict: lazy control math is insufficient in the whole 64-frame pipeline

CI #678 on 301fad95, Windows/Linux SUCCESS, included 16 full
integrated FROZEN-vs-LAZY rows per OS (8 scenarios, both controllers)
and the full 48-case end-to-end PCM/M16 verified suite per OS.
All 16 integrated rows independently PASS PCM bitwise against the
real frozen IModule and drained the detector. At 192 kHz, FROZEN/
LAZY mean quantum durations were nearly indistinguishable: the
four paired scenario ratios on Windows were 0.990, 0.996, 0.998
and 1.001; on Linux 1.003, 1.002, 0.989 and 1.001.
This falsifies the notion that skipping unused controller exp/pow
alone is a sufficient solution to real-time operation. Preserve
the positive numerical equality evidence but stop treating the
lazy optimization as the leading real-time candidate.

Next [m17-extended] investigation: separate the two code regions
previously combined under the name 'controller cost': actual lookahead
selector choice (rolling cache query) and controller advance (frozen
or lazy control recurrence). Capture per-quantum accumulated selector,
advance and all-other wall times (mean/p99/worst) on eight
48/192k mono/stereo ceiling -6/-12 scenarios, with both controllers
in the same test. Preserve exact IModule PCM equality, M16 postcheck
and memory caps. The additional timing reads **change benchmark
overhead**, so resulting absolute 64-frame times MUST NOT be directly
compared to uninstrumented #678 to claim improvements/regressions,
nor used for realtime qualification. They determine which operation
deserves the next algorithmic proof. No production code changes.


## #679 EXTENDED result and the exact FAST/EXTENDED/FQ boundary

CI #679 at 88502f34: Windows/Linux both SUCCESS, complete Focused
profile confirmed by runner logs (Linux: 55 CTests, 403.06 s;
Windows: 59 CTests, 434.76 s). All full extended M17 verification,
checkpoint, PCM reference comparisons and M16 postverification ran.
**EXTENDED means the complete *Focused CI*, not PO-controlled Full
Qualification.** FAST applies ONLY to the exact experimental PR55
branch, when the latest commit *subject* lacks [m17-extended]:
full M17 verification/checkpoint CTests are deferred and an explicit
selection of short M17 QtTest cases is run instead. All non-PR55
branches retain complete original Focused behavior. The experimental
source and full M17 maths remain unqualified and must not merge.

The #679 full-quantum integrated attribution separates previously
conflated selector and recurrence costs. At 192kHz, across 4 FROZEN
scenarios, averages of the observed per-scenario proportions:
- Windows: rolling future lookahead selection 49.6%, frozen controller
  recurrence 11.6%, detector/cache/PCM and other timing 38.8%.
- Linux: selector 62.6%, controller 4.9%, other 32.4%.
The temporal instrumentation itself perturbs wall duration: compare
stage attribution, NOT absolute quantum time vs uninstrumented #678.
64-frame hard-real-time still not established. Do not invest more
isolated effort in lazy release exp/pow as the leading bottleneck.

Next **FAST-only diagnostic**: one selected QtTest slot separately
exercises the experimental group-bound selector on deterministic
9000-event, 256-query sparse/dense windows at 48/192kHz. Record
exact numbers of events visited and skipped by its certified group
upper bounds and compare *every* selection and weighted attack-floor
bit-for-bit with an independent brute-force all-events Hann oracle.
Those synthetic events deliberately include sub-Ceiling events to
probe skipped zero-demand groups; do not treat the visit ratio as a
measurement of actual bounded detector streams, real music, or CPU
performance. Publish a small SelectorWork CSV artifact on this branch.
This ordinary diagnostic change shall use FAST (no
[m17-extended] commit subject), unlike algorithm modifications
requiring the full extended PCM/M16 oracles. If FAST fails or the
mathematical bound is falsified, correct and rerun before proceeding.


## #681 two-tier bounded lookahead prototype (experimental only)

A second conservative hierarchy stores one maximum exact binary64 demand
for every aligned 512-event superblock (16 existing 32-event groups). A
complete, aligned superblock within the **certified** lookahead may be
skipped only when `max_demand * suffix_Hann(first_future_lag)` is
**strictly less** than the already selected attack floor. Any incomplete
superblock, bound equality or non-prunable block follows the existing
32-event group / original-order event walk. This leaves the original
Hann binary64 evaluation and strict winner tie order untouched, while
potentially avoiding repeated 32-group bound checks for distant events.

The test-only 48/192k sparse/dense independent brute-force oracle now
counts superblocks pruned and checks per-query visit+skip conservation.
The full [m17-extended] Focused CI is required because this changes the
experimental selector execution path: real M17 PCM/M16 bit identity,
checkpoint, six sample rates, channel configurations and EOS remain
obligatory. This is a *candidate hypothesis*, not a claimed speedup;
no improvement is asserted until paired CPU/quantum timing with p99 and
max is available. No frozen M17 or production code is modified.


## #682 FAST paired selector measurement (same-run A/B)

The preceding #681 EXTENDED Windows/Linux succeeded with exact 6-rate
PCM/M16 and frozen-controller oracles. The deterministic work CSV
recorded 640/3584 complete 512-event superblocks skipped per 256-query
case at 48/192 kHz, with identical visited-event counts and exact
selection results to the prior group-only path. Cross-run timing
comparison remains noisy and **192k/64-frame realtime is NOT proven**.

Next test-only diagnostic compiles the prior 32-event grouped traversal
and the new 512-event superblock traversal from a single templated
selector implementation. The production-facing experimental `choose()`
continues to select the superblock path; the explicit
`choose_group_only_for_test()` is *never called by the renderer*.
The same prepared cache and 256 deterministic queries are timed with
alternating A/B order; both results must agree bit-for-bit with each
other and with the independent all-event binary64 Hann oracle. Each
platform exports separate mean/p99/max selector nanoseconds and
paired speed ratio for sparse/dense 48/192k. This instrumented,
synthetic, short-run comparison is exploratory, not an audio-thread
benchmark, an end-to-end realtime claim, or FQ. Test-only FAST is used
because the result returned by `choose()` is structurally unchanged;
full Focused EXTENDED remains required for any subsequent runtime
algorithm changes. No production M17, PR54 or main changes.


## #683 EXTENDED: Conservative 256+512-event hierarchy

CI #682 FAST Windows/Linux SUCCESS. Paired selector-only A/B exact
winner/attack-floor checks PASS. At 192kHz the 512-event selector gained
2.53x/2.67x on Windows and 2.36x/2.67x on Linux (sparse/dense), versus
32-event groups on the SAME synthetic workload. The 48k sparse Windows
case was nearly neutral (0.989x). Those timings exclude the controller,
real detector input, overall quantum, and application audio callback.

The isolated experimental candidate now first tests a complete aligned
512-event superblock, then an aligned 256-event halfblock if applicable,
then exact 32-event groups and original-order event inspection. Each bound
is the exact maximum demand multiplied by the monotone conservative
suffix-Hann upper weight at the FIRST future event. Pruning requires
strict inequality; equality and partial blocks follow the prior path.
The original 512-only path and 32-only path remain accessible ONLY through
named test functions (not registry or renderer) for a three-way identical
cache, rotated-order benchmark at 48/192k. All three results must match
one another and an independent brute-force binary64 Hann oracle exactly.

This alters the PR55 test-only selector runtime and therefore requires
EXTENDED Focused CI for full real-IModule six-rate mono/stereo ceiling,
M16 postverification, streaming EOS, bounded events and checkpoints.
This is a falsifiable candidate: no speedup or 192k realtime guarantee
until same-host timing and per-64-frame budget/p99 evidence. No frozen
M17 DSP, PR54, staging, integration, main or LANE B code touched.


## #685 FAST: Real detector sourced event-density oracle

CI #684 Windows/Linux EXTENDED SUCCESS, including six-rate exact PCM/M16
and checkpoint oracles. Paired 256+512 vs 512-only synthetic selector timing
was mixed: at 192kHz sparse, Windows 0.984x and Linux 0.947x (regressions);
at 192kHz dense, Windows 1.066x and Linux 1.076x (small gains).
The test-only 256 block addition is not yet demonstrated to be a
generalizable optimization. Overall audio realtime remains NOT PROVEN.

To avoid further unproductive hierarchy changes, this FAST-only test
generates source PCM in 48 deterministic cases: six output sample rates,
mono/stereo, -6/-12 dBTP, sparse transient/dense wave inputs. It feeds
the actual frozen signed detector via the unregistered bounded M17 feeder,
drains EOS, and uses *active signed events* to build the same selector cache.
For 128 contiguous controller ticks starting at m=-62 it independently
compares all three selector paths to each other and a brute-force binary64
Hann attack-floor and strict-tie winner oracle. Per-host paired selector
mean/p99/max and skip coverage append to the existing SelectorWork CSV,
with pattern prefix "detector_" distinct from previous invented-event rows.
The existing output path, original frozen M17 implementation, checkpoint
ABI, active registry, and product PR54 are unchanged.

This exercise is a limited **test-only** source-detector workload. It is
neither a long musical audio benchmark nor a guarantee for 192k/64-frame
hard realtime. No LANE B R1 protector was adopted. The independent
R1 feasibility gate found no passing protector and has no authority to
change the bit-identical LANE A task.

    
## #686 EXTENDED: sparse Hann one-time exact binary64 cache

CI #685 FAST Windows/Linux SUCCESS: 48 detector-sourced cases,
six sample rates × mono/stereo × {-6,-12 dBTP} × sparse/dense.
Every 32-group / 512-event / 512+256-event selector matched independent
all-event Hann binary64 oracle for attack floor and selected event.

Across 24 detector-sourced sparse scenarios per platform, the additional
256-event level yielded essentially neutral median A/B effects; it also
regressed on several cases. Dense scenarios gained only modestly.
For truly sparse *prepared* batches with fewer than 64 active events,
the current selector still recalculates identical Hann cosines per
future event and per tick. This is the next isolated optimization.

On a nonempty batch, prepare() now constructs immutable weights (and
suffix bounds) once if missing; empty batches do not allocate weights.
The original binary64 cosine expression, its exact evaluation order,
and original per-event strict '>' update order are retained. Sparse
selection reads weights[lag] rather than recomputing the same cosine.
All former group/superblock semantics, event coverage, detector ABI,
frozen controller and PCM multiplication remain untouched.

A test-only original sparse cosine selector independently compares the
old and cached results bit-for-bit, including winner and attack floor,
on actual signed detector workloads with fewer than 64 active events.
Additional short, noisy same-run per-query timing is diagnostic only;
this does not by itself prove improved full quantum or hard real-time.
The next Focused CI is EXTENDED (full M17/M16 PCM, checkpoint, EOS, six
rates and mono/stereo), as the isolated selector runtime changed. No
LANE B R1 protector, frozen M17 production, PR54/main, FQ or merge.


## #687 EXTENDED: Cache exact sparse event demands (same binary64)

CI #686 completed Windows/Linux EXTENDED SUCCESS. On detector-sourced
fixtures, 8/24 sparse cases on each platform contain 1..63 active events;
previous work eliminated repeated Hann cosines for this path, but sparse
choose() still recomputes the exact logarithmic demand for every future
event on every controller tick.

The isolated test selector now precomputes the same `20*log10(peak/Ceiling)`
for EVERY prepared event, not merely batches of 64+ events. Sparse choose
uses `demands_[index]`, computed by `demand_db()` in the original event
order with identical FP operations. No density threshold, Hann attack
computation, strict winner, event tick, controller, PCM gain, or checkpoint
ABI changes. The test-only legacy sparse path recomputes both log10 and
cosine on demand; for 1..63 detector-sourced events per batch, the test
alternates old/new execution order, cross-checks both bit-for-bit with
the selector result and independent all-event Hann oracle, and exports
paired mean/p99/max and ratio in SelectorWork CSV. Diagnostic per-call
nanoseconds exclude batch preparation and cannot establish whole-quantum
realtime or lifecycle resource limits.

This changes PR55's isolated selector and therefore mandates full
EXTENDED Focused CI for real IModule PCM/M16 exactness, all six sample
rates, mono/stereo, ceiling, EOS and checkpoint. No change to product
M17 frozen, PR54/main, UI, FQ, LANE B protector or merge.


## #688 FAST: Certified selector cache construction versus selection

CI #687 EXTENDED Windows and Linux both SUCCESS; PCM/M16 six-rate oracle,
checkpoint, signed detector EOS and the sparse exact binary64 old-vs-new
selection checks remain PASS. On real detector events with 1..63 retained
active records, old repeated cosine/log10 selection vs cached Hann/log10
showed a median 6.90x Windows and 2.39x Linux selector-only advantage
across eight qualifying scenarios per platform, with strongly varying
system clock granularity and no end-to-end realtime assertion.

The next test-only FAST diagnostic adds same-event preparation cost to
48 detector-sourced scenarios, without changing the experimental selector.
It separately times a first/cold prepare() (includes Hann table creation),
a second/warm prepare() on exactly the same validated event batch, and
the total 128 controller tick calls to choose() after preparation.
A warm/preparation-over-selection ratio distinguishes per-batch cache
rebuilding from repeated choice traversal. New columns are appended to
the existing SelectorWork CSV for all detector-based rows; the original
invented-event rows have zero/not-applicable entries.

These preparation measurements deliberately use ONE 128-tick certified
window and do not model the repeated advancing 64-frame output quantum.
One cold and one warm sample per case is subject to allocator, CI-runner
jitter, and timer granularity; treat results as a bottleneck screen,
not as a precise throughput claim. The independent strict binary64 Hann
winner and attack-floor oracle remains unchanged and mandatory.
Fast Focused CI is adequate because only tests and experiment notes
change; future implementation change again mandates EXTENDED.
No R1 replacement, frozen M17 change, PR54/main, FQ, UI, or merge.


## #689 EXTENDED: Incremental cached exact log10 demands

CI #688 FAST Windows/Linux SUCCESS. Its detector-sourced benchmark
showed that dense warm `prepare()` on one certified batch can exceed
the 128-tick selector work, notably at 192kHz with 4.68x Windows and
1.95x Linux maximum prep/selection ratio in these limited fixtures.
This is not a callback realtime measurement.

The isolated selector now reuses an immediately preceding event demand
iff both the signed detector tick and binary64 peak bits match in two
sorted, fully validated batch snapshots. The selector Ceiling is fixed
over the instance lifetime. Reused demand bits were computed by the
unchanged original `20*log10(peak/Ceiling)` routine. All new/changed
events recompute via that same expression; no floating-point operand
order, strict Hann winner or PCM multiply is changed. A failed batch
never modifies the previous accepted state or reuse counters.

Test-only `prepare_full_recompute_for_test()` forces the original
per-event demand calculations. The existing 48-case FAST detector test
(six rates, mono/stereo, two ceilings, sparse/dense) now advances from
its initial physically certified window to the adjacent 64-frame batch.
It measures incremental vs full-recompute preparation and compares
the subsequent 16 selected future-event winners and binary64 attack
floors bit-for-bit. Its short `incremental,` CSV rows are distinct
from existing SelectorWork columns; the timing order is fixed and
must NOT be treated as stable platform-wide throughput or an end-to-end
realtime proof. The independent brute-force all-event Hann oracle
continues to qualify the earlier 128-tick window.

The algorithm change triggers full EXTENDED Focused CI including the
true M17 IModule PCM, six sample rates, M16, EOS and checkpoint gates.
No frozen production M17, PR54/main, UI, FQ, merge or R1 substitution.


## #691 FAST: Fair warm-cache paired demand reuse benchmark

CI #690 Windows/Linux EXTENDED SUCCESS. In 48 detector-derived
adjacent batches, 73,094 / 91,303 event-demand records (80.06%)
matched exact tick+binary64 peak and were reused. No M17 frozen
PCM/M16 mismatch was observed. The prior experimental timing,
however, compared a WARM incremental selector against a newly
created COLD full-recompute selector. Previous speedup ratios
therefore confounded cached Hann initialization with log10 reuse.
Do not interpret them as the incremental demand speedup.

This FAST-only test correction initializes the full-recompute baseline
with the same certified prior eventSpan twice as the candidate, and
asserts both built Hann weights exactly once. Only THEN it times
the next contiguous 64-frame batch. Execution order is rotated
between scenarios by a deterministic parity rule. The resulting
`incremental` CSV rows mark benchmark order, WARM_BOTH and the new
independent next-window all-event strict binary64 Hann winner oracle
(16 fresh controller ticks per case), in addition to peer equality.
No selector implementation, PCM, controller, detector, registry,
L4, R1 protector, PR54 or main code changes.

Runner noise, single-sample prep timing, and absent callback overhead
still prohibit any claim of hard realtime. Future runtime change
requires EXTENDED Focused CI. This test/docs-only refinement uses FAST.


## #692 EXTENDED: Reuse bounded batch allocations (candidate)

The #691 FAST paired warm-cache results showed demand-reuse speed ratio
median 1.068x Windows and 0.895x Linux (48 detector-sourced cases),
despite an 80% exact reusable demand fraction. The extra lookup and
repeated allocation overhead can outweigh removing log10. This does
NOT establish 192k/64-frame realtime.

The isolated PR55 selector now holds bounded spare vectors for active
signed events, computed demands and three-level aggregate maxima.
All input validation finishes before any scratch mutation. Completed
scratch state swaps into the accepted snapshot; the old snapshot is
retained as the next scratch buffer. All old group/superblock/halfblock
maxima are ZEROED with assign() before rebuild (plain resize would be
incorrect). The immutable Hann weights and strict binary64 winner
comparison are unchanged. Extra buffers increase bounded memory use:
this is not an RT allocation-free nor overall-lifecycle memory claim.

A test-only heap-fresh selector path performs the EXACT same cached
demand reuse but allocates fresh batch arrays. The 48 detector
fixtures compare both strategies from identical prior snapshot,
with alternating order, exact event/winner and attack-floor
comparisons, and the existing independent brute-force Hann oracle.
The result appends scratch/fresh timings to incremental CSV rows
without confusing them with end-to-end callback timing.

As runtime selector code changed, execute full EXTENDED Focused CI
on Windows/Linux (real PCM, M16, six rates, stereo, EOS/checkpoint).
No frozen M17, main/PR54, R1 protector, UI, FQ or merge.


## #693 EXTENDED: Integrated 64-frame stage decomposition

CI #692 Windows/Linux EXTENDED SUCCESS with real IModule PCM/M16 and
checkpoint/EOS exactness. Across 48 detector-sourced paired selector
batches, reusable scratch buffers vs fresh allocations gained only
1.017x median Windows, 1.026x median Linux, with 28/48 and 34/48 wins,
and mixed dense 192kHz results. This is insufficient to claim hard RT.
The existing integrated lazy-quantum observer at 192kHz still misses
the 333.33 microsecond 64-frame budget. For instance #692 Windows
192kHz LAZY mean per quantum ranged 424.7..540.6 microseconds; Linux
ranged 2237.8..2610.7 microseconds. Instrumented CI runners are not
audio-realtime hosts and these measurements are diagnostic only.

This test-only change adds separate bounded stage durations to the
existing 48/192kHz mono/stereo, two-ceiling, FROZEN/LAZY integrated
64-frame quantum observer. Stage clocks surround: detector feed and
discard, certified event batch extraction, selector.load (including
prepare and group aggregation). Existing select and controller advance
per-tick clocks stay unchanged. The new residual is total quantum minus
all five measured stages and includes PCM writes, peak_at, test checks,
timer overhead and remaining bookkeeping. The old 'other' aggregation
remains for backwards comparison. Per-scenario mean/p99/max clocks are
written as 12 new CSV columns to IntegratedLazy Windows/Linux artifacts.

The added instrumentation is test-only and does NOT change the
selector, frozen M17 backend, RGS PCM, exact arithmetic, or module
registry. We deliberately invoke EXTENDED Focused CI because its
isolated FAST profile does not execute integrated lazy-quantum
diagnostics and FULL targeted verification is required to collect
the evidence. EXTENDED is not Full Qualification, and no gate
state, PR54/main, UI or LANE B R1 is modified. Do not make any hard
realtime claims from these extra timing probes.


## #694 EXTENDED: Bounded final feasibility sprint (CI #694–#697)

PO checkpoint after CI #693: the exploratory optimization loop is
now limited to an explicit four-CI evidence and decision sequence,
with at most one reserved corrective CI for a red build/test. This
is a planned decision horizon, NOT a guarantee of achieving 192kHz
hard realtime by a fixed number of commits. Production substitution,
relaxing six-rate/neutrality constraints or changing UI would require
a separate PO decision.

- CI #694 (this commit): test-only, 64-frame integrated diagnostics
  for per-quantum active detector events and selector visit/skip work.
  It adds mean/p99/max active event, visit and skip counts, plus total
  conservative 512- and 256-event blocks skipped and visit percentage
  to both Windows and Linux CSV. All observations use the already
  actual signed-detector, certified lookahead and bit-exact IModule
  PCM-versus-frozen oracle. No new runtime selector algorithm.
- CI #695: at most ONE bounded, falsifiable exact selector change
  selected from #694 work distribution. Same-run integrated 48/192kHz
  A/B timing must support whole-quantum gain on BOTH runners, not just
  synthetic microbenches; if unjustified, replace this with a NO-CHANGE
  decision gate rather than constructing speculative variants.
- CI #696: independent repeated integrated and p99/worst evidence,
  strict binary64/PCM/M16 and six-rate/mono/stereo guard. Reject a
  regression in any protected oracle or a merely noisy one-host gain.
- CI #697: publish a provisional GO / further directed research /
  STOP-and-offline-fallback technical recommendation. A go/no-go to
  replace frozen M17 remains exclusively PO-authorized and requires
  separate Full Qualification and product gates.

Current #693 measured integrated LAZY at 192kHz 64 frames:
Windows mean quantum 425908–537764 ns, p99 555300–819200 ns;
Linux mean 2834985–3355787 ns, p99 3802314–4782267 ns.
Hard deadline is 333333 ns. The instrumented CI runners are
NOT hard realtime audio machines; their high latency is diagnostic,
not proof of general impossibility. Dominant Windows mean cost
is selector choice 173095–195594 ns; selector load
76575–104632 ns; detector/feed 68673–130138 ns;
controller 62539–63390 ns. At 48kHz/64 frame the
budget is 1333333 ns, and Windows meets these limited hosted
scenario means. None of this is a hard RT qualification.

Any #695 runtime algorithm change MUST use EXTENDED Focused CI;
#694 uses EXTENDED to execute integrated profiling (omitted by
the FAST test profile). No FQ, merges, main or PR54 changes,
no UI change, no protector R1 and no reopening C4.


## #695 EXTENDED: Single exact monotone-window experiment

CI #694 EXTENDED passed Windows and Linux with the certified
48/192kHz 64-frame PCM-identical integrated diagnostic. For
192kHz LAZY Windows, the selector mean ranged 122.8–140.4 us
per quantum, with 3.68–4.69% of future detector events visited.
The conservative hierarchy already skips >95% of events, so yet
another group-pruning level is unlikely to solve the main cost.
Mean overall quantum was 287.6–367.4 us on this hosted run,
with p99 381–461 us, exceeding the 333.33 us hard budget
in all four Windows scenarios. Linux timing exceeds it by far.
These are instrumented CI runners, not hard realtime proof.

#695 makes ONE isolated algorithmic change: scheduler-driven
monotone window boundary tracking. The rolling scheduler already
proves strictly consecutive controller ticks, so a cached lower
index for tick m and upper index for m+H can advance by walking only
events that actually crossed those boundaries. The first tick and
any nonsequential access still use EXACT original upper_bound,
and every successfully prepared batch resets cursors to avoid
indices from a previous snapshot. Frozen Hann binary64 weights,
strict winner ties, detector event order, ceiling, stereo linking,
controller state and exact PCM multiplication are untouched.
The original arbitrary-order choose() and a new test-only rolling
binary-search selector remain independently callable.

The real detector 48/192kHz integrated 64-frame test now executes
BOTH CURSOR and BINARY selector modes for each FROZEN/LAZY controller
using the same input and certified feed/batch/PCM oracle. It
alternates mode ordering and records full quantum mean/p99/max,
misses and the full stage attribution for each mode. This is
the falsifiable same-run A/B. No standalone microbench result
can substitute for a whole-quantum improvement on both platforms.
Because PR55 experimental selector runtime and rolling scheduler
change, full EXTENDED Focused CI is required. Any red stops
new optimization and triggers correction/rollback on PR55 only.
No frozen M17, PR54/main, FQ, UI, R1, C4 or merge.
