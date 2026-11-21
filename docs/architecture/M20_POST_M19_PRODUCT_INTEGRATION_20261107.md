# L1-M20 — Product-facing tonal/stereo scope, PC-free staging
Date: 2026-11-07

## Upstream hold
M18 PR #84: focused #1094 Windows/Linux SUCCESS at `f3ce03f6`,
but physical Windows acceptance deferred (no PC) and no main merge.
M19 PR #86: new real five-metric AnalysisScope API, focused M19
Windows/Linux SUCCESS on `543556fa`; independent Draft, NO MERGE.
This branch builds only on that exact M19 candidate and never edits
M18 or M19 predecessor refs, existing DSP, M17 renderer or FIR.

## Immutable-source reconstruction
29 M20-specific files were imported by exact Git blob SHA from
preproduction PR #67 at `95d5df3b`, without the old base ancestry.
Their original internal FFT/Hann/PSD reuse, six-rate geometry, P02
mean-channel power and Grid100 centered correlation remain unchanged.

## Implemented Product-facing adapter
- Public `rgsml/analysis/tonal_stereo_analyzer.hpp` and one
  `rgsml_analysis` implementation source compile a typed read-only
  `AudioBufferView` AnalysisScope. No second spectral transform.
- Bounded ring-window STFT accumulation yields mean linear PSD and
  primary 1/12-octave band energies. No entire historical per-frame PSD
  materialization or QML-side spectral math.
- Mono is NOT_APPLICABLE for stereo metrics, never synthesized as dual
  mono; silence and denominator failure have explicit statuses.
- Width uses only supplied retained-loudness block unions, else
  explicitly marked full-scope fallback; Grid100 400-ms/100-ms
  correlation reports P10/P50/P90 and actual absolute origins.
- Normalized tonal-band fractions are absent without externally
  supplied *authoritative* per-band eligibility. Caller-supplied
  MISSING/ELIGIBLE/LOW_SUPPORT values are validated; no fake zero,
  unsafe support threshold or implied Phase-6 status.
- Invalid SOURCE/OUTPUT frame domains, nonfinite PCM, malformed masks
  and unsupported clocks fail before emitting a completed result.
- Six-rate QtTest exercises mono/stereo, FFT, consistent power,
  complete-window-gate absence, empty retained support, immutable
  poisoned out-of-scope samples and profile normalizations.
- The independent M20 focused workflow runs Windows+Linux Product
  CMake/QtTest plus original M19 and M20 numerical reference tests.

## Outstanding for canonical closure
Official Phase-6 FeatureDefinitionRegistry/QTP metadata, M19
authoritative retained-block producer subscriptions, per-band support
and lifecycle, real PREPARED/GOLD/PROCESSED ownership, GUI-01
composition and M20 Gate U/G. In this phase, the GUI can remain a
private isolated QtQuick preview. No physical Windows PC and no
individual FQ required by updated PO instructions; a later shared
qualification gate remains.

**STATUS: M20 Product-facing engine staging, not merged or final.**
