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

qt_add_executable(rgsml_platform_contract_tests
    test_platform_contracts.cpp
)

target_link_libraries(rgsml_platform_contract_tests
    PRIVATE
        rgsml_core
        Qt6::Test
)

rgsml_apply_project_warnings(rgsml_platform_contract_tests)

add_test(NAME rgsml_platform_contract_tests COMMAND rgsml_platform_contract_tests)
set_tests_properties(rgsml_platform_contract_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

foreach(audio_test IN ITEMS format buffer wav_reader)
    if(audio_test STREQUAL "wav_reader")
        set(audio_test_source test_wav_reader.cpp)
    else()
        set(audio_test_source "test_audio_${audio_test}.cpp")
    endif()
    qt_add_executable("rgsml_audio_${audio_test}_tests"
        "${CMAKE_CURRENT_SOURCE_DIR}/unit/audio/${audio_test_source}"
    )
    target_link_libraries("rgsml_audio_${audio_test}_tests"
        PRIVATE
            rgsml_audio
            Qt6::Test
    )
    rgsml_apply_project_warnings("rgsml_audio_${audio_test}_tests")
    add_test(
        NAME "rgsml_audio_${audio_test}_tests"
        COMMAND "rgsml_audio_${audio_test}_tests"
    )
    set_tests_properties("rgsml_audio_${audio_test}_tests" PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        LABELS "audio"
    )
endforeach()
set_tests_properties(rgsml_audio_wav_reader_tests PROPERTIES
    LABELS "audio;audio_golden"
)

qt_add_executable(rgsml_source_resource_tests
    unit/audio/test_source_resource.cpp
)
target_link_libraries(rgsml_source_resource_tests
    PRIVATE
        rgsml_audio
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_source_resource_tests)
add_test(NAME rgsml_source_resource_tests COMMAND rgsml_source_resource_tests)
set_tests_properties(rgsml_source_resource_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "audio;source_resource"
)

qt_add_executable(rgsml_windows_resource_reader_tests
    integration/platform/test_windows_resource_reader.cpp
)
target_link_libraries(rgsml_windows_resource_reader_tests
    PRIVATE
        rgsml_platform
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_windows_resource_reader_tests)
add_test(
    NAME rgsml_windows_resource_reader_tests
    COMMAND rgsml_windows_resource_reader_tests
)
set_tests_properties(rgsml_windows_resource_reader_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "platform"
)

qt_add_executable(rgsml_source_selection_tests
    integration/platform/test_source_selection.cpp
    ${CMAKE_SOURCE_DIR}/app/src/source_selection_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/source_selection_view_model.hpp
)
target_include_directories(rgsml_source_selection_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/app/src
)
target_link_libraries(rgsml_source_selection_tests
    PRIVATE
        rgsml_audio
        rgsml_platform
        Qt6::Core
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_source_selection_tests)
add_test(NAME rgsml_source_selection_tests COMMAND rgsml_source_selection_tests)
set_tests_properties(rgsml_source_selection_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "platform;source_resource"
)

qt_add_executable(rgsml_source_metadata_panel_smoke
    ui_smoke/test_source_metadata_panel.cpp
    ${CMAKE_SOURCE_DIR}/app/src/source_selection_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/source_selection_view_model.hpp
)
target_include_directories(rgsml_source_metadata_panel_smoke
    PRIVATE
        ${CMAKE_SOURCE_DIR}/app/src
)
target_link_libraries(rgsml_source_metadata_panel_smoke
    PRIVATE
        rgsml_audio
        rgsml_platform
        rgsml_ui
        rgsml_uiplugin
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_source_metadata_panel_smoke)
add_test(
    NAME rgsml_source_metadata_panel_smoke
    COMMAND rgsml_source_metadata_panel_smoke
)
set_tests_properties(rgsml_source_metadata_panel_smoke PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;QML2_IMPORT_PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>/../qml"
    LABELS "ui_smoke"
)

foreach(dependency_case
    core_dependency_graph_allowed
    core_public_headers_standalone
    core_forbidden_include_contract
    dependency_rule_negative_fixture
    dependency_rule_positive_fixture
    audio_dependency_graph_allowed
    audio_public_headers_standalone
    audio_forbidden_include_contract
    audio_dependency_rule_negative_fixture
    audio_dependency_rule_positive_fixture
    platform_dependency_graph_allowed
    platform_public_headers_standalone
    platform_forbidden_public_contract
    platform_dependency_rule_negative_fixture
    platform_dependency_rule_positive_fixture)
    add_test(
        NAME "${dependency_case}"
        COMMAND
            "${CMAKE_COMMAND}"
            "-DCASE=${dependency_case}"
            "-DRGSML_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
            "-DRGSML_TEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            "-DTEST_GENERATOR=${CMAKE_GENERATOR}"
            "-DTEST_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
            "-DTEST_CONFIG=$<CONFIG>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_dependency_rules.cmake"
    )
    set_tests_properties("${dependency_case}" PROPERTIES
        RUN_SERIAL TRUE
        LABELS "dependency_contract"
    )
endforeach()

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
