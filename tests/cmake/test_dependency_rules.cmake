if(NOT DEFINED CASE OR NOT DEFINED RGSML_SOURCE_DIR OR NOT DEFINED RGSML_TEST_BINARY_DIR)
    message(FATAL_ERROR "dependency-rule test arguments are incomplete")
endif()

if(CASE STREQUAL "core_forbidden_include_contract"
    OR CASE STREQUAL "audio_forbidden_include_contract")
    if(CASE STREQUAL "core_forbidden_include_contract")
        set(layer_name core)
    else()
        set(layer_name audio)
    endif()
    file(GLOB_RECURSE core_sources LIST_DIRECTORIES FALSE
        "${RGSML_SOURCE_DIR}/${layer_name}/include/*.hpp"
        "${RGSML_SOURCE_DIR}/${layer_name}/src/*.cpp")
    foreach(core_source IN LISTS core_sources)
        file(STRINGS "${core_source}" source_lines)
        foreach(source_line IN LISTS source_lines)
            if(source_line MATCHES "^[ \t]*#[ \t]*include[ \t]*[<\"]([^>\"]+)[>\"]")
                set(included_header "${CMAKE_MATCH_1}")
                if(included_header MATCHES "^(Qt|Q[A-Z]|windows\\.h$|jni\\.h$|filesystem$|fstream$)")
                    message(FATAL_ERROR
                        "RGSML_FORBIDDEN_INCLUDE: ${core_source} includes ${included_header}")
                endif()
            endif()

            string(REGEX REPLACE "//.*$" "" code_only "${source_line}")
            if(code_only MATCHES "(^|[^A-Za-z0-9_])(QString|QUrl|QAudio|HANDLE|JNIEnv|jobject|std::filesystem)([^A-Za-z0-9_]|$)")
                message(FATAL_ERROR
                    "RGSML_FORBIDDEN_TYPE: ${core_source} contains a native boundary type")
            endif()
        endforeach()
    endforeach()
    message(STATUS "RGSML_FORBIDDEN_INCLUDE_CONTRACT_PASS")
    return()
endif()

set(fixture_case "graph")
set(expect_failure FALSE)
set(build_fixture FALSE)
if(CASE STREQUAL "core_public_headers_standalone")
    set(fixture_case "headers")
    set(build_fixture TRUE)
elseif(CASE STREQUAL "dependency_rule_negative_fixture")
    set(fixture_case "negative")
    set(expect_failure TRUE)
elseif(CASE STREQUAL "dependency_rule_positive_fixture")
    set(fixture_case "positive")
    set(build_fixture TRUE)
elseif(CASE STREQUAL "audio_dependency_graph_allowed")
    set(fixture_case "audio_graph")
elseif(CASE STREQUAL "audio_public_headers_standalone")
    set(fixture_case "audio_headers")
    set(build_fixture TRUE)
elseif(CASE STREQUAL "audio_dependency_rule_positive_fixture")
    set(fixture_case "audio_positive")
    set(build_fixture TRUE)
elseif(CASE STREQUAL "audio_dependency_rule_negative_fixture")
    set(fixture_case "audio_negative")
    set(expect_failure TRUE)
elseif(NOT CASE STREQUAL "core_dependency_graph_allowed")
    message(FATAL_ERROR "unknown dependency-rule case: ${CASE}")
endif()

