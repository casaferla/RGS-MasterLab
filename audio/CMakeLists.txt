add_library(rgsml_audio STATIC
    src/audio_buffer.cpp
    src/audio_format.cpp
    src/playback_sample_rate_adapter.cpp
    src/source_resource.cpp
    src/waveform_summary.cpp
    src/wav_reader.cpp
)

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
else()
    target_compile_options(rgsml_audio PRIVATE -fno-fast-math)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/RgsmlDependencyRules.cmake")
rgsml_assert_target_dependencies(TARGET rgsml_audio)
