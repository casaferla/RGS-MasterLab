# RGS MasterLab — L1-M22 Private Parallel Preproduction Contract
Date: 2026-11-04 | Status: PRIVATE CANDIDATE, NOT production, NOT FQ

## Frozen authority and real gates
The frozen source of truth is RGS ContentBox:
- M22 Targeted Integration Delta ACCEPTED/FROZEN v1.0 (Drive id 1LmdhDTvATlTkFTOzI0udmppG1a2LRYJ3)
- M22 Preimplementation Authority Packet Chat v0.2 (id 1-uG64ih8tVwlkwAftOWujjO4AZWH85qZuLLJKIGigr4)
- M22 I+G+L Qualification and Listening Preproduction (id 1W2HhElTzYlVXjKdqOhSdabTTuoBH-iSR).

L1-M22 FULL Manual Chain + Preview/Full SAME-RATE:
PREPARED -> exact stored-order Gain/EQ/Compressor/Stereo-M/S manual chain
-> M17 terminal True-Peak Limiter -> M18 PCM24 TPDF/roundTiesToEven/overload-fail
or binary32 without dither -> M09 same safe export writer.
Requested state is PROCESSED. Final Output SRC is identity only, processingHz == targetHz.
No Reference Match, DNA, advanced Restore, new DSP, second renderer, second writer, automatic UI wiring.

Preview(region) = canonical crop(Full, region), with exactly the same chain/terminal
configuration and absolute output-frame address for PCM24 Philox. For stateful
modules use exact compatible checkpoint OR replay from canonical state origin.
Hidden prefix or EOS/tail must not alter logical output frame count.
Chunk partitions are a performance detail, not sound identity.
No source/Gold mutation, partial publish, silent PCM24 clamp or false success.
M22 acceptance gates **I + G + L**; L is real human listening, never CI substitute.

## Product Owner hard dependency gate — NO EARLY INTEGRATION
1. M17 formally AND physically qualified by Product Owner.
2. Post-M17 CI architecture audit/reset completed.
3. M18 integrated AND qualified.
4. M19 integrated AND qualified.
5. M20 integrated AND qualified.
6. M21 integrated AND qualified.
7. Only then live canonical M22 API/renderer reconciliation, full I+G+L and explicit PO GO.
No merge to any predecessor, M21, main or automatic merge before these gates.

