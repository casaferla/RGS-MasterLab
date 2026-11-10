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

## 5. First staged implementation checkpoint (nonrelease)

Implemented on the current single-writer branch, in the **existing** `app/src/m21_product_status_view_model.hpp/.cpp`, as the `phase6ReleaseReadouts` Qt property and `phase6_release_readouts()` method. The method first checks `production_authorized()`, the real Qt owner thread and the one authentic inbox; current `production_authorized()` stays hard **false**. When future certified authority is granted, the implementation is prepared to project:
- M19 current selected source's available non-M16 LUFS/RMS/sample peak/crest values;
- M20 current selected source's P06 Width and Grid100 p10/p50/p90 and authentic linear static PSD, while never inventing unsupported normalized 1/12-octave dB;
- M21 one 9-row table for PREPARED/REFERENCE/PROCESSED and the preexisting REF-minus-current deltas, only for strictly `Quality::present`, finite, current owner/scope cells; row 3 M16 TP stays masked.

Current returned map is **always empty** and is not consumed in QML. The existing status-only lightweight readiness bridge is unchanged. The native Windows real-REFERENCE integration test now asserts an empty C++ readout AND an empty Qt property even after legitimate short and >128-MiB REFERENCE analysis. Source guard checks the strict release guard and no QML binding. This is real Product presentation preparation but **not** official Phase-6 FeatureStatus/Gate G, not a release, and not a visible new numeric widget.

The existing M16/M19–M21 selector `permanentlyLit` rule already ensured correct no-bypass row brightness and icon glow. A PO-requested text-only accessibility/tooltip correction prevents the three read-only analyzers from being mislabeled Manual; the frozen FQ99 GUI guard now normalizes only this exact text delta and the previously approved vector addition. Analyzer geometry, opacity/glow expressions, processing chain and all old core icon silhouettes remain unchanged. Additional native and source guard tests assert this precise parity.

No other changes to Phase-6 mathematical policy or calculation are authorized by this checkpoint. Follow-up implementation must reconcile a genuine existing canonical registry/official numeric Gate G authority and only then activate `phase6ReleaseReadouts` in the QML controls. Do not set `production_authorized()` true merely to demonstrate numbers in screenshots.

## 6. Later PO clarification, same date — display genuine measurements NOW

The PO explicitly clarified: **real M19–M21 results should be shown immediately in the development panels, not withheld until Gate G; formal qualification can be tracked separately.** This supersedes **only** the earlier restriction on *development visibility*, not the mathematical P01–P06 contracts, official qualification, version receipts, ownership checks or release status. No synthetic values and no stale epochs.

The existing `M21ProductStatusViewModel` therefore exposes two deliberately distinct properties:
- `analysisReadouts`: read-only real PCM observation for the development GUI, extracted exclusively from `M21ProductPublicationInbox::view_current()` on the owner Qt thread with current source/epoch/lifecycle/quality, finite PCM metrics and independent REFERENCE identity. It is shown even though formal Gate G is pending.
- `phase6ReleaseReadouts`: remains empty until the canonical verified `production_authorized()` issuer is actually present; **do not confuse current observations with an official validated FeatureStatus**.

The permanent `AnalysisStatusEditor` forwards `analysisReadouts` to the already approved `AnalysisApprovedContent`; the existing M19 numeric-well font/spacing, M20 chart, and M21 six-column table now show finite actual results and REF-minus-current deltas. The M20 graph converts authentic PSD linear-power bins to a *relative display scale only*, with no fabricated normalized tonal band data. A missing/invalid/stale metric shows “—”, not zero. M16-dependent True Peak remains absent when the true M16 scope is unavailable, including long WAV streaming. No control triggers DSP mutation.

**Tooltip audit and completion:** prior M19–M21 integration had 1-second stationary tooltip support on the three chain icons and module state only; the interior panels **did not** have the per-metric/per-source help used by older DSP editors. Under this PO clarification, add `StudioTooltipDwell` to the M19 tiles, per-role loudness relation, M20 PSD, width, Grid100 P10/P50/P90, band-support notice, the M21 rows, and PREPARED/REFERENCE/PROCESSED selectors. All use the existing 1000-ms dwell/point-motion reset; none is an editable field or a new bypass affordance.

