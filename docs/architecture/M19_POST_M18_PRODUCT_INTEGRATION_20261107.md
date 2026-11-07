# L1-M19 — Product implementation without physical Windows PC
Date: 2026-11-07

## Frozen predecessor boundary
- M17 qualified and merged in main.
- M18 is a **separate Draft** PR #84, exact Focused CI #1094 Windows/Linux PASS
  at `f3ce03f65483af63f2832c3d7edf98a83b76af7e`.
- M18 physical Windows acceptance is explicitly deferred by the PO,
  because no PC is currently available. **M18 is not marked complete,
  qualified or merged.**
- This M19 branch is a **separate staging implementation fork of the
  exact M18 candidate**, not an M19 merge into main or into M18 PR #84.
  Do not retarget M18, rewrite its frozen head or begin any final FQ.
- There are no independent FQ runs for M19–M22: Focused native Windows
  and Linux numerical tests only, then one shared FQ after integration,
  subject to PO authorization.

## Original preproduction reused bit-for-bit
M19 independent Draft #64 at `0c92b14c2808f888ba2d2a48b4cca470ba8c4f8b`
contributed its 21 M19-specific files (frozen K banks, scope adapter,
whole-programme numerical candidate, mathematical selftests, Python
oracles and private workflow) **without** copying the outdated
pre-M17/M18 ancestry. No candidate K coefficient or M16 FIR changed.

## Product integration added on this branch
- Public `analysis/basic_level_analyzer.hpp` typed read-only API.
- `measure_basic_level_scope`: actual `AudioBufferView` subview,
  exact absolute frame origin + domain check, six supported sample rates,
  four scalar calculations and *explicit M16 regional TP unavailable*,
  not a false 0 dBTP or surrogate.
- `measure_basic_level_full_resource`: five scalars for an actual
  complete owning `AudioBuffer`, invoking the **existing public M16**
  full-resource measurement and preserving its
  `SUPPORTED_UNQUALIFIED` status.
- `rgsml_analysis` source registration includes existing
  `true_peak_measurement.cpp` adapter and new M19 analysis.
  No change to source-ordered M16 FIR/strict-FP implementation.
- Native QtTest: six-rate full resource, authoritative M16 equivalence,
  region poison/domain refusal, explicit silence and short duration.
- Independent M19 Windows/Linux Focused workflow on a distinct
  candidate branch; **not** Full Qualification or GUI acceptance.

## Deferred canonical authority / UI
The present public API is Product-facing numerical groundwork. An
official Phase-6 FeatureDefinitionRegistry, lifecycle status/QTP,
AnalysisScope invalidation tokens, M19 result subscription to real
PREPARED/GOLD/PROCESSED resource owners, formal independent G and
Windows GUI acceptance must be reconciled before asserting M19 closure.
No fake live LUFS/TP or second TP filter may be added to shortcut it.
Preview/UI can remain approximate in this PC-free integration phase.

## Dependency sequence
M19 -> M20 -> M21 -> M22 are developed in isolated branch succession.
M20/M21/M22 original private PR #67/#70/#79 stay untouched and Draft.
Every M19 delivery must preserve M18 SOURCE/Project/export Single Writer
Epoch, immutable M17 terminal candidate, final WAV and 16 FIRs.

**STATUS: PRODUCT M19 IMPLEMENTATION STAGING; NOT RELEASED OR MERGED.**
