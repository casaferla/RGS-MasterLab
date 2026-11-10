# M21 Private Evidence Integrity — nonrelease evidence, 10 November 2026

**Status:** staging-only / private preproduction. NOT Phase-6 FeatureDefinitionRegistry, Gate S, Gate G, numeric QML publication, physical Windows approval, or a Full Qualification.

## Qualified technical baseline

Focused CI #47 on commit `1c6e09a7ee60ab0fb5d4e5ddd7edf49a64ce8c6d` completed SUCCESS on Windows and Linux. It qualified bounded M19–M21 async owner-thread capture, single-flight worker and stale role/epoch rejection. It did NOT qualify the additional integrity checks below.

## New private snapshot invariants (this macro-lot)

- M20 static PSD bins: finite increasing physical frequencies, no duplicates, frequencies from 0 to `sample_rate / 2` inclusive, no negative/unbounded/nonfinite power. Zero physical PSD is legitimate; a numeric zero mislabelled `MISSING` is not. The private consumer caps at 1,048,576 points to avoid unbounded validation.
- M21 external tonal bands: producer-supplied `band_id` must be unique, but ordering is not imposed; each numeric shape-dB cell requires common-support eligibility. Band profile resolution is only 1/12 or 1/3, with an external pair-scoped comparison-support ID; a missing profile must not carry an orphan proof ID. Cap 4,096 private bands. No value is inferred from M20 normalized linear fraction.
- M19/M20 scalar and owner contracts remain unchanged: finite values for `PRESENT`, no value for `MISSING` / `STALE` / `N/A`; 6 supported rates, nonzero generation, valid resource identity, supported scope label and overflow-safe absolute scope.
- Publication is fail-closed: a rejected replacement cannot change `STALE` to `CURRENT`; REFERENCE keeps its independent sample rate/scope while Golden-delta calculations are invalidated when REFERENCE changes; no PREPARED/PROCESSED substitution.
- Original M16 True Peak gate remains missing until explicitly qualified by its own canonical authority; Gold remains the internal persisted identifier but GUI shows REFERENCE.

## What has NOT been authorized

The accepted P01–P06 semantics DO NOT imply `PHASE6_IMPLEMENTATION_AUTHORIZED_BY_THIS_DECISION` or `GATE_G_EXECUTED_BY_THIS_DECISION`. No numeric QML, second feature registry, fake signatures, altered DSP algorithms, new FIR assets, changed FQ99 qualified DSP editor, or merge to `main` is part of this lot. The M21 Product ViewModel still exposes only lifecycle/quality, with `productionAuthorized = false`.

## Test gate

New Qt native cases are appended to the already executed `rgsml_m23_alpha_diagnostic_tests` on Windows and Linux. They verify PSD bounds/ordering/power, unique independently certified band IDs, absence of phantom zeros, bad scope/rate/identity, stale-state retention, and independent REFERENCE comparison. Source-contract checks are updated to require the new validators and their tests. These are focused native regressions, not physical PO acceptance.

## Next dependency

A genuine, versioned canonical Phase-6 producer/FeatureStatus and signature authority, plus real Product owner/scope/epoch verification and dedicated Gate S/G/M, must exist before any M19–M21 numeric display. Confirm explicit Product Owner authorization for implementation that crosses this boundary. Until then, complete safe source-side tests and present `UNAVAILABLE` / `STALE` honestly.
