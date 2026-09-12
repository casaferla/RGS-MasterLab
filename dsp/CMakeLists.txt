add_library(rgsml_dsp STATIC
    src/gain_parameters.cpp
    src/gain_module.cpp
    src/module_descriptor.cpp
    src/module_registry.cpp
    src/processing_chain.cpp
)

target_include_directories(rgsml_dsp
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(rgsml_dsp
    PUBLIC
        rgsml_core
        rgsml_audio
)

set_property(TARGET rgsml_dsp PROPERTY RGSML_DEPENDENCY_LAYER DSP)
rgsml_apply_project_warnings(rgsml_dsp)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/RgsmlDependencyRules.cmake")
rgsml_assert_target_dependencies(
    TARGET rgsml_dsp
    FORBIDDEN_LAYERS QT QML PLATFORM UI PROJECT RENDER)
