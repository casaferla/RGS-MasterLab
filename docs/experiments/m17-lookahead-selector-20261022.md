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
