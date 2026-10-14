add_library(rgsml_analysis STATIC
    src/spectrum_config.cpp
    src/hann.cpp
    src/fft.cpp
    src/psd.cpp
    src/log_binning.cpp
    src/smoothing.cpp
    src/spsc_ring.cpp
    src/live_spectrum_analyzer.cpp
    src/true_peak_itu4x_channel_stream.cpp
    src/true_peak_full_resource.cpp
)
# M16 Gate G: preserve source-ordered binary64 FIR products, additions, and
# final dBTP conversion. Do NOT use fast math or implicit FMA contraction.
# Source-file scope is intentional: no unrelated spectrum/FFT DSP behavior
# should change as a result of this compiler policy.
set(_rgsml_true_peak_strict_fp_sources
    src/true_peak_itu4x_channel_stream.cpp
    src/true_peak_full_resource.cpp
)
if(MSVC)
    # MSVC /fp:strict disallows contraction/reassociation and handles
    # IEEE special values correctly (no FP trap is enabled by this setting).
    set(_rgsml_true_peak_strict_fp_options "/fp:strict")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
    set(_rgsml_true_peak_strict_fp_options "-fno-fast-math;-ffp-contract=off")
else()
    message(FATAL_ERROR
        "M16 True-Peak requires an audited strict-FP compiler policy.")
endif()
set_source_files_properties(
    ${_rgsml_true_peak_strict_fp_sources}
    PROPERTIES COMPILE_OPTIONS "${_rgsml_true_peak_strict_fp_options}"
)
unset(_rgsml_true_peak_strict_fp_sources)
unset(_rgsml_true_peak_strict_fp_options)

target_include_directories(rgsml_analysis
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(rgsml_analysis
    PUBLIC
        rgsml_core
        rgsml_audio
)

set_property(TARGET rgsml_analysis PROPERTY RGSML_DEPENDENCY_LAYER ANALYSIS)
rgsml_apply_project_warnings(rgsml_analysis)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/RgsmlDependencyRules.cmake")
rgsml_assert_target_dependencies(
    TARGET rgsml_analysis
    FORBIDDEN_LAYERS QT QML PLATFORM UI PROJECT RENDER)
