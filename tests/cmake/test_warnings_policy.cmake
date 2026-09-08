set(fixture "${RGSML_SOURCE_DIR}/tests/cmake/warnings_policy_fixture")

function(configure_case value case_name result_variable)
    set(binary "${RGSML_TEST_BINARY_DIR}/warnings-policy/${case_name}")
    file(REMOVE_RECURSE "${binary}")
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            -S "${fixture}"
            -B "${binary}"
            -G "${TEST_GENERATOR}"
            -A "${TEST_PLATFORM}"
            "-DRGSML_WARNINGS_AS_ERRORS=${value}"
            "-DRGSML_WARNINGS_MODULE=${RGSML_SOURCE_DIR}/cmake/RgsmlWarnings.cmake"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Warnings fixture configure failed: ${output}\n${error}")
    endif()
    file(READ "${binary}/warnings-result.txt" content)
    set(${result_variable} "${content}" PARENT_SCOPE)
endfunction()

configure_case(ON on on_result)
configure_case(OFF off off_result)

if(TEST_MSVC)
    foreach(flag IN ITEMS "/W4" "/permissive-" "/WX")
        if(NOT on_result MATCHES "project=.*${flag}")
            message(FATAL_ERROR "Warnings-as-errors ON is missing ${flag}: ${on_result}")
        endif()
    endforeach()
    if(off_result MATCHES "project=.*(/WX)")
        message(FATAL_ERROR "Warnings-as-errors OFF unexpectedly contains /WX")
    endif()
    foreach(flag IN ITEMS "/W4" "/permissive-")
        if(NOT off_result MATCHES "project=.*${flag}")
            message(FATAL_ERROR "Warnings-as-errors OFF is missing ${flag}")
        endif()
    endforeach()
else()
    foreach(flag IN ITEMS "-Wall" "-Wextra" "-Wpedantic" "-Werror")
        if(NOT on_result MATCHES "project=.*${flag}")
            message(FATAL_ERROR "Warnings-as-errors ON is missing ${flag}")
        endif()
    endforeach()
    if(off_result MATCHES "project=.*(-Werror)")
        message(FATAL_ERROR "Warnings-as-errors OFF unexpectedly contains -Werror")
    endif()
endif()

foreach(result IN ITEMS "${on_result}" "${off_result}")
    if(NOT result MATCHES "unconfigured=<none>")
        message(FATAL_ERROR "Warnings leaked to an unconfigured fixture target: ${result}")
    endif()
    if(NOT result MATCHES "directory=<none>")
        message(FATAL_ERROR "Warnings policy changed directory-global options: ${result}")
    endif()
    if(result MATCHES "cxx_flags=.*(/WX|-Werror)")
        message(FATAL_ERROR "Warnings policy changed global CXX flags: ${result}")
    endif()
endforeach()
