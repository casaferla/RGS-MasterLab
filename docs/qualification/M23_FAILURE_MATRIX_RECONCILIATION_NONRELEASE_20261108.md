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

## 6. Chat #78 — M23 #4 authentic CTest failures and exact repair

[CI #4](https://github.com/casaferla/RGS-MasterLab/actions/runs/42531621259)
at `26db14127b1242c03373b0e48eea091ca00b0ab3` **FAILED on both OS after
actual C++ compile and CTest execution**, not at YAML launch. Bounded receipts:
Windows 5/7 CTest suites passed; new AlphaDiagnosticReport and the physical
transactional exporter suite failed. Linux 3/4 passed; only Alpha failed.
Linux's four failed Alpha Qt assertions all follow from one reversed check in
`from_failed_statuses`: a true **failed** `core::Status` has
`has_value()==false`, so `!*observed.status` mistakenly rejected the
very statuses it was designed to aggregate. Surgical writer commit
`9ddaacef85ee56d5e0a66d501dd9e8c7d2cf8c04` checks
`observed.status->has_value()` instead, preserving rejection of null or
successful statuses and never touching Error.message/details. Promoted by
non-force expected-head CAS to the M23 CI candidate, triggering another push
check; its CI run number/result must be independently verified.

## 7. Chat #78 — new additional staging proofs, unqualified pending CI

Only on writer `staging/m23-failure-matrix-writer-no-ci-20261108`,
after the above surgical correction, added the following **test-scope** probes.
No production contract was changed: the test seam defaults to NONE, with
existing candidate write/close/validation/commit sequence unchanged otherwise.

- `FLUSH_CLOSE` test-only failure imposed at the **real candidate close**
  return boundary after a complete native WAV write/finalize. The handle is
  actually closed; injected failed status requires owned-candidate cleanup.
  This tests safe response to a close failure; it does not assert the host
  operating system suffered an actual disk/flush fault.
- `CANDIDATE_REOPEN` distinct injected error immediately before
  `validate_candidate`, after real finalized candidate close. Both fault
  tests prove no destination, no task-owned temporary file, source byte
  identity, and privacy-safe report assembled from the **actual** failed
  exporter status.
- `CANDIDATE_TRUNCATED_WAV` physically resizes a successfully written,
  closed, task-owned candidate to eight bytes; existing candidate reader and
  validation genuinely reopen the damaged bytes and must reject them without
  publishing or touching SOURCE.
- Real MANUAL Gain module rejects **NaN / +Inf / -Inf** in supplied samples
  during genuine render execution with `InvalidAudioSample`; no successful
  Render Preview is returned and SOURCE bits are preserved. **This is not
  equivalent to Product FULL M17/M22 nonfinite failure injection.**

Source inventory at this staging revision becomes **15 existing / 5 partial
/ 0 categories without any source test**. Five partial scopes remain:
`nonfinite_dsp_input` (needs Product FULL),
`short_write` (precise partial-byte injection not proved),
`gold_destination_collision` (Product Gold-specific admission),
`unknown_semantic_module` (malicious semantic open),
`alpha_diagnostic_report` (full Product lifecycle wiring).
The `test_audit_failure_matrix.py` count assertions have been updated. The
new tests MUST compile/run on Windows/Linux and bind genuine CTest JUnit
before any qualified PASS; source code or static audit alone proves nothing.
M23 I+P+M, Gate M physical, M24, Q1-Q6, Phase-6 Gate G and official cold FQ
remain separate **OPEN** gates. PR #99 remains Draft, main unchanged;
the candidate may be behind this writer while the previous CI completes.

## 8. After verified M23 #7 — reuse first, Product-specific coverage on writer only

The preceding [M23 #7](https://github.com/casaferla/RGS-MasterLab/actions/runs/42534362615)
`5639bd7ea9d78e94a810c4e968aaa536c95e512e` is **completed
SUCCESS Windows + Linux**. That qualified the prior 15/5 source matrix and
native M23 diagnostic/close/reopen/candidate corruption checks, not this new
unqualified writer-only batch.

M23 rechecked original M09 and M10 tests before implementing new code:

- `tests/unit/audio/test_wav_writer.cpp` already proves actual
  `IResourceWriter` short byte transfers are completed losslessly and
  zero-progress writes fail with canonical `IoFailure`. Its original native
  CTest target has been added to the M23 workflow on **both** Windows and
  Linux; the existing M09 exporter injected WRITER_IO still covers
  transaction cleanup.
- `ProjectSessionTest::atomic_rejection` already creates a real .rgsml
  containing an unknown **active** sound-changing module, then proves the
  current live Source/mastering chain remains unchanged after failed open.
  `future_opaque_preservation_and_non_degraded_b4` independently proves
  intentionally future opaque *non-sound* content is preserved. These
  existing tests are reused, not copied.
- Real four-active-DSP M22 Product test now binds an independent physically
  selected REFERENCE/GOLD WAV, and after an original-Q0 48k->44.1k M17
  checked master it attempts explicitly consented FULL M18 output both
  **directly to GOLD** and to a **Win32 CreateHardLinkW alias**. The
  original FAILED terminal worker error must be `InvalidState`; no
  verified-destination receipt is permitted; the actual SOURCE and
  REFERENCE/GOLD file bytes and alias bytes remain unchanged. The worker
  code is projected to a no-release `AlphaDiagnosticReport`, with all
  unproven safety flags deliberately UNKNOWN. Both attempts are ACKed
  before the existing independent successful F32 and PCM24 exports.
- The same real four-DSP upstream renderer now injects NaN, +Inf and -Inf
  before any PROCESSED stage publication. It must fail
  `InvalidAudioSample`; the source buffer is then restored for the
  original successful FULL path. No M17 authorization is falsely minted
  from a failed manual render.

The 20-category **source-code inventory** on this new candidate is now
**19 existing / 1 partial / 0 uncovered**. The only remaining partial is
`alpha_diagnostic_report` **full** Product/Project persistence and
build/baseline diagnostic-lifecycle integration. It cannot be converted
to a fully qualified gate merely because the worker code reaches the
nonrelease diagnostic reporter. For Product diagnostics, especially
physical cleanup outcomes, unobserved facts remain UNKNOWN.

These changes MUST pass new actual Windows+Linux CTest before source
qualifications can be accepted. Even with a green CI, M23 Gate I/P/M,
physical Windows PO Gate M, official cold Windows FQ, M22/Q1–Q6 formal
acceptance and Phase-6 Gate G remain independently controlled. No automatic
merge to main, no release approval, no hidden DSP/SRC/encoder replacement.
