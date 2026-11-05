# M18 — reconciliation checkpoint after accepted M17 and CI reset (5 November 2026)

**Status: isolated NO-CI staging only. NOT a product integration, NOT qualified, NOT merge-authorized.**

## Verified starting point
- Accepted `main` after PO-approved #83 reset: `304f593c89e7079aefa31727055b0880243245e4`.
- Frozen prior M18 independent preproduction: PR #63, commit `345890bdea2c45f80e6256793b51ce2c2523f55a`, branch `experimental/m18-dither-20261102`. Its PR base is the old M17 `995e3f976f752d148847821bd33b56ce656bfbb8` and must NOT be merged directly.
- Git compare before import: 60 source-side commits / 281 current-main-side commits, divergent from old M17; do not rewrite or force-push PR #63.
- New single-writer branch: `staging/m18-post-m17-reconciliation-no-ci-20261105`, derived from accepted `main`, without PR and without intentional CI events.

## This controlled transplant
- Reuse the **original Git blobs**, unchanged byte-for-byte, for 37 M18-only preproduction sources/tests/reference docs: 8 `audio/src/internal/m18_*.hpp` internal headers, `tools/qualification/m18` native/Python independent oracles, three historical documentation files, and the corrected QtTest `tests/unit/audio/test_m18_counter_dither_philox.cpp`.
- Reconcile the one shared file `tests/CMakeLists.txt` against **current** main, inserting only the isolated M18 Philox/TPDF QtTest target before the existing global FIR CTest environment loop. Never replace the current M17 CMake with the old snapshot.
- Do **not** transplant the three old M18 GitHub Actions workflows yet; they target the obsolete `experimental/m18-dither-20261102` branch and risk redundant/incorrect CI. They remain retrievable unchanged on PR #63, to be adapted once the product integration batch is coherent.
- No M17 product source, Qt/QML, limiter, master audition, DEVICE, Final SRC, canonical WAV writer, output JSON schema or Windows publishing behavior is changed by this transplant.
- An earlier isolated M18 CI PASS is evidence of *preproduction code* only. It does not qualify the newly reconciled CMake/product baseline, Windows FQ, formal Gate G or delivered audio.

## Mandatory application integration on THIS staging (not yet implemented)
1. **Immutable output PCM ownership.** Connect the accepted M17 final Master binary64 read-only frame owner to `M18MasterFrameProvider::read_exact`. Validate absolute-frame addressing, mono/stereo, 44.1/48/88.2/96/176.4/192 kHz and lifetime/epoch. Never read DEVICE monitoring samples or regenerate M17 independently.
2. **Output contract and persistence.** Adopt the final PCM24/Float32 selection and exact 64-bit dither seed under canonical project/output schema, with unknown-field preservation and no ambient FP dependence. No Float32 dither, no dither-off control for PCM24 v1, no noise shaping.
3. **Single authoritative writer.** Adapt the existing Windows `TransactionalAudioExporter`, `WavWriter`, error and cancellation policy; the isolated `m18_transactional_file.hpp` remains a test prototype, not a competing production publisher. Real no-overwrite, crash/ACL/path and multi-GiB RF64 tests still required.
4. **Delivered-PCM evidence.** Compute SHA/readback and independently decode the *published* PCM24/F32 file, invoke canonical M16 true-peak measurement, report precise ceiling outcomes; no undisclosed trim or post-dither SRC.
5. **Product qualification.** Expand current Windows/Linux focused regression coverage for M18; independently execute mathematical and exact-byte oracles, proposed 87-statistics evidence (adopt Gate G thresholds explicitly before claiming pass), actual Windows cold FQ, six rates/mono/stereo/PCM24/F32/RF64, physical listening and PO acceptance.

## No-CI / no-merge discipline
- Make large coherent batches on this no-PR staging, validate syntax and ownership before one PR promotion/CI. Never push micro-fixes to an active PR, never allow hidden auto-merge, never fetch complete GitHub Actions logs.
- Before future promotion compare exact HEAD SHA and main ancestry, check that `main` has not moved, obtain one Windows+Linux green candidate and separate FQ/PO acceptance where required.
- M19–M22 PRs stay Draft and blocked by the sequential integration/qualification chain. No changes to their branches in this lot.
- Neither the existing M17 FQ #97 nor the CI reset #1035 certifies M18.

