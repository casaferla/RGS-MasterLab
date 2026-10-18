add_library(rgsml_render STATIC
    src/audible_compressor_telemetry_resolver.cpp
    src/audible_stereo_ms_telemetry_resolver.cpp
    src/compressor_telemetry_collector.cpp
    src/compressor_telemetry_history.cpp
    src/render_preview.cpp
    src/render_request.cpp
    src/true_peak_limiter_terminal_candidate.cpp
    src/true_peak_limiter_terminal_check.cpp
    src/render_result.cpp
    src/stereo_ms_execution_signature.cpp
)

target_include_directories(rgsml_render
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(rgsml_render
    PUBLIC
        rgsml_core
        rgsml_audio
        rgsml_dsp
        rgsml_analysis
)

set_property(TARGET rgsml_render PROPERTY RGSML_DEPENDENCY_LAYER RENDER)
rgsml_apply_project_warnings(rgsml_render)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/RgsmlDependencyRules.cmake")
rgsml_assert_target_dependencies(
    TARGET rgsml_render
    FORBIDDEN_LAYERS QT QML PLATFORM UI PROJECT)

# M17 verification trim uses exact binary64 scalars and frozen M16
# measurement. Keep strict FP scoped to the new candidate implementation;
# never modify protected M16 numerical source flags.
if(MSVC)
    set_source_files_properties(src/true_peak_limiter_terminal_check.cpp
        PROPERTIES COMPILE_OPTIONS "/fp:strict")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
    set_source_files_properties(src/true_peak_limiter_terminal_check.cpp
        PROPERTIES COMPILE_OPTIONS "-fno-fast-math;-ffp-contract=off")
else()
    message(FATAL_ERROR "M17 verification trim requires audited strict FP.")
endif()
