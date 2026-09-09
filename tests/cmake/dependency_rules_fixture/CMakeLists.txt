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

if(FIXTURE_CASE MATCHES "^audio_" OR FIXTURE_CASE STREQUAL "core_negative_audio")
    add_subdirectory("${RGSML_SOURCE_DIR}/audio" rgsml_audio_build)
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

if(FIXTURE_CASE STREQUAL "audio_negative_qt")
    add_library(fixture_forbidden INTERFACE)
    set_property(TARGET fixture_forbidden PROPERTY RGSML_DEPENDENCY_LAYER QT)
    target_link_libraries(rgsml_audio PRIVATE fixture_forbidden)
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
