# R1d-P2 Streaming TP — I0 research callback adapter

**Nonproduction, nonrelease prototype.** This folder compiles as an **independent CMake project**. It neither registers a DSP in the RGS Product catalogue nor modifies M17 or the M16 Live meter.

## Exact provenance (checked)

- `pinned/stream.hpp`: SHA-256 `014a22c17559509f032c2b86f13571708ed3692dbdd28142a2ecf06ad7e22f48`.
- `pinned/music_p2.hpp`: SHA-256 `d9758415fa2885235bee9d3b3a6c8cbb108450b552478b84cc248abb0fc9d605`.
- Both identical byte-for-byte between the original `RGS_R1d_P2_Gate_B_Evidence_2026-11-11.zip` and the subsequent `RGS_R1d_Physical_Gate_Windows_x64_2026-11-11.zip`, with full evidence ZIP SHA-256 `4859c2aa7a601f189e101e24538575867301431e19436d38739a841af83a1f39`.
- `pinned/internal/true_peak_itu4x_coefficients.hpp` is the unmodified M16 4x/12-tap coefficient bank from the pinned research's `src/pinned/analysis/...` tree. It is **not** a new reference FIR.

## Build on a development machine

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DR1D_RESEARCH_PHASE_SSE=OFF
cmake --build build
ctest --test-dir build --output-on-failure
cmake -S . -B build-sse -DCMAKE_BUILD_TYPE=Release -DR1D_RESEARCH_PHASE_SSE=ON
cmake --build build-sse
ctest --test-dir build-sse --output-on-failure
```

This builds only a **research test executable**, not a standalone application for the organisation-managed Windows PC; no PowerShell is needed when the future portable EXE is delivered.

## API & math

- `CallbackEngine::prepare(Configuration)` is off the audio thread and allocates the pinned controller/safety state exactly once.
- `CallbackEngine::process(input, output)` is a no-allocation, no-lock synchronous callback adapter. For **N input frames it emits N output frames**. Output initially contains zeros representing the deterministic algorithmic delay.
- `latency_frames = round(fs * 0.0015) + horizon(eta) + 63`. The final `63` is the batch-released 64-frame safety quantum offset; it does **not** describe the device callback period. For eta=2%, horizon=247, so latency=382 at 48 kHz and 598 at 192 kHz.
- `finish(tail)` requires exactly `latency_frames()` output slots, supplies the end-of-stream controller input and safety drain, and does **not** emit the 11 M16 analyzer-only bridge zeros as audio. The compensated programme equals the originally supplied frame count. Do not cut off `finish()` during later host integration.
- The exact ceiling is checked by pinned safety using the M16 coefficients. No clipping at ±1 is applied to float input. The new P2 module is **in-chain**, not a substitute for M17 **after Final SRC**.
- The fixed internal 64-frame quantum is **not** a claim of physical support for a 64-frame WASAPI period; the physical research used a native 512-frame device period.
- `process()` rejects nonfinite and unreasonable input before starting the callback, zeros the output and faults closed on error. It cannot safely restart without a new off-thread instance. Any host checkpoint, seek, loop, live parameter changes, and bypass must be designed around epoch ownership; they are not implemented in this I0 adapter.

## Tests actually run (Linux x64)

- Native CMake build with C++20 and strict FP: `-ffp-contract=off -fno-fast-math -frounding-math -msse2 -mfpmath=sse`, compile macros `RESEARCH_EASED_TAIL`, `RESEARCH_P2_RECOVERY`, `RESEARCH_P2_CACHE`.
- All 6 rates (44.1/48/88.2/96/176.4/192 kHz), mono+stereo, eta2 interior exactness; 48k/192k hot stereo+mono; short programmes 1/5/121 samples; eta5 with +5dB drive; external independently batched offline renderer comparison; chunk sizes 1,7,64,511,512,1024; M16 peaks below C; fail-closed NaN; zero allocations in callback/drain after `prepare` using `operator new` test witness.
- Scalar and phase-SSE2 builds: **complete test transcript including PCM FNV hashes matched exactly** on the same Linux x64 system; one CTest test PASSED per build. No Windows build or audio callback execution has been conducted by this I0 step.

## Gates not closed

The tested PCM is in-memory `double`, not the *encoded* final exported PCM. I0 is not I1 whole suite, no device deadline/ETW certification, not an IModule/Product module, no AutoMatch end-to-end 27-feature L3 or Q0/Fresh Verify, and no new Windows GUI runner. Do not change product, CI/FQ or frozen contracts on the strength of this receipt.

Authority: [Google Drive I0 mandate](https://docs.google.com/document/d/1XBlRR0XwKM7suZjc7rnXwZFJMclpvhg4tfc3yYJo3Vg/edit).

## I1: transport epoch/session research (12 November 2026)

Added `p2_epoch_session.hpp`: transactional off-audio-thread new epoch with monotonically increasing token; stale callback silencing; cancel/seek/loop/reconfigure reset; bypass bit-exact identity at the **same delay** as P2 (no limiter ceiling guarantee while bypassed); explicit EOS tail. No allocations, Qt, filesystem, or mutex in per-block processing. The host must first quiesce callbacks before new epoch/cancel and must handle mute/crossfade on discontinuities. This is not a gapless handoff or production IModule.

The original I0 test target remains in the CMake project. New `r1d_p2_lifecycle_tests` exercises 6 Fs and mono/stereo across short/long signals and block partitions, active PCM versus direct I0 engine, M16 in-memory TP ceiling, bypass equivalence, invalid PCM, stale token, parameter changes and heap witness. Native Linux strict-FP GCC14 scalar+SSE2 both **1/1 PASS**, Clang17 **1/1 PASS**, and GCC ASan/UBSan **1/1 PASS**. No Windows/device test has run in I1.

See [I1 transport contract](TRANSPORT_CONTRACT_I1.md) and [I1 local test receipt](I1_LOCAL_TEST_RECEIPT_20261112.md). Product checkpoint, callback synchronization, click-free switching, encoded PCM, AutoMatch and portable no-PowerShell Windows GUI remain open. No product merge or M17 changes.
