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
