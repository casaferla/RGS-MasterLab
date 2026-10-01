add_library(rgsml_dsp STATIC
    src/gain_parameters.cpp
    src/gain_module.cpp
    src/parametric_eq_parameters.cpp
    src/parametric_eq_module.cpp
    src/parametric_eq_response.cpp
    src/internal/parametric_eq_coefficients.cpp
    src/module_descriptor.cpp
    src/module_registry.cpp
    src/processing_chain.cpp
    src/module_parameter_codec.cpp
)

target_include_directories(rgsml_dsp
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(rgsml_dsp
    PUBLIC
        rgsml_core
        rgsml_audio
    PRIVATE
        rgsml_vendor_json
)

set_property(TARGET rgsml_dsp PROPERTY RGSML_DEPENDENCY_LAYER DSP)
rgsml_apply_project_warnings(rgsml_dsp)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/RgsmlDependencyRules.cmake")
rgsml_assert_target_dependencies(
    TARGET rgsml_dsp
    FORBIDDEN_LAYERS QT QML PLATFORM UI PROJECT RENDER)
