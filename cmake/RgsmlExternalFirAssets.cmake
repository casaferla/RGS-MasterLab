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
    set(_asset_root "${CMAKE_BINARY_DIR}/generated/rgsml/fir")
    # The PO authorized distribution of ALL 16 exact source-pinned original
    # FIR binaries. A public repository checkout with no 14-file source ZIP
    # remains safe for Focused development; official FQ is fail-closed.
    if(NOT DEFINED RGSML_ORIGINAL_Q0_SOURCE_ZIP
       OR RGSML_ORIGINAL_Q0_SOURCE_ZIP STREQUAL "")
        set(RGSML_ORIGINAL_Q0_SOURCE_ZIP
            "${_fir_source_root}/resources/fir/phase11_original_14_q0.zip")
    endif()
    # One direct checked-in source file per Q0 coefficient set. The two
    # Playback coefficients remain bit-exactly exported from frozen .inc.
    set(_source_dir "${_fir_source_root}/resources/fir")
    set(_original_names)
    foreach(_ratio
        "1_2" "1_4" "2_1" "4_1" "40_147" "80_147"
        "147_40" "147_80" "147_160" "147_320"
        "147_640" "160_147" "320_147" "640_147")
        string(REPLACE "_" ";" _lm "${_ratio}")
        list(GET _lm 0 _l)
        list(GET _lm 1 _m)
        list(APPEND _original_names "RGSMSR1_L${_l}_M${_m}.bin")
    endforeach()
    set(_direct_source_files)
    foreach(_name IN LISTS _original_names)
        if(EXISTS "${_source_dir}/${_name}")
            list(APPEND _direct_source_files "${_source_dir}/${_name}")
        endif()
    endforeach()
    list(LENGTH _direct_source_files _original_count)
    if(_original_count GREATER 0 AND _original_count LESS 14)
        message(FATAL_ERROR "PARTIAL_ORIGINAL_FIR_SOURCE_IMPORT: ${_original_count}/14")
    endif()
    set(_source_dir_present FALSE)
    if(_original_count EQUAL 14)
        set(_source_dir_present TRUE)
    endif()
    set(_source_zip_present FALSE)
    if(EXISTS "${RGSML_ORIGINAL_Q0_SOURCE_ZIP}")
        set(_source_zip_present TRUE)
    endif()
    set(_source_available FALSE)
    set(_export_archive_args)
    if(_source_dir_present)
        # Preferred exact direct-file distribution, no private historical ZIP.
        set(_source_available TRUE)
        list(APPEND _export_archive_args --originals-dir "${_source_dir}")
    elseif(_source_zip_present)
        # Optional historical recovery route: exact whole-ZIP SHA is checked.
        set(_source_available TRUE)
        list(APPEND _export_archive_args --originals-zip "${RGSML_ORIGINAL_Q0_SOURCE_ZIP}")
    endif()
    if(RGSML_PROVENANCE_TYPE STREQUAL "FQ" AND NOT _source_available)
        message(FATAL_ERROR "FQ_REQUIRES_ALL_16_REAL_FIR_SOURCE_ASSETS")
    endif()
    if(RGSML_PROVENANCE_TYPE STREQUAL "FQ")
        list(APPEND _export_archive_args --require-originals)
    endif()

    set(_sources
        "${_fir_source_root}/audio/src/internal/playback_src_kernel_44100_48000.inc"
        "${_fir_source_root}/audio/src/internal/playback_src_kernel_48000_44100.inc"
        "${_fir_source_root}/audio/include/rgsml/audio/internal/phase11_frozen_rgsmsr1_pins.hpp"
        "${_fir_source_root}/tools/qualification/playback_src/export_frozen_external_fir.py"
        "${_fir_source_root}/tools/qualification/final_src/install_private_q0_originals.py"
    )
    if(_source_dir_present)
        list(APPEND _sources ${_direct_source_files})
    elseif(_source_zip_present)
        list(APPEND _sources "${RGSML_ORIGINAL_Q0_SOURCE_ZIP}")
    endif()
    set(_names
        playback_src_kernel_44100_48000.bin
        playback_src_kernel_48000_44100.bin
        fir_manifest.json
    )
    if(_source_available)
        # All fourteen originals share one resource directory and manifest.
        list(APPEND _names ${_original_names})
    endif()
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
            ${_export_archive_args}
        DEPENDS ${_sources}
        COMMENT "RGSML exact frozen 16-FIR asset catalog: 2 Playback + 14 approved original-Q0 when source available"
        VERBATIM
    )
    add_custom_target(rgsml_external_fir_assets DEPENDS ${_files})
    set(RGSML_PLAYBACK_FIR_BUILD_ROOT "${_asset_root}" PARENT_SCOPE)
    set(RGSML_FIR_INSTALL_FILES "${_files}" PARENT_SCOPE)
    set(RGSML_ALL_16_FIR_PACKAGE_AVAILABLE "${_source_available}" PARENT_SCOPE)
endfunction()
