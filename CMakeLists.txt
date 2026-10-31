cmake_minimum_required(VERSION 3.25)

project(RGSMasterLab VERSION 0.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/RgsmlOptions.cmake)
include(cmake/RgsmlWarnings.cmake)
include(cmake/RgsmlToolchainManifest.cmake)

set(RGSML_QT_ROOT "$ENV{RGSML_QT_ROOT}" CACHE PATH "Prefix of the local Qt kit")
if(RGSML_QT_ROOT)
    file(TO_CMAKE_PATH "${RGSML_QT_ROOT}" RGSML_QT_ROOT_CMAKE)
    list(PREPEND CMAKE_PREFIX_PATH "${RGSML_QT_ROOT_CMAKE}")
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS
    Core
    Gui
    Multimedia
    Qml
    Quick
    QuickControls2
    QuickDialogs2
)
if(RGSML_BUILD_TESTS)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
endif()

qt_standard_project_setup(REQUIRES 6.8)

set(RGSML_TOOLCHAIN_MANIFEST
    "${CMAKE_BINARY_DIR}/generated/rgsml/ToolchainManifest.json"
)
rgsml_generate_current_toolchain_manifest("${RGSML_TOOLCHAIN_MANIFEST}")

# Playback FIR coefficients are EXTERNAL application resources. Generate
# only bit-exact frozen copies at configure time, never regenerate filters.
include(cmake/RgsmlExternalFirAssets.cmake)
rgsml_configure_external_fir_assets()

add_subdirectory(core)
add_subdirectory(third_party)
add_subdirectory(project)
add_subdirectory(audio)
add_subdirectory(dsp)
add_subdirectory(analysis)
# B3: add the non-qualifying public DTO adapter OUTSIDE the cryptographically
# locked M16 numerical target definition. The frozen analysis/CMakeLists.txt
# and its strict-FP source list are preserved byte-for-byte by the B2i guard.
target_sources(rgsml_analysis PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/analysis/src/true_peak_measurement.cpp")

# C2D5 chronological True-Peak transport is registered OUTSIDE the frozen
# Gate-G analysis/CMakeLists.txt file, exactly like the B3 public DTO adapter.
# Strict binary64 arithmetic is explicitly scoped to this additional file;
# the protected original FIR source policies remain byte-identical.
set(_rgsml_c2d5_chrono_source
    "${CMAKE_CURRENT_SOURCE_DIR}/analysis/src/true_peak_chronological_buckets.cpp")
target_sources(rgsml_analysis PRIVATE "${_rgsml_c2d5_chrono_source}")
if(MSVC)
    set(_rgsml_c2d5_chrono_strict_fp_options "/fp:strict")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
    set(_rgsml_c2d5_chrono_strict_fp_options "-fno-fast-math;-ffp-contract=off")
else()
    message(FATAL_ERROR
        "M16 C2D5 chronological analysis requires strict-FP compiler support.")
endif()
set_source_files_properties("${_rgsml_c2d5_chrono_source}"
    TARGET_DIRECTORY rgsml_analysis
    PROPERTIES COMPILE_OPTIONS "${_rgsml_c2d5_chrono_strict_fp_options}")
unset(_rgsml_c2d5_chrono_strict_fp_options)
unset(_rgsml_c2d5_chrono_source)
add_subdirectory(render)
add_subdirectory(platform)
add_subdirectory(ui)
add_subdirectory(app)

include(cmake/RgsmlInstallDeploy.cmake)
rgsml_configure_install_deploy(rgsml_app "${RGSML_TOOLCHAIN_MANIFEST}")

if(RGSML_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
    # Separate M17 private-GR proof target, reusing the exact working
    # app-test dependencies without modifying the large frozen test suite.
    include(cmake/RgsmlTerminalPrivateGrTests.cmake)
    include(cmake/RgsmlTerminalProductActivationTests.cmake)
endif()
