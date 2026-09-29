add_library(rgsml_platform STATIC
    windows/src/internal/playback_support.cpp
    windows/src/internal/sha256.cpp
    windows/src/transactional_audio_exporter.cpp
    windows/src/windows_resource_writer.cpp
    windows/src/windows_resource_reader.cpp
    windows/src/windows_audio_playback_service.cpp
    windows/src/windows_resource_identity.cpp
)

target_include_directories(rgsml_platform
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/windows/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/windows/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../audio/src/internal
)

target_link_libraries(rgsml_platform
    PUBLIC
        rgsml_core
        rgsml_audio
        rgsml_analysis
    PRIVATE
        Qt6::Core
        Qt6::Multimedia
)

set_property(TARGET rgsml_platform PROPERTY RGSML_DEPENDENCY_LAYER PLATFORM)
rgsml_apply_project_warnings(rgsml_platform)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/RgsmlDependencyRules.cmake")
rgsml_assert_target_dependencies(
    TARGET rgsml_platform
    FORBIDDEN_LAYERS DSP QML UI PROJECT
)
