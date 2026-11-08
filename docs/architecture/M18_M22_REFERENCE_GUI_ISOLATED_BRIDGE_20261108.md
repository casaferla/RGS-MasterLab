# M18–M22 + public REFERENCE GUI — isolated pre-integration bridge

Date: 2026-11-08. State: DRAFT / TECHNICAL PRE-INTEGRATION / NO MAIN MERGE.

## Immutable inputs
- M22 candidate PR #89, SHA `5a5b9b7e4b01779832e32662d5c6b66b23087ce9`: Focused CI #12 Windows/Linux SUCCESS. This history already contains sequential M18→M19→M20→M21→M22 code.
- Public REFERENCE and standalone M19–M21 Qt GUI PR #95, SHA `b7a30ad2fb22a9d718a3e75adbf8d337bbccf6dd`: Focused CI #6 Windows/Linux SUCCESS.

## Import protocol
Single writer: `staging/m18-m22-reference-ui-convergence-no-ci-20261108`. Candidate for CI only: `staging/m18-m22-reference-ui-bridge-candidate-20261108`. Imported 32 immutable Git blobs exactly from PR #95, with NO merge/cherry-pick of divergent branch histories. Root `tests/CMakeLists.txt` is the only overlapping modified path: compose the 34-line standalone M19–M21 test registration into frozen M22 CMake, leaving all M22 Product targets unchanged. No old branch-specific REFERENCE workflow is imported because its fixed base guard is inapplicable here; bridge guard independently verifies source blob provenance and M22 invariants.

## Boundaries
- Strictly non-Product M19–M21 QML preview; fixture/demo map, `productionAuthorized=false`, no Main.qml analyzer registration and no fabricated spectrum/true-peak/owner status.
- The public user interface says REFERENCE. Technical `GOLD` tokens, project schema, API, and gold-color names remain frozen.
- Do NOT claim canonical Product owner/Phase-6 Gate, physical Windows acceptance, M18 A01–A15, final FQ, or M18–M22 closure merely because focused CI passes.
- M17 GR HISTORY redesign remains separate PR #85 and **follows** M18–M22 in PO sequence.
- No merges to main or predecessor branches without explicit PO approval. No automatic CMake alteration outside isolated typed Qt target registration.

## Gate
Run the isolated provenance checker, real C++ M19/M20/M21 and M22 whole-Master parity tests on the appropriate OS, canonical real Qt module REFERENCE smoke and standalone 24-shot Qt UI harness. Preserve bounded failure artifacts; keep candidate frozen while CI runs. Subsequent work after this gate: connect genuine M19/M20/M21 typed Product signals to independent QML wrappers under canonical owner/provenance authorization, then resolve remaining M22 scheduler/Phase-6/real M18 export gates and PO physical acceptance.

## Follow-up macro-lot — real M18 Product readiness to independent Qt panels (after CI #1 PASS)

Source candidate PR #96 @ `064a8dd6c433c8313e7e75c6175c839a3bbdf953`: Focused Bridge CI #1 [Windows/Linux SUCCESS](https://github.com/casaferla/RGS-MasterLab/actions/runs/42482702607). The PO requested to continue M18–M22 completion before separate M17 GR HISTORY.

New private header `app/src/internal/m21_product_qt_readiness_bridge.hpp` consumes the **real**, Qt-owner-validated `M21ProductPublicationInbox::view_current` and M18 Complete Binding `Fanout::OwnerSnapshot`. Three independent M19/M20/M21 routes select PREPARED/REFERENCE/PROCESSED without mutating or merging the underlying evidence; source role REFERENCE is a user-facing label for the persisted internal GOLD column. The identity fingerprint contains the M18 epoch, route revision, independently selected column, original result identity, producer generation, scope, frame domain and lifecycle. A delayed frame is rejected if any changes. A worker call returns static empty maps before touching routing or Product state.

**Important: this is not Product GUI activation.** All three Qt maps are always `ready=false`, `fixtureMode=false`, `productionAuthorized=false`, with no LUFS, TP, PSD, tonal bands or metrics numbers. Their additional `privateEvidenceStatus` reflects the actual private inbox as CURRENT/STALE/MISSING but cannot confer canonical Phase-6 authorization. The original private typed snapshots remain owned and revalidated by M21 Product, not QML.

