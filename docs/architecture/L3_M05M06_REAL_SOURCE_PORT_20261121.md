# L3 M05/M06 — M19/M16/M20 real Product feature port (nonrelease)

Source authority:
- L3-M05 **ACCEPTED_FROZEN**, 2026-10-06, Tonal T1–T6 and Loudness L1–L3, including TP scoped/full-resource separation, independent PREPARED/REFERENCE pair support and fixed configured domain masses.
- L3-M06 **ACCEPTED_FROZEN**, 2026-10-06, Dynamics D01–D12, Stereo S01–S06, accepted B01/B02/B03 masses/support and safety semantics.
- L3-M10 P01–P05 **ACCEPTED_FROZEN**, 2026-10-07: authoritative 27 binding groups, M08 optimizationTarget, full configuration support gate 0.60 and deterministic measured tie/scheduler.
These source contracts are **not edited** by this source-port implementation.

## Actually connected, WITHOUT a new audio analyzer

`app/src/internal/l3_m05m06_real_analysis_port.hpp` consumes ONLY
`analysis::BasicLevelAnalysisResult` and
`analysis::TonalStereoAnalysisResult` from the existing canonical
Product pipeline. `OfflineProductCandidateMeasurement::observed_m05m06`
is read-only and **does not rerender audio, rescan FFT or replay the
reference**. The M18/SOURCE/REFERENCE owner must supply and re-authenticate
all candidate/reference epochs after worker completion; this DTO
**cannot authenticate its own owner claims**.

| Frozen feature group | What is imported from REAL Product | Remaining solver-grade prerequisite |
| --- | --- | --- |
| M05 L1 Integrated | M19 gated Integrated LUFS, exact K-bank name/hash/scope | Explicit M07/M08 VALID/CURRENT, ≥10-s reference-grade and optimizationTarget |
| M05 L3 True Peak | M16 full-owning-resource measurement exposed by M19 | Actual M16 qualification; separate M05 regional estimator where scope requires it; project safety constraint independent |
| M05 T1 normalized 1/12 profile | M20 normalized eligible-band fractions converted ONCE to `10log10(f)`, signed canonical band indices preserved | M07 authored canonical eligibility, ≥2-s reference-grade, paired common comparison bandwidth/support, 1/3 mutually exclusive fallback where required |
| M06 D05 PLR | Existing SAME-scope qualified TP minus Integrated | No substitution when TP unqualified or no LUFS |
| M06 D06 global crest | M19 Sample Peak minus full RMS readout | Exact producer status and M06 scope/owner eligibility |
| M06 S01 broadband width | M20 width ratio, `MONO_ENDPOINT_LOG2`; width0→z−24 and 0<width<2^-24 rejected | Retain exact mono/side-muted endpoint, safety and protected masks, M07 validity |
| M06 S02 broadband correlation | M20 Grid100 raw P10/P50/P90 and frozen Fisher-Z transform incl. exact ±1 endpoints | M06 pairSupport=min(valid/complete native grid), full source authority, raw P10 stereo safety |
| M05 T2–T6/L2; M06 D01–D04/D07–D12, S03–S06 | **Not produced by this port** | Actual frozen feature producers and supporting source evidence, never invented missing/zero or reweighting |

The public `Readiness::measured` status means solely a **finite
observation by an existing M19/M20/M16 producer**. It is NOT a Phase-6
`VALID/CURRENT` label, not source grade/eligibility authorization,
not a completed M05/M06 configured feature mass, and not M08 target
attainment. The incomplete 27-binding set always fails
`full_m05m06_objective_ready()`; no `Jtarget`, achieved or M13
verification can be claimed from this source port.

## Native gate

Real independent 48-kHz stereo PCM is actually analyzed on both
full owning resources by the M19 full-resource and M20 scope public
APIs. Native tests assert measured integrated loudness, real width,
M16 qualifier distinction, exact M20 canonical signed band geometry,
explicit fixture-only supplied eligibility and paired normalized
power-fraction dB. Supplementary tests check S01 width0 and subfloor,
S02 Fisher endpoints, native Grid100 validity ratio, PLR and global
crest provenance, M18 owner/scope failure, missing band mask refusal,
and nonpublishing M03 candidate integration. Fixture-only pretend
eligibility MUST NEVER be promoted to Product/M07 authority.

The next functional milestone is implementing/reusing the missing
M05/M06 authoritative feature producers and their stable M07/M08
feature registry/target bindings, then wiring true full-cost
candidate evaluation into M10 with M11/M12 ownership and later fresh
M13 verification. A partial M21 nine-cell probe is not an alternative.

