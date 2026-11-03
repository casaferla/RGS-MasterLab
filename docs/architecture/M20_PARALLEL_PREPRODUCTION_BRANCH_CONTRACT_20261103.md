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


## Macro-lot #2 — real canonical FFT/PSD whole-scope reconstruction (still isolated)
New PRIVATE m20_scope_spectrum_candidate.hpp actually links the existing Radix2Fft, HannWindow and compute_onesided_psd for all six supported Fs, derives each complete frame at logical AnalysisScope start, computes mean-channel POWER P02 before linear-power frame aggregation, and produces the full static average PSD plus measured 1/12-band overlap energies. The caller MUST supply the true canonical retained-gate bitmask (one bit per complete frame) when gate exists. No M19 retained-block synthesis, unapproved half-open center encoding or fallback from an empty **available** gate; missing gate permits ALL_FRAMES with explicit candidate fallback. The band-energy result is NOT yet a fully qualified normalized profile because canonical LOW_SUPPORT eligibility/status is unresolved. No public feature/Qt implementation.
Additional independent native test executes the true existing FFT/Hann/PSD for all six clocks with nonzero logical scope origin; exact reconstructed-scope equality for 1,7,31,127,4096 frame feeder chunks, stereo dual-mono vs mono P02, externally authoritative partial/empty masks, insufficient complete STFT windows and wrong mask rejection. This validates numerical *logical-scope reconstruction*, NOT a bounded-memory live streamer or final Gate G. Linux and Windows perform Debug+Release strict-fp runs in the same isolated M20 CI lane.


## Macro-lot #3 — private normalized 1/12–1/3 tonal profiles and common-bandwidth support
Private `m20_normalized_profile_candidate.hpp` adds single-profile / explicitly-supplied comparison-bandwidth normalization over actual M20 scope-average Phase-6 PSD, for six authorized Fs, using frozen partial-bin cell overlap and only **externally supplied** band eligibility (0=missing, 1=valid, 2=low support are *private wire codes*, NOT canonical FeatureStatus/LOW_SUPPORT definitions). Zero real power is distinct from unavailable frequency; entirely missing/zero power emits an explicit diagnostic with null fractions. Missing/LOW_SUPPORT never receives a fabricated zero. When a common comparison range is explicitly passed, only whole bands fully contained in its exact intersection may contribute; the denominator is **recomputed from measured linear energies** of those bands, never obtained by trimming existing fractions. The 1/3 resolution is an explicitly selected substitute, not a second simultaneous target. No frequency support auto-extension, hidden display floor, new FFT, user-facing QML, persistence, Reference Match or claim of Phase-6 canonical support/status readiness.
Additional portable C++20 real FFT/Hann/PSD tests cover six Fs, nonzero logical scope, both resolutions, external missing and low-support, full common support, 44.1k upper 19845 Hz, gain² invariance, exact normalized sum, silence, invalid/mismatched eligibility, NaN/Infinity and invalid restrictions; independent Python Fraction/Decimal oracle covers half-bin cell integrals, rational conservation, eligible-only normalization and linear-power dB discriminator. M20-only Windows MSVC and Linux GCC Debug+Release are required for CI green. This does NOT replace genuine Gate U/G, canonical QTP/status and LOW_SUPPORT authority mapping.


## Macro-lot #4 — real read-only AudioBufferView scope adapter, no public registration
PRIVATE `m20_audio_scope_adapter_candidate.hpp` now invokes the real existing `AudioBufferView::subview(absoluteBegin,frameCount)` after exact SOURCE/OUTPUT `FrameDomainId` check, selecting only the actual signed FrameIndex range and mono/stereo binary64 planes. Out-of-scope poison PCM must never be touched; in-scope nonfinite is categorical InvalidAudioSample, wrong frame domain is InvalidFrameRange, six-allowed-rate mismatch UnsupportedOperation, inconsistent external spectral frame mask InvalidState. No hidden format conversion or sample write. It returns a PRIVATE scope wrapper with existing scope-spectrum candidate, P06 width and independent local correlation, without renaming or creating any canonical AnalysisScope/FeatureDefinitionRegistry.
A native C++20 fixture actually constructs AudioBuffer via the unchanged production `audio/src/audio_buffer.cpp` and tests at 44.1/48/88.2/96/176.4/192k, both mono and stereo, nonzero absolute origin, explicit selected scope inside larger owning resource, NaN poison directly before and after scope, gated/empty/mismatched spectral mask, missing/empty width gate support, mismatched SOURCE-vs-OUTPUT domain, invalid selection, in-scope poison rejection and preservation of original resource bytes. Windows and Linux, Debug and Release, strict FP link real existing `audio`, `core`, FFT/Hann/PSD sources in the M20-only lane.
This is **not a qualified M20 application integration**: canonical Phase-6 AnalysisScope ownership, actual M19 loudness mask and P06 retained block identity, M20 result schema/status/QTP, Qt playback/UI binding and formal U+G are still missing.