The real Windows `rgsml_project_session_tests` now validates:
- Actual verified M17/M18 ProductSession PREPARED/PROCESSED analysis → distinct private panel roles/current owner with M16 TP withheld.
- Independent true WAV REFERENCE selection → both selected REFERENCE panels CURRENT while absent PREPARED/PROCESSED are MISSING; clearGold revokes both.
- A per-panel source routing revision makes a previous queued Qt frame stale without changing audio data.
- Parameter-only Gain change revokes both M17 verified Master and old panel epoch; no state inherited from the merely unchanged structural chain revision.
- Worker thread is denied access before any routing/Qt owner inspection and receives only empty maps.
- Earlier M22 whole-Master Preview/Full bitwise parity and M18 WAV export tests are **preserved without modification**, not replaced by these new assertions.

The bridge provenance checker now permits only additive tests in `test_project_session.cpp`, never deleting/changing original M22 test lines; the source M22 Product baseline remains frozen. Subsequent work MUST obtain real Phase-6 status authority, implement a release-qualified Product presenter, and then connect it to the independent docked/floating Qt wrappers with physical PO verification. Do not reclassify the private bridge as production.

## PO build cadence 2026-11-08 — macro-lots of up to ~40 meaningful commits

The PO explicitly wants larger, coherent single-writer macro-lots (typically 20–40 meaningful commits, not a mandatory quota) to reduce CI overhead. Use one Windows+Linux focused CI at the end of a validated macro-lot and freeze its CI candidate until terminal results; small corrections after a red CI are exceptions. Never edit the active CI candidate during a running check. No auto-merge to any predecessor/main and no FQ before full Product integration. The 7-commit previous bridge lot was qualified by focused CI #3, exact SHA `ba376284fe97d6d2bf4634fc7e11e726c76170b8`, Windows and Linux SUCCESS; that SHA remains the stable PR #96 CI candidate.

### Current next-lot staging start (not yet CI-qualified)

- Real M21 Product private Qt source quality tags: per-panel M19 levels (LUFS/RMS/sample peak/crest), M20 stereo (width/correlation P10/P50/P90) and M21 metrics evidence. Only string quality status from canonical retained M21 inbox; no scalar values, no spectral bins and no normalized shape/spectrum synthesis. Never present an M16 True Peak number; quality explicitly M16 UNQUALIFIED.
- Quality labels provided only for `Lifecycle::current`; independent REFERENCE and PREPARED/PROCESSED owners cannot substitute one another. Missing/stale results return completely masked quality maps.
- Existing M18 ProductSession native tests extended with actual current prepared/processed and independent Gold selected WAV quality, per-panel separation, stale masking, and no numeric readouts. Test checks nonnumerical strings at value level.
- Frozen provenance checker now requires actual quality-only guard and explicit M16 disqualification. **Do not promote/dispatch** until the larger coherent Product/UI lot has been completed and audited.

The next major gate is **genuine canonical Phase-6 release authorization and Product UI signal wiring**, not another private fixture. The work above improves private readiness without claiming that unresolved authorization is already implemented.

## Traceable closure ledger (current audited status; not FQ)

| Milestone | Confirmed implementation | Blocking release evidence |
| --- | --- | --- |
| M18 | Native guarded Master export; PCM24/Float32, persisted preference, Windows ProductSession focused tests | PO physical A01–A15, full file/encoder/output-rate grid, final integrated FQ |
| M19 | Public native Level Analyzer and actual PCM→M21 Product private inbox | Canonical Phase-6 FeatureStatus authority and physical Product UI measurement acceptance |
| M20 | Public native tonal/stereo PCM analyzer and authentic independent spectral geometry | Canonical PSD/tonal band Product release, correct unavailable/mono support, physical visual sign-off |
| M21 | Real PREPARED/PROCESSED and independent selected REFERENCE provenance; private Qt readiness and quality-only metadata | Authenticated release publisher/Phase-6 registry, three live independent Qt panel bindings, visual gate; never use TEST fixtures |
| M22 | Real M17 whole-Master Preview/Full absolute-frame/zero-copy bit-parity + M18 protected full output | Complete canonical executable RenderExecutionPlan, six-rate SRC/latency/history, no hidden second limiter, end-to-end output verification and final FQ |

