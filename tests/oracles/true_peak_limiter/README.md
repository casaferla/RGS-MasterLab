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

U4/U5 64-bit absolute-frame boundary hardening: although FrameRange input
can be representable near INT64_MAX, process now rejects before touching
state, PCM or GR if original frame origin + programme count + mandatory D
drain would exceed FrameIndex. Every supported Fs, mono/stereo, invalid
first frame and invalid second contiguous block leave checkpoint/witness
and signed-zero PCM unchanged; the exact legal exclusive endpoint MAX
remains valid and successfully drains. Separately, canonical-checksum
checkpoint forgeries at 44.1/192 kHz whose origin would overflow the
eventual drain are rejected atomically despite a valid next-input frame.
No DSP sound mathematics, terminal routing, UI or M16 frozen files change.

U4 shared-buffer / in-place alias contract: six Fs x mono/stereo x -12/0
dBTP x N={1,2D+31} stress real M17 process with exactly one owning
AudioBuffer passed as both source view and destination mutable view. Full
N+D raw production, D crop, GR witnesses and every resulting PCM sample
must reproduce the separate-buffer streaming oracle bit for bit, including
-0.0 and hot first/last samples. Nonfinite aliased input must be rejected
atomically without PCM, checkpoint or gain-witness mutation. This is
Core-first evidence, not authorization of a production host renderer.

U4/U5 multi-resource instance lifecycle: six Fs, mono/stereo, Ceilings
-12/0 dBTP and three sequential, distinct programme resources / frame
origins validate original prepare, post-EOS reset and post-EOS reprepare.
Each independently rendered reference (new instance) must be bit-identical
after exactly D raw crop. Finalized calls reject further process and cannot
emit stray gain witnesses. Both lifecycle entrypoints clear prior detector,
PCM delay, controller GR, timing and frame identity without changing strict
FP, sonic algorithms or permitting actual host resource ownership claims.

U4 EOS lifecycle negative/continuation matrix: six Fs, mono/stereo and
-12/0dBTP with a real D-frame drain. A premature EOS flag must fail before
any PCM (including -0.0), checkpoint or applied-GR observer mutation; a
legal first partial drain disables checkpoint extraction. Invalid SOURCE
domain reprepare, post-drain process and discontinuous drain must not alter
the pending real frames. The remaining drain resumed in irregular chunks
must reconstruct the separately processed N-frame PCM bitwise on the same
strict-FP backend. Still only Core-first; not product host qualification.

U4/U5 cancellation regression (same-backend Core-first only): the six-rate,
mono/stereo, two-Ceiling sequential-resource test now deliberately starts a
hot fourth resource and cancels it mid-programme before reset, then starts
another and cancels it after a true partial D drain before reprepare.
Outstanding M17 checkpoint ownership must disappear, the newly unbound
checkpoint must equal a pristine instance and the new source's exact PCM
and GR witness must match a separately prepared reference without prior
history, even with radically different nonzero absolute frame origins.
This is a model-level cancellation probe, NOT an implemented product stop,
renderer owner or UI contract.

Core-first combined pre-host integrity matrix (NOT production qualified):
reprepare the SAME M17 instance with a different output sample rate and
opposite mono/stereo layout while its preceding old-rate resource is
unfinished, or already has a partial D drain. Six adjacent old/new Fs
pairs x both layouts x two Ceilings x both cancellation points require
whole-resource PCM bitwise identity with a separately created module,
including reset of detector, lookahead, delay and absolute timeline.
Separately, a six-rate mono/stereo two-Ceiling live observer test detaches
the optional GR witness sink for 17 real emitted programme frames, then
reattaches it for the remaining input and EOS. Whole PCM must still match
no-sink output bitwise; GR observations contain the correct 17-frame
provenance gap with no invented records. This does not implement renderer
identity, final SRC, UI GR or U8 listening qualification.

Pre-integration batch: U4 maximumBlockFrames contract (64 frames, D > 64)
at six Fs, mono/stereo, -12/0 dBTP. A 65-frame process request before
stream binding and after the first accepted 64-frame block, and a 65-frame
finalize request before drain, all reject atomically: signed-zero PCM,
gain witness and exact binary checkpoint unchanged. Accepted 64-frame
stream blocks and D drain segmented to <=64 frames must reconstruct
every N-frame sample bitwise against the independent fresh render.
U5 checkpoint format-boundary matrix additionally cancels a 193-frame
old-Fs resource by new-Fs prepare (six adjacent rate pairs, -12/0 dBTP).
Obsolete digest-valid checkpoints fail without modifying the new pristine
state; a new 287-frame checkpoint matches the independent freshly prepared
instance byte-for-byte and restores to a third same-backend instance.
No protected M16 edits, renderer work or U8 claims.

Core-first final numerical-negative combined guard (no U8 sonic claim):
all six OUTPUT_RATE sample rates x mono/stereo x -12/0 dBTP test a
low-level sequence containing +0, -0, +/-binary64 subnormal minima,
half the smallest normal, tiny normals, and exact fractions, including
independent asymmetric stereo. Chunk partitions must be bit-identical,
each real quiet PCM sample must preserve its original bit pattern,
and all N real gain witnesses remain neutral (gain=1, GR=0).
An active STEREO stream separately injects NaN and +Infinity into the
last sample of the right-hand input channel after a valid first block.
Before any PCM/GR/checkpoint mutation the entire second block must be
rejected; an immediate corrected same-range retry must reconstruct
the independently processed and exactly D-cropped PCM bitwise.
This test-only fixture does NOT certify renderer ownership or U8 listening.
