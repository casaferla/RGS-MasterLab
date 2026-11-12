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

## Focused CI #1 — red on both OS, corrected test link dependency

[CI run #1](https://github.com/casaferla/RGS-MasterLab/actions/runs/43255826282)
on candidate SHA `45baa02c9ea0e642e1edac2d05ee101d0826e983`
completed FAILURE on both Ubuntu and Windows after successful source compilation
but before any native test executed. Actual errors: GNU undefined references /
MSVC LNK2019 to `ModuleRegistry::create`,
`ModuleRegistry::with_streaming_true_peak_preview`, and
`ProcessingChain::create`. Root cause: the new test target was linked to
`rgs_p2_research_real_abi` (base IModule) rather than the already present
`rgs_p2_research_chain_abi` (contains genuine registry/chain sources).
Corrected exactly that target link; neither P2 implementation nor frozen
M17 or real Product semantics changed. The new CI result must be verified
before any green claim.

## Focused CI #2 — Linux pass / Windows link failure (recorded 12 Nov 2026)
[Focused CI #2](https://github.com/casaferla/RGS-MasterLab/actions/runs/43256249744)
at `6eed3fe4f98fe418e9d2c4d552e7918bedf157c8`
ended overall FAILURE. The Ubuntu build AND native tests completed SUCCESS;
Windows MSVC built the P2/registry source but failed to link
`r1d_p2_optin_registry_tests.exe`: `LNK2019` references to authentic
built-in M11/M12/M14/M15/M17 Product factories/parameters, whose original
implementations were not in the intentionally reduced research ABI archive.
GNU gc-sections eliminates those uncalled methods; MSVC /Gy /OPT:REF
still requires their vtables' dependencies. This is an incomplete *test
fixture*, not a validated Windows P2 PCM test and not an observed P2
processing defect.

For CI #3, the isolated research chain ABI library adds original
`dsp/src` implementations and `core/src/sha256.cpp` as immutable
source references. No stub, workaround link flags, M17 edit or new frozen
contract. The full MSVC CTest result remains to be checked.
## Distinct preview-only typed P2 controls (candidate, CI not yet executed)
Added `StreamingTruePeakLimiterParameters` with Ceiling [-12,0] dBTP,
Drive [-24,24] dB, margin 2 or 5%, research default -1/0/2.
Appended this type to the existing variant (no change to old alternatives)
and introduced a separate strict `parameters/0.1.0` codec counting duplicate
JSON keys. Product opt-in factory accepts only P2's typed payload and
rejects M17's Ceiling payload. No change to M17's own codec or controls;
no automatic persistence, no UI or sound-preview publication from this lot.
## I2.5 Renderer typed-provenance gate (staging; no false activation)
The original `RenderRequest` now recognizes P2 as a parameterized manual
DSP and rejects missing, duplicated, or M17-mismatched bindings, including
when bypassed. `RenderResult` appends a distinct P2 execution-signature
payload holding exact Ceiling/Drive/margin, without disturbing the existing
four payload variants. The real `render_preview.cpp` emits matching
PROCESSED/BYPASS identity only when typed parameters are actually bound;
its normal latency compensation, EOS and processing path are reused, with
M17 terminal guard untouched.
New test compiles original RenderRequest and exercises real RGS chain/typed
authority. An additional OBJECT CMake target compiles the complete original
RenderPreview on Windows/Linux but does NOT execute a full render. A full
audio/owner/Qt verified preview is still OPEN until its own gate.
