cmake_minimum_required(VERSION 3.30)
project(rgsml_dependency_rules_fixture LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED RGSML_SOURCE_DIR OR NOT DEFINED FIXTURE_CASE)
    message(FATAL_ERROR "fixture arguments are incomplete")
endif()

include("${RGSML_SOURCE_DIR}/cmake/RgsmlWarnings.cmake")
include("${RGSML_SOURCE_DIR}/cmake/RgsmlDependencyRules.cmake")
add_subdirectory("${RGSML_SOURCE_DIR}/core" rgsml_core_build)

if(FIXTURE_CASE MATCHES "^(audio_|platform_)"
    OR FIXTURE_CASE STREQUAL "core_negative_audio")
    add_subdirectory("${RGSML_SOURCE_DIR}/audio" rgsml_audio_build)
endif()

if(FIXTURE_CASE MATCHES "^platform_" OR FIXTURE_CASE STREQUAL "audio_negative_platform")
    set(RGSML_QT_ROOT "$ENV{RGSML_QT_ROOT}" CACHE PATH "Qt fixture prefix")
    if(RGSML_QT_ROOT)
        list(PREPEND CMAKE_PREFIX_PATH "${RGSML_QT_ROOT}")
    endif()
    find_package(Qt6 6.8 REQUIRED COMPONENTS Core Multimedia)
    add_subdirectory("${RGSML_SOURCE_DIR}/platform" rgsml_platform_build)
endif()

if(FIXTURE_CASE STREQUAL "audio_negative_multimedia")
    set(RGSML_QT_ROOT "$ENV{RGSML_QT_ROOT}" CACHE PATH "Qt fixture prefix")
    if(RGSML_QT_ROOT)
        list(PREPEND CMAKE_PREFIX_PATH "${RGSML_QT_ROOT}")
    endif()
    find_package(Qt6 6.8 REQUIRED COMPONENTS Core Multimedia)
endif()

if(FIXTURE_CASE STREQUAL "graph")
    rgsml_assert_target_dependencies(TARGET rgsml_core)
    return()
endif()

if(FIXTURE_CASE STREQUAL "headers")
    set(public_headers
        audio_playback_service.hpp
        checked_integer.hpp
        error.hpp
        frame_time.hpp
        processing_state.hpp
        rational.hpp
        resource_io.hpp
        resource_reference.hpp
        result.hpp
        strong_id.hpp
        uuid.hpp)
    foreach(public_header IN LISTS public_headers)
        string(REPLACE "." "_" header_target "standalone_${public_header}")
        set(header_source "${CMAKE_CURRENT_BINARY_DIR}/${header_target}.cpp")
        file(WRITE "${header_source}"
            "#include <rgsml/core/${public_header}>\nint ${header_target}() { return 0; }\n")
        add_library("${header_target}" OBJECT "${header_source}")
        target_link_libraries("${header_target}" PRIVATE rgsml_core)
        rgsml_apply_project_warnings("${header_target}")
    endforeach()
    return()
endif()

if(FIXTURE_CASE STREQUAL "positive")
    add_executable(dependency_consumer allowed_consumer.cpp)
    target_link_libraries(dependency_consumer PRIVATE rgsml_core)
    rgsml_apply_project_warnings(dependency_consumer)
    rgsml_assert_target_dependencies(TARGET dependency_consumer)
    return()
endif()

if(FIXTURE_CASE STREQUAL "audio_graph")
    rgsml_assert_target_dependencies(TARGET rgsml_audio)
    rgsml_assert_target_dependencies(TARGET rgsml_core)
    return()
endif()

if(FIXTURE_CASE STREQUAL "audio_headers")
    set(public_headers
        audio_buffer.hpp
        audio_buffer_view.hpp
        audio_format.hpp
        playback_sample_rate_adapter.hpp
        source_resource.hpp
        wav_format.hpp
        wav_reader.hpp)
    foreach(public_header IN LISTS public_headers)
        string(REPLACE "." "_" header_target "audio_standalone_${public_header}")
        set(header_source "${CMAKE_CURRENT_BINARY_DIR}/${header_target}.cpp")
        file(WRITE "${header_source}"
            "#include <rgsml/audio/${public_header}>\nint ${header_target}() { return 0; }\n")
        add_library("${header_target}" OBJECT "${header_source}")
        target_link_libraries("${header_target}" PRIVATE rgsml_audio)
        rgsml_apply_project_warnings("${header_target}")
    endforeach()
    return()
