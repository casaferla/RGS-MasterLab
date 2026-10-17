# M17 Core-first — independent detector oracle (unqualified)

Authority: PO M17 sonic freeze v1.0, startup forward delta m = -62,
Work C AC-01..AC-07. This test oracle is **not** a release result.

- The 4x rational coefficients originate in the six-file protected M16
  mathematical baseline, never in the new M17 detector under test.
- Test-only direct O(N) materialized convolution reconstructs the complete
  anchor event set. The new DSP implementation uses a chronological ring
  and must match the direct oracle for every emitted tick and peak.
- Physical signed tick locations are:
  anchor a[j] at m = 2j - 47 and midpoint v[j] at m = 2j - 62.
  A single streaming quarter emits *delayed* a[j-8], then v[j].
- For N >= 1 the phase scan includes 4N genuine source-driven quarters,
  followed by exactly 59 detector-only quarters: 44 for protected M16
  finite-support zero extension, 15 more to drain 16-tap midpoint history.
  Last midpoint tick = 8N+54. Neither boundary zeros nor negative m
  create programme PCM frames.
- The controller must start at m=-62 neutral, advance missing ticks with
  q=0, and evaluate density[q current tick] before adaptive release.
  This oracle intentionally does NOT compute rControl or attach a fake
  production limiter.
- First impulse x[0]=4, C=-12dBTP, nonzero anchor at j=13/m=-21 has
  amplitude exactly 4*730/8192=0.3564453125. This positive requirement
  occurs before programme origin tick 0.
- Frozen midpoint binary64 little-endian SHA256:
  C6E63340BE925C8A35136C78B1E6248B57813ED8C4A7B808A2E9F0F33F0DF4F7
- Cross-platform exact PCM/controller evidence is **NOT** yet available.
  These two test sources are staged before the full U1–U7 four-executable
  CI carrier; do not label a green build of unrelated modules as M17 PASS.

Negative cases: nonfinite PCM, empty EOS, repeated finalize, input after
finalize, wrong channels, incomplete phase state. Resume/checkpoint, complete
controller, latency compensation and M16 full-resource verifier are still
required before executing the full C gate and making sound claims.
