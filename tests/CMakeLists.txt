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

qt_add_executable(rgsml_m10_sha_tests spikes/l1_m10/test_sha256.cpp)

qt_add_executable(rgsml_project_value_tests unit/project/test_project_snapshot.cpp)

add_test(NAME rgsml_project_dependency_contract_tests
    COMMAND "${CMAKE_COMMAND}"
        "-DRGSML_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        "-DRGSML_TEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
        "-DTEST_GENERATOR=${CMAKE_GENERATOR}"
        "-DTEST_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DTEST_CONFIG=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependency_rules_fixture/test_project_dependency_rules.cmake")
set_tests_properties(rgsml_project_dependency_contract_tests PROPERTIES
    RUN_SERIAL TRUE LABELS "dependency_contract;project")

qt_add_executable(rgsml_project_json_tests unit/project/test_project_json.cpp)

qt_add_executable(rgsml_project_roundtrip_tests unit/project/test_project_json.cpp)
target_include_directories(rgsml_project_roundtrip_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/project/src/internal)
target_link_libraries(rgsml_project_roundtrip_tests PRIVATE rgsml_project Qt6::Test)
rgsml_apply_project_warnings(rgsml_project_roundtrip_tests)
add_test(NAME rgsml_project_roundtrip_tests COMMAND rgsml_project_roundtrip_tests)
set_tests_properties(rgsml_project_roundtrip_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "project;roundtrip")

qt_add_executable(rgsml_project_package_tests unit/project/test_project_package.cpp)
target_include_directories(rgsml_project_package_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/project/src/internal)

qt_add_executable(rgsml_project_security_tests unit/project/test_project_package.cpp)
target_include_directories(rgsml_project_security_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/project/src/internal)
target_link_libraries(rgsml_project_security_tests PRIVATE rgsml_project Qt6::Test)
rgsml_apply_project_warnings(rgsml_project_security_tests)
add_test(NAME rgsml_project_security_tests COMMAND rgsml_project_security_tests)
set_tests_properties(rgsml_project_security_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "project;security")

qt_add_executable(rgsml_project_session_tests
    integration/app/test_project_session.cpp
    ${CMAKE_SOURCE_DIR}/app/src/project_session_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/project_session_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_chain_state.cpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_chain_state.hpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_preview_controller.cpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_preview_controller.hpp
    ${CMAKE_SOURCE_DIR}/app/src/gain_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/gain_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/eq_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/eq_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/compressor_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/compressor_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/dsp_module_adapter.cpp
    ${CMAKE_SOURCE_DIR}/app/src/dsp_module_adapter.hpp
    ${CMAKE_SOURCE_DIR}/app/src/dsp_chain_adapter_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/dsp_chain_adapter_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/source_selection_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/source_selection_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/gold_selection_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/gold_selection_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/audition_source_selector.cpp
    ${CMAKE_SOURCE_DIR}/app/src/audition_source_selector.hpp
    ${CMAKE_SOURCE_DIR}/app/src/audition_region_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/audition_region_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/playback_transport_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/playback_transport_view_model.hpp)
target_include_directories(rgsml_project_session_tests PRIVATE ${CMAKE_SOURCE_DIR}/app/src)
target_link_libraries(rgsml_project_session_tests PRIVATE
    rgsml_project rgsml_render rgsml_platform rgsml_ui Qt6::Core Qt6::Test)
rgsml_apply_project_warnings(rgsml_project_session_tests)
add_test(NAME rgsml_project_session_tests COMMAND rgsml_project_session_tests)
set_tests_properties(rgsml_project_session_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "project;app")
target_link_libraries(rgsml_project_package_tests PRIVATE rgsml_project Qt6::Test)
rgsml_apply_project_warnings(rgsml_project_package_tests)
add_test(NAME rgsml_project_package_tests COMMAND rgsml_project_package_tests)
set_tests_properties(rgsml_project_package_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "project;package;security")
target_include_directories(rgsml_project_json_tests PRIVATE ${CMAKE_SOURCE_DIR}/project/src/internal)
target_link_libraries(rgsml_project_json_tests PRIVATE rgsml_project Qt6::Test)
rgsml_apply_project_warnings(rgsml_project_json_tests)
add_test(NAME rgsml_project_json_tests COMMAND rgsml_project_json_tests)
set_tests_properties(rgsml_project_json_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "project;json")
target_link_libraries(rgsml_project_value_tests PRIVATE rgsml_project Qt6::Test)
rgsml_apply_project_warnings(rgsml_project_value_tests)
add_test(NAME rgsml_project_value_tests COMMAND rgsml_project_value_tests)
set_tests_properties(rgsml_project_value_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "project;project_value")
target_link_libraries(rgsml_m10_sha_tests PRIVATE rgsml_core Qt6::Test)
rgsml_apply_project_warnings(rgsml_m10_sha_tests)
add_test(NAME rgsml_m10_sha_tests COMMAND rgsml_m10_sha_tests)
set_tests_properties(rgsml_m10_sha_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "l1_m10;sha256")

