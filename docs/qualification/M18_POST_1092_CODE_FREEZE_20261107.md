# M18 final Product completion — code freeze for focused debug
Date: 2026-11-07
Status: **SOURCE FREEZE ONLY; NOT QUALIFIED, NOT MERGED**

## Baseline and scope

- Baseline: focused #1092, Windows SUCCESS + Linux SUCCESS,
  SHA `8088601acba8743a93e129b3a52f1e48fa692b4f`.
- Follow-up: coherent post-#1092 no-CI staging batch. Freeze is the
  commit **containing this document**; the PR #84 candidate can be
  advanced only by a lease-checked fast-forward once no CI is active.
- Under no circumstances interpret green compilation alone as
  Product Owner physical acceptance or as M18 release qualification.
- `main` and frozen M16/M17/Final SRC DSP/FIR remain unchanged.

## All development code submitted together for debug

1. Exact M18 GUI availability derives from the same full binding,
   current SOURCE, manual chain, accepted terminal Ceiling, selected
   Project MASTER rate and verified M17 stage as the consent gate.
   No stale renderer pointer or unsupported format can authorize output.
2. Added M17 **verified-stage Product epoch watermark**. A parameter-
   only Gain/EQ/Compressor edit can preserve a structural chain
   revision, but must still invalidate M18 export eligibility until
   the NEXT accepted M17 full Master. Repeated exports after successful
   terminal ACK preserve valid verified-stage provenance.
3. Checked SOURCE first-open and same-rate replacement execute all
   real MANUAL reset-to-default mutations inside their single physical
   SOURCE lease, including synchronous M17 retirement. Project
   Open/SaveAs use their separate guarded physical batch; no mutation
   may race final export publication.
4. Canonical M18 Project OUTPUT preference saves PCM24 TPDF or IEEE
   Float32 and the EXACT full unsigned-64 PCM24 seed. Float32 never
   carries dither. Legacy and unrelated unknown output fields retain
   conservative behavior and are not silently discarded. The historical
   experimental m18Draft* keys remain unknown, not a migration.
5. User-facing WAV Export displays non-overwrite policy, cancel/ACK
   status, delivered bytes and SHA only after verified file readback.
   Windows standalone deploy smoke also checks the installed FIR set.
6. Positive real Product integration plus regressions exercise first
   SOURCE, Project reopen, DSP reset, native output-choice roundtrip,
   parameter-only stale M17 protection, delivered WAV refusal and
   repeated verified export.

## Intentionally NOT a missing M18 feature

Reusable per-module and whole-chain DSP preset files are L2-M08,
after L1-M24. Their names/extensions are not frozen. Ordinary SOURCE
replacement still follows current Product's default-reset semantics;
M18 only guarantees the reset is correctly authorized and atomic.
Do not claim preset save/load is implemented as part of this M18 batch.

## Debug gate (begin only after exact SHA promotion)

- PR #84 Draft, Windows + Linux focused CI, exactly one run at once.
- If red, inspect **bounded failing steps/artifacts only**; do not
  download full job logs. Do not add unrelated scope; repair and retest
  the first demonstrated cause.
- Require canonical Project preference and real ProjectSession tests,
  transaction/decoded/M16 delivery tests, Qt/QML app build, deployed
  Windows standalone smoke and 16 installed FIRs.
- After green, physical PO acceptance matrix:
  `docs/qualification/M18_PO_PHYSICAL_ACCEPTANCE_20261107.md`.
- Only after physical PASS may PO explicitly authorize the M18 merge.
  Full Qualification once after M18–M22 integration, as agreed.

**No CI result is asserted for any of this post-#1092 code yet.**
