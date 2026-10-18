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
- The separate test-only `replay_control_from_events` now implements an
  offline reference recurrence from the **direct M16-based 8x event list**.
  It does not invoke the M17 controller or read its state or gain observer.
  For the same strict-FP backend, six Fs x four Ceilings x mono/stereo x
  two adversarial waveforms compare the reference and applied gain bitwise,
  then reconstruct both output lanes from those predicted gains.
- First impulse x[0]=4, C=-12dBTP, nonzero anchor at j=13/m=-21 has
  amplitude exactly 4*730/8192=0.3564453125. This positive requirement
  occurs before programme origin tick 0.
- Frozen midpoint binary64 little-endian SHA256:
  C6E63340BE925C8A35136C78B1E6248B57813ED8C4A7B808A2E9F0F33F0DF4F7
- The offline replay is exact only **within the same FP backend**; it is not
  a cross-platform golden-bit fixture. Cross-backend and physical Windows
  U8 qualification are still outstanding, irrespective of focused CI green.

Negative cases: nonfinite PCM, empty EOS, repeated finalize, input after
finalize, wrong channels, incomplete phase state. Other M17 tests separately
exercise checkpoint/restore, end-of-stream compensation and M16 full-resource
verification. Resource ownership in the actual renderer, product bypass, U8
listening, and full production qualification remain open.

Checkpoint U5 addendum: the dedicated checkpoint suite exercises repeated
same-backend replacement on 8191-frame nonzero-origin 44.1/192-kHz stereo
streams across D and EOS, validating serialized shape, atomic forged-state
rejection, PCM and gain-witness bitwise continuity. A separate six-rate
bound-envelope test rejects one-ULP changes solely in the right lane, owner
and chain revision mismatches. These are TEST-ONLY proofs, not production
resource ownership, CPU-budget certification or acoustic qualification.

AC-06 inactive-path test-only reference (not shipped bypass): six sample
rates x mono/stereo x four Ceilings x N in {1,D+17}, including hot PCM,
negative zero and subnormals, must bit-copy exactly N frames with the
original absolute range. No M17 instance, latency, drain, GR, M16 trim or
Ceiling clamping occurs. Independent M16 observation remains above Ceiling
for deliberately hot bypassed PCM; this is intentionally NOT a bypass
Ceiling certification. The separate enabled run is observably attenuated,
verified by full-resource M16, and cannot be confused with bypass identity.
Actual host/renderer bypass routing and zero-latency product integration
remain unqualified, requiring separate authorized scope.

U6 transactional test-only verification addendum: the M16 global scalar is
applied first to an owning candidate OUTPUT_RATE resource; complete M16
remeasurement must pass before a bitwise PCM commit to the caller. Invalid
Ceiling, wrong frame domain, nonfinite PCM or >0.10dB trim guard failures
leave every original lane bit-for-bit untouched. Six-rate mono/stereo
flat-versus-irregular chunk matrices check exact PCM before/after one-shot
whole-resource M16 verification, including last-frame transients. This
improves the TEST harness only and does not qualify the product renderer.

U4/U5/U6 combined integration: the real M17 core is interrupted at frame
1 or N-1 and restored into a separately prepared same-FP-backend instance
through the TEST-ONLY full-resource owner envelope. All six supported rates,
mono/stereo, -12/0dBTP, N<D and N>2D, and irregular frame partitions
must yield the identical compensated N-frame PCM as uninterrupted playback.
The final transactional M16 full-resource measurement/trim and every PCM
sample also agree, with status SUPPORTED_UNQUALIFIED. The invalid foreign
owner is rejected atomically before a valid restore. This is not a claim
of product renderer ownership, bypass routing, listening quality, or U8.