## Source references
- https://github.com/casaferla/RGS-MasterLab/pull/63
- https://github.com/casaferla/RGS-MasterLab/pull/83
- https://github.com/casaferla/RGS-MasterLab/actions/runs/42095370616
- `docs/architecture/M18_INTEGRATION_SEAM_CHECKLIST_20261103.md` retained from old M18 as historical boundary analysis; this checkpoint supersedes its outdated M17 blocking status.


## Macro-lot 2 — nonpublishing M17 PCM -> M18 frame reader
- New audio/src/internal/m18_owned_master_frame_provider.hpp holds the exact immutable binary64 AudioBuffer, implements M18MasterFrameProvider::read_exact, validates six supported rates, OUTPUT_RATE domain, zero frame origin, mono/stereo, whole resource and per-request range/layout; copies sample bits without floating-point arithmetic, rejects invalid requests before writing any output.
- TerminalPrivateProductSession now has a PRIVATE capture_m18_verified_master_for_read(selectedMasterRate) method. It checks current_verified_private_master() on the QObject owner thread, exact project MASTER rate and existing M16 final check, and alias-retains the SAME M17 compensated_pcm owner. No fresh M17 limiter, SOURCE resampling, DEVICE PCM or UI/export activation.
- Registered rgsml_m18_master_owner_bridge_tests QtTest, exercising six rates x two layouts, signed zero/subnormal bit-exactness, 4096 chunk crossing, owner lifetime, invalid SOURCE/frame-origin/rate/empty inputs and no partial writes on rejection.
- Safety caveat: a retained PCM buffer does not prove current selection after the session changes. A future file commit must independently revalidate live session/epoch/format, canonical Windows transactional publishing and decoded post-delivery M16 True Peak; the M18 preproduction filesystem publisher cannot be silently activated.
- State: on isolated no-PR staging only, UNCOMPILED/UNQUALIFIED on this newer base; no main merge, no M18 WAV UI, no product schema invented, no FQ or CI triggered by this source batch.


## Macro-lot 3 — exact seed and PCM24/Float32 output candidate (nonactivated)
- Independent project-layer class M18OutputPreferenceCandidateCodec in project/include/rgsml/project/m18_output_preference_candidate.hpp and project/src/m18_output_preference_candidate.cpp. It uses the already-accepted OpaqueJsonValue and nlohmann integer JSON parsing, with no Qt/audio/render/platform dependency.
- A candidate PCM24 selection requires explicit uint64 dither_seed, including UINT64_MAX and values above 2^53. A candidate Float32 selection MUST omit seed; changing PCM24 to Float32 drops the former candidate seed. No dither-off or noise-shaping option. No implied default for legacy project output.
- Candidate-only JSON field labels are m18DraftTargetEncoding and m18DraftDitherSeed. THESE ARE NOT APPROVED PERSISTENCE KEYS. The codec is not called by ProjectSession, project serializer autosave, M17, M18 production exporter, UI or public render; keys must be reconciled with final project-output schema authority before production. This deliberately avoids treating a draft key as an accepted .rgsmaster contract. Existing MasterOutputRatePreference::only_known_fields will recognize those keys as unknown, a deliberate fail-closed guard until an explicit schema upgrade.
- Operation preserves unrelated output keys and nested future objects, including targetMasterOutputRateHz. Candidate reads reject partial, malformed, negative, decimal, string, bool or excess-uint64 seeds. No coercion of seed through binary64, to avoid the 2^53 precision trap. A Float32 value carrying any seed is invalid.
- New QtTest rgsml_m18_output_preference_candidate_tests covers legacy output, full uint64 boundary and above-2^53 roundtrips, old master clock preservation, unknown field preservation, Float32 seed exclusion, changes between formats and adversarial invalid JSON/candidates.
- No product save/export path was modified or enabled. This is **preproduction** on a no-PR/no-CI staging branch and has not been Windows/Linux compiled or qualified on the accepted M17 baseline. Pending mandatory approval of final field naming + actual .rgsmaster roundtrip/writer integration.
