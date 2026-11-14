# R1d-P2 I1 local evidence receipt — 2026-11-12

## Provenance / scope

Research-only `p2_epoch_session.hpp` plus `tests/test_p2_lifecycle.cpp`, compiled against the existing **unchanged** `p2_callback_engine.hpp` and pinned original `stream.hpp`, `music_p2.hpp`, and M16 coefficient bank. Original source archive already pinned by I0. No Product `IModule`, no changes to DSP registry/serialisation/GUI/M17, no user-PC run.

## Actual execution

Environment: container Linux x86_64, GCC 14.2.0, CMake 3.31.6, C++20. Flags in CMake `-O2 -Wall -Wextra -Wpedantic -ffp-contract=off -fno-fast-math -frounding-math -msse2 -mfpmath=s`; M16 path `RESEARCH_P2_PHASE_SSE` enabled separately.

**Local I1 target (with original I0 source target preserved in repository):**

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DR1D_RESEARCH_PHASE_SSE=OFF
cmake --build build
ctest --test-dir build --output-on-failure
cmake -S . -B build-sse -DCMAKE_BUILD_TYPE=Release -DR1D_RESEARCH_PHASE_SSE=ON
cmake --build build-sse
ctest --test-dir build-sse --output-on-failure
```

Local isolated target result: scalar **1/1 CTest PASS**; phase-SSE2 **1/1 CTest PASS**, stdout SHA256 BOTH = `72b861d71c15bb6c9c8db6d57c6d76325423b9cd8c696c11952b77fff9a27fbf`, text file byte-for-byte equal. Build after adjusted allocation witness emits zero GCC warnings. All 6 sample rates, mono/stereo, 6 programme lengths and 6 callback block partitions, bit-identical active output to direct I0 engine and bypass identical to source after latency compensation; M16 on in-memory double output <= C. Transactional rejection of invalid config, unique epoch token and stale callback silencing validated. No-heap witness on process+finish after prepare, P2 & bypass, 192k stereo 512-frame blocks. The test uses a single-process deterministic suite, not physical audio deadline measurement.

**Sanitizers:** separate Debug build with `-fsanitize=address,undefined -fno-omit-frame-pointer`; after explicitly routing test-only nothrow `operator new` through the same custom allocation witness, **1/1 ASan+UBSan CTest PASS** with `ASAN_OPTIONS=detect_leaks=1`. First sanitizer attempt reported a **test-harness** alloc/dealloc mismatch (nothrow new was not intercepted while custom delete used `free`). Fixed by adding nothrow allocation overload; no production source changed. Report this attempt, not hide it.

**Second compiler:** Clang 17.0.0 Linux strict-FP Release target: **1/1 CTest PASS**, without compiler warnings. Not a Windows/MSVC test.

## Caveats / not done

This I1 work is isolated transport state research, not final I1 Product acceptance. No real hardware, Windows toolchain run, cross-thread adversarial race, sample-rate-change while callbacks continue, product checkpoint, bypass crossfade, Qt listener, AutoMatch canonical L3, encoded M16, CI/FQ or release. Epoch changes require an off-thread host barrier; no live toggle inside callback. No claim of click-free seamless switching. No production integration GO.

Cited mandates: [I0](https://docs.google.com/document/d/1XBlRR0XwKM7suZjc7rnXwZFJMclpvhg4tfc3yYJo3Vg/edit) and [longitudinal ledger](https://docs.google.com/document/d/1eaHB5FnSPkKOGZZVzJlRFb1r5V2uxTeIZnpMtBB0na0/edit).
