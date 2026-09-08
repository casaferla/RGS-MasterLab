qt_add_executable(rgsml_tests
    test_bootstrap.cpp
    test_core_primitives.cpp
    unit/core/include_checked_integer.cpp
    unit/core/include_error.cpp
    unit/core/include_frame_time.cpp
    unit/core/include_processing_state.cpp
    unit/core/include_rational.cpp
    unit/core/include_result.cpp
    unit/core/include_strong_id.cpp
    unit/core/include_uuid.cpp
)

target_link_libraries(rgsml_tests
    PRIVATE
        rgsml_core
        Qt6::Test
)

rgsml_apply_project_warnings(rgsml_tests)

add_test(NAME rgsml_tests COMMAND rgsml_tests)
set_tests_properties(rgsml_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_test(
    NAME toolchain_manifest_synthetic_contract
    COMMAND
        "${CMAKE_COMMAND}"
        "-DRGSML_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        "-DRGSML_TEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_toolchain_manifest.cmake"
)

add_test(
    NAME toolchain_manifest_current_contract
    COMMAND
        "${CMAKE_COMMAND}"
        "-DMANIFEST=${RGSML_TOOLCHAIN_MANIFEST}"
        "-DEXPECTED_SYSTEM=${CMAKE_SYSTEM_NAME}"
        "-DEXPECTED_ARCHITECTURE=${CMAKE_GENERATOR_PLATFORM}"
        "-DEXPECTED_SDK=${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}"
        "-DEXPECTED_CMAKE=${CMAKE_VERSION}"
        "-DEXPECTED_GENERATOR=${CMAKE_GENERATOR}"
        "-DEXPECTED_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DEXPECTED_COMPILER_ID=${CMAKE_CXX_COMPILER_ID}"
        "-DEXPECTED_COMPILER_VERSION=${CMAKE_CXX_COMPILER_VERSION}"
        "-DEXPECTED_TOOLSET=${CMAKE_VS_PLATFORM_TOOLSET}"
        "-DEXPECTED_QT=${Qt6_VERSION}"
        "-DEXPECTED_GIT=${RGSML_DETECTED_GIT_VERSION}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_current_toolchain_manifest.cmake"
)

add_test(
    NAME warnings_policy_contract
    COMMAND
        "${CMAKE_COMMAND}"
        "-DRGSML_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        "-DRGSML_TEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
        "-DTEST_GENERATOR=${CMAKE_GENERATOR}"
        "-DTEST_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DTEST_MSVC=${MSVC}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_warnings_policy.cmake"
)

add_test(
    NAME install_deploy_smoke
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBINARY_DIR=${CMAKE_BINARY_DIR}"
        "-DBUILD_CONFIG=$<CONFIG>"
        "-DBUILD_MANIFEST=${RGSML_TOOLCHAIN_MANIFEST}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_install_deploy_smoke.cmake"
)
set_tests_properties(install_deploy_smoke PROPERTIES RUN_SERIAL TRUE)
