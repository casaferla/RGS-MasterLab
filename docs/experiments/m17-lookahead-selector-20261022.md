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
