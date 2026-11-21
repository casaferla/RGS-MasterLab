# R1d-P2 I1.7: reproducible isolated ProcessingChain/Registry test

From the RGS repository root, with GCC or Clang and CMake:

```sh
cmake -S research/r1d-p2-live -B build/r1d-p2-i17 -DCMAKE_BUILD_TYPE=Release -DR1D_RESEARCH_PHASE_SSE=OFF
cmake --build build/r1d-p2-i17 -j4
ctest --test-dir build/r1d-p2-i17 -R r1d_p2_real_processing_chain_tests --output-on-failure

cmake -S research/r1d-p2-live -B build/r1d-p2-i17-sse -DCMAKE_BUILD_TYPE=Release -DR1D_RESEARCH_PHASE_SSE=ON
cmake --build build/r1d-p2-i17-sse -j4
ctest --test-dir build/r1d-p2-i17-sse -R r1d_p2_real_processing_chain_tests --output-on-failure
```

For sanitizer runs on GCC, configure with `-DCMAKE_BUILD_TYPE=Debug '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'`. Only the unused Product factory registry translation unit is excluded from sanitizer instrumentation in the isolated target to avoid linking unrelated factories; the tested Chain/P2/Audio is instrumented.

No Windows run, CI run, FQ or physical device timing is implied by these local commands.
