# RGS MasterLab — Streaming True Peak Limiter / I2 Product opt-in registry — 2026-11-12

## Authority and containment
PO-authorized Single Writer branch: `staging/r1d-p2-i2-preview-writer-no-ci-20261112`, exactly based on Windows FQ #101 completed success SHA `616c3b7a9952220dfedb488bf39aa068df6d2e97`. FQ run https://github.com/casaferla/RGS-MasterLab/actions/runs/42898634288 . The manual physical gate remains open. Frozen PR #99, main, M17 terminal, and older M23 writer are not targets of this lot.

## Actual changes
- `dsp/include/rgsml/dsp/module_registry.hpp` exposes explicit `ModuleRegistry::with_streaming_true_peak_preview(ModuleRegistry baseline)`.
- `dsp/src/module_registry.cpp` implements this opt-in by preserving every registration from its caller-supplied baseline and adding one Dynamics / MASTER / MANUAL / must-be-last / nonduplicable `rgsml.dsp.streaming-true-peak-limiter` descriptor with a genuine `IModuleFactory` returning the tested research P2 adapter. Uses explicit compile-time research profile `RESEARCH_EASED_TAIL`, `RESEARCH_P2_RECOVERY`, `RESEARCH_P2_CACHE` in this one implementation translation unit. Default initial configuration: ceiling -1.0 dBTP (linear 0.8912509381337456), Drive 0.0 dB, eta=2%; research-only settings, no persisted default contract.
- The frozen original `create_dsp_package_v1()` function and `package_v1_specs()` remain byte-for-byte unchanged; existing terminal M17 ID, factory and processing stage are untouched. The P2 factory is **never created by the original v1 path**.
- New focused research test `research/r1d-p2-live/tests/test_p2_optin_registry.cpp` validates explicit augmentation, duplicate rejection, stage/order invariants, M17 kept terminal, actual factory instantiation, 48/192k mono/stereo audio render bit-identical to the pinned P2 EpochSession including EOS and latency. Added only one test target to existing research CMake.

## Tests really executed in Linux x86_64 local container
- CMake GCC14 Release scalar: PASS; GCC14 Release SSE2: PASS; Clang17 Release: PASS. Their complete test stdout SHA256 matched: `0b1955cce4a62e35d10765b05bd774ef11006ffd8ba299356cb8df76a6163b72`.
- GCC14 Debug AddressSanitizer+UndefinedBehaviorSanitizer: PASS, with longstanding test CMake limitation that only the existing `module_registry.cpp` translation unit is uninstrumented to prevent unrelated Product factories' unresolved link stubs; real P2 processing and test remain sanitizer-instrumented.
- **Investigation of an initial failing ASan attempt:** New/delete type-size mismatch arose because `RESEARCH_P2_CACHE` affected `Payload` size differently in `module_registry.cpp` and the test translation unit. Corrected by explicitly pinning the three algorithm profile macros before including the original research adapter in the Product factory translation unit. Recompiled all modes; ASan passed. No false claim that the initial attempt was green.
- No actual Windows build, Qt GUI, playback, AutoMatch, Full Qualification, physical device or serialized Project test in this lot.

## What is **not** yet implemented
The Product application still calls the frozen v1 package path and its original `MasteringChainState` and DSP-chain adapter. This opt-in factory is a Product-code **infrastructure seam**, NOT an activated DSP Chain row, not audible master preview and not an import-safe persisted module. Next steps require explicit Product bootstrap call to the new registry, typed parameters/persistence with legacy-safe restore, actual execution bindings and preview scheduler, latency compensation, UI adapter/QML editor and actual Windows tests. Never present an ACTIVE UI until its audio truly routes through P2.

## Reproduction
`cmake -S research/r1d-p2-live -B <build> -G Ninja -DCMAKE_BUILD_TYPE=Release` then build and execute `r1d_p2_optin_registry_tests`; set `-DR1D_RESEARCH_PHASE_SSE=ON` for the alternate test backend. For sanitizer mode configure `-DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'` and execute the same target. This assumes the I1.7 mirror-style source layout which the research CMake originally used; the test built against original RGS core/audio/dsp sources as restored from FQ candidate.
