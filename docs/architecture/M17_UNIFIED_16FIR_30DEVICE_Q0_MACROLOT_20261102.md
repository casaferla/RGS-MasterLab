# M17 — unified 16-FIR catalog, 30 nonidentity DEVICE directions (2026-11-02)

Status: **STAGING CODE CANDIDATE / NOT YET FQ OR PHYSICALLY QUALIFIED**. Product Owner specifically authorized a macro-lot to make six SOURCE/MASTER/DEVICE sample rates operate under one coherent original quality policy after #970.

## Starting canonical gate
- PR #61 Draft source SHA `2a5e7c7acfabeaafd4bf18f0e5054c4ce0c4cb2c`.
- Focused #970 https://github.com/casaferla/RGS-MasterLab/actions/runs/41500887156 **SUCCESS** Windows+Linux; predecessor version, not evidence for this macro-lot.
- Main untouched; all 16 source files were already directly versioned and bundled in the self-contained Windows FQ #90 ZIP. No FIR coefficient binaries are changed.

## One code/asset policy
- Every one of six sample rates (44.1, 48, 88.2, 96, 176.4, 192 kHz) has deterministic exact identity when input==output, no resampler.
- The **30 directed nonidentity routes** all resolve their FIR in the SAME external application asset root, with independently frozen SHA and true exact ratio; no generated filter. Two original historical Playback coefficients are preserved byte-exact for the two 44.1↔48 directions. Remaining 28 directions reuse the 14 authentic frozen Q0 RGSMSR1 source-bin assets, ratio-bound, parsed/checked for full 52-byte header, shape, symmetry, and digest; no silent fallback on absent/corrupt assets. The cache has immutable ownership and is cleared when resource root changes.
- All ordinary SOURCE→DEVICE playback block conversion now uses that same `PlaybackSampleRateAdapter` polyphase path; all `MASTER→DEVICE` monitoring PCM builds using the same adapter/verified external kernel (including all 28 formerly experimental DEVICE paths); the extra separate prototype generator is no longer entered by the real MONITOR path.
- True SOURCE→MASTER final M17 continues independently to apply the original Q0 FIR at the selected rate before M17/M16 and retains complete zero-origin master ownership. Two legacy 44.1↔48 Playback coefficients are not claimed identical to their Q0 MASTER coefficients; they are a separately frozen pair in the same source inventory. Identity routes are bitwise copies.
- The isolated legacy `materialize_full_resource_output_rate_candidate` SOURCE-domain exploratory offline candidate keeps its pre-existing NONRELEASE generator solely for qualification fixtures; **the productive M17 MASTER and monitoring are never routed through that prototype**. No premature inference of Q1–Q6/device release from using authentic coefficients.
- Monitor peak RAM preflight now accounts for the exact original frozen Q0 geometry (header + every binary64 tap) and conservative multiply counts, rather than a synthetic 128-tap family. It preserves Windows host memory and endpoint validation, source/master/device owner boundaries, cancellation, same-rate identity, half-open-ceil time map, and explicit FQ gate.
- Source SHA pins, symmetric binary64 values, M17 limiter DSP, M16 analysis, Windows hardware service, I/O, render coefficients, manifest `releaseAuthority=false` and pending DEVICE qualification flags stay unchanged; no fabricated LIVE meter telemetry.

## CI regression matrix and FAIL-CLOSED contracts
- All 30 ordinary playback adapter combinations must instantiate with nonempty trusted SHA; all 28 original Q0 relationships must match the real independent frozen (L,M) pins; 2 historic SHA/PCM bits remain exact.
- New 28-route test compares actual callback-block output against `render_phase11_strict_scalar_reference` on the same authentic Q0 coefficient bits, with 2-channel, chunked/absolute-frame input and bit-exact output equality. The six identities remain no-FIR.
- Device memory 36-rate matrix now expects actual frozen prototype tap counts, not invented temporary prototype memory.
- Tamper/missing original resource root and unauthenticated original SHA must fail closed, with no Q0 self-signed fallback.
- Master/Device independence, output rate, no source alias, float64 WAV, 36×36 directed tests and existing performance guard (including 192→48 1-second stereo in Release) must all remain green. **Do not weaken benchmarks to make the new heavier kernels pass.** If performance fails, optimize the implementation under separately bit-exact conditions and rerun CI.

## Still mandatory for M17 product acceptance
1. One coherent staging macro-lot CAS promoted to Draft PR once ready, Focused Windows+Linux PASS on that exact SHA (any red fixed staging-only, no main).
2. PO-triggered full cold Windows FQ ZIP; verify all 16 original SHA-pinned FIR binaries and actual Q0/Playback monitor routes, native Qt settings and tests.
3. Physical PO tests on 44.1→48, 48→88.2, 192→48, 192 MASTER→48 DEVICE and cross-rate variants; no UI freezes, safe Cancel, time/ETA and A/B ownership, audio continuity/underrun and exact return. Candidate is NOT automatically physically accepted.
4. M16/M17 post-limiter true-peak and consumed DEVICE/MONITOR LIVE telemetry require separate honest source/owner qualification, not invented values. Original PO observations of non-working visual LED/glow/Parametric EQ and no LIVE during monitor remain explicit open gates.
5. Scope Q1–Q6 final per-route product qualification, selected MASTER+DEVICE independence and staging/main policy decisions with Product Owner. No release/main merge until acceptance. Only after M17 acceptance: CI structural audit/reset, then M18 Dither.

All metrics and acceptance are explicit; neither one successful past FQ #90 nor a single green Focused CI can certify the new 30-route DEVICE runtime. No full logs collected.
