# M20 Basic Tonal + Stereo Analyzer — PARALLEL PREPRODUCTION CONTRACT
Date: 2026-11-03. Status: DRAFT PRIVATE CANDIDATE, not integrated, not qualified.

## Unbreakable dependency sequence
1. M17 PR #61 must be formally and physically qualified by Product Owner (the M17 FQ CI alone is not acceptance).
2. Perform the already required post-M17 CI architecture/reset audit.
3. M18 PR #63 must be integrated and formally qualified.
4. M19 PR #64 must be integrated and formally qualified.
5. Only then reconcile M20 and complete its own canonical U+G gates, physical Windows application binding, review and explicit PO integration authorization.

**NO M20 MERGE OR INTEGRATION PRIOR TO ALL FOUR UPSTREAM GATES, NO MERGE TO main WITHOUT PO AUTHORISATION.**

## Single-writer and ancestry
Fork parent M19 Draft PR #64 HEAD frozen at 0c92b14c2808f888ba2d2a48b4cca470ba8c4f8b (2026-11-03).
M20 no-CI single-writer: staging/m20-tonal-stereo-preproduction-20261103.
M20 Draft PR head: experimental/m20-tonal-stereo-20261103, base: experimental/m19-basic-level-20261103, never main.
No mutation of parent M19/M18/M17 branches, all 16 immutable FIR source binaries, export, DAC, DSP, Qt/QML application, or canonical repository schemas. Stage coherent lots; CAS lease fast-forward PR once per lot; independent dual-OS Windows/Linux Debug+Release CI.

## Product authority and non-scope
Accepted/frozen 2026-09-12 M20 Targeted Materialization Delta, RGS ContentBox file 1Hzzn6IIQHImJvK6Ac_072pD3svA5L0j2.
Phase-6 2026-10-05 P01-P06 closure file 1rBiwPOz5lMGDMGuKyFNVzw9d9SIBhS3Znx3aWVr-eE4 binds P02 = MEAN_CHANNEL_POWER and P06 = LOUDNESS_GATED_ACTIVE_UNION for broadband stereo.
Independent M20 mathematical pre-audit file 1o5JMQcmlplFfdLgyAzgfook6CxJjeqVCJKmurpIkS1U is PASS_WITH_REQUIRED_CORRECTIONS, NOT canonical Gate G.
Truthful visualization pre-audit file 1PTvYQYtvOqx2Wd93Y3SRkmTu1Z1ulYLLV21yxLCODmw is also PASS_WITH_REQUIRED_CORRECTIONS and its proposed static view plus independent live B3 bay is NOT a frozen PO-approved UI composition.

Tonal: existing FFT/Hann/PSD canonical basis, 4096/48000 second window, ties-even sample geometry, periodic Hann, one-sided PSD, linear-power averaging; LOUDNESS_GATED_ACTIVE when authoritative retained intervals are available, otherwise ALL_FRAMES with explicit reason. Gate available but no retained activity is insufficient content, NOT fallback. Frequency support [20,min(20000,0.45*Fs)] Hz; canonical 1/12-octave feature with 1/3 substitute, partial-bin PSD overlap, no fabricated zero for unsupported bands. P02 stereo channel mean POWER (not downmixed samples).
Stereo: orthonormal M/S broadband width sqrt(ES/EM) over P06 retained interval union with no duplicate samples, gate unavailable => ALL_SCOPE with explicit reason, available empty => insufficient content; no silent floor/limiter; width may exceed one. Local correlation uses independent 400 ms/100 ms direct rational Grid100, local-mean removal, both AC powers >=1e-10, denominator floor 1e-20, only valid full windows; P10/P50/P90 are Type 7. Mono stereo metrics not applicable.
Only whole-scope static tonal spectrum/profile, width and correlation summary are Alpha targets; no spectral centroid, rolloff, tonal tilt, sideFraction, stereo imaging or multiband width, L/R balance, Reference Match, DSP repair, histogram or QML math. BasicTonalStereoArtifact is an aggregate view, not a new persisted analyzer schema.

## First preproduction macro-lot boundaries
Private C++20 numerical/geometry candidates in analysis/src/internal/m20_*.hpp, no production source or public feature API. Independent Fraction/Decimal Python oracles plus native portable stereo/tonal and REAL existing FFT/Hann/PSD linking test, both OS Debug and Release. Not full scope streaming, not complete gate-mask ownership, not official Phase-6 numerical QTP, not canonical FeatureStatus mapping or integrated UI.
Open blockers: live Phase-6 FeatureDefinitionRegistry/AudioBuffer AnalysisScope, authenticated M19 Integrated-Loudness retained-block availability and explicit support/reason identity, threshold for validBandEnergy, exact gate center/endpoint encoding and scope origin, FFT identity/QTP, canonical DTO, Windows UI binding, genuine independent formal U+G and PO acceptance. Do not invent these contracts.
