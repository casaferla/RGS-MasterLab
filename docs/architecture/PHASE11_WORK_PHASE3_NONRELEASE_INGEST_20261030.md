# Final SRC Q0 Phase3 — private Work handoff, NONRELEASE intake (30 October 2026)

## Scope and state

This repository stores **no private Phase3 ZIP, source coefficient binaries, independent proof transcripts, or Q1 reference audio**. The exact private Work handoff supplied by the Product Owner is SHA256 `b6dea3257aa2b4202f297094750e59d0f29d4bd871b106bae092c80417c40251`. Its immutable source dependencies are original Q0 ZIP `94d3340667ab5c86b2db5f2d0e191f58b5d05863413312fb3545faed51798ce4` and Phase2 ZIP `dbc5d6ba083aa54274b041e39885cce04333d767192de7c69e4ba476456fadb6`.

The independent Work report states 28/28 complete continuous spectral bands, 14/14 source FIR assets independently regenerated at 256 and 512 bits (28 coefficient receipts), nine guarded Phase2 replays after an SSE MXCSR rounding flaw was identified and corrected, 24 Phase3 tests, six Q0 preflight tests and 72 limited offline Q1/Q2 cases. These are **technical NONRELEASE results reported by Work**, not this repository's canonical approval or product qualification.

The chat operator ran a local archive audit on the exact owner-supplied ZIP; 28 bands, 28 coefficient receipts, nine guarded replays and the seven OPEN gate statuses passed provenance/inventory checks. The reproducibility / mathematical proof still requires main-operator review. No existing 44 older third-order MPFR checkpoint receipts were reclassified.

## Audit command

```bash
python tools/qualification/final_src/audit_work_phase3_handoff_nonrelease.py /trusted/private/Work_Phase3_NONRELEASE.zip
python -m unittest discover -s tools/qualification/final_src -p 'test_audit_work_phase3_handoff_nonrelease.py' -v
```

The tool is intentionally read-only, SHA-pins the **external** private archive, verifies ZIP member hashes and selected cross-receipt bindings, and fails on any forged product admission or Q0–Q6 PASS. It does **not** invoke the producer, verify the spectral inequality anew, run native Qt or change SRC admission. The ZIP and original private FIR assets must remain outside public GitHub and CI artifacts.

## Explicit M17 boundary

- Canonical Final SRC Q0 SRC-only review is **OPEN** pending operator approval; Q1–Q6 remain OPEN in their applicable product scope.
- **All 28 nonqualified SOURCE→MASTER paths remain blocked.** Do not change `m17_final_src_candidate_admissible`, the Q0 asset pins, the frozen DSP, device ownership or the accepted MASTER gate on this evidence alone.
- Normal M17 single Prepare -> immutable MASTER -> separate Master Audition DEVICE conversion/return has its own real Qt tests and still needs physical Windows Product Owner validation. No second Prepare should be introduced.
- No M18 start, full FQ or merge to `main` absent the governing Product Owner gate.

The fast route to M18 is to finish canonical Q0 SRC intake and the applicable actual M17/Final SRC conversion tests in one cohesive package, not broaden this to Reference Match or all unrelated Final Pipeline work.

## Next qualified package: independent exact-rational envelope check

An additional short portable script `tools/qualification/final_src/phase3_exact_model_q0_nonrelease.py` rederives the specifically stated Phase3 FFT twiddle, butterfly depth, input roundoff, gradual underflow and passband sufficient-bound inequalities with Python `Fraction`. The operator executed the same arithmetic locally (PASS). Both existing Focused Q0 preflight jobs will execute the script; **no private Q0 binaries or Work ZIP** are needed in CI. This is an arithmetic **NONRELEASE** confirmation of declared formulae, not an independent C++ producer or MPFR implementation proof and not M17 SRC admission. Work Phase3 formal error model and original SHA remain unchanged; all Q0–Q6 gates remain OPEN.

## 2026-10-31 — PO IMPLEMENTATION-SCOPE OVERRIDE FOR FINAL SRC / M17

**Mandatory interpretation:** M17 Final SRC's usable release candidate is validated on the **existing product implementation**; 36 SOURCE→MASTER pairs and one M17 Prepare with independent DEVICE listening are required. Q0 proofs for 14 kernels/28 bands and applicable SRC correctness/alias/partition checks are not waived. Q1–Q6 cross-pipeline duties for *unimplemented* Android, Reference Match and other future features do **not** block M17 completion. Do not mark an overall Final Pipeline gate complete by passing the M17 subset. Subsequent platform and Reference Match integrations receive their own qualification when implemented. Product Owner's next milestone after the M17 FQ gate is unequivocally **M18 Dither**; no intervening features or premature M18 work.
