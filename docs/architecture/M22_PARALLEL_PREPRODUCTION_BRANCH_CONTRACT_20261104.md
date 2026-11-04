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
