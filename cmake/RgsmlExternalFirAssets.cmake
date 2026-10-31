include_guard(GLOBAL)
# Convert the EXISTING, frozen C++ uint64 data to exact external resources.
# This does not run the MPFR FIR generator or requalify DSP math.
function(rgsml_configure_external_fir_assets)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(_asset_root "${CMAKE_BINARY_DIR}/generated/rgsml/fir/playback")
    file(MAKE_DIRECTORY "${_asset_root}")
    execute_process(
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tools/qualification/playback_src/export_frozen_external_fir.py"
            --source-root "${CMAKE_SOURCE_DIR}"
            --output-root "${_asset_root}"
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err
    )
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR "EXTERNAL_FIR_EXACT_EXPORT_FAILED: ${_out} ${_err}")
    endif()
    set(_names
        playback_src_kernel_44100_48000.bin
        playback_src_kernel_48000_44100.bin
        playback_fir_manifest.json
    )
    set(_files)
    foreach(_name IN LISTS _names)
        list(APPEND _files "${_asset_root}/${_name}")
    endforeach()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/tools/qualification/playback_src/export_frozen_external_fir.py"
        "${CMAKE_SOURCE_DIR}/audio/src/internal/playback_src_kernel_44100_48000.inc"
        "${CMAKE_SOURCE_DIR}/audio/src/internal/playback_src_kernel_48000_44100.inc")
    set(RGSML_PLAYBACK_FIR_BUILD_ROOT "${_asset_root}" PARENT_SCOPE)
    set(RGSML_PLAYBACK_FIR_INSTALL_FILES "${_files}" PARENT_SCOPE)
endfunction()
