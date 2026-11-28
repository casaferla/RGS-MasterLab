add_library(rgsml_dsp STATIC
    src/imodule.cpp
    src/gain_parameters.cpp
    src/gain_module.cpp
    src/dc_offset_correction.cpp
    src/parametric_eq_parameters.cpp
    src/dynamic_eq_parameters.cpp
    src/internal/dynamic_eq_c01_coefficients.cpp
    src/internal/dynamic_eq_c01_engine.cpp
    src/dynamic_eq_module.cpp
    src/transient_shaper_parameters.cpp
    src/transient_shaper_module.cpp
    src/internal/transient_shaper_c00_engine.cpp
    src/internal/transient_shaper_effective_memory.cpp
    src/parametric_eq_module.cpp
    src/parametric_eq_response.cpp
    src/compressor_parameters.cpp
    src/compressor_module.cpp
    src/stereo_ms_parameters.cpp
    src/true_peak_limiter_parameters.cpp
    src/true_peak_limiter_module.cpp
    src/true_peak_limiter_offline_experimental.cpp
    src/stereo_ms_width.cpp
    src/stereo_ms_width_response.cpp
    src/stereo_ms_module.cpp
    src/stereo_ms_crossover.cpp
    src/stereo_ms_crossover_runtime.cpp
    src/internal/parametric_eq_coefficients.cpp
    src/module_descriptor.cpp
    src/module_registry.cpp
    src/processing_chain.cpp
    src/module_parameter_codec.cpp
)

target_include_directories(rgsml_dsp
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        # The isolated M17 experimental selector headers include the frozen
        # internal DSP state, never expose this directory to other layers.
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(rgsml_dsp
    PUBLIC
        rgsml_core
        rgsml_audio
    PRIVATE
        rgsml_vendor_json
)

# The offline candidate follows exactly the protected binary64 policy.
if(MSVC)
    set_source_files_properties(src/true_peak_limiter_offline_experimental.cpp
        PROPERTIES COMPILE_OPTIONS "/fp:strict")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
    set_source_files_properties(src/true_peak_limiter_offline_experimental.cpp
        PROPERTIES COMPILE_OPTIONS "-fno-fast-math;-ffp-contract=off")
endif()

set_property(TARGET rgsml_dsp PROPERTY RGSML_DEPENDENCY_LAYER DSP)
rgsml_apply_project_warnings(rgsml_dsp)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/RgsmlDependencyRules.cmake")
rgsml_assert_target_dependencies(
    TARGET rgsml_dsp
    FORBIDDEN_LAYERS QT QML PLATFORM UI PROJECT RENDER)

# M17 strict-FP arithmetic remains source-scoped. Do not change existing DSP
# numerical compilation policies or the protected M16 analysis implementation.
if(MSVC)
    set_source_files_properties(src/true_peak_limiter_module.cpp
        src/dc_offset_correction.cpp
        src/internal/dynamic_eq_c01_coefficients.cpp
        src/internal/dynamic_eq_c01_engine.cpp
        src/dynamic_eq_module.cpp
        src/internal/transient_shaper_c00_engine.cpp
        src/internal/transient_shaper_effective_memory.cpp
        src/transient_shaper_module.cpp
        PROPERTIES COMPILE_OPTIONS "/fp:strict")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
    set_source_files_properties(src/true_peak_limiter_module.cpp
        src/dc_offset_correction.cpp
        src/internal/dynamic_eq_c01_coefficients.cpp
        src/internal/dynamic_eq_c01_engine.cpp
        src/dynamic_eq_module.cpp
        src/internal/transient_shaper_c00_engine.cpp
        src/internal/transient_shaper_effective_memory.cpp
        src/transient_shaper_module.cpp
        PROPERTIES COMPILE_OPTIONS "-fno-fast-math;-ffp-contract=off")
else()
    message(FATAL_ERROR "M17 True Peak Limiter needs an audited strict-FP compiler")
endif()
