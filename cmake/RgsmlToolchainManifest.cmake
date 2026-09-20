include_guard(GLOBAL)

function(_rgsml_json_escape input output_variable)
    set(value "${input}")
    string(REPLACE "\\" "\\\\" value "${value}")
    string(REPLACE "\"" "\\\"" value "${value}")
    string(REPLACE "\n" "\\n" value "${value}")
    string(REPLACE "\r" "\\r" value "${value}")
    string(REPLACE "\t" "\\t" value "${value}")
    set(${output_variable} "${value}" PARENT_SCOPE)
endfunction()

function(rgsml_write_toolchain_manifest)
    set(one_value_arguments
        OUTPUT
        SYSTEM_NAME
        ARCHITECTURE
        WINDOWS_SDK_VERSION
        CMAKE_VERSION
        GENERATOR
        GENERATOR_PLATFORM
        COMPILER_ID
        COMPILER_VERSION
        TOOLSET
        QT_VERSION
        GIT_VERSION
    )
    cmake_parse_arguments(ARG "" "${one_value_arguments}" "" ${ARGN})

    foreach(argument IN LISTS one_value_arguments)
        if(NOT DEFINED ARG_${argument})
            message(FATAL_ERROR "Missing ToolchainManifest argument: ${argument}")
        endif()
    endforeach()

    _rgsml_json_escape("${ARG_SYSTEM_NAME}" RGSML_MANIFEST_SYSTEM_NAME)
    _rgsml_json_escape("${ARG_ARCHITECTURE}" RGSML_MANIFEST_ARCHITECTURE)
    _rgsml_json_escape("${ARG_WINDOWS_SDK_VERSION}" RGSML_MANIFEST_WINDOWS_SDK_VERSION)
    _rgsml_json_escape("${ARG_CMAKE_VERSION}" RGSML_MANIFEST_CMAKE_VERSION)
    _rgsml_json_escape("${ARG_GENERATOR}" RGSML_MANIFEST_GENERATOR)
    _rgsml_json_escape("${ARG_GENERATOR_PLATFORM}" RGSML_MANIFEST_GENERATOR_PLATFORM)
    _rgsml_json_escape("${ARG_COMPILER_ID}" RGSML_MANIFEST_COMPILER_ID)
    _rgsml_json_escape("${ARG_COMPILER_VERSION}" RGSML_MANIFEST_COMPILER_VERSION)
    _rgsml_json_escape("${ARG_TOOLSET}" RGSML_MANIFEST_TOOLSET)
    _rgsml_json_escape("${ARG_QT_VERSION}" RGSML_MANIFEST_QT_VERSION)
    _rgsml_json_escape("${ARG_GIT_VERSION}" RGSML_MANIFEST_GIT_VERSION)

    get_filename_component(output_directory "${ARG_OUTPUT}" DIRECTORY)
    file(MAKE_DIRECTORY "${output_directory}")
    configure_file(
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ToolchainManifest.json.in"
        "${ARG_OUTPUT}"
        @ONLY
        NEWLINE_STYLE UNIX
    )
endfunction()

function(rgsml_generate_current_toolchain_manifest output)
    find_package(Git REQUIRED)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" --version
        RESULT_VARIABLE git_result
        OUTPUT_VARIABLE detected_git_version
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_VARIABLE git_error
    )
    if(NOT git_result EQUAL 0)
        message(FATAL_ERROR "Unable to determine Git version: ${git_error}")
    endif()
    string(REGEX REPLACE "^git version " "" detected_git_version "${detected_git_version}")
    set(RGSML_DETECTED_GIT_VERSION "${detected_git_version}" PARENT_SCOPE)

    set(architecture "${CMAKE_GENERATOR_PLATFORM}")
    if(architecture STREQUAL "Win32")
        set(architecture "x86")
    elseif(architecture STREQUAL "ARM64")
        set(architecture "arm64")
    elseif(architecture STREQUAL "")
        set(architecture "${CMAKE_SYSTEM_PROCESSOR}")
    endif()

    set(windows_sdk_version "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
    if(windows_sdk_version STREQUAL "")
        set(windows_sdk_version "not-applicable")
    endif()

    set(generator_platform "${CMAKE_GENERATOR_PLATFORM}")
    if(generator_platform STREQUAL "")
        set(generator_platform "not-applicable")
    endif()

    set(toolset "${CMAKE_VS_PLATFORM_TOOLSET}")
    if(toolset STREQUAL "")
        set(toolset "not-applicable")
    endif()

    rgsml_write_toolchain_manifest(
        OUTPUT "${output}"
        SYSTEM_NAME "${CMAKE_SYSTEM_NAME}"
        ARCHITECTURE "${architecture}"
        WINDOWS_SDK_VERSION "${windows_sdk_version}"
        CMAKE_VERSION "${CMAKE_VERSION}"
        GENERATOR "${CMAKE_GENERATOR}"
        GENERATOR_PLATFORM "${generator_platform}"
        COMPILER_ID "${CMAKE_CXX_COMPILER_ID}"
        COMPILER_VERSION "${CMAKE_CXX_COMPILER_VERSION}"
        TOOLSET "${toolset}"
        QT_VERSION "${Qt6_VERSION}"
        GIT_VERSION "${detected_git_version}"
    )
endfunction()
