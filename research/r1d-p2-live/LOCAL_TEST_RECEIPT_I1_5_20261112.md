# I1.5 local test receipt — research-only, 2026-11-12

Source tested: `p2_planar_host_bridge.hpp` and `tests/test_p2_planar_bridge.cpp`.

Run commands on Linux x86_64:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
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

Observed: GCC14 Release 1/1 PASS; Clang17 Release 1/1 PASS; GCC14 ASan/UBSan Debug 1/1 PASS; warnings-as-errors enabled. 864 valid-flow combinations plus invalid/fault/boundary cases. No errors detected by sanitizers. No heap-allocation instrumentation: the bridge itself uses fixed arrays and no allocating operations in its `process` and `finish`, but this run does not prove the real Product callback is allocation-free.

The test uses a **mock Session**. It is deliberately NOT a test of the pinned P2 DSP engine, actual I1 EpochSession, RGS IModule, Qt UI or Windows physical audio. The next integration gate must compile and test with the real EpochSession and real RGS host types before claiming completion.