if(fixture_case STREQUAL "audio_negative")
    foreach(negative_case audio_negative_qt core_negative_audio)
        set(fixture_build_dir
            "${RGSML_TEST_BINARY_DIR}/dependency-rules-${CASE}-${negative_case}")
        file(REMOVE_RECURSE "${fixture_build_dir}")
        set(configure_command
            "${CMAKE_COMMAND}"
            -S "${RGSML_SOURCE_DIR}/tests/cmake/dependency_rules_fixture"
            -B "${fixture_build_dir}"
            -G "${TEST_GENERATOR}"
            "-DRGSML_SOURCE_DIR=${RGSML_SOURCE_DIR}"
            "-DFIXTURE_CASE=${negative_case}"
            "-DRGSML_WARNINGS_AS_ERRORS=ON")
        if(DEFINED TEST_PLATFORM AND NOT TEST_PLATFORM STREQUAL "")
            list(APPEND configure_command -A "${TEST_PLATFORM}")
        endif()
        execute_process(
            COMMAND ${configure_command}
            RESULT_VARIABLE configure_result
            OUTPUT_VARIABLE configure_output
            ERROR_VARIABLE configure_error)
        if(configure_result EQUAL 0)
            message(FATAL_ERROR
                "audio negative dependency fixture unexpectedly configured: ${negative_case}")
        endif()
        if(negative_case STREQUAL "audio_negative_qt")
            set(expected_diagnostic
                "RGSML_DEPENDENCY_RULE_VIOLATION: target 'rgsml_audio' reaches forbidden dependency 'fixture_forbidden' in layer 'QT'")
        else()
            set(expected_diagnostic
                "RGSML_DEPENDENCY_RULE_VIOLATION: target 'rgsml_core' reaches forbidden dependency 'rgsml_audio' in layer 'AUDIO'")
        endif()
        set(combined_output "${configure_output}\n${configure_error}")
        string(REGEX REPLACE "[ \t\r\n]+" " " normalized_output "${combined_output}")
        string(FIND "${normalized_output}" "${expected_diagnostic}" diagnostic_position)
        if(diagnostic_position EQUAL -1)
            message(FATAL_ERROR
                "audio negative fixture missed canonical diagnostic (${negative_case}):\n${combined_output}")
        endif()
    endforeach()
    message(STATUS "RGSML_AUDIO_DEPENDENCY_RULE_NEGATIVE_PASS")
    return()
endif()

set(fixture_build_dir "${RGSML_TEST_BINARY_DIR}/dependency-rules-${CASE}")
file(REMOVE_RECURSE "${fixture_build_dir}")

set(configure_command
    "${CMAKE_COMMAND}"
    -S "${RGSML_SOURCE_DIR}/tests/cmake/dependency_rules_fixture"
    -B "${fixture_build_dir}"
    -G "${TEST_GENERATOR}"
    "-DRGSML_SOURCE_DIR=${RGSML_SOURCE_DIR}"
    "-DFIXTURE_CASE=${fixture_case}"
    "-DRGSML_WARNINGS_AS_ERRORS=ON")
if(DEFINED TEST_PLATFORM AND NOT TEST_PLATFORM STREQUAL "")
    list(APPEND configure_command -A "${TEST_PLATFORM}")
endif()

execute_process(
    COMMAND ${configure_command}
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error)

set(combined_output "${configure_output}\n${configure_error}")
if(expect_failure)
    if(configure_result EQUAL 0)
        message(FATAL_ERROR "negative dependency fixture unexpectedly configured")
    endif()
    set(expected_diagnostic
        "RGSML_DEPENDENCY_RULE_VIOLATION: target 'dependency_consumer' reaches forbidden dependency 'fixture_forbidden' in layer 'QT'")
    string(REGEX REPLACE "[ \t\r\n]+" " " normalized_output "${combined_output}")
    string(FIND "${normalized_output}" "${expected_diagnostic}" diagnostic_position)
    if(diagnostic_position EQUAL -1)
        message(FATAL_ERROR
            "negative dependency fixture missed canonical diagnostic:\n${combined_output}")
    endif()
    message(STATUS "RGSML_DEPENDENCY_RULE_NEGATIVE_PASS")
    return()
endif()

if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR "dependency fixture configure failed:\n${combined_output}")
endif()

if(build_fixture)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${fixture_build_dir}" --config "${TEST_CONFIG}"
        RESULT_VARIABLE build_result
        OUTPUT_VARIABLE build_output
        ERROR_VARIABLE build_error)
    if(NOT build_result EQUAL 0)
        message(FATAL_ERROR
            "dependency fixture build failed:\n${build_output}\n${build_error}")
    endif()
endif()

message(STATUS "RGSML_DEPENDENCY_RULE_CASE_PASS: ${CASE}")