**Candidate CI #3** Windows and Linux green at `ba376284fe97d6d2bf4634fc7e11e726c76170b8`. Single-writer successor work is unqualified until the next focused gate. The prior M21 product private inbox is **not** a canonical Phase-6 release registry. Neither a copied owner identity nor a quality tag grants publication: the private `M21ProductQtReadinessBridge` always returns `ready=false`/`productionAuthorized=false`.

### Follow-on sublot: provenance and numerical verification

- A queued Qt frame must preserve the **complete** native M18 `M18ProductExportIdentity`, not merely an epoch. This covers source generation, project generation, master generation, chain revision, master Fs and rate revision. All are cross-checked again on the Qt owner thread on publication attempt.
- Native ProductSession tests deliberately tamper source/project/master-rate revisions, independent M21 measurement generation, resource realization ID and absolute scope to prove every mismatch refuses `still_current`. None creates a fake source, result or export permission.
- The combined bridge workflow now re-runs the previous ten independent M19/M20 numerical reference checks per platform **before** configuring the heavy Qt compiler; these cover P01/Six Rate K, F07, scope/tonal profile/stereo without fabricating downstream data. Guarding M22's immutable Product base and the existing true Preview/Full alias tests remains mandatory.
- All modifications belong to the same no-CI single writer. No candidate advancement while building the macro-lot.

## PO macro-lot: authenticated private M20 PSD + M22 Preview admission (2026-11-08)

The sole Chat Operativa writer has confirmed the apparent interleaved commits were issued earlier by the same interrupted conversation. Single-writer ownership is resumed. This lot accumulates changes only in `staging/m18-m22-reference-ui-convergence-no-ci-20261108` and retains the separate PR #96 CI candidate on the last known dual-OS green SHA until promotion. One combined focused CI at the end of the lot, not one CI per commit.

### M20 genuine spectrum

`app/src/internal/m21_product_spectrum_evidence.hpp` is a **private authenticated proof**, not a released QML chart. It reads M21 Product's independently accepted M20 PSD from the actual current Product Source/Gold/Processed column and exact route. It rejects a different M18 owner, selected source identity/generation, incompatible scope/rate, invalid bins, synthetic flat spectrum, nonmonotonic frequencies and all non-current evidence. A second epoch-and-route check after reading the bins prevents queued reuse. The numbers never leave the private C++ proof and cannot make `productionAuthorized=true`. Native real ProductSession tests compare every genuine M20 bin against the unrounded M20 worker result and prove Source/REFERENCE ownership and clearGold/routing invalidation.

### M22 genuine verified full-Master Preview

`app/src/internal/m22_product_preview_admission.hpp` obtains a true full-M17 binary64 `AudioBufferView` subset from the existing M18 complete Product authority, with exact Output-Rate timebase, absolute window and shared ownership of the verified full M17 stage. It checks current M18 epoch and all original Product identity; **off-Qt-thread calls are rejected before any mutable Product read**. The retained view is not a permission to play/export, and a queued use must revalidate the checked full-M17 stage, original frame range, channels and the exact channel pointer aliases (no local DSP rerun/copy). The existing real ProductSession test now checks five boundary windows, actual channel memory identity, illegal range, wrong-thread refusal, and revocation on a parameter-only Gain edit.

### Static and authority gates

The provenance checker explicitly whitelists precisely these new private files and requires their genuine owner, thread, generation and Source/REFERENCE test checks. No M19/M20/M21 backend DSP or canonical persistence, M16 TP, M17 limiter, FIR, exporter or QML Main registration has been rewritten. The technical GOLD source identity remains stable even when user-visible REFERENCE text is used. Historical Phase-6 authority documents on Drive are a semantic source, **not permission to invent or declare an already materialized canonical FeatureDefinitionRegistry**. No new registry, parallel status authority, fake release ticket or alternate analysis algorithm is introduced by this lot.

Status remains **PREINTEGRATION ONLY**, pending the focused dual-OS CI, completion of canonical Product/Phase-6 release bindings and M22 end-to-end scheduler, M18 A01–A15/Windows physical PO tests and a single final integrated Full Qualification. No main merge without PO GO. M17 GR HISTORY stays later on PR #85.
