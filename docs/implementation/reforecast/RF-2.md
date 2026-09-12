# RF-2 — Post-L1-M08 core/render integration checkpoint

## Checkpoint status

`RF-2 — MATERIALIZED_AT_L1-M08; FROZEN PHASE-12 ESTIMATES UNCHANGED`

This checkpoint records actual evidence available at acceptance of CODEX TASK
011 / L1-M08. It does not edit Phase 12, RF-1, frozen task estimates, risk
states, scope, or Product Owner decisions.

## Frozen Task 011 estimate

| Scenario | Frozen PDE |
|---|---:|
| Optimistic | 2 |
| Most Likely | 3 |
| Conservative | 5 |

No elapsed-time variance is asserted. Agent-active, human-review, listening,
host-overhead, iteration, and configure/build/test-cycle durations span
interrupted sessions and were not captured by a common clock; the canonical
telemetry therefore records them as unavailable rather than reconstructing
them.

## Actual integration evidence

| Signal | Accepted actual |
|---|---|
| Baseline tests | 50 Debug / 50 Release |
| Final tests | 59 Debug / 59 Release |
| Registry transition | 11/0/0 -> 11/1/1 |
| Production DSP | Gain-v1 only |
| Render capability | bounded, synchronous, same-rate identity/Gain preview |
| Defects found and corrected | 2 |
| Regressions | 0 |
| Required human gate | PASS |
| Listening device | Altoparlanti (High Definition Audio Device), headphones physically connected |
| Dependency downloads | 0 |
| Blocking category at acceptance | none |

The implementation materialized one exact production DSP and the first
Qt-free/platform-free render boundary without changing the accepted Task 010
module interfaces or enabling any other descriptor. Independent MPFR oracle,
chunk-partition identity, Source immutability, dependency rules, tests-off,
deploy smoke, and manifest gates all passed.

## Defects and host evidence

Two implementation defects were corrected before the final matrix: the
non-zero Gain path was reduced to the frozen single multiplication per sample,
and a test-only non-owning-view comparison helper was disambiguated. Neither
left a regression. A non-gate harness-help probe incurred a bounded host-side
loader wait; the required CLI smoke with process-local Qt discovery passed and
no process, detection, or quarantine remained.

## Forward-looking recommendation

Keep the frozen Phase-12 estimates unchanged. L1-M08 proves the structural
route from canonical Source through an immutable chain snapshot to a bounded
owned preview, but one completed Gain implementation is not enough evidence to
re-estimate future DSP, export, Gold, Reference Match, platform, or
qualification work. Measure the next Product Owner-authorized implementation
checkpoint with a common elapsed-time source and preserve separate agent,
human-review, listening, and host-overhead durations before considering any
forecast adjustment.

## Preserved state

- `R12-01 — INHERITED UNCHANGED`
- `R12-02 — MITIGATED_FOR_WINDOWS_L1-M05 / RESIDUAL_MONITORING_OPEN`
- `RF-1 — MATERIALIZED; FROZEN ESTIMATES UNCHANGED`
- `ALT_F4_ACTIVE_CAPTURE_RESIDUAL — PO_ACCEPTED_DEFERRED / NON-BLOCKING`
- `INPUT FORMAT SCOPE — WAV_ONLY`
- `CODEX TASK 012 — NOT PREPARED / NOT STARTED`
