if(NOT DEFINED RGSML_SOURCE_DIR OR NOT DEFINED RGSML_TEST_BINARY_DIR)
    message(FATAL_ERROR "Project dependency fixture arguments are incomplete")
endif()

file(GLOB_RECURSE public_headers LIST_DIRECTORIES FALSE
    "${RGSML_SOURCE_DIR}/project/include/*.hpp")
foreach(header IN LISTS public_headers)
    file(STRINGS "${header}" header_lines)
    foreach(line IN LISTS header_lines)
        if(line MATCHES "^[ \t]*#[ \t]*include[ \t]*[<\"]([^>\"]+)[>\"]")
            set(inc "${CMAKE_MATCH_1}")
            if(inc MATCHES "^(Qt|Q[A-Z]|windows\\.h$|nlohmann/|mz\\.|rgsml/(app|ui|platform|audio|dsp|render)/)")
                message(FATAL_ERROR "RGSML_PROJECT_FORBIDDEN_PUBLIC_INCLUDE: ${header}: ${inc}")
            endif()
        endif()
    endforeach()
endforeach()

foreach(fixture_case IN ITEMS project_graph project_headers
    project_negative_qt project_negative_qml project_negative_platform
    project_negative_audio project_negative_dsp project_negative_render
    project_negative_ui)
    set(fixture_dir "${RGSML_TEST_BINARY_DIR}/dependency-rules-${fixture_case}")
    file(REMOVE_RECURSE "${fixture_dir}")
    set(configure_command "${CMAKE_COMMAND}"
        -S "${RGSML_SOURCE_DIR}/tests/cmake/dependency_rules_fixture"
        -B "${fixture_dir}" -G "${TEST_GENERATOR}"
        "-DRGSML_SOURCE_DIR=${RGSML_SOURCE_DIR}"
        "-DFIXTURE_CASE=${fixture_case}"
        "-DRGSML_WARNINGS_AS_ERRORS=ON")
    if(DEFINED TEST_PLATFORM AND NOT TEST_PLATFORM STREQUAL "")
        list(APPEND configure_command -A "${TEST_PLATFORM}")
    endif()
    execute_process(COMMAND ${configure_command}
        RESULT_VARIABLE configure_result
        OUTPUT_VARIABLE configure_output ERROR_VARIABLE configure_error)
    if(fixture_case MATCHES "^project_negative_")
        if(configure_result EQUAL 0)
            message(FATAL_ERROR "Project negative dependency fixture configured: ${fixture_case}")
        endif()
        string(REGEX REPLACE "^project_negative_" "" layer "${fixture_case}")
        string(TOUPPER "${layer}" layer)
        set(expected "RGSML_DEPENDENCY_RULE_VIOLATION: target 'rgsml_project' reaches forbidden dependency 'fixture_forbidden' in layer '${layer}'")
        set(output "${configure_output}\n${configure_error}")
        string(REGEX REPLACE "[ \t\r\n]+" " " output "${output}")
        string(FIND "${output}" "${expected}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "Project negative fixture missed diagnostic (${fixture_case}):\n${configure_output}\n${configure_error}")
        endif()
    else()
        if(NOT configure_result EQUAL 0)
            message(FATAL_ERROR "Project fixture failed (${fixture_case}):\n${configure_output}\n${configure_error}")
        endif()
        if(fixture_case STREQUAL "project_headers")
            execute_process(COMMAND "${CMAKE_COMMAND}" --build "${fixture_dir}"
                --config "${TEST_CONFIG}"
                RESULT_VARIABLE build_result
                OUTPUT_VARIABLE build_output ERROR_VARIABLE build_error)
            if(NOT build_result EQUAL 0)
                message(FATAL_ERROR "Project public headers do not compile:\n${build_output}\n${build_error}")
            endif()
        endif()
    endif()
endforeach()
message(STATUS "RGSML_PROJECT_DEPENDENCY_CONTRACT_PASS")