## Parentage, writer and touched surfaces
Immutable parent M21 PR #70 HEAD on 2026-11-04:
0a65990091f8ddc641888df114069d5e7db75fc4.
Writer: staging/m22-full-manual-chain-preproduction-20261104, NO CI.
PR head: experimental/m22-full-manual-chain-20261104.
PR base: experimental/m21-metrics-delta-20261103, PR must stay Draft.
Single writer/epoch; one complete staging macro-lot per CI, fast-forward head only on
positive private checks, no force updates to any existing ref.
Only PRIVATE render/src/internal/m22_* and tools/qualification/m22/** plus this
contract and M22-exclusive Actions workflow. Existing Qt app, renderer,
M17 limiter, M18 dither, M19/M20/M21, CMake, main, export all untouched.

## Macro-lot #1 exact scope
Metadata-only, immutable-by-interface ResolvedRenderRequest candidate, separate
preflight RenderExecutionPlan candidate, stored module order, true BYPASS_IDENTITY,
terminal order, checked cumulative latency, same-rate six allowed clocks,
strict window dimensions, absolute output-frame origin, full-source replay
policy, no partial plan on failure. Typed FROZEN intent includes Source and
Prepared revisions, chain revision, module types/algorithms/parameters/bypass,
limiter signature, output format, PCM24 ditherSeed, and execution policy.

Standalone C++20 native executable on GCC Linux and MSVC Windows both Debug and
Release, Python strict nonactivation/allow-list guard. No compiled production
DSP use, no claim of genuine M17/M18 binding or exact PCM equivalence, no
actual renderer, exporter, human audio or real I/G/L qualification yet.

## Unresolved canonical integration seams
ModuleExecutionBinding and ProcessingChain real snapshots, runtime checkpoint
source provenance, stage-aware multi-node latency and EOS, M17 authoritative
DSP/TP validation, M18 terminal format and dither, M09 safe actual writer,
PREPARED/PROCESSED ownership and actual M19-M21 analyzer delivery,
live product asynchronous job generations, UI Design Authority.
Qualified predecessor implementations may alter DTO/API shapes; reconcile
then, never create a shadow production engine.

## Private macro-lot #2 — ACTUAL M18 terminal codec, not invented DSP
C++ selftest includes the real existing `audio/src/internal/m18_terminal_sample_codec.hpp`
under M18 preproduction without changing any upstream code. Six sample rates, mono/stereo,
PCM24/Float32, Full and multiple start/middle/end Preview crops. EXACT byte identity
under chunk sizes 1/57/73/4096 using absolute output-frame address; deliberately
local-reseeded PCM24 mismatch is asserted. Real M18 preflight rejects PCM24 overload
and Float32 nonfinite input without modifying sentinel output. Double samples supplied
are deterministic *test fixtures*, NOT a real rendered M17 master. Therefore this
is a useful terminal seam proof only: no real DSP chain, physical listening, or I/G/L.

## 2026-11-04 CI launch observation
Initial Git-data ref publication of macro-lots #1/#2 did not expose an M22 run
in the GitHub Actions API at check time. A bounded sequential content-API
checkpoint on the same isolated PR branch is used to register the new
workflow's push event; staging is then fast-forwarded to the identical HEAD.
Until an actual Actions run reports completion, CI remains NOT VERIFIED.

## Private macro-lot #3 — exact state-origin / full-terminal history authority
CI #41881925854 initial result: Linux native Debug+Release PASS; Windows
request/plan and scope PASS, but M18 test step RED due MSVC /WX C4267
(implicit size_t -> uint32_t, two call sites). Fixed by explicitly using
uint32_t channel count in the M18 real terminal fixture. No DSP mathematics
was changed. The failed CI remains historical evidence, not retrospectively green.

New native adversarial *artificial history oracle* rejects both an invalid
region-local state reset and an invalid Preview-local programme terminal
verification boundary; full-source terminal dependency and actual EOS are
mandatory to maintain M17 final global M16 verification/trim equivalence.
It compares start/middle/end Preview against Full crop under multiple legal
chunk partitions. It is expressly **not real EQ/Compressor/Stereo/M17 DSP**.
Preflight plan now carries required_terminal_processing_end_frame() equal to
the source length: a Preview must reuse a matching verified complete M17
artifact or evaluate the complete programme; stop-at-region-end cannot
silently be treated as equivalent. This remains a high-risk real M17
integration seam pending full source/owner binding.

MSVC warning fixed; workflow returned to single push-trigger (PR trigger
experiment was temporary and unnecessary once the delayed initial push CI
became visible). Every new CI is evidence for the exact tested SHA ONLY.

## Private macro-lot #4 — actual canonical typed ProcessingChain bridge, compile-only
Uses *real* existing dsp::ProcessingChain / ModuleInstance / ModuleRegistry /
ModuleExecutionBinding / typed ParameterPayload / ModuleDescriptor definitions,
without converting params to rounded UI display strings or guessing their
sonic signatures. Rejects unknown or terminal inline types, wrong MASTER/MANUAL
stage/segment, provenance/linkage mismatch, descriptor/factory or binding
absence/duplication, wrong parameter variant and missing/duplicate externally
frozen parameter fingerprint evidence; preserves actual stored order, instance
UUID, user bypass and distinct enabled/controller/domain suspension flags.
Requires an EXTERNALLY supplied verified parameter fingerprint and effective
active-node latency. There is no accepted canonical parameter serializer or
runtime requirement source bound here. Compiler integration only (Windows MSVC
and Linux GCC Debug/Release syntax); NO actual production render, DSP or
true end-to-end I/G/L. This is preproduction-only.

## Lot #4 private CI target checkpoint
The typed canonical API bridge is compile-only, so its Linux/Windows strict
compiler screen requires the exact lot #4 SHA, not inherited M22 lot #3 green.
A dedicated scoped push to this Draft PR head explicitly records the current
handoff. If a CI run does not appear, the CI status remains UNVERIFIED; no
manual I+G+L or upstream integration gate is inferred.

## Private macro-lot #5 — M22 plan to REAL M18 transactional WAV exporter
Candidate binds only the immutable M22 metadata to existing M18WavSpec,
absolute output-frame offset, and m18_export_offline_candidate. No new
codec/dither/WAV writer or transaction mechanism. CI executes actual private
M18 disk publication and readback across 6 sample rates x mono/stereo x
PCM24/Float32, Full and middle-region Preview on actual WAV files. Compare
Preview payload EXACTLY with Full WAV payload crop and independent bounded
M18 output oracle; test seed=0 Float32, immutable double input, collision
no-overwrite, and no published file/private staging on bad provider or
PCM24 overload. Test input is SYNTHETIC binary64 data from an externally
supplied M18MasterFrameProvider, NOT an authenticated PREPARED/M17 PROCESSED
realization. This is an isolated M18 integration seam rehearsal; existing M18
source or canonical transaction code is UNCHANGED. Official I+G+L,
physical listening, M17 genuine full-programme verification and
M09 canonical export integration remain strictly BLOCKED.

## Private macro-lot #6 — M17 REAL typed outcome and nonconforming quarantine
A PRIVATE read-only adapter references the actual current
rgsml::render::TruePeakLimiterMeasuredOutcome variant,
TruePeakLimiterCheckedCandidate and
TruePeakLimiterMeasuredNonconformingCandidate. It categorically rejects
the measured NONCONFORMING outcome (which requires its own explicit
PO consent before any audition/export, NOT automatic M22 publication).
It checks the full OUTPUT_RATE resource origin 0, whole programme frame
count and SAME-RATE sample clock against frozen M22, expected terminal
bypass, valid supported full-resource M16 evidence when active, and
canonical inactive-bypass with NO false M16/GR. The external prerequisite
gates (M17 physical+formal, CI audit/reset, M18–M21 qualified/merged,
and verified exact resource owner) all default false. Nothing opens them
on green private CI. Supported-unqualified M16 stays correctly labeled
and is NOT promoted to QUALIFIED merely by a private test.
Separate synthetic scalar witness tests exercise rejection branches;
actual M17 C++ variant is compiled as the input type, but M17 full-DSP
render/verified owning resource and export publication are NOT executed.
No device audition, no auto-consent, no true production gate.
