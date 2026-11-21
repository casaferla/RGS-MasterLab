# L3-M03 — Transient Shaper C00 / IModule staging integration

Source authority: 2026-10-06 M03 P01–P08 PO closure and adopted mathematical
pre-audit §13–§32, with later 2026-10-14 true-EOS/public checkpoint
clarification. These contracts are **not changed** in this staging lot.

## DSP law implemented (C00)

- MASTER-capable frozen `rgsml.dsp.transient-shaper` identity,
  algorithm `1.0.0`, typed parameter schema
  `rgsml.dsp.transient-shaper.parameters/1.0.0`.
- Exactly two public typed parameters: Attack and Sustain amounts,
  each in [-12,+12] dB, default 0; no threshold, ratio, sensitivity,
  mix, lookahead, routing, stereo-link mode or hidden third control.
- P[n] is max absolute mono/stereo input PCM, one shared detector,
  three fixed continuous peak envelopes (f,m,l) at 30/10/150 ms.
  Updated m uses **updated** f, as frozen.
- Normalized positive rise/decay descriptors with epsilon=1e-9,
  contributions in dB separately smoothed at 0.25 ms, shared
  same-frame audio multiplication at original Fs.
- Supported Fs exactly 44.1/48/96/192 kHz, mono/stereo only,
  latency=0, lookahead=0, drained audio frames=0, no clipping,
  normalization, event classifier or oversampling.
- At Attack=Sustain=0, current-frame output is an exact
  binary64/signed-zero-preserving copy while all five sonic states
  are updated. Distinct common-chain BYPASS remains separate.
- Preallocated max-block staging ensures transactional process
  output and five-scalar state; failed input, nonfinite intermediate,
  nonfinite output or invalid view commits neither.
- P07 conservative effective pre-context metadata derived by the
  adopted nonlinear-conditioning law; Fs=48 kHz gives 5,397,190
  frames (~112.44 s). **No delay buffer or 112-second scratch allocation,
  no claim of exact truncated-prefix equivalence.** Exact seek
  requires an authenticated compatible checkpoint or source-prefix replay.
- Sole common IModule checkpoint API, 48-byte opaque payload
  (8-byte next-frame marker + 5 binary64 states), exact
  type/version/schema/backend/fingerprint/frame-domain/format compatibility,
  reject invalid state/timeline without mutation.
- Accepted 2026-10-14 clarification: no public checkpoint during or
  after **true** EOS (category
  `RUNTIME_CHECKPOINT_DURING_FINALIZE_UNSUPPORTED`). Selected
  range end alone must not be inferred as source EOS.

## Native focused gate and exclusions

Focused Windows/Linux gate includes C00 core and IModule Qt tests:
frozen first-sample impulses at 44.1/48/96/192k, independent
per-frame binary64 oracle, ragged chunks/in-place, signed zero,
stereo swap/antiphase, reset, exact checkpoint continuation,
source-frame timeline/true-EOS and strict transactional failures.

Production GUI registration, `.rgsml` serialization, editors,
AutoMatch coordinate ownership, M04 listening/perceptual verdict,
Windows physical real-time characterization, full FQ and consumer
release authorization are **not** asserted by this math gate.

Existing L1 `rgsml.dsp.transient-shaper` registry placeholder is
kept factory-less. Next integration must attach an opt-in factory
to that existing descriptor, never register a duplicate, and must
prove Product full-prefix rendering and offline candidate lifecycle
before any UI activation.
