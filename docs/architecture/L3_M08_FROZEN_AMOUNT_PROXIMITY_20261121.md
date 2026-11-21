# L3-M08 — Frozen Amount, DesiredTarget and P02 proximity arithmetic (nonrelease)

Authority: L3-M08 P01 and P02 **ACCEPTED_FROZEN** (2026-10-07),
M07 P01 + closure and original M08 pre-audit §§4–5/11–12. No
historical authority text or M05/M06 math was modified.

`l3_m08_frozen_amount_target.hpp`:
- Exact M07 four-domain order, with Global separate from four domains,
  independent Amount revision, normalized persisted binary64 [0,1],
  -0 Amount canonicalized to +0 while source coordinate sign remains.
- Exact RNE64 `a=RN64(global×domain)`, endpoint selection for a=0/a=1,
  and three separately rounded operations `delta=zGold-zPrepared`,
  `movement=a×delta`, `zDesired=zPrepared+movement`. Intermediate
  arithmetic uses volatile binary64 storage to suppress contraction
  into a fused operation. Native test checks the frozen Crest
  Prepared12/Gold9 and effectiveAmount0.4→desired10.8 example.
- Distinct MONO_SIDE_MUTED vs POSITIVE_WIDTH_FLOOR exact endpoints,
  no new endpoint status inferred from a coincident numeric result
  of an interior interpolation. Original source -0 bits preserved.
- Explicit two positive subnormal stored Amounts rounding to a=0:
  NUMERICAL_EFFECTIVE_AMOUNT_UNDERFLOW_TO_ZERO; not a user amount0,
  controller suspension or zero target. Missing source input remains
  missing even at Amount0.
- No Amount JSON/persistence, target checksum/derivation signature,
  projection to satisfy hard constraints, achieved measurement,
  REF_LINKED plan or M13 verification asserted.

`l3_m08_frozen_proximity_reporting.hpp`:
- Receives already-authenticated P/Gold/C hierarchical distance and
  same required comparison population. It NEVER recomputes M05/M06
  feature loss or silently crops a missing candidate component.
- P02-A: initial within full deadband and candidate still close
  means raw=1, PROXIMITY_ALREADY_MATCHED. Candidate departure
  returns null with explicit reason; never a fake 0/0=1.
- P02-B: any required candidate component dropout returns null
  CANDIDATE_REQUIRED_COMPONENT_MISSING; no survivor reweight.
- Ordinary raw proximity = 1−distance(C,Gold)/distance(P,Gold).
  Overshoot and negative worsening are stored raw; UI clipping does
  not sanitize or alter the observed result.
- P02-C: explicit requested domain mask independent of Amount.
  Every canonical domain has fixed mass 0.25. Each current comparable
  domain contributes a nullable 0.25×raw; structural coverage,
  known/unknown effective coverage, conditional comparable-domain
  denominator, PARTIAL and NO_COMPARABLE_DOMAIN are separate carriers.
  Exactly the frozen example (0.8,0.4 with two unavailable FULL
  domains) returns contributions 0.20+0.10, coverage 0.50,
  conditional proximity 0.60, PARTIAL, **not** unqualified Full Match.
- Native tests cover signed-zero, nonfinite/out-of-range Amount,
  ordered domain set, endpoints, subnormal underflow, candidate dropout,
  already-matched/leave, raw -1 worsening, explicit request
  membership, missing effective coverage and conditional reporting.

M08 proximity requires authenticated M07/M05/M06 comparative
population and Phase-6 source eligibility; the module does not
fabricate those values or the M13 status. The correct Product-facing
integration will consume this numerical core with M08 lifecycle and
the existing offline M10 worker, not a new DSP/FFT registry or
alternative renderer.

The current canonical DSP factory count is runtime evidence, not a
frozen M07 policy constant. CI #17's observed base count is five;
the M07 native test now tests immutable base + exactly two opt-in
C01/M03 factory additions, without holding a stale historical count.
