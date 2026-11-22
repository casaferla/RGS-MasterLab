cmake_minimum_required(VERSION 3.25)
project(RGSTransientResearch LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
set(ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
add_executable(transient_research_qml_smoke
    test_transient_qml_bridge.cpp
    transient_activity_qt_bridge.hpp
    transient_research_controls.hpp
    "${ROOT}/dsp/src/internal/transient_shaper_c00_engine.cpp"
    "${ROOT}/dsp/src/transient_shaper_parameters.cpp"
    "${ROOT}/core/src/error.cpp")
target_include_directories(transient_research_qml_smoke PRIVATE
    "${ROOT}/core/include"
    "${ROOT}/audio/include"
    "${ROOT}/dsp/include"
    "${ROOT}/dsp/src"
    "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(transient_research_qml_smoke PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
if(MSVC)
    target_compile_options(transient_research_qml_smoke PRIVATE /W4 /WX /fp:strict)
else()
    target_compile_options(transient_research_qml_smoke PRIVATE
        -Wall -Wextra -Werror -fno-fast-math -ffp-contract=off)
endif()
