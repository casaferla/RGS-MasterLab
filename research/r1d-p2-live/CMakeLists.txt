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