## Further M05 tonal reuse — T2–T6 (subsequent writer macro-lot)

`l3_m05_real_tonal_statistics.hpp` reads **the original M20
mean-channel canonical time-averaged PSD and the existing 1/12-octave
band energies**. It does NOT perform any other STFT/FFT, introduce a
second spectral producer or assume the M07 common-band comparison.

- T3 spectral centroid is the exact continuous clipped-cell first
  moment divided by common-band PSD energy.
- T4/T5 are the first positive continuous PSD cell whose cumulative
  power crosses 85%/95% with within-cell frequency interpolation,
  never nearest-bin.
- All three use an **identical M07-supplied commonComparisonBandwidth**
  fully supported by actual M20 Fs and the frozen source E≥1e-12
  condition. The raw Hz and transformed log2(f/1Hz) observations
  remain separately available.
- T2 tilt and T6 8–16k HF slope reuse the M20 measured canonical
  band energy per Hz. Named full intervals (100–10k, 8–16k), ≥3
  usable bands and ≥80% above-floor are enforced before unweighted
  OLS in (log2(fc/1kHz),10log10(energy/Hz)).
- **The exact spectral floor is an external M07/configuration input**;
  without it, OLS is unavailable. Source/reference comparison
  must use the same immutable interval/floor/evidence identity.
- No M05 ≥2-s source-grade/Phase-6 status, absent 1/3 fallback,
  M08 optimizationTarget, 27-feature objective, constraint or
  M13 verification is manufactured from mathematical observations.
  `OfflineProductCandidateMeasurement::observed_m05_tonal`
  reuses the Product's already computed M20 readings.

Native analytical test oracles include *flat power/Hz* PSD
(E=9900, centroid=5050Hz, rolloff P85=8515Hz, P95=9505Hz for
[100,10000]) and synthetic 2.5dB/oct power-density OLS over
actual M20 band geometry, missing/invalid energy, explicit floor,
full named intervals, ≥80% support and direct M03 Product reuse.

## M05 L2 + M06 D01 — source-grade later, one existing K pass now

A subsequent additive writer lot extends the **preexisting** M19
K-weighting per-100-ms quarter source, without a second filter,
duplicated audio traversal, changed integrated one-pass power gate,
true-peak estimator, retained gate union or any of M19's five static
numerical scalars:

- Whole complete chronological 3.000-s windows start every 0.100 s
  (30 consecutive exact K-weighted quarters). The source series
  records *every* complete start, its absolute source frame, an
  optional ungated 3-s LUFS value and the original K-bank identity.
  Finite values below -70 LUFS are retained, zero-power windows
  remain NULL and complete-window denominator is never shortened
  to improve apparent support. Insufficient length and numerical
  undefined remain separate.
- L2 canonical Type-7 P50 uses the unrounded finite source window
  population with no temporal alignment to REFERENCE. Source
  support is valid native / complete native. Pair support is the
  frozen MIN(PreparedSupport,ReferenceSupport), recorded once.
- M06 D01 uses exactly those original complete ST windows for
  Type-7 P10/P50/P90/P95, returning the three macro-shape components
  [P10-P50, P90-P50, P95-P50]. They share the original D01 mass
  0.15 under Macro=0.50; no duplicated 0.15 for each percentile.
  M19 Type-7 P50 source value is cross-checked; no re-K-filter pass.
- Source-grade (>=10s active, authentic fully valid Grid100 series),
  canonical complete-time owner/Phase6 FeatureStatus, M07 stable
  binding/evidence signatures and M08 optimizationTarget are still
  independent admission gates. A finite ST value or D01 observation
  MUST NOT be labelled measured full M10/J, achieved or M13 verified.
- Regression test uses 10-second actual 48-kHz mono/stereo PCM,
  checking exactly 71 complete windows, all original M19 five scalar
  metrics and M16 scope semantics, nonzero absolute frame offset,
  gain translation 20log10(2) across each ST reading/P50/Integrated
  and D01 gain-shift invariant differences.
- Synthetic mathematical oracle additionally checks Type-7 percentiles
  on [0...9]: P10=0.9, P50=4.5, P90=8.1, P95=8.55, and D01
  relative components [-3.6,+3.6,+4.05].
- The same Windows/Linux focused gate includes original M19 public
  Product analyzer and independent streaming REFERENCE regression
  targets, not a second long FQ campaign.
