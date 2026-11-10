# RGS MasterLab — PO GO: limited Phase-6 M19–M21 Product numerical delivery

**Date:** 2026-11-10. **Decision:** PO explicitly authorizes implementation work on the existing M19–M21 Product numeric presentation. **Gate G has NOT been executed.** **Release/numeric-QML activation: NO until qualified.**
**Baseline:** focused CI user-reported Win/Linux GREEN at `41381ef37f1d250df413d73915951f7e0078c8ac`; PR #99 Draft; `main` unchanged.
**Term:** UI **REFERENCE**, internal/API/project **GOLD** unchanged. RGS means Reference Guided Sound.

## 1. Exact scope of this GO

Implement **one** genuine, owner-checked, limited Product path from real M19/M20 analysis via the **existing** M21 Product worker, typed metric import, M21 publication inbox, and the permanent QML M19/M20/M21 pages. Do not duplicate FFT, loudness, width, correlation, source state, owner epoch, QML models, or registry. Preserve FQ99 Windows min-size geometry 1184 × 688 and protected DSP/icon layout.

- **M19:** canonical P01 genuine integrated loudness (LUFS); full-scope RMS and sample peak (dBFS); crest (dB). Values permitted only with valid current scope, source, sample rate and qualified per-feature authority.
- **M20:** genuine static mean PSD (linear spectral power, not fabricated normalized shape), genuine active loudness-gated P06 stereo Width ratio, Grid100 local correlation p10/p50/p90, mono/silence/no-support states. Do not infer authoritative spectral mask, eligible octave-band dB or `normalized_shape_db` from M19 gate or linear-power fractions.
- **M21:** render the **existing** authenticated PREPARED / independent REFERENCE / PROCESSED columns and deltas `REFERENCE − CURRENT` from **unrounded** available operands. Preserve independent scope, no assumed time synchrony, and honest missing/stale/invalid/low-support status.
- **M16 True Peak:** **remain explicitly unavailable** within these readouts until *separate* M16 numerical authority and measurement coverage is qualified. Never copy sample peak to true peak or extrapolate regional TP. It must not block correctly qualified other fields.
- P02 MEAN_CHANNEL_POWER, P03 cell-first-moment policy and P06 loudness-gated active union remain frozen as recorded by the 2026-10-05 P01–P06 semantic closure. No P05 macro-dynamics occupancy, Phase 7 AutoMatch, Level 2/3 DSP or unknown new metrics.

## 2. Gate prerequisites are not waived by this authorization

The frozen Product Owner Phase-6 residual semantic decision dated 2026-10-05 states `PHASE6_IMPLEMENTATION_AUTHORIZED_BY_THIS_DECISION=NO` and `GATE_G_EXECUTED_BY_THIS_DECISION=NO`. **Today's separate PO GO** authorizes the specifically bounded implementation above; it **does not** retroactively alter the frozen decision, change its formulas, count as Gate G, or authorize a numeric release.

Before even one user-visible numeric QML value becomes active, the implementation must carry genuine versioned canonical FeatureDefinitionRegistry/FeatureStatus evidence (or reconcile a pre-existing authoritative registry, not create a parallel private one), immutable current producer/realization identity, P01 exact bank member ID/hash, selected scope/domain, channel/rate/version, P02/P03/P06 policy IDs as applicable, measured support, quality/reason and official **Gate G** mathematical qualification of the released subset. Otherwise every UI field remains unavailable/STALE with no fabricated zero. A private `ProductMetricsOwnerClaim` is **not** official Phase-6 authority.

Any UI binding added ahead of Gate G must be non-activatable in current Product builds, and tested for fail-closed behavior. The status-only `M21ProductStatusViewModel::production_authorized()==false` and current QML unavailable state remain until the authoritative Gate G release path is actually implemented and approved. Never flip this flag solely because a focused CI passes.

## 3. Exact icon and non-bypass semantics (PO instruction)

- The already integrated vector `analysis-levels`, `analysis-spectrum`, `analysis-delta` glyphs reside in the **existing** 16 px `StudioIcon` slot, rendered as `AnalysisModuleIcon` QML Shapes. No new bitmap icon, size or chain row.
- M16 meter and M19–M21 are permanently visible observer/service entries, `nonProcessingService=true`, `bypassSupported=false`, `bypass=false`; none is a ProcessingChain DSP. They have no ON/OFF, BYP, reset or audio mutation.
- The existing `DspChainSelector.qml` `permanentlyLit` condition controls **both** whole-row bypass dimming and icon opacity/halo exactly like M16. The analysis icon stays fully bright/glowing even while its metrics are missing, STALE or awaiting Gate G. Source availability is an *editor data* status, not an observer bypass.
- Correct the text-only accessible/tooltip interpretation for always-on observers to say `Always on · Read-only` rather than falsely saying `Manual`. Preserve all legacy 7 icons and FQ99 layout. The existing frozen-GUI byte comparator may normalize **only** this explicitly PO-requested small text delta and the already approved additive vector child.

## 4. Focused qualification and exit

Build/run existing CTest and M23 source guards on Windows and Linux, including M19 full-scope six-rate tests, M20 stereo/P06, independent >128-MiB WAV REFERENCE real decoder + owner/inbox, nonprocessing module adapter parity, vector-QML geometry and new limited numeric candidate validation. Keep user-visible numbers gated until a separate official Gate G result. No automatic merge of PR #99, no change to `main`, no FQ PASS without PO physical Windows M18/M23 and M24 17-step/listening verdict. No false closure from code coverage.

Implementation staging follows the single-writer epoch; make substantial batches, one CI promotion per coherent candidate, no invented commit counts. Record actual result in Drive.
