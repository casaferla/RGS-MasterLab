if(NOT DEFINED CASE OR NOT DEFINED RGSML_SOURCE_DIR OR NOT DEFINED RGSML_TEST_BINARY_DIR)
    message(FATAL_ERROR "DSP dependency-rule test arguments are incomplete")
endif()

if(CASE STREQUAL "dsp_forbidden_public_contract")
    file(GLOB_RECURSE dsp_sources LIST_DIRECTORIES FALSE
        "${RGSML_SOURCE_DIR}/dsp/include/*.hpp"
        "${RGSML_SOURCE_DIR}/dsp/src/*.cpp"
        "${RGSML_SOURCE_DIR}/dsp/src/*.hpp")
    foreach(dsp_source IN LISTS dsp_sources)
        file(STRINGS "${dsp_source}" source_lines)
        foreach(source_line IN LISTS source_lines)
            if(source_line MATCHES "^[ \t]*#[ \t]*include[ \t]*[<\"]([^>\"]+)[>\"]")
                set(included_header "${CMAKE_MATCH_1}")
                if(included_header MATCHES "^(Qt|Q[A-Z]|windows\\.h$|jni\\.h$|filesystem$|fstream$)"
                    OR included_header MATCHES "^rgsml/(app|ui|platform|project|render)/")
                    message(FATAL_ERROR
                        "RGSML_FORBIDDEN_INCLUDE: ${dsp_source} includes ${included_header}")
                endif()
            endif()

            string(REGEX REPLACE "//.*$" "" code_only "${source_line}")
            if(code_only MATCHES "(^|[^A-Za-z0-9_])(QString|QUrl|QAudio|HANDLE|JNIEnv|jobject|std::filesystem)([^A-Za-z0-9_]|$)")
                message(FATAL_ERROR
                    "RGSML_FORBIDDEN_TYPE: ${dsp_source} contains a native boundary type")
            endif()
        endforeach()
    endforeach()
    message(STATUS "RGSML_DSP_FORBIDDEN_PUBLIC_CONTRACT_PASS")
    return()
endif()

set(build_fixture FALSE)
if(CASE STREQUAL "dsp_dependency_graph_allowed")
    set(fixture_case "dsp_graph")
elseif(CASE STREQUAL "dsp_public_headers_standalone")
    set(fixture_case "dsp_headers")
    set(build_fixture TRUE)
elseif(CASE STREQUAL "dsp_dependency_rule_positive_fixture")
    set(fixture_case "dsp_positive")
    set(build_fixture TRUE)
elseif(CASE STREQUAL "dsp_dependency_rule_negative_fixture")
    set(fixture_case "dsp_negative")
else()
    message(FATAL_ERROR "unknown DSP dependency-rule case: ${CASE}")
endif()

if(fixture_case STREQUAL "dsp_negative")
    foreach(negative_case
        dsp_negative_qt
        dsp_negative_qml
        dsp_negative_platform
        dsp_negative_ui
        dsp_negative_project
        dsp_negative_render
        core_negative_dsp
        audio_negative_dsp)
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
                "DSP negative dependency fixture unexpectedly configured: ${negative_case}")
        endif()
        if(negative_case STREQUAL "core_negative_dsp")
            set(expected_diagnostic
                "RGSML_DEPENDENCY_RULE_VIOLATION: target 'rgsml_core' reaches forbidden dependency 'rgsml_dsp' in layer 'DSP'")
        elseif(negative_case STREQUAL "audio_negative_dsp")
            set(expected_diagnostic
                "RGSML_DEPENDENCY_RULE_VIOLATION: target 'rgsml_audio' reaches forbidden dependency 'rgsml_dsp' in layer 'DSP'")
        else()
            string(REGEX REPLACE "^dsp_negative_" "" forbidden_layer "${negative_case}")
            string(TOUPPER "${forbidden_layer}" forbidden_layer)
            set(expected_diagnostic
                "RGSML_DEPENDENCY_RULE_VIOLATION: target 'rgsml_dsp' reaches forbidden dependency 'fixture_forbidden' in layer '${forbidden_layer}'")
        endif()
        set(combined_output "${configure_output}\n${configure_error}")
        string(REGEX REPLACE "[ \t\r\n]+" " " normalized_output "${combined_output}")
        string(FIND "${normalized_output}" "${expected_diagnostic}" diagnostic_position)
        if(diagnostic_position EQUAL -1)
            message(FATAL_ERROR
                "DSP negative fixture missed canonical diagnostic (${negative_case}):\n${combined_output}")
        endif()
    endforeach()
    message(STATUS "RGSML_DSP_DEPENDENCY_RULE_NEGATIVE_PASS")
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
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR
        "DSP dependency fixture configure failed:\n${configure_output}\n${configure_error}")
endif()

if(build_fixture)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${fixture_build_dir}" --config "${TEST_CONFIG}"
        RESULT_VARIABLE build_result
        OUTPUT_VARIABLE build_output
        ERROR_VARIABLE build_error)
    if(NOT build_result EQUAL 0)
        message(FATAL_ERROR
            "DSP dependency fixture build failed:\n${build_output}\n${build_error}")
    endif()
endif()

message(STATUS "RGSML_DSP_DEPENDENCY_RULE_CASE_PASS: ${CASE}")