qt_add_executable(rgsml_m10_json_tests spikes/l1_m10/test_strict_json.cpp)
target_link_libraries(rgsml_m10_json_tests PRIVATE rgsml_vendor_json Qt6::Test)
if(MSVC)
    target_compile_options(rgsml_m10_json_tests PRIVATE /EHsc)
endif()
rgsml_apply_project_warnings(rgsml_m10_json_tests)
add_test(NAME rgsml_m10_json_tests COMMAND rgsml_m10_json_tests)
set_tests_properties(rgsml_m10_json_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "l1_m10;json")

qt_add_executable(rgsml_m10_store_tests spikes/l1_m10/test_store_stream.cpp)
target_link_libraries(rgsml_m10_store_tests PRIVATE rgsml_core MINIZIP::minizip Qt6::Test)
rgsml_apply_project_warnings(rgsml_m10_store_tests)
add_test(NAME rgsml_m10_store_tests COMMAND rgsml_m10_store_tests)
set_tests_properties(rgsml_m10_store_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "l1_m10;minizip;store")

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

qt_add_executable(rgsml_audio_wav_writer_tests
    unit/audio/test_wav_writer.cpp
)

qt_add_executable(rgsml_export_checksum_tests
    unit/platform/test_export_checksums.cpp
)

qt_add_executable(rgsml_transactional_audio_export_tests
    integration/platform/test_transactional_audio_exporter.cpp
)
target_include_directories(rgsml_transactional_audio_export_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/platform/windows/src
)
target_compile_definitions(rgsml_transactional_audio_export_tests
    PRIVATE
        RGSML_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
target_link_libraries(rgsml_transactional_audio_export_tests
    PRIVATE
        rgsml_render
        rgsml_platform
        Qt6::Core
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_transactional_audio_export_tests)
add_test(NAME rgsml_transactional_audio_export_tests COMMAND rgsml_transactional_audio_export_tests)
set_tests_properties(rgsml_transactional_audio_export_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "audio;audio_golden;render;platform;export;transaction"
)
target_include_directories(rgsml_export_checksum_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/platform/windows/src
)
target_link_libraries(rgsml_export_checksum_tests
    PRIVATE
        rgsml_platform
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_export_checksum_tests)
add_test(NAME rgsml_export_checksum_tests COMMAND rgsml_export_checksum_tests)
set_tests_properties(rgsml_export_checksum_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "audio;platform;checksum;export"
)
target_link_libraries(rgsml_audio_wav_writer_tests
    PRIVATE
        rgsml_audio
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_audio_wav_writer_tests)
add_test(NAME rgsml_audio_wav_writer_tests COMMAND rgsml_audio_wav_writer_tests)
set_tests_properties(rgsml_audio_wav_writer_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "audio;audio_golden;wav_writer;export"
)

qt_add_executable(rgsml_waveform_summary_tests
    unit/audio/test_waveform_summary.cpp
)
target_include_directories(rgsml_waveform_summary_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/audio/src
)
target_link_libraries(rgsml_waveform_summary_tests
    PRIVATE
        rgsml_audio
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_waveform_summary_tests)
add_test(NAME rgsml_waveform_summary_tests COMMAND rgsml_waveform_summary_tests)
set_tests_properties(rgsml_waveform_summary_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "audio;waveform"
)

qt_add_executable(rgsml_waveform_wav_tests
    integration/audio/test_waveform_wav.cpp
)
target_link_libraries(rgsml_waveform_wav_tests
    PRIVATE
        rgsml_audio
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_waveform_wav_tests)
add_test(NAME rgsml_waveform_wav_tests COMMAND rgsml_waveform_wav_tests)
set_tests_properties(rgsml_waveform_wav_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "audio;audio_golden;waveform;waveform_golden;source_resource"
)

qt_add_executable(rgsml_playback_src_tests
    unit/audio/test_playback_sample_rate_adapter.cpp
)
target_link_libraries(rgsml_playback_src_tests
    PRIVATE
        rgsml_audio
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_playback_src_tests)
add_test(NAME rgsml_playback_src_tests COMMAND rgsml_playback_src_tests)
set_tests_properties(rgsml_playback_src_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "audio;audio_golden;playback;playback_src"
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
    ${CMAKE_SOURCE_DIR}/app/src/playback_transport_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/playback_transport_view_model.hpp
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
    ${CMAKE_SOURCE_DIR}/app/src/project_session_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/project_session_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/audition_source_selector.cpp
    ${CMAKE_SOURCE_DIR}/app/src/audition_source_selector.hpp
    ${CMAKE_SOURCE_DIR}/app/src/audition_region_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/audition_region_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/gold_selection_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/gold_selection_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/live_spectrum_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/live_spectrum_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/playback_transport_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/playback_transport_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/source_selection_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/source_selection_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/eq_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/eq_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/gain_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/gain_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/compressor_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/compressor_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/dsp_module_adapter.cpp
    ${CMAKE_SOURCE_DIR}/app/src/dsp_module_adapter.hpp
    ${CMAKE_SOURCE_DIR}/app/src/dsp_chain_adapter_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/dsp_chain_adapter_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_chain_state.cpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_chain_state.hpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_preview_controller.cpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_preview_controller.hpp
    ${CMAKE_SOURCE_DIR}/app/src/windows_window_chrome_helper.cpp
    ${CMAKE_SOURCE_DIR}/app/src/windows_window_chrome_helper.hpp
)
target_include_directories(rgsml_source_metadata_panel_smoke
    PRIVATE
        ${CMAKE_SOURCE_DIR}/app/src
)
target_link_libraries(rgsml_source_metadata_panel_smoke
    PRIVATE
        rgsml_audio
        rgsml_analysis
        rgsml_platform
        rgsml_project
        rgsml_render
        rgsml_ui
        rgsml_uiplugin
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_source_metadata_panel_smoke)
if(MSVC)
    target_compile_options(rgsml_source_metadata_panel_smoke PRIVATE /wd4702)
endif()
add_test(
    NAME rgsml_source_metadata_panel_smoke
    COMMAND rgsml_source_metadata_panel_smoke
)
set_tests_properties(rgsml_source_metadata_panel_smoke PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;QML2_IMPORT_PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>/../qml"
    LABELS "ui_smoke;waveform_navigation;audition_region;playback;source_resource"
)

qt_add_executable(rgsml_playback_support_tests
    unit/platform/test_playback_support.cpp
)

qt_add_executable(rgsml_audition_source_selector_tests
    unit/app/test_audition_source_selector.cpp
    ${CMAKE_SOURCE_DIR}/app/src/audition_source_selector.cpp
    ${CMAKE_SOURCE_DIR}/app/src/audition_source_selector.hpp
    ${CMAKE_SOURCE_DIR}/app/src/audition_region_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/audition_region_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/gold_selection_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/gold_selection_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/playback_transport_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/playback_transport_view_model.hpp
)
target_include_directories(rgsml_audition_source_selector_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/app/src
)
target_link_libraries(rgsml_audition_source_selector_tests
    PRIVATE
        rgsml_render
        rgsml_platform
        Qt6::Core
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_audition_source_selector_tests)
add_test(
    NAME rgsml_audition_source_selector_tests
    COMMAND rgsml_audition_source_selector_tests
)
set_tests_properties(rgsml_audition_source_selector_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "app;gold_audition;playback;render"
)

qt_add_executable(rgsml_windows_resource_identity_tests
    integration/platform/test_windows_resource_identity.cpp
)
target_link_libraries(rgsml_windows_resource_identity_tests
    PRIVATE
        rgsml_platform
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_windows_resource_identity_tests)
add_test(
    NAME rgsml_windows_resource_identity_tests
    COMMAND rgsml_windows_resource_identity_tests
)
set_tests_properties(rgsml_windows_resource_identity_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "platform;resource_identity;gold_audition"
)

qt_add_executable(rgsml_pcm_playback_tests
    integration/platform/test_pcm_playback.cpp
)
target_include_directories(rgsml_pcm_playback_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/platform/windows/src/internal
)
target_link_libraries(rgsml_pcm_playback_tests
    PRIVATE
        rgsml_audio
        rgsml_platform
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_pcm_playback_tests)
add_test(NAME rgsml_pcm_playback_tests COMMAND rgsml_pcm_playback_tests)
set_tests_properties(rgsml_pcm_playback_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "platform;playback;gold_audition"
)
target_include_directories(rgsml_playback_support_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/platform/windows/src/internal
)
target_link_libraries(rgsml_playback_support_tests
    PRIVATE
        rgsml_audio
        rgsml_platform
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_playback_support_tests)
add_test(NAME rgsml_playback_support_tests COMMAND rgsml_playback_support_tests)
set_tests_properties(rgsml_playback_support_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "platform;playback"
)

qt_add_executable(rgsml_playback_composition_tests
    integration/platform/test_playback_composition.cpp
    ${CMAKE_SOURCE_DIR}/app/src/playback_transport_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/playback_transport_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/source_selection_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/source_selection_view_model.hpp
)
target_include_directories(rgsml_playback_composition_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/app/src
)
target_link_libraries(rgsml_playback_composition_tests
    PRIVATE
        rgsml_audio
        rgsml_platform
        Qt6::Core
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_playback_composition_tests)
add_test(
    NAME rgsml_playback_composition_tests
    COMMAND rgsml_playback_composition_tests
)
set_tests_properties(rgsml_playback_composition_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "platform;source_resource;playback"
)

qt_add_executable(rgsml_waveform_geometry_tests
    unit/ui/test_waveform_geometry.cpp
)
target_include_directories(rgsml_waveform_geometry_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/ui/src
)
target_link_libraries(rgsml_waveform_geometry_tests
    PRIVATE
        rgsml_audio
        rgsml_ui
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_waveform_geometry_tests)
add_test(NAME rgsml_waveform_geometry_tests COMMAND rgsml_waveform_geometry_tests)
set_tests_properties(rgsml_waveform_geometry_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "waveform;waveform_navigation;ui_smoke"
)

qt_add_executable(rgsml_waveform_viewport_tests
    unit/ui/test_waveform_viewport.cpp
)
target_include_directories(rgsml_waveform_viewport_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/ui/src
)
target_link_libraries(rgsml_waveform_viewport_tests
    PRIVATE
        rgsml_audio
        rgsml_ui
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_waveform_viewport_tests)
add_test(NAME rgsml_waveform_viewport_tests COMMAND rgsml_waveform_viewport_tests)
set_tests_properties(rgsml_waveform_viewport_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "waveform;waveform_navigation"
)

qt_add_executable(rgsml_waveform_interaction_tests
    unit/ui/test_waveform_item_interaction.cpp
)
target_include_directories(rgsml_waveform_interaction_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/ui/src
)
target_link_libraries(rgsml_waveform_interaction_tests
    PRIVATE
        rgsml_audio
        rgsml_ui
        rgsml_uiplugin
        Qt6::Gui
        Qt6::Quick
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_waveform_interaction_tests)
add_test(NAME rgsml_waveform_interaction_tests COMMAND rgsml_waveform_interaction_tests)
set_tests_properties(rgsml_waveform_interaction_tests PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;QML2_IMPORT_PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>/../qml"
    LABELS "waveform;waveform_navigation;audition_region;ui_smoke"
)

qt_add_executable(rgsml_audition_region_tests
    integration/app/test_audition_region_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/audition_region_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/audition_region_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/playback_transport_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/playback_transport_view_model.hpp
)
target_include_directories(rgsml_audition_region_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/app/src
        ${CMAKE_SOURCE_DIR}/ui/src
)
target_link_libraries(rgsml_audition_region_tests
    PRIVATE
        rgsml_core
        rgsml_ui
        Qt6::Core
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_audition_region_tests)
add_test(NAME rgsml_audition_region_tests COMMAND rgsml_audition_region_tests)
set_tests_properties(rgsml_audition_region_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "audition_region;playback;waveform_navigation"
)

qt_add_executable(rgsml_source_waveform_view_model_tests
    integration/app/test_source_waveform_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/playback_transport_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/playback_transport_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/source_waveform_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/source_waveform_view_model.hpp
)
target_include_directories(rgsml_source_waveform_view_model_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/app/src
        ${CMAKE_SOURCE_DIR}/ui/src
)
target_link_libraries(rgsml_source_waveform_view_model_tests
    PRIVATE
        rgsml_audio
        rgsml_platform
        rgsml_ui
        Qt6::Core
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_source_waveform_view_model_tests)
add_test(
    NAME rgsml_source_waveform_view_model_tests
    COMMAND rgsml_source_waveform_view_model_tests
)
set_tests_properties(rgsml_source_waveform_view_model_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "waveform;source_resource;playback"
)

qt_add_executable(rgsml_playback_device_probe
    integration/platform/test_playback_device_probe.cpp
)
target_link_libraries(rgsml_playback_device_probe
    PRIVATE
        Qt6::Core
        Qt6::Multimedia
)
rgsml_apply_project_warnings(rgsml_playback_device_probe)
add_test(NAME rgsml_playback_device_probe COMMAND rgsml_playback_device_probe)
set_tests_properties(rgsml_playback_device_probe PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Multimedia>"
    LABELS "playback_device"
)

foreach(dsp_test IN ITEMS
    module_descriptor
    module_registry
    processing_chain
    structural_integration)
    qt_add_executable("rgsml_dsp_${dsp_test}_tests"
        "${CMAKE_CURRENT_SOURCE_DIR}/unit/dsp/test_${dsp_test}.cpp"
    )
    target_include_directories("rgsml_dsp_${dsp_test}_tests"
        PRIVATE
            ${CMAKE_SOURCE_DIR}/dsp/src
    )
    target_link_libraries("rgsml_dsp_${dsp_test}_tests"
        PRIVATE
            rgsml_dsp
            Qt6::Test
    )
    rgsml_apply_project_warnings("rgsml_dsp_${dsp_test}_tests")
    add_test(
        NAME "rgsml_dsp_${dsp_test}_tests"
        COMMAND "rgsml_dsp_${dsp_test}_tests"
    )
    set(dsp_test_labels "dsp")
    if(dsp_test STREQUAL "module_registry")
        list(APPEND dsp_test_labels module_registry)
    elseif(dsp_test STREQUAL "processing_chain")
        list(APPEND dsp_test_labels processing_chain)
    elseif(dsp_test STREQUAL "structural_integration")
        list(APPEND dsp_test_labels module_registry processing_chain)
    endif()
    set_tests_properties("rgsml_dsp_${dsp_test}_tests" PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        LABELS "${dsp_test_labels}"
    )
endforeach()

qt_add_executable(rgsml_stereo_ms_crossover_runtime_tests
    unit/dsp/test_stereo_ms_crossover_runtime.cpp
)
target_link_libraries(rgsml_stereo_ms_crossover_runtime_tests
    PRIVATE
        rgsml_dsp
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_stereo_ms_crossover_runtime_tests)
add_test(NAME rgsml_stereo_ms_crossover_runtime_tests
    COMMAND rgsml_stereo_ms_crossover_runtime_tests)
set_tests_properties(rgsml_stereo_ms_crossover_runtime_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "dsp;stereo_ms;lr12;lr24;recursive"
)

qt_add_executable(rgsml_stereo_ms_crossover_tests
    unit/dsp/test_stereo_ms_crossover.cpp
)
target_link_libraries(rgsml_stereo_ms_crossover_tests
    PRIVATE
        rgsml_dsp
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_stereo_ms_crossover_tests)
add_test(NAME rgsml_stereo_ms_crossover_tests
    COMMAND rgsml_stereo_ms_crossover_tests)
set_tests_properties(rgsml_stereo_ms_crossover_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "dsp;stereo_ms;lr12;lr24;coefficients"
)

qt_add_executable(rgsml_stereo_ms_module_tests
    unit/dsp/test_stereo_ms_module.cpp
)
target_link_libraries(rgsml_stereo_ms_module_tests
    PRIVATE
        rgsml_dsp
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_stereo_ms_module_tests)
add_test(NAME rgsml_stereo_ms_module_tests
    COMMAND rgsml_stereo_ms_module_tests)
set_tests_properties(rgsml_stereo_ms_module_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "dsp;stereo_ms;runtime"
)

qt_add_executable(rgsml_stereo_ms_parameters_tests
    unit/dsp/test_stereo_ms_parameters.cpp
)
target_link_libraries(rgsml_stereo_ms_parameters_tests
    PRIVATE
        rgsml_dsp
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_stereo_ms_parameters_tests)
add_test(NAME rgsml_stereo_ms_parameters_tests
    COMMAND rgsml_stereo_ms_parameters_tests)
set_tests_properties(rgsml_stereo_ms_parameters_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "dsp;stereo_ms;parameters"
)

qt_add_executable(rgsml_gain_tests
    unit/dsp/test_gain.cpp
)
target_link_libraries(rgsml_gain_tests
    PRIVATE
        rgsml_dsp
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_gain_tests)
add_test(NAME rgsml_gain_tests COMMAND rgsml_gain_tests)
set_tests_properties(rgsml_gain_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "dsp;gain"
)

qt_add_executable(rgsml_compressor_tests
    unit/dsp/test_compressor.cpp
)
target_link_libraries(rgsml_compressor_tests
    PRIVATE
        rgsml_dsp
        rgsml_render
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_compressor_tests)
add_test(NAME rgsml_compressor_tests COMMAND rgsml_compressor_tests)
set_tests_properties(rgsml_compressor_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "dsp;compressor"
)

qt_add_executable(rgsml_compressor_gate_g_tests
    unit/dsp/test_compressor_gate_g.cpp
)
target_include_directories(rgsml_compressor_gate_g_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/oracles/compressor)
target_link_libraries(rgsml_compressor_gate_g_tests
    PRIVATE
        rgsml_dsp
        rgsml_render
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_compressor_gate_g_tests)
add_test(NAME rgsml_compressor_gate_g_tests COMMAND rgsml_compressor_gate_g_tests)
set_tests_properties(rgsml_compressor_gate_g_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "dsp;compressor;oracle;gate_g"
)

qt_add_executable(rgsml_compressor_no_alloc_tests
    unit/dsp/test_compressor_no_alloc.cpp
)
target_link_libraries(rgsml_compressor_no_alloc_tests
    PRIVATE
        rgsml_dsp
        rgsml_render
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_compressor_no_alloc_tests)
add_test(NAME rgsml_compressor_no_alloc_tests COMMAND rgsml_compressor_no_alloc_tests)
set_tests_properties(rgsml_compressor_no_alloc_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "dsp;compressor;no_alloc"
)

qt_add_executable(rgsml_parametric_eq_tests
    unit/dsp/test_parametric_eq.cpp
)
target_link_libraries(rgsml_parametric_eq_tests
    PRIVATE
        rgsml_dsp
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_parametric_eq_tests)
add_test(NAME rgsml_parametric_eq_tests COMMAND rgsml_parametric_eq_tests)
set_tests_properties(rgsml_parametric_eq_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "dsp;parametric_eq"
)

qt_add_executable(rgsml_parametric_eq_response_tests
    unit/dsp/test_parametric_eq_response.cpp
)
target_link_libraries(rgsml_parametric_eq_response_tests
    PRIVATE
        rgsml_dsp
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_parametric_eq_response_tests)
add_test(NAME rgsml_parametric_eq_response_tests COMMAND rgsml_parametric_eq_response_tests)
set_tests_properties(rgsml_parametric_eq_response_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "dsp;parametric_eq;response"
)

qt_add_executable(rgsml_module_parameter_codec_tests
    unit/dsp/test_module_parameter_codec.cpp
)
target_link_libraries(rgsml_module_parameter_codec_tests
    PRIVATE
        rgsml_dsp
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_module_parameter_codec_tests)
add_test(NAME rgsml_module_parameter_codec_tests COMMAND rgsml_module_parameter_codec_tests)
set_tests_properties(rgsml_module_parameter_codec_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "dsp;codec"
)

qt_add_executable(rgsml_mastering_chain_state_tests
    unit/app/test_mastering_chain_state.cpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_chain_state.cpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_chain_state.hpp
)
target_include_directories(rgsml_mastering_chain_state_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/app/src
)
target_link_libraries(rgsml_mastering_chain_state_tests
    PRIVATE
        rgsml_dsp
        rgsml_core
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_mastering_chain_state_tests)
add_test(NAME rgsml_mastering_chain_state_tests COMMAND rgsml_mastering_chain_state_tests)
set_tests_properties(rgsml_mastering_chain_state_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "app;mastering_chain"
)

qt_add_executable(rgsml_eq_view_model_tests
    unit/app/test_eq_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/eq_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/eq_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_chain_state.cpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_chain_state.hpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_preview_controller.cpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_preview_controller.hpp
)
target_include_directories(rgsml_eq_view_model_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/app/src
)
target_link_libraries(rgsml_eq_view_model_tests
    PRIVATE
        rgsml_render
        rgsml_dsp
        rgsml_audio
        rgsml_core
        Qt6::Core
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_eq_view_model_tests)
add_test(NAME rgsml_eq_view_model_tests COMMAND rgsml_eq_view_model_tests)
set_tests_properties(rgsml_eq_view_model_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "app;parametric_eq;view_model;render"
)

qt_add_executable(rgsml_gain_view_model_tests
    unit/app/test_gain_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/gain_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/gain_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_chain_state.cpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_chain_state.hpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_preview_controller.cpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_preview_controller.hpp
)
target_include_directories(rgsml_gain_view_model_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/app/src
)
target_link_libraries(rgsml_gain_view_model_tests
    PRIVATE
        rgsml_render
        rgsml_dsp
        rgsml_audio
        rgsml_core
        Qt6::Core
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_gain_view_model_tests)
add_test(NAME rgsml_gain_view_model_tests COMMAND rgsml_gain_view_model_tests)
set_tests_properties(rgsml_gain_view_model_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "app;gain;view_model;render"
)

qt_add_executable(rgsml_mastering_preview_controller_tests
    unit/app/test_mastering_preview_controller.cpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_chain_state.cpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_chain_state.hpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_preview_controller.cpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_preview_controller.hpp
)
target_include_directories(rgsml_mastering_preview_controller_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/app/src
)
target_link_libraries(rgsml_mastering_preview_controller_tests
    PRIVATE
        rgsml_render
        rgsml_dsp
        rgsml_audio
        rgsml_core
        Qt6::Core
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_mastering_preview_controller_tests)
add_test(NAME rgsml_mastering_preview_controller_tests COMMAND rgsml_mastering_preview_controller_tests)
set_tests_properties(rgsml_mastering_preview_controller_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "app;mastering_chain;render"
)

qt_add_executable(rgsml_render_preview_tests
    unit/render/test_render_preview.cpp
)
target_link_libraries(rgsml_render_preview_tests
    PRIVATE
        rgsml_render
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_render_preview_tests)
add_test(NAME rgsml_render_preview_tests COMMAND rgsml_render_preview_tests)
set_tests_properties(rgsml_render_preview_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "dsp;gain;render"
)

qt_add_executable(rgsml_gain_golden_tests
    audio_golden/gain/test_gain_golden.cpp
)
target_link_libraries(rgsml_gain_golden_tests
    PRIVATE
        rgsml_dsp
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_gain_golden_tests)
add_test(NAME rgsml_gain_golden_tests COMMAND rgsml_gain_golden_tests)
set_tests_properties(rgsml_gain_golden_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "dsp;gain;audio_golden"
)

qt_add_executable(rgsml_parametric_eq_golden_tests
    audio_golden/parametric_eq/test_parametric_eq_golden.cpp
)
target_link_libraries(rgsml_parametric_eq_golden_tests
    PRIVATE
        rgsml_dsp
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_parametric_eq_golden_tests)
add_test(NAME rgsml_parametric_eq_golden_tests COMMAND rgsml_parametric_eq_golden_tests)
set_tests_properties(rgsml_parametric_eq_golden_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "dsp;parametric_eq;audio_golden"
)

qt_add_executable(rgsml_render_wav_tests
    integration/render/test_render_wav.cpp
)
target_link_libraries(rgsml_render_wav_tests
    PRIVATE
        rgsml_render
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_render_wav_tests)
add_test(NAME rgsml_render_wav_tests COMMAND rgsml_render_wav_tests)
set_tests_properties(rgsml_render_wav_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "audio;audio_golden;dsp;gain;render"
)

if(WIN32)
    qt_add_executable(rgsml_export_functional_gate
        manual/export/export_functional_gate.cpp
    )
    target_include_directories(rgsml_export_functional_gate
        PRIVATE
            ${CMAKE_SOURCE_DIR}/platform/windows/src
    )
    target_link_libraries(rgsml_export_functional_gate
        PRIVATE
            rgsml_render
            rgsml_platform
            Qt6::Core
    )
    set_target_properties(rgsml_export_functional_gate PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
    rgsml_apply_project_warnings(rgsml_export_functional_gate)

    qt_add_executable(rgsml_gain_listening_gate
        manual/gain_preview/gain_listening_gate.cpp
    )
    target_link_libraries(rgsml_gain_listening_gate
        PRIVATE
            rgsml_render
            rgsml_platform
            Qt6::Core
            Qt6::Multimedia
    )
    set_target_properties(rgsml_gain_listening_gate PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
    rgsml_apply_project_warnings(rgsml_gain_listening_gate)

    qt_add_executable(rgsml_compressor_listening_gate
        manual/compressor_preview/compressor_listening_gate.cpp
    )
    target_link_libraries(rgsml_compressor_listening_gate
        PRIVATE
            rgsml_render
            rgsml_platform
            Qt6::Core
            Qt6::Multimedia
    )
    set_target_properties(rgsml_compressor_listening_gate PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
    rgsml_apply_project_warnings(rgsml_compressor_listening_gate)
endif()

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
    platform_dependency_rule_positive_fixture
    platform_multimedia_link_contract
    audio_multimedia_dependency_rule_negative_fixture)
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

foreach(dependency_case
    render_dependency_graph_allowed
    render_public_headers_standalone
    render_forbidden_public_contract
    render_dependency_rule_negative_fixture
    render_dependency_rule_positive_fixture)
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
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependency_rules_fixture/test_render_dependency_rules.cmake"
    )
    set_tests_properties("${dependency_case}" PROPERTIES
        RUN_SERIAL TRUE
        LABELS "dependency_contract;render"
    )
endforeach()

foreach(dependency_case
    dsp_dependency_graph_allowed
    dsp_public_headers_standalone
    dsp_forbidden_public_contract
    dsp_dependency_rule_negative_fixture
    dsp_dependency_rule_positive_fixture)
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
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependency_rules_fixture/test_dsp_dependency_rules.cmake"
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

set(expected_manifest_architecture "${CMAKE_GENERATOR_PLATFORM}")
if(expected_manifest_architecture STREQUAL "Win32")
    set(expected_manifest_architecture "x86")
elseif(expected_manifest_architecture STREQUAL "ARM64")
    set(expected_manifest_architecture "arm64")
elseif(expected_manifest_architecture STREQUAL "")
    set(expected_manifest_architecture "${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(expected_manifest_windows_sdk "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
if(expected_manifest_windows_sdk STREQUAL "")
    set(expected_manifest_windows_sdk "not-applicable")
endif()

set(expected_manifest_generator_platform "${CMAKE_GENERATOR_PLATFORM}")
if(expected_manifest_generator_platform STREQUAL "")
    set(expected_manifest_generator_platform "not-applicable")
endif()

set(expected_manifest_toolset "${CMAKE_VS_PLATFORM_TOOLSET}")
if(expected_manifest_toolset STREQUAL "")
    set(expected_manifest_toolset "not-applicable")
endif()

add_test(
    NAME toolchain_manifest_current_contract
    COMMAND
        "${CMAKE_COMMAND}"
        "-DMANIFEST=${RGSML_TOOLCHAIN_MANIFEST}"
        "-DEXPECTED_SYSTEM=${CMAKE_SYSTEM_NAME}"
        "-DEXPECTED_ARCHITECTURE=${expected_manifest_architecture}"
        "-DEXPECTED_SDK=${expected_manifest_windows_sdk}"
        "-DEXPECTED_CMAKE=${CMAKE_VERSION}"
        "-DEXPECTED_GENERATOR=${CMAKE_GENERATOR}"
        "-DEXPECTED_PLATFORM=${expected_manifest_generator_platform}"
        "-DEXPECTED_COMPILER_ID=${CMAKE_CXX_COMPILER_ID}"
        "-DEXPECTED_COMPILER_VERSION=${CMAKE_CXX_COMPILER_VERSION}"
        "-DEXPECTED_TOOLSET=${expected_manifest_toolset}"
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

qt_add_executable(rgsml_analysis_tests
    unit/analysis/rgsml_analysis_test.cpp
)
target_link_libraries(rgsml_analysis_tests
    PRIVATE
        rgsml_analysis
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_analysis_tests)
add_test(NAME rgsml_analysis_tests COMMAND rgsml_analysis_tests)
set_tests_properties(rgsml_analysis_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "analysis;unit"
)

qt_add_executable(rgsml_live_spectrum_oracle_tests
    unit/analysis/live_spectrum_oracle_test.cpp
)
target_link_libraries(rgsml_live_spectrum_oracle_tests
    PRIVATE
        rgsml_analysis
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_live_spectrum_oracle_tests)
add_test(NAME rgsml_live_spectrum_oracle_tests COMMAND rgsml_live_spectrum_oracle_tests)
set_tests_properties(rgsml_live_spectrum_oracle_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "analysis;oracle"
)

qt_add_executable(rgsml_live_spectrum_stress_tests
    unit/analysis/live_spectrum_stress_test.cpp
)
target_link_libraries(rgsml_live_spectrum_stress_tests
    PRIVATE
        rgsml_analysis
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_live_spectrum_stress_tests)
add_test(NAME rgsml_live_spectrum_stress_tests COMMAND rgsml_live_spectrum_stress_tests)
set_tests_properties(rgsml_live_spectrum_stress_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "analysis;stress"
)

qt_add_executable(rgsml_dsp_module_adapter_tests
    unit/app/test_dsp_module_adapter.cpp
    ${CMAKE_SOURCE_DIR}/app/src/dsp_module_adapter.cpp
    ${CMAKE_SOURCE_DIR}/app/src/dsp_module_adapter.hpp
    ${CMAKE_SOURCE_DIR}/app/src/dsp_chain_adapter_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/dsp_chain_adapter_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/gain_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/gain_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/eq_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/eq_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/compressor_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/compressor_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_chain_state.cpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_chain_state.hpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_preview_controller.cpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_preview_controller.hpp
)

qt_add_executable(rgsml_compressor_view_model_tests
    unit/app/test_compressor_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/compressor_view_model.cpp
    ${CMAKE_SOURCE_DIR}/app/src/compressor_view_model.hpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_chain_state.cpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_chain_state.hpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_preview_controller.cpp
    ${CMAKE_SOURCE_DIR}/app/src/mastering_preview_controller.hpp
)
target_include_directories(rgsml_compressor_view_model_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/app/src
)
target_link_libraries(rgsml_compressor_view_model_tests
    PRIVATE
        rgsml_render
        rgsml_dsp
        rgsml_audio
        rgsml_core
        Qt6::Core
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_compressor_view_model_tests)
add_test(NAME rgsml_compressor_view_model_tests COMMAND rgsml_compressor_view_model_tests)
set_tests_properties(rgsml_compressor_view_model_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "app;compressor;view_model;render"
)
target_include_directories(rgsml_dsp_module_adapter_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/app/src
)
target_link_libraries(rgsml_dsp_module_adapter_tests
    PRIVATE
        rgsml_render
        rgsml_dsp
        rgsml_audio
        rgsml_core
        Qt6::Core
        Qt6::Test
)
rgsml_apply_project_warnings(rgsml_dsp_module_adapter_tests)
add_test(NAME rgsml_dsp_module_adapter_tests COMMAND rgsml_dsp_module_adapter_tests)
set_tests_properties(rgsml_dsp_module_adapter_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "app;dsp_adapter;view_model"
)
