add_library(rgsml_analysis STATIC
    src/spectrum_config.cpp
    src/hann.cpp
    src/fft.cpp
    src/psd.cpp
    src/log_binning.cpp
    src/smoothing.cpp
    src/spsc_ring.cpp
    src/live_spectrum_analyzer.cpp
)

target_include_directories(rgsml_analysis
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(rgsml_analysis
    PUBLIC
        rgsml_core
        rgsml_audio
)

set_property(TARGET rgsml_analysis PROPERTY RGSML_DEPENDENCY_LAYER ANALYSIS)
rgsml_apply_project_warnings(rgsml_analysis)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/RgsmlDependencyRules.cmake")
rgsml_assert_target_dependencies(
    TARGET rgsml_analysis
    FORBIDDEN_LAYERS QT QML PLATFORM UI PROJECT RENDER)
