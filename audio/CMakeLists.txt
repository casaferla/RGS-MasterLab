add_library(rgsml_audio STATIC
    src/audio_buffer.cpp
    src/audio_format.cpp
    src/playback_sample_rate_adapter.cpp
    private_io/external_playback_fir_assets.cpp
    src/full_resource_output_rate_candidate.cpp
    src/phase11_final_src_candidate.cpp
    src/source_resource.cpp
    src/waveform_summary.cpp
    src/wav_reader.cpp
    src/wav_writer.cpp
)

# Even when Focused CI builds explicit test targets (not ALL), the frozen
# external resources must be built before any audio consumer can execute.
add_dependencies(rgsml_audio rgsml_external_fir_assets)

target_include_directories(rgsml_audio
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(rgsml_audio
    PUBLIC
        rgsml_core
)

set_property(TARGET rgsml_audio PROPERTY RGSML_DEPENDENCY_LAYER AUDIO)
rgsml_apply_project_warnings(rgsml_audio)

if(MSVC)
    target_compile_options(rgsml_audio PRIVATE /fp:strict)
    # The frozen 192-tap playback SRC must sustain real-time output in Debug
    # as well as Release. Optimize only its implementation translation unit;
    # preserve strict floating-point semantics and Debug symbols elsewhere.
    set_property(SOURCE src/playback_sample_rate_adapter.cpp
        APPEND PROPERTY COMPILE_OPTIONS
        "$<$<CONFIG:Debug>:/O2>")
    set_property(SOURCE src/playback_sample_rate_adapter.cpp
        PROPERTY VS_SETTINGS
        "$<$<CONFIG:Debug>:BasicRuntimeChecks=Default>"
        "$<$<CONFIG:Debug>:InlineFunctionExpansion=AnySuitable>")
else()
    target_compile_options(rgsml_audio PRIVATE -fno-fast-math)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/RgsmlDependencyRules.cmake")
rgsml_assert_target_dependencies(TARGET rgsml_audio)

# Phase-11 candidate strict-scalar convolution must not fuse product and
# accumulation: preserves frozen separate IEEE binary64 operations. It is
# not a release admission path and does not alter protected playback SRC.
if(NOT MSVC)
    set_source_files_properties(src/phase11_final_src_candidate.cpp
        PROPERTIES COMPILE_OPTIONS "-fno-fast-math;-ffp-contract=off")
endif()
