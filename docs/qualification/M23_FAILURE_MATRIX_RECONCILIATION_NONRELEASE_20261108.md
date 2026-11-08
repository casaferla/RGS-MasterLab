# RGS MasterLab — M23 Failure Matrix Reconcilation (nonrelease)

Date: 2026-11-08
Source baseline: `31b81571eef1497c0d561105cc9e619ea95aafe6` — M22 CI #17 dual green
Authoritative L1-M23 packet: `RGS_MasterLab_L1-M23_Preimplementation_Authority_Packet_Chat_v0.2` (Drive ID `1PyprKrKWJGjUohPkAcsc2Zw0iEoqJrATZfdGaKdHU_M`).
**State: reconciliation/qualification tooling only. M23 I+P+M NOT PASSED. No M24 or FQ entitlement.**

## 1. The actual reuse, not duplicate code

M23 already has real predecessor tests:

- `rgsml_transactional_audio_export_tests` (Windows): actual new-file transaction, Source collision, hardlink alias, pre-existing unrelated destination, eight fault injection stages including write, postwrite validation, encoded/decoded checksum, commit, destination readback and cleanup.
- `rgsml_m18_windows_candidate_delivery_tests` (Windows): actual decoded WAV integrity, post-decode M16/True Peak, a six-rate/mono-stereo/two-encoding grid, Physical Product worker rollback and ticket revocation races.
- `rgsml_project_session_tests` (Windows): real Source/Gold protection, malformed open without replacing the live session, chain and project persistence, current M22 full/Preview Product provenance.
- `rgsml_project_package_tests` (portable): malformed package metadata, raw-header/CRC/digest/canonical integrity and limits.
- `rgsml_render_preview_tests` (portable): invalid window, missing module, stateful DSP Preview/Full and complete timeline fidelity.
- `rgsml_project_json_tests` (portable, extra cross-check): strict schema, known/opaque content handling, numeric identity.

These are **existing code/test fixtures**. They are not granted executed/PASS status until the new M23 workflow really builds and runs them for each OS and a corresponding CTest JUnit receipt is verified.

## 2. Twenty-category reconciliation and qualification rules

`tools/qualification/m23/failure_matrix_reconciliation.json` maintains 20 frozen categories:

- **12 covered at source** by a concrete existing named Qt test slot; a focused CI must actually execute the named CTest suite before recording `EXECUTED_PASS`.
- **5 partially evidenced**: real GOLD direct/hard-link alias no-replace and nonrelease diagnostic collection are now test-backed, alongside precise short-write vs generic writer-I/O failure; independently malformed/truncated WAV vs generic candidate-validation fault; unknown sound-changing module malicious-open case vs legacy opaque/degraded behavior.
- **3 explicit gaps**: nonfinite DSP injection through the real full Product, flush/close error after writing, and candidate reopen failure distinct from validation. GOLD identity-aware Product admission and diagnostic lifecycle are still only PARTIALLY evidenced, not qualified.

A static source match is not proof of fault injection: category `qualified=false` in every M23 reconciliation receipt, even when its predecessor CTest suite passes. Missing categories **remain visible and prevent M23 closure**. They must be implemented/tested or explicitly resolved under frozen Product Owner M23 authority. Never reclassify an indirect generic fault as the missing precise failure proof.

## 3. Single writer, one focused CI, bounded reports

Branch writer: `staging/m23-failure-matrix-writer-no-ci-20261108`.
CI candidate: `staging/m23-failure-matrix-focused-ci-20261108`.

The new `.github/workflows/m23-failure-matrix-focused.yml` builds only the genuine already-existing test targets appropriate to each OS, executes those real Qt/CTest suites, emits CTest JUnit and uses the fail-closed independent `audit_failure_matrix.py` to write a JSON receipt. Its own tamper/self-tests prove omitted/reordered categories, fake PASS flags, empty/failed/skipped/conflicting JUnit and missing test code cannot become positive evidence. Only bounded failure excerpts and final receipt are uploaded; no full GitHub job logs.

The receipt always keeps `m23_closed=false`, `physical_gate_m=false`, `full_fq_passed=false`, `release_qualified=false`. It never changes the actual M09/M10/M17/M18/M22 implementation, the 16 original FIRs, persisted GOLD identifiers, visible REFERENCE labels or the original FQ workflow.

## 4. Work still actually needed

Remaining M23 engineering belongs to a further **coherent large macro-lot**, not a sequence of gratuitous one-assertion CIs: integrate genuine missing failure-injection seams with the existing writer/worker, add exact Gold alias protection and decoded readback where missing, implement an immutable/privacy-safe `AlphaDiagnosticReport` over real ErrorCode/Status and evidence, and exercise manual Windows Gate M.

The official FQ is still manual-dispatch-only and must target a **single integrated** M18–M23 candidate after real Windows M23 I+P+M and the prerequisites. Phase-6 P01–P06 semantic closure **does not itself authorize implementation or Gate G**; it is a distinct PO checkpoint. User listening and first real remaster M24 cannot be self-certified.

Do not merge `main` without explicit Product Owner authorization. No concurrent CI candidates.


## 5. After M23 CI #2 dual green — AlphaDiagnosticReport and GOLD alias proofs

[M23 focused #2](https://github.com/casaferla/RGS-MasterLab/actions/runs/42528282396) on SHA `18f270bd96d7f595506be984626c1fa9291c6c3f` COMPLETED SUCCESS Windows and Linux. This confirms that the already existing predecessor suite executions and no-release reconciliation tool pass; it **does not** pass M23 I+P+M.

The next writer-only targeted evidence set adds a **real C++ `AlphaDiagnosticReport`** inside `app/src/internal/m23_alpha_diagnostic_report.hpp`, aggregating genuine `core::Status` / `core::ErrorCode` without a second error taxonomy. It discards original Error.message(), details, raw resource path, credentials and raw pointer identities, stores only canonical `error_code_token` and an explicit observation of SOURCE, GOLD, existing destination, project state, publication and candidate cleanup. Unproven evidence remains UNKNOWN; even fully observed safe failure **never** gives M23 I/P/M or RELEASE authorization. Cross-platform native C++ tests feed real core error statuses with secret-bearing messages and assert sanitized report, status rejection and fail-closed/safety semantics.

The existing **Windows M09 FAIL_IF_EXISTS exporter** is exercised against an independent real `GOLD.wav` file and its real Win32 hard-link alias (Source is separately opened). Both must be refused by the original transaction, with Gold, Source and any unrelated destination bytes unchanged and no task-owned temporary files. The resulting two **actual** exporter errors feed the new diagnostic. This proves existing no-replace on Gold, but does not pretend that the Product has gained a new Gold-specific strong identity preflight. Category stays `partial`, not `existing`.

The 20-category matrix therefore now has **12 existing-source, 5 partial-source, 3 missing-source** categories. The same M23 dual-OS CI builds/executes the new diagnostic target and real Windows GOLD alias integration target, binds genuine CTest JUnit receipts and **always** marks `m23_closed=false`. No algorithm, FIR, source output, M09 writer, original M18 encoder, M17, M22 engine, GOLD persisted token, Main or QML changed. Remaining three gaps and partial scopes must be resolved with the frozen M23 authority before actual Gate I+P; physical Windows Gate M is separately required.
