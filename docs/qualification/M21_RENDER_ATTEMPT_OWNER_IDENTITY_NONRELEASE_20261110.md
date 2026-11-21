# M21 Product Private: Exact Immutable Render Owner Deduplication — 10 Nov 2026

**Status:** staging-only, nonrelease. This is not numerical GUI authorization, a canonical Phase-6 FeatureDefinitionRegistry, M18 physical acceptance, nor FQ.

## Observation

An M21 scheduler already keeps an immutable M18 identity key per analysis attempt, and its admission path is scoped by exact Product epoch, SOURCE resource and processed realization. But a computed `PREPARED` / `PROCESSED` buffer can have a **different actual shared immutable RenderResult owner** after a Product update. A deduplication gate based on textual keys alone could skip that new buffer if the key components remained unchanged. The live Product epoch and same-pointer acceptance gates remain authoritative and are NOT bypassed.

## Change

The scheduler retains, per render role, `M21PrivateRenderAttempt`, whose fingerprint combines the unchanged full Product identity key with a **weak reference** to the immutable RenderResult (type-erased `weak_ptr<const void>`). Suppress a duplicate only if both key and the still-live exact RenderResult owner pointer match. A new immutable PCM owner with the same key is admitted. Expired or missing owners are never mistaken for a current render.

On successful single-flight admission the weak owner is remembered; the old capture is not retained by this bookkeeping. A still-running worker itself is permitted to hold its original immutable audio owner, but no second worker is created until it finishes. The Qt-owner completion continues to call `M21PrivateSingleFlight::matches_expected` and `M21ProductPublicationInbox::accept_render`, including `M21ProductOwnerAnalysis::still_current` against the latest actual Product state.

REFERENCE is an independent WAV resource, not a RenderResult. Its existing separate file+epoch key continues unchanged; no false alignment, SOURCE clone, or synthetic reference owner is introduced.

## Proof

Five native tests in the existing Windows/Linux `rgsml_m23_alpha_diagnostic_tests`: same key/same immutable owner deduplicates; different real owner with same epoch/key is admitted; no retained PCM ownership after old shared pointer disappears; role/scope key transitions invalidate dedup; null and expired owners never suppress a new analysis. A source guard verifies exact driver adoption of the weak owner key and all five tests.

### Explicit non-goals

No DSP algorithm, M19/M20 measures, M21 comparison mathematics, QML layout, icon, M18 export path, SourceResource persistence, authorization gate or Gate G changes. This fixes scheduling and resource lifetime only. True numerical M19–M21 publication is still blocked by `production_authorized() == false`, independent of any focused CI outcome.
