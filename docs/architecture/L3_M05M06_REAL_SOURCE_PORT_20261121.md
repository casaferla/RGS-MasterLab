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
