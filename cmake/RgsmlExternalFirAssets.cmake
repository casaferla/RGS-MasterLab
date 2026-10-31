include_guard(GLOBAL)
# Exact external resource extraction is a BUILD dependency, not a CMake
# configuration side effect. No new filter math, MPFR or end-user Python.
function(rgsml_configure_external_fir_assets)
    # Standalone CMake dependency fixtures have their own CMAKE_SOURCE_DIR.
    # They must still resolve the real original frozen coefficient sources,
    # and must not bypass the external FIR target in isolated builds.
    if(DEFINED RGSML_EXTERNAL_FIR_SOURCE_ROOT
       AND NOT RGSML_EXTERNAL_FIR_SOURCE_ROOT STREQUAL "")
        set(_fir_source_root "${RGSML_EXTERNAL_FIR_SOURCE_ROOT}")
    else()
        set(_fir_source_root "${CMAKE_SOURCE_DIR}")
    endif()
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(_asset_root "${CMAKE_BINARY_DIR}/generated/rgsml/fir/playback")
    set(_sources
        "${_fir_source_root}/audio/src/internal/playback_src_kernel_44100_48000.inc"
        "${_fir_source_root}/audio/src/internal/playback_src_kernel_48000_44100.inc"
        "${_fir_source_root}/audio/include/rgsml/audio/internal/phase11_frozen_rgsmsr1_pins.hpp"
        "${_fir_source_root}/tools/qualification/playback_src/export_frozen_external_fir.py"
    )
    set(_names
        playback_src_kernel_44100_48000.bin
        playback_src_kernel_48000_44100.bin
        playback_fir_manifest.json
    )
    set(_files)
    foreach(_name IN LISTS _names)
        list(APPEND _files "${_asset_root}/${_name}")
    endforeach()
    add_custom_command(
        OUTPUT ${_files}
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_asset_root}"
        COMMAND "${Python3_EXECUTABLE}"
            "${_fir_source_root}/tools/qualification/playback_src/export_frozen_external_fir.py"
            --source-root "${_fir_source_root}"
            --output-root "${_asset_root}"
        DEPENDS ${_sources}
        COMMENT "RGSML exact-frozen 2 playback FIR external asset export / 16 SHA manifest"
        VERBATIM
    )
    add_custom_target(rgsml_external_fir_assets DEPENDS ${_files})
    set(RGSML_PLAYBACK_FIR_BUILD_ROOT "${_asset_root}" PARENT_SCOPE)
    set(RGSML_PLAYBACK_FIR_INSTALL_FILES "${_files}" PARENT_SCOPE)
endfunction()
