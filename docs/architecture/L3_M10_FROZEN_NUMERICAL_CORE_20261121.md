# L3-M10 — Frozen deterministic numerical refinement slice (nonrelease)

Authority: **L3-M10 P01–P05 ACCEPTED_FROZEN**, 7 October 2026.
Baseline: `RGS_MasterLab_L3-M10_Deterministic_ReferenceMatchSolver_Objective_Coordinates_Budget_Search_August_Authority_Reconciliation_PreAudit_Work_2026-10-07_v1.0` §§6,15–18,21,24.
PO final closure: `RGS_MasterLab_L3-M10_Deterministic_ReferenceMatchSolver_Product_Owner_Closure_ACCEPTED_FROZEN_2026-10-07_v1.0`.

## Implemented pure numerical policy (not production M10)

`app/src/internal/l3_m10_frozen_coordinate_refiner.hpp`:
- Consumes **supplied** authoritative semanticNodeId/parameter key
  unsigned canonical bytes and a fully measured seed key, no minted
  persistence UUID or QML-side sort.
- Maximum 48 active coordinates, exactly four single sweeps
  [0.25,0.125,0.0625,0.03125]. Each coordinate's minus and plus
  proposals originate from the *same pre-direction current vector*.
  Both are evaluated before accepting an updated incumbent for the
  next coordinate. No hidden rerender of CURRENT, extra sweep,
  adaptive step or backtracking.
- Bounds outside normalized [0,1] are rejected preflight without
  rendering/clamp. Further actual hard legality / coupled predicates
  / M09 evidence / DSP prepare conditions belong to the authorized
  source-side adapter.
- Measured P05 total order: quantized totalJ, quantized Jtarget,
  quantized R, active owned module count, unsigned canonical CURRENT
  type/semantic-role/parameter tuple, and unsigned topology/evidence
  identity. Q=1e−9 ties-to-even. Raw scores/bytes retained.
  **Neither proxy score nor band count is a tie key.**
- Adapter event receipts distinguish complete executed render+analyze,
  failed attempted render/analyze, known preflight rejection,
  applicable exact-completed cache reuse, and budget refusal.
  No 801st render can be attempted; cache may still legitimately
  return completed evidence at budget 800. Best measured valid result
  never discarded by an invalid attempted direction.
- Outcome is explicitly nonpublishing; cancellation/budget exhaustion
  do not claim achieved/verified proximity. Topology seed attempts
  and initial incumbent score are inputs supplied by M09/M05–M06,
  not synthesized.

`app/src/internal/l3_m10_frozen_coordinate_transform.hpp`:
- Pure M10 P01 normalization with **caller-supplied frozen bounds**:
  additive/threshold/percent linear, frequency log2, positive
  time/Q/shelf log, ratio log from1, zero-inclusive time with
  t0=0.1ms.
- Singleton bounds are fixed coordinates, never divided by zero;
  NaN, Inf and out-of-bound values reject, no clamp, automatic
  reference rebase, replacement M09 band/evidence or silent
  transformation of manual DSP defaults.
- Native independent QtTest covers endpoint/roundtrip, 48-coordinate
  schedule, exact minus/plus same-current order, all four steps,
  quantized comparator, preflight/out-of-range, attempted failure,
  exact complete-cache hit, budget cap and cancellation.

## Not yet implemented or claimed

This slice **does not** supply the 27 authoritative M05/M06 features,
hierarchical support-aware objective and hard candidate constraints,
the M07–M09 canonical serializer/seed/topology adapter (domain8 /
combined32 / measured shortlist2), M08 Amount/proximity lifecycle,
REF_LINKED materialization/persistence M11–M12, Product/Qt connector,
true full-song performance measurements or new M13 authoritative
verification. The previously built nine-cell private M21 probe is
a separate engineering instrument and **MUST NOT** be represented as
the M10 frozen full objective.

The M10 P01 historical automatic-use masks contain fields/seed laws
not all present in the currently accepted C01 Dynamic EQ and M03
Transient Shaper manual module schemas. Availability/compatibility
must be proven explicitly before marking a topology READY: no
invented controls, mapped time constants, forced topology or silent
schema upgrades. The accepted milestone contracts remain unchanged.
