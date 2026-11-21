# RGS MasterLab — M18 prequalification CI layers and reproducible WAV evidence

**3 November 2026 · preproduction only · draft PR #63 · no M17/main integration**

## Reason for the split

A 90-second isolated C++ check and a 15–20-minute whole-application Qt build do **not** cover the same product boundary. The efficient approach is to retain all qualifying historical product tests but to trigger them at the appropriate integration gate, rather than with every low-level algorithm edit.

| Layer | When executed | Checks | Explicit nonclaim |
|---|---|---|---|
| 0 — private staging | During large single-writer macro-lots | Local Debug+Release native C++ / Python oracle; reject bad diffs before promotion | Not CI, not Windows app |
| 1 — M18 rapid | Promotion to M18 Draft #63 | Independent Windows MSVC / Linux GCC Debug+Release, exact codec, 24 true WAVs, SHA/readback, negative publisher, postdecoded real M16 FIR | Not M17 integration, not Qt or formal Gate G |
| 2 — Proposed full/math + audio | Dedicated changed candidate/evidence files or manual dispatch | 87 full-length candidate statistics, independent exact-row/bounds audit + 15 falsifications, 7 microscopic diagnostic WAVs, 4 audible synthetic stereo WAVs, independent WAV verification; Windows ZIP artifact | Not qualified/PO-adopted Gate G or app listening FQ |
| 3 — Post-M17 application FQ | Accepted M17 + mandatory CI architecture audit/reset + owner-approved M18 integration | Actual CMake/Qt whole-app builds, resource/seed/project binding, authoritative Windows writer, physical RF64, actual delivered True Peak, test install, owner listening | Must pass independently before main merge |

The second-layer workflow is `.github/workflows/m18-audio-prequalification.yml`. It remains separate from `.github/workflows/m18-preproduction.yml` (rapid) and the older `.github/workflows/m18-gate-g-candidate.yml` (initial 87 candidate). It is deliberately M18 branch-guarded, never touches M17 PR #61, `main`, the Windows FQ, Qt/QML, or 16 original FIR assets.

## What the independent evidence proves

The proposed statistical transcript uses **N = 2^20 per primary statistic, 3 frozen seeds, 2 channels**, and a fully enumerated 87-test population:

- 6 dither means and 6 second moments;
- 24 disjoint lag correlations at 1, 7, 31, 1024 samples;
- 3 stereo cross-channel correlations;
- 24 quantization-error means and 24 second moments over four dyadic input fixtures.

The independent Python parser `audit_m18_proposed_gate_g_metrics.py` validates **all 87 rows**, the predeclared bounds, full population coordinates, no duplicates or missing metrics, counts and recomputed worst ratio. The separate adversarial selftest injects 15 forged metric transcripts. The acceptance bounds are those of the independent October pre-audit **proposal**, and remain **not formally adopted** as release requirements.

### Real audio artifacts (11)

The bundle is produced with the actual isolated C++ `m18_export_offline_candidate`, using the existing WAV encoder/transaction candidate; it contains:

- **Seven microscopic PCM24 diagnostic WAVs:** quarter-LSB sine at 48 and 192 kHz, 2-LSB sine, fade, sparse/reverb-like impulses, LF decay, identical binary64 stereo with independent per-channel dither. Their signal is near or below audibility at normal playback levels: **do not turn the listening volume excessively high**; inspect with a numerical analyzer.
- **Four audible original stereo WAVs:** five-second synthesized tonal texture with bass/harmonics and deliberate fade-out, 44.1 and 48 kHz, each in PCM24 and Float32. These are musical-tone test signals produced without third-party recordings. A sound difference, if perceived, cannot alone prove a quantization defect, because the playback path/device is unqualified.

The Python bundle validator reopens each physical file, checks canonical header geometry, frame count, independent SHA-256 and diagnostic spectral/dither properties, verifies PCM24 vs F32 decoded numerical agreement and plausible peak/RMS/fade of audible tracks, and rejects altered content. SHA evidence and source SHA live in `manifest.json` and `SHA256SUMS.txt`; Windows CI publishes all 11 WAVs as a GitHub Actions artifact with 14-day retention. Linux publishes only the verified manifest and checksums, because platform `sin`/`libm` may alter the synthetic source at low bits: cross-platform **identical** synthetic WAV SHA is not asserted.

## Follow-up required before production acceptance

Completion of this isolated evidence does not certify M18 integrated with the final-MASTER PCM owned by M17, the canonical project output/seed schema, the Windows `TransactionalAudioExporter`, true multi-GiB RF64, fault/power-loss durability, physical user listening FQ, or formal adopted Gate G. The ongoing M17 Windows FQ must first pass; afterward perform the **mandatory post-M17 CI reset/architecture audit**, forward-rebase M18 with nonforce SHA checks, integrate the application through its existing owner interfaces, and only then request product owner release/merge authorization.

The proposed future practice is **large macro-lots, incremental local checks, rapid independent CI per promotion, heavier CI at prequalification, full FQ only at product integration gates**. This is not permission to remove historic test coverage or to build indefinitely without compilation.