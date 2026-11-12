# I1.5 local test receipt — research-only, 2026-11-12

Sources tested: `p2_planar_host_bridge.hpp`, `tests/test_p2_planar_bridge.cpp` (mock contract), and `tests/test_p2_planar_real.cpp` (actual I1 epoch + pinned P2). Original I0 and I1 header blobs match GitHub: `ea0fdb9e74628c654f4c9dd1641bdd18765a0094` and `463f225eccc52cfa67f88198a87ead275ffbd539`.

Run commands on Linux x86_64:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
cmake -S . -B build-sse -DCMAKE_BUILD_TYPE=Release -DR1D_RESEARCH_PHASE_SSE=ON
cmake --build build-sse
ctest --test-dir build-sse --output-on-failure
cmake -S . -B build-clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release
cmake --build build-clang
ctest --test-dir build-clang --output-on-failure
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-asan
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build-asan --output-on-failure
```

Observed after adding the real EpochSession test: GCC14 Release scalar 2/2 PASS; GCC14 Release SSE2 2/2 PASS; Clang17 Release 2/2 PASS; GCC14 ASan/UBSan Debug 2/2 PASS; warnings-as-errors enabled. Scalar/SSE2 both emit `FNV64=3f935d3414b7e93b` for the real P2 PCM test. 864 valid-flow combinations plus invalid/fault/boundary cases. No errors detected by sanitizers. No heap-allocation instrumentation: the bridge itself uses fixed arrays and no allocating operations in its `process` and `finish`, but this run does not prove the real Product callback is allocation-free.

The mock Session test proves planar bridge contract mapping, not audio quality. The second test uses the **REAL I1 EpochSession and pinned P2 DSP**, checking direct-vs-bridge PCM bytes and in-memory M16 True Peak on several synthetic sources. Neither test instantiates the real RGS `IModule`, `AudioBufferView`, `ModuleRegistry` or Qt host. Product IModule, Windows device, QML render, encoded M16, and AutoMatch are still OPEN.
