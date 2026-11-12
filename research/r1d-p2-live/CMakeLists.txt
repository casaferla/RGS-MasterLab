cmake_minimum_required(VERSION 3.20)
project(rgs_r1d_p2_callback_research LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(R1D_RESEARCH_PHASE_SSE "Use pinned two-phase SSE2 M16 kernels" OFF)
add_executable(r1d_p2_callback_tests tests/test_p2_callback.cpp)
if(R1D_RESEARCH_PHASE_SSE)
  target_compile_definitions(r1d_p2_callback_tests PRIVATE RESEARCH_P2_PHASE_SSE)
endif()
target_include_directories(r1d_p2_callback_tests PRIVATE . pinned)
target_compile_definitions(r1d_p2_callback_tests PRIVATE
  RESEARCH_EASED_TAIL RESEARCH_P2_RECOVERY RESEARCH_P2_CACHE)
if(MSVC)
  target_compile_options(r1d_p2_callback_tests PRIVATE /W4 /fp:strict /arch:SSE2)
else()
  target_compile_options(r1d_p2_callback_tests PRIVATE -O2 -Wall -Wextra -Wpedantic
    -ffp-contract=off -fno-fast-math -frounding-math -msse2 -mfpmath=sse)
endif()
enable_testing()
add_test(NAME r1d_p2_callback_tests COMMAND r1d_p2_callback_tests)

# I1 epoch/transport research target. The original I0 test is unchanged.
add_executable(r1d_p2_lifecycle_tests tests/test_p2_lifecycle.cpp)
if(R1D_RESEARCH_PHASE_SSE)
  target_compile_definitions(r1d_p2_lifecycle_tests PRIVATE RESEARCH_P2_PHASE_SSE)
endif()
target_include_directories(r1d_p2_lifecycle_tests PRIVATE . pinned)
target_compile_definitions(r1d_p2_lifecycle_tests PRIVATE
  RESEARCH_EASED_TAIL RESEARCH_P2_RECOVERY RESEARCH_P2_CACHE)
if(MSVC)
  target_compile_options(r1d_p2_lifecycle_tests PRIVATE /W4 /fp:strict)
else()
  target_compile_options(r1d_p2_lifecycle_tests PRIVATE -O2 -Wall -Wextra -Wpedantic
    -ffp-contract=off -fno-fast-math -frounding-math -msse2 -mfpmath=sse)
endif()
add_test(NAME r1d_p2_lifecycle_tests COMMAND r1d_p2_lifecycle_tests)

# I1.5 standalone planar/epoch contract test; mock epoch is explicit.
# No Product IModule, QML or ModuleRegistry wiring.
add_executable(r1d_p2_planar_host_bridge_tests tests/test_p2_planar_bridge.cpp)
target_include_directories(r1d_p2_planar_host_bridge_tests PRIVATE .)
if(MSVC)
  target_compile_options(r1d_p2_planar_host_bridge_tests PRIVATE /W4 /fp:strict)
else()
  target_compile_options(r1d_p2_planar_host_bridge_tests PRIVATE
    -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off
    -fno-fast-math -frounding-math)
endif()
add_test(NAME r1d_p2_planar_host_bridge_tests
         COMMAND r1d_p2_planar_host_bridge_tests)

# I1.5 real P2+EpochSession-to-planar bridge test. Nonproduction only.
add_executable(r1d_p2_planar_real_epoch_tests tests/test_p2_planar_real.cpp)
target_include_directories(r1d_p2_planar_real_epoch_tests PRIVATE . pinned)
target_compile_definitions(r1d_p2_planar_real_epoch_tests PRIVATE
  RESEARCH_EASED_TAIL RESEARCH_P2_RECOVERY RESEARCH_P2_CACHE)
if(R1D_RESEARCH_PHASE_SSE)
  target_compile_definitions(r1d_p2_planar_real_epoch_tests PRIVATE
    RESEARCH_P2_PHASE_SSE)
endif()
if(MSVC)
  target_compile_options(r1d_p2_planar_real_epoch_tests PRIVATE /W4 /fp:strict)
else()
  target_compile_options(r1d_p2_planar_real_epoch_tests PRIVATE
    -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off
    -fno-fast-math -frounding-math -msse2 -mfpmath=sse)
endif()
add_test(NAME r1d_p2_planar_real_epoch_tests
         COMMAND r1d_p2_planar_real_epoch_tests)

# I1.6: genuine RGS IModule ABI compatibility research only.
# This independent target compiles the existing unmodified core/audio/dsp
# implementations directly from the repository root. NOT product wiring.
set(RGS_P2_ABI_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
add_library(rgs_p2_research_real_abi STATIC
    ${RGS_P2_ABI_ROOT}/core/src/checked_integer.cpp
    ${RGS_P2_ABI_ROOT}/core/src/error.cpp
    ${RGS_P2_ABI_ROOT}/core/src/frame_time.cpp
    ${RGS_P2_ABI_ROOT}/core/src/rational.cpp
    ${RGS_P2_ABI_ROOT}/audio/src/audio_buffer.cpp
    ${RGS_P2_ABI_ROOT}/audio/src/audio_format.cpp
    ${RGS_P2_ABI_ROOT}/dsp/src/imodule.cpp
    ${RGS_P2_ABI_ROOT}/dsp/src/module_descriptor.cpp)
target_include_directories(rgs_p2_research_real_abi PUBLIC
    ${RGS_P2_ABI_ROOT}/core/include
    ${RGS_P2_ABI_ROOT}/audio/include
    ${RGS_P2_ABI_ROOT}/dsp/include)
add_executable(r1d_p2_real_imodule_tests tests/test_p2_real_imodule.cpp)
target_include_directories(r1d_p2_real_imodule_tests PRIVATE . pinned)
target_link_libraries(r1d_p2_real_imodule_tests PRIVATE rgs_p2_research_real_abi)
target_compile_definitions(r1d_p2_real_imodule_tests PRIVATE
    RESEARCH_EASED_TAIL RESEARCH_P2_RECOVERY RESEARCH_P2_CACHE)
if(R1D_RESEARCH_PHASE_SSE)
    target_compile_definitions(r1d_p2_real_imodule_tests PRIVATE
        RESEARCH_P2_PHASE_SSE)
endif()
if(MSVC)
    target_compile_options(r1d_p2_real_imodule_tests PRIVATE /W4 /fp:strict)
    target_compile_options(rgs_p2_research_real_abi PRIVATE /W4 /fp:strict)
else()
    target_compile_options(r1d_p2_real_imodule_tests PRIVATE
        -O2 -Wall -Wextra -Wpedantic -Werror
        -ffp-contract=off -fno-fast-math -frounding-math -msse2 -mfpmath=sse)
    target_compile_options(rgs_p2_research_real_abi PRIVATE
        -ffp-contract=off -fno-fast-math -frounding-math)
endif()
add_test(NAME r1d_p2_real_imodule_tests COMMAND r1d_p2_real_imodule_tests)

# I1.7: isolated real ProcessingChain + real ModuleRegistry factory proof.
# Original package_v1 registrations are NEVER changed.
set(RGS_P2_CHAIN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")

# Full genuine DSP factory dependencies: MSVC keeps the whole registry
# translation unit (unlike GNU gc-sections), including unused Product
# factory vtables. Link ORIGINAL source implementations in this isolated
# research target rather than mock or suppress any unresolved symbol.
# M17's original source is compiled unmodified, and is never executed in
# the MANUAL streaming P2 test.

add_library(rgs_p2_research_chain_abi STATIC
  ${RGS_P2_CHAIN_ROOT}/core/src/checked_integer.cpp
  ${RGS_P2_CHAIN_ROOT}/core/src/error.cpp
  ${RGS_P2_CHAIN_ROOT}/core/src/frame_time.cpp
  ${RGS_P2_CHAIN_ROOT}/core/src/rational.cpp
  ${RGS_P2_CHAIN_ROOT}/core/src/uuid.cpp
  ${RGS_P2_CHAIN_ROOT}/audio/src/audio_buffer.cpp
  ${RGS_P2_CHAIN_ROOT}/audio/src/audio_format.cpp
  ${RGS_P2_CHAIN_ROOT}/dsp/src/imodule.cpp
  ${RGS_P2_CHAIN_ROOT}/dsp/src/module_descriptor.cpp
  ${RGS_P2_CHAIN_ROOT}/dsp/src/module_registry.cpp
  ${RGS_P2_CHAIN_ROOT}/dsp/src/processing_chain.cpp
  ${RGS_P2_CHAIN_ROOT}/core/src/sha256.cpp
  ${RGS_P2_CHAIN_ROOT}/dsp/src/gain_parameters.cpp
  ${RGS_P2_CHAIN_ROOT}/dsp/src/gain_module.cpp
  ${RGS_P2_CHAIN_ROOT}/dsp/src/parametric_eq_parameters.cpp
  ${RGS_P2_CHAIN_ROOT}/dsp/src/parametric_eq_module.cpp
  ${RGS_P2_CHAIN_ROOT}/dsp/src/parametric_eq_response.cpp
  ${RGS_P2_CHAIN_ROOT}/dsp/src/internal/parametric_eq_coefficients.cpp
  ${RGS_P2_CHAIN_ROOT}/dsp/src/compressor_parameters.cpp
  ${RGS_P2_CHAIN_ROOT}/dsp/src/compressor_module.cpp
  ${RGS_P2_CHAIN_ROOT}/dsp/src/stereo_ms_parameters.cpp
  ${RGS_P2_CHAIN_ROOT}/dsp/src/stereo_ms_module.cpp
  ${RGS_P2_CHAIN_ROOT}/dsp/src/stereo_ms_width.cpp
  ${RGS_P2_CHAIN_ROOT}/dsp/src/stereo_ms_width_response.cpp
  ${RGS_P2_CHAIN_ROOT}/dsp/src/stereo_ms_crossover.cpp
  ${RGS_P2_CHAIN_ROOT}/dsp/src/stereo_ms_crossover_runtime.cpp
  ${RGS_P2_CHAIN_ROOT}/dsp/src/true_peak_limiter_parameters.cpp
  ${RGS_P2_CHAIN_ROOT}/dsp/src/true_peak_limiter_module.cpp
  ${RGS_P2_CHAIN_ROOT}/dsp/src/module_parameter_codec.cpp)
target_include_directories(rgs_p2_research_chain_abi PUBLIC
  ${RGS_P2_CHAIN_ROOT}/core/include
  ${RGS_P2_CHAIN_ROOT}/audio/include
  ${RGS_P2_CHAIN_ROOT}/dsp/include)
target_include_directories(rgs_p2_research_chain_abi SYSTEM PRIVATE
  ${RGS_P2_CHAIN_ROOT}/third_party/nlohmann_json/single_include)
add_executable(r1d_p2_real_processing_chain_tests
  tests/test_p2_real_processing_chain.cpp)
target_include_directories(r1d_p2_real_processing_chain_tests PRIVATE . pinned)
target_link_libraries(r1d_p2_real_processing_chain_tests PRIVATE
  rgs_p2_research_chain_abi)
target_compile_definitions(r1d_p2_real_processing_chain_tests PRIVATE
  RESEARCH_EASED_TAIL RESEARCH_P2_RECOVERY RESEARCH_P2_CACHE)
if(R1D_RESEARCH_PHASE_SSE)
  target_compile_definitions(r1d_p2_real_processing_chain_tests
    PRIVATE RESEARCH_P2_PHASE_SSE)
endif()
if(MSVC)
  target_compile_options(r1d_p2_real_processing_chain_tests PRIVATE /W4 /fp:strict /Gy)
  target_compile_options(rgs_p2_research_chain_abi PRIVATE /W4 /fp:strict /Gy)
  target_link_options(r1d_p2_real_processing_chain_tests PRIVATE /OPT:REF)
else()
  target_compile_options(r1d_p2_real_processing_chain_tests PRIVATE
    -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off
    -fno-fast-math -frounding-math -ffunction-sections
    -fdata-sections -msse2 -mfpmath=sse)
  target_compile_options(rgs_p2_research_chain_abi PRIVATE
    -ffp-contract=off -fno-fast-math -frounding-math
    -ffunction-sections -fdata-sections)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(r1d_p2_real_processing_chain_tests PRIVATE
      -Wl,--gc-sections)
  endif()
endif()
# Unused built-in Product factory code in module_registry.cpp would pull
# unrelated frozen DSP symbols under UBSan. For this isolated proof only,
# do not instrument that ONE TU. The real ProcessingChain, audio and P2
# remain instrumented; no full Product-registry sanitizer claim is made.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set_source_files_properties(
    ${RGS_P2_CHAIN_ROOT}/dsp/src/module_registry.cpp
    PROPERTIES COMPILE_OPTIONS "-fno-sanitize=all")
endif()
add_test(NAME r1d_p2_real_processing_chain_tests
  COMMAND r1d_p2_real_processing_chain_tests)

# I1.8: deterministic fresh-epoch candidate full replay for AutoMatch feasibility.
# This does NOT implement canonical L3/M10 or a runtime checkpoint.
add_executable(r1d_p2_fresh_evaluation_tests tests/test_p2_fresh_evaluation.cpp)
target_include_directories(r1d_p2_fresh_evaluation_tests PRIVATE . pinned)
target_link_libraries(r1d_p2_fresh_evaluation_tests PRIVATE
  rgs_p2_research_real_abi)
target_compile_definitions(r1d_p2_fresh_evaluation_tests PRIVATE
  RESEARCH_EASED_TAIL RESEARCH_P2_RECOVERY RESEARCH_P2_CACHE)
if(R1D_RESEARCH_PHASE_SSE)
  target_compile_definitions(r1d_p2_fresh_evaluation_tests PRIVATE
    RESEARCH_P2_PHASE_SSE)
endif()
if(MSVC)
  target_compile_options(r1d_p2_fresh_evaluation_tests PRIVATE /W4 /fp:strict)
else()
  target_compile_options(r1d_p2_fresh_evaluation_tests PRIVATE
    -O2 -Wall -Wextra -Wpedantic -Werror
    -ffp-contract=off -fno-fast-math -frounding-math -msse2 -mfpmath=sse)
endif()
add_test(NAME r1d_p2_fresh_evaluation_tests COMMAND r1d_p2_fresh_evaluation_tests)

# I2 preview opt-in factory + genuine Product ModuleRegistry test.
# No activation in Product Main/Project or default package_v1.
add_executable(r1d_p2_optin_registry_tests tests/test_p2_optin_registry.cpp)
target_include_directories(r1d_p2_optin_registry_tests PRIVATE . pinned)
target_link_libraries(r1d_p2_optin_registry_tests PRIVATE rgs_p2_research_chain_abi)
target_compile_definitions(r1d_p2_optin_registry_tests PRIVATE
  RESEARCH_EASED_TAIL RESEARCH_P2_RECOVERY RESEARCH_P2_CACHE)
if(R1D_RESEARCH_PHASE_SSE)
  target_compile_definitions(r1d_p2_optin_registry_tests PRIVATE RESEARCH_P2_PHASE_SSE)
endif()
if(MSVC)
  target_compile_options(r1d_p2_optin_registry_tests PRIVATE /W4 /fp:strict)
else()
  target_compile_options(r1d_p2_optin_registry_tests PRIVATE
    -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -fno-fast-math
    -frounding-math -ffunction-sections -fdata-sections -msse2 -mfpmath=sse)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(r1d_p2_optin_registry_tests PRIVATE -Wl,--gc-sections)
  endif()
endif()
add_test(NAME r1d_p2_optin_registry_tests COMMAND r1d_p2_optin_registry_tests)

# I2.5 genuine RenderRequest validation and RenderPreview compile-only ABI gate.
# Execution of full Qt Product RenderPreview remains separately gated.
add_executable(r1d_p2_render_request_tests
    tests/test_p2_render_request.cpp
    ${RGS_P2_CHAIN_ROOT}/render/src/render_request.cpp)
target_include_directories(r1d_p2_render_request_tests PRIVATE
    ${RGS_P2_CHAIN_ROOT}/render/include)
target_link_libraries(r1d_p2_render_request_tests PRIVATE
    rgs_p2_research_chain_abi)
if(MSVC)
  target_compile_options(r1d_p2_render_request_tests PRIVATE /W4 /fp:strict)
else()
  target_compile_options(r1d_p2_render_request_tests PRIVATE
    -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -fno-fast-math
    -frounding-math)
endif()
add_test(NAME r1d_p2_render_request_tests COMMAND r1d_p2_render_request_tests)
# Compile actual render_preview.cpp on Windows/Linux for all new signature
# variant usages. An OBJECT target does not assert link or PCM runtime proof.
add_library(r1d_p2_render_preview_compile_gate OBJECT
  ${RGS_P2_CHAIN_ROOT}/render/src/render_preview.cpp)
target_include_directories(r1d_p2_render_preview_compile_gate PRIVATE
  ${RGS_P2_CHAIN_ROOT}/render/include
  ${RGS_P2_CHAIN_ROOT}/analysis/include)
target_link_libraries(r1d_p2_render_preview_compile_gate PRIVATE
  rgs_p2_research_chain_abi)
if(MSVC)
  target_compile_options(r1d_p2_render_preview_compile_gate PRIVATE /W4 /fp:strict)
else()
  target_compile_options(r1d_p2_render_preview_compile_gate PRIVATE
    -Wall -Wextra -Wpedantic -ffp-contract=off -fno-fast-math -frounding-math)
endif()