endif()

if(FIXTURE_CASE STREQUAL "audio_positive")
    add_executable(audio_dependency_consumer audio_allowed_consumer.cpp)
    target_link_libraries(audio_dependency_consumer PRIVATE rgsml_audio)
    rgsml_apply_project_warnings(audio_dependency_consumer)
    rgsml_assert_target_dependencies(TARGET rgsml_audio)
    return()
endif()

if(FIXTURE_CASE STREQUAL "platform_graph")
    rgsml_assert_target_dependencies(
        TARGET rgsml_platform
        FORBIDDEN_LAYERS DSP QML UI)
    return()
endif()

if(FIXTURE_CASE STREQUAL "platform_headers")
    set(header_source "${CMAKE_CURRENT_BINARY_DIR}/platform_standalone_header.cpp")
    file(WRITE "${header_source}"
        "#include <rgsml/platform/windows/windows_resource_reader.hpp>\n#include <rgsml/platform/windows/windows_audio_playback_service.hpp>\nint platform_header() { return 0; }\n")
    add_library(platform_standalone_header OBJECT "${header_source}")
    target_link_libraries(platform_standalone_header PRIVATE rgsml_platform)
    rgsml_apply_project_warnings(platform_standalone_header)
    return()
endif()

if(FIXTURE_CASE STREQUAL "platform_positive")
    add_executable(platform_dependency_consumer platform_allowed_consumer.cpp)
    target_link_libraries(platform_dependency_consumer PRIVATE rgsml_platform)
    rgsml_apply_project_warnings(platform_dependency_consumer)
    rgsml_assert_target_dependencies(
        TARGET rgsml_platform
        FORBIDDEN_LAYERS DSP QML UI)
    return()
endif()

if(FIXTURE_CASE STREQUAL "platform_multimedia_graph")
    get_target_property(platform_links rgsml_platform INTERFACE_LINK_LIBRARIES)
    string(JOIN ";" platform_links_joined ${platform_links})
    foreach(required_link rgsml_audio Qt6::Core Qt6::Multimedia)
        string(FIND "${platform_links_joined}" "${required_link}" link_position)
        if(link_position EQUAL -1)
            message(FATAL_ERROR
                "rgsml_platform is missing required inward link: ${required_link}")
        endif()
    endforeach()
    rgsml_assert_target_dependencies(
        TARGET rgsml_platform
        FORBIDDEN_LAYERS DSP QML UI)
    return()
endif()

if(FIXTURE_CASE STREQUAL "audio_negative_platform")
    target_link_libraries(rgsml_audio PRIVATE rgsml_platform)
    rgsml_assert_target_dependencies(TARGET rgsml_audio)
    return()
endif()

if(FIXTURE_CASE STREQUAL "audio_negative_qt")
    add_library(fixture_forbidden INTERFACE)
    set_property(TARGET fixture_forbidden PROPERTY RGSML_DEPENDENCY_LAYER QT)
    target_link_libraries(rgsml_audio PRIVATE fixture_forbidden)
    rgsml_assert_target_dependencies(TARGET rgsml_audio)
    return()
endif()

if(FIXTURE_CASE STREQUAL "audio_negative_multimedia")
    target_link_libraries(rgsml_audio PRIVATE Qt6::Multimedia)
    rgsml_assert_target_dependencies(TARGET rgsml_audio)
    return()
endif()

if(FIXTURE_CASE STREQUAL "core_negative_audio")
    target_link_libraries(rgsml_core PRIVATE rgsml_audio)
    rgsml_assert_target_dependencies(TARGET rgsml_core)
    return()
endif()

if(FIXTURE_CASE STREQUAL "negative")
    add_library(fixture_forbidden INTERFACE)
    set_property(TARGET fixture_forbidden PROPERTY RGSML_DEPENDENCY_LAYER QT)
    add_library(fixture_bridge INTERFACE)
    target_link_libraries(fixture_bridge INTERFACE fixture_forbidden)
    add_executable(dependency_consumer forbidden_consumer.cpp)
    target_link_libraries(dependency_consumer PRIVATE fixture_bridge rgsml_core)
    rgsml_assert_target_dependencies(TARGET dependency_consumer)
    return()
endif()

message(FATAL_ERROR "unknown FIXTURE_CASE: ${FIXTURE_CASE}")
