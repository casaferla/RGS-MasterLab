cmake_minimum_required(VERSION 3.25)
project(RGSDynamicEqEditorResearch LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
add_executable(dynamic_eq_clone_layout_smoke test_dynamic_eq_layout.cpp)
target_link_libraries(dynamic_eq_clone_layout_smoke PRIVATE
  Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
if(NOT MSVC)
  target_compile_options(dynamic_eq_clone_layout_smoke PRIVATE -Wall -Wextra -Werror)
endif()

set(ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
add_executable(dynamic_eq_dynamics_smoke
  test_dynamic_eq_dynamics.cpp
  dynamic_eq_research_dynamics_model.hpp
  "${ROOT}/dsp/src/dynamic_eq_parameters.cpp"
  "${ROOT}/core/src/error.cpp"
  "${ROOT}/core/src/uuid.cpp")
target_include_directories(dynamic_eq_dynamics_smoke PRIVATE
 "${ROOT}/core/include" "${ROOT}/dsp/include")
target_link_libraries(dynamic_eq_dynamics_smoke PRIVATE
 Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
if(NOT MSVC)
 target_compile_options(dynamic_eq_dynamics_smoke PRIVATE -Wall -Wextra -Werror -fno-fast-math -ffp-contract=off)
endif()

add_executable(dynamic_eq_activity_qml_smoke
  test_dynamic_eq_activity_qml.cpp
  dynamic_eq_activity_qt_bridge.hpp
  "${ROOT}/dsp/src/internal/dynamic_eq_c01_engine.cpp"
  "${ROOT}/dsp/src/internal/dynamic_eq_c01_coefficients.cpp"
  "${ROOT}/dsp/src/dynamic_eq_parameters.cpp"
  "${ROOT}/core/src/error.cpp"
  "${ROOT}/core/src/uuid.cpp")
target_include_directories(dynamic_eq_activity_qml_smoke PRIVATE
  "${ROOT}/core/include" "${ROOT}/dsp/include"
  "${ROOT}/dsp/src" "${ROOT}/audio/include"
  "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(dynamic_eq_activity_qml_smoke PRIVATE
  Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
if(NOT MSVC)
  target_compile_options(dynamic_eq_activity_qml_smoke PRIVATE
    -Wall -Wextra -Werror -fno-fast-math -ffp-contract=off)
endif()

# Native Qt Core check of one typed C01 band owner (nonrelease).
add_executable(dynamic_eq_common_smoke
  test_dynamic_eq_common.cpp
  dynamic_eq_research_common_view_model.hpp
  dynamic_eq_research_dynamics_model.hpp
  "${ROOT}/dsp/src/dynamic_eq_parameters.cpp"
  "${ROOT}/dsp/src/internal/dynamic_eq_c01_coefficients.cpp"
  "${ROOT}/core/src/uuid.cpp"
  "${ROOT}/core/src/error.cpp")
target_include_directories(dynamic_eq_common_smoke PRIVATE
  "${ROOT}/core/include" "${ROOT}/dsp/include")
target_link_libraries(dynamic_eq_common_smoke PRIVATE Qt6::Core)
if(NOT MSVC)
  target_compile_options(dynamic_eq_common_smoke PRIVATE
    -Wall -Wextra -Werror -fno-fast-math -ffp-contract=off)
endif()

# Common QObject is actually instantiated in the Dynamic EQ QML test.
# Link the exact frozen C01 coefficient generator; no duplicate response DSP.
target_sources(dynamic_eq_dynamics_smoke PRIVATE
  dynamic_eq_research_common_view_model.hpp
  "${ROOT}/dsp/src/internal/dynamic_eq_c01_coefficients.cpp")