Native Product tests must prove **both** the visible observed data with genuine short and >128-MiB WAV REFERENCE, and the still-empty official Phase-6 release property. Source/Qt guards must assert the QML reads only the existing authenticated Product property and never runs an analyzer or fabricates UI numbers. No M16 release, Gate G PASS, physical FQ or merge is inferred from these development visuals.

## 7. PO continuous CI iteration — M21 delta sign presentation

After the PO-confirmed green for `1bdebf702ab579d3a7737e000c67314ec864e3df`, the next focused lot makes the two M21 `REFERENCE − CURRENT` delta columns **visibly signed**: strictly positive values show `+`, negative values retain `−`, and zeros or values rounded to zero remain neutral. The underlying `m21_private::Comparison` unrounded delta is never modified; this is presentation-only. Correlation/width continue to use three decimals and levels two decimals. Guarded by `test_product_navigation_guards.py`. No M16 qualification, Phase-6 Gate G or main merge implied. Continue CI iteration until the user is ready to run the complete FQ on Windows.

## 8. Pre-FQ consolidation audit — 2026-11-10

The PO's target is **few, substantial CI candidates before one complete physical FQ**, not a quota of commits. The currently green-focused M23 workflow already executes the native Windows 11-case M23 receipt suite, three real M19/M20/reference analyzer suites on both operating systems, the M22 disk-export harness, FQ99 GUI byte guard, and the M19–M21 Product source guard. Re-running or recreating these as separate tiny jobs would not add qualification.

The actual **remaining operator-dependent gates** are Windows native screenshot/geometry inspection at minimum 1184×688, physical M18 verified WAV export/readback, M23 operator fault matrix, M24 complete real-master/listening procedure, and the overall FQ verdict. A green focused CI does **not** close these.

The current candidate tightens the development M20 PSD admission to require the same nonempty resource identity and nonzero measured scope as numeric scalar cells. Its source guard explicitly protects those predicates. This is a real consistency defect fix, not a new analyzer, fabricated band profile, M16 True Peak authority or Gate G.

**Exit rule:** Before any further CI, audit outstanding actionable code gaps against existing tests and documentation; group all independent fixes into a single candidate. Do not create artificial commits or run a CI for documentation-only or cosmetic changes. When all automatable gates are complete, request the PO's physical FQ rather than endlessly issuing micro-CIs. PR #99 remains Draft and `main` untouched.

## 9. PO research separation and current L1 FQ readiness

The 2026-11-10 PO decision **PARALLEL RESEARCH AUTHORIZED / NON FROZEN DSP** is authoritative at:
https://docs.google.com/document/d/1iYhmqTtwuSdEwZ25pv8Cgp3eGs_C2rD2p3hd5nTBHM4/edit

The proposed Streaming True Peak Limiter is **research only**, isolated from the L1 writer. It does not authorize edits to M16/M17, Final SRC/FIR, frozen L3-M05–M13 contracts, any new production DSP, or the M17 terminal role. An independent Work study has no writer access. Future integration requires a distinct PO GO after mathematical proof/counterexamples, comparative testing, cost/latency audit and architectural review. Carry this reference in the next operational handoff; do not block L1-M18–M24.

The PO confirmed focused CI green at `63d105d4cdc01f0f2668597be9102720a84a04b8`. The final development-display consistency fix makes *both* signs of sub-display-precision M21 deltas neutral zero, while preserving underlying unrounded REFERENCE-minus-CURRENT mathematics. The source guard checks the display contract. This is not Phase-6 Gate G, M16 TP authority or physical FQ.

**Handoff FQ gates:** run the actual native Windows minimum-size GUI/screenshots (1184×688), M18 verified export/readback, M23 fault matrix and M24 complete master/listening 17-step procedure; record physical verdict and only then decide release/merge. Existing focused CI covers 11 Windows M23 CTests, three M19/M20/reference suites, Linux portable CTests, M22 disk harness, frozen-GUI and source guards. Do not infer physical PASS from CI. Preserve Draft PR #99, Single-Writer Epoch, and `main`.
