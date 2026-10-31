include_guard(GLOBAL)
# Exact external resource extraction is a BUILD dependency, not a CMake
# configuration side effect. No new filter math, MPFR or end-user Python.
function(rgsml_configure_external_fir_assets)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(_asset_root "${CMAKE_BINARY_DIR}/generated/rgsml/fir/playback")
    set(_sources
        "${CMAKE_SOURCE_DIR}/audio/src/internal/playback_src_kernel_44100_48000.inc"
        "${CMAKE_SOURCE_DIR}/audio/src/internal/playback_src_kernel_48000_44100.inc"
        "${CMAKE_SOURCE_DIR}/audio/include/rgsml/audio/internal/phase11_frozen_rgsmsr1_pins.hpp"
        "${CMAKE_SOURCE_DIR}/tools/qualification/playback_src/export_frozen_external_fir.py"
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
            "${CMAKE_SOURCE_DIR}/tools/qualification/playback_src/export_frozen_external_fir.py"
            --source-root "${CMAKE_SOURCE_DIR}"
            --output-root "${_asset_root}"
        DEPENDS ${_sources}
        COMMENT "RGSML exact-frozen 2 playback FIR external asset export / 16 SHA manifest"
        VERBATIM
    )
    add_custom_target(rgsml_external_fir_assets DEPENDS ${_files})
    set(RGSML_PLAYBACK_FIR_BUILD_ROOT "${_asset_root}" PARENT_SCOPE)
    set(RGSML_PLAYBACK_FIR_INSTALL_FILES "${_files}" PARENT_SCOPE)
endfunction()
