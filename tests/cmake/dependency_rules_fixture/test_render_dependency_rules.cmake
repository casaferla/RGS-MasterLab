if(NOT DEFINED CASE OR NOT DEFINED RGSML_SOURCE_DIR OR NOT DEFINED RGSML_TEST_BINARY_DIR)
    message(FATAL_ERROR "Render dependency-rule test arguments are incomplete")
endif()

if(CASE STREQUAL "render_forbidden_public_contract")
    file(GLOB_RECURSE render_sources LIST_DIRECTORIES FALSE
        "${RGSML_SOURCE_DIR}/render/include/*.hpp"
        "${RGSML_SOURCE_DIR}/render/src/*.cpp"
        "${RGSML_SOURCE_DIR}/render/src/*.hpp")
    foreach(render_source IN LISTS render_sources)
        file(STRINGS "${render_source}" source_lines)
        foreach(source_line IN LISTS source_lines)
            if(source_line MATCHES "^[ \t]*#[ \t]*include[ \t]*[<\"]([^>\"]+)[>\"]")
                set(included_header "${CMAKE_MATCH_1}")
                if(included_header MATCHES "^(Qt|Q[A-Z]|windows\\.h$|jni\\.h$|filesystem$|fstream$)"
                    OR included_header MATCHES "^rgsml/(app|ui|platform|project)/")
                    message(FATAL_ERROR
                        "RGSML_FORBIDDEN_INCLUDE: ${render_source} includes ${included_header}")
                endif()
            endif()
            string(REGEX REPLACE "//.*$" "" code_only "${source_line}")
            if(code_only MATCHES "(^|[^A-Za-z0-9_])(QString|QUrl|QAudio|HANDLE|JNIEnv|jobject|std::filesystem)([^A-Za-z0-9_]|$)")
                message(FATAL_ERROR
                    "RGSML_FORBIDDEN_TYPE: ${render_source} contains a native boundary type")
            endif()
        endforeach()
    endforeach()
    message(STATUS "RGSML_RENDER_FORBIDDEN_PUBLIC_CONTRACT_PASS")
    return()
endif()

if(CASE STREQUAL "render_dependency_graph_allowed")
    set(fixture_case "render_graph")
    set(build_fixture FALSE)
elseif(CASE STREQUAL "render_public_headers_standalone")
    set(fixture_case "render_headers")
    set(build_fixture TRUE)
elseif(CASE STREQUAL "render_dependency_rule_positive_fixture")
    set(fixture_case "render_positive")
    set(build_fixture TRUE)
elseif(CASE STREQUAL "render_dependency_rule_negative_fixture")
    foreach(negative_case
        render_negative_qt
        render_negative_qml
        render_negative_platform
        render_negative_ui
        render_negative_project)
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
                "Render negative dependency fixture unexpectedly configured: ${negative_case}")
        endif()
        string(REGEX REPLACE "^render_negative_" "" forbidden_layer "${negative_case}")
        string(TOUPPER "${forbidden_layer}" forbidden_layer)
        set(expected_diagnostic
            "RGSML_DEPENDENCY_RULE_VIOLATION: target 'rgsml_render' reaches forbidden dependency 'fixture_forbidden' in layer '${forbidden_layer}'")
        set(combined_output "${configure_output}\n${configure_error}")
        string(REGEX REPLACE "[ \t\r\n]+" " " normalized_output "${combined_output}")
        string(FIND "${normalized_output}" "${expected_diagnostic}" diagnostic_position)
        if(diagnostic_position EQUAL -1)
            message(FATAL_ERROR
                "Render negative fixture missed canonical diagnostic (${negative_case}):\n${combined_output}")
        endif()
    endforeach()
    message(STATUS "RGSML_RENDER_DEPENDENCY_RULE_NEGATIVE_PASS")
    return()
else()
    message(FATAL_ERROR "Unknown Render dependency-rule case: ${CASE}")
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
        "Render dependency fixture configure failed:\n${configure_output}\n${configure_error}")
endif()
if(build_fixture)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${fixture_build_dir}" --config "${TEST_CONFIG}"
        RESULT_VARIABLE build_result
        OUTPUT_VARIABLE build_output
        ERROR_VARIABLE build_error)
    if(NOT build_result EQUAL 0)
        message(FATAL_ERROR
            "Render dependency fixture build failed:\n${build_output}\n${build_error}")
    endif()
endif()
message(STATUS "RGSML_RENDER_DEPENDENCY_RULE_CASE_PASS: ${CASE}")
