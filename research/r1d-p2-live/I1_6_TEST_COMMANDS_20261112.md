# Real ABI research CTest reproduction (not Product CI)

From `research/r1d-p2-live/` within the repo:

```bash
cmake -S . -B build-i1-6 -DCMAKE_BUILD_TYPE=Release -DR1D_RESEARCH_PHASE_SSE=OFF
cmake --build build-i1-6 --target r1d_p2_real_imodule_tests
ctest --test-dir build-i1-6 -R '^r1d_p2_real_imodule_tests$' --output-on-failure
cmake -S . -B build-i1-6-sse -DCMAKE_BUILD_TYPE=Release -DR1D_RESEARCH_PHASE_SSE=ON
cmake --build build-i1-6-sse --target r1d_p2_real_imodule_tests
ctest --test-dir build-i1-6-sse -R '^r1d_p2_real_imodule_tests$' --output-on-failure
```

Local **reproduction used genuine baseline RGS 19 files**, not a fake ABI. GCC 14.2 / Clang 17 / ASan+UBSan results documented in `I1_6_REAL_RGS_IMODULE_REPORT_20261112.md`. No Windows testing has been run in this step. Never confuse an internal 64-frame P2 quantum with a qualified 64-frame native hardware callback.
