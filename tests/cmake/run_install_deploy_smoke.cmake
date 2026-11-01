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
# Installed artifact MUST contain external frozen FIR coefficients.
# Their trusted hash is fixed, not read from the loose manifest.
set(_fir_root "${stage}/bin/resources/fir/playback")
foreach(_pair IN ITEMS 44100_48000 48000_44100)
    set(_asset "${_fir_root}/playback_src_kernel_${_pair}.bin")
    if(NOT EXISTS "${_asset}")
        message(FATAL_ERROR "INSTALLED_EXTERNAL_FIR_MISSING: ${_asset}")
    endif()
    file(SHA256 "${_asset}" _actual)
    if(_pair STREQUAL "44100_48000")
        set(_expected "ec8c4d554eb4ede15f58eead1fede923e74c4692c57832bda716e3fc826926b4")
    else()
        set(_expected "37cfda8e885e2d00646ec60d02bdf5b7dcb85348fca416e065bab71a824efb22")
    endif()
    if(NOT _actual STREQUAL _expected)
        message(FATAL_ERROR "INSTALLED_EXTERNAL_FIR_SHA_MISMATCH: ${_pair}")
    endif()
endforeach()
# PUBLIC FQ installs only 2 immutable Playback binaries but exposes a
# trusted 16-entry SHA-only catalog for private offline provisioning.
# No real private 14-Q0 coefficient binary may leak into public artifacts.
set(_inventory "${_fir_root}/fir_manifest.json")
if(NOT EXISTS "${_inventory}")
    message(FATAL_ERROR "INSTALLED_UNIFIED_FIR_MANIFEST_MISSING")
endif()
file(READ "${_inventory}" _fir_manifest)
string(JSON _manifest_schema GET "${_fir_manifest}" schema)
if(NOT _manifest_schema STREQUAL "rgsml.external-src-fir-assets/2")
    message(FATAL_ERROR "INSTALLED_UNIFIED_FIR_SCHEMA_INVALID")
endif()
string(JSON _manifest_assets LENGTH "${_fir_manifest}" assets)
if(NOT _manifest_assets EQUAL 16)
    message(FATAL_ERROR "INSTALLED_UNIFIED_FIR_INVENTORY_NOT_16")
endif()
string(JSON _manifest_routes LENGTH "${_fir_manifest}" routes)
if(NOT _manifest_routes EQUAL 30)
    message(FATAL_ERROR "INSTALLED_UNIFIED_FIR_ROUTE_MATRIX_NOT_30")
endif()
string(JSON _manifest_release GET "${_fir_manifest}" releaseAuthority)
if(_manifest_release)
    message(FATAL_ERROR "PRIVATE_FIR_RELEASE_AUTHORITY_MUST_REMAIN_FALSE")
endif()
file(GLOB _private_binary_assets "${_fir_root}/RGSMSR1_L*_M*.bin")
if(_private_binary_assets)
    message(FATAL_ERROR "PUBLIC_FQ_MUST_NOT_SHIP_PRIVATE_Q0_BINARY_ASSETS")
endif()

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
