if(NOT BUILD_CONFIG)
    message(FATAL_ERROR "BUILD_CONFIG is required")
endif()

set(stage "${BINARY_DIR}/stage/L0-M02/${BUILD_CONFIG}")
file(REMOVE_RECURSE "${stage}")
execute_process(
    COMMAND
        "${CMAKE_COMMAND}" --install "${BINARY_DIR}"
        --config "${BUILD_CONFIG}"
        --prefix "${stage}"
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "Staged install failed:\n${install_output}\n${install_error}")
endif()

set(executable "${stage}/bin/RGSMasterLab.exe")
set(installed_manifest "${stage}/share/RGSMasterLab/ToolchainManifest.json")
if(NOT EXISTS "${executable}")
    message(FATAL_ERROR "Staged executable is missing: ${executable}")
endif()
if(BUILD_CONFIG STREQUAL "Debug")
    set(platform_plugin "${stage}/plugins/platforms/qwindowsd.dll")
else()
    set(platform_plugin "${stage}/plugins/platforms/qwindows.dll")
endif()
if(NOT EXISTS "${platform_plugin}")
    message(FATAL_ERROR "Staged Qt Windows platform plugin is missing: ${platform_plugin}")
endif()
if(NOT EXISTS "${installed_manifest}")
    message(FATAL_ERROR "Installed ToolchainManifest is missing")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${BUILD_MANIFEST}" "${installed_manifest}"
    RESULT_VARIABLE compare_result
)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "Build and installed ToolchainManifest files differ")
endif()

execute_process(
    COMMAND "${executable}" --rgsml-deploy-smoke
    RESULT_VARIABLE smoke_result
    OUTPUT_VARIABLE smoke_output
    ERROR_VARIABLE smoke_error
    TIMEOUT 20
)
if(NOT smoke_result EQUAL 0)
    message(FATAL_ERROR "Deploy smoke failed (${smoke_result}):\n${smoke_output}\n${smoke_error}")
endif()
if(smoke_output MATCHES "(qml|QML|warning|Warning)" OR
   smoke_error MATCHES "(qml|QML|warning|Warning)")
    message(FATAL_ERROR "Deploy smoke emitted QML errors or warnings:\n${smoke_output}\n${smoke_error}")
endif()
