# M17 combined realistic product activation test, built only in Windows
# Focused CI and in the full qualification. Reuse the established, portable
# source closures to avoid cherry-picking partial terminal device mocks.
get_target_property(_rgsml_act_a rgsml_audition_source_selector_tests SOURCES)
get_target_property(_rgsml_act_b rgsml_mastering_chain_state_tests SOURCES)
if(NOT _rgsml_act_a OR NOT _rgsml_act_b)
    message(FATAL_ERROR "M17 activation integration requires existing app-test closures")
endif()
list(FILTER _rgsml_act_a EXCLUDE REGEX "test_audition_source_selector\\.cpp$")
list(FILTER _rgsml_act_b EXCLUDE REGEX "test_mastering_chain_state\\.cpp$")
set(_rgsml_act_sources ${_rgsml_act_a} ${_rgsml_act_b}
    # M17's coordinator now depends on the authoritative Project MASTER
    # Q_OBJECT. Both implementation and header must participate in this
    # executable's link/AUTOMOC closure, not just the production app.
    "${CMAKE_SOURCE_DIR}/app/src/master_output_rate_state.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/master_output_rate_state.hpp"
    "${CMAKE_SOURCE_DIR}/app/src/terminal_product_activation_coordinator.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/terminal_product_activation_coordinator.hpp"
    "${CMAKE_SOURCE_DIR}/app/src/terminal_monitor_view_model.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/terminal_monitor_view_model.hpp"
    # Real MasteringPreviewController signal/worker regression uses the
    # same Qt source and AUTOMOC closure as the product executable.
    "${CMAKE_SOURCE_DIR}/app/src/mastering_preview_controller.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/mastering_preview_controller.hpp"
    "${CMAKE_SOURCE_DIR}/tests/unit/app/test_terminal_product_activation.cpp")
list(REMOVE_DUPLICATES _rgsml_act_sources)
# Avoid deferred Windows LNK2019/LNK2001 if a future test closure drops
# the Project MASTER state. Verify both translation unit and Q_OBJECT.
foreach(_rgsml_act_required IN ITEMS
    "${CMAKE_SOURCE_DIR}/app/src/master_output_rate_state.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/master_output_rate_state.hpp")
    if(NOT _rgsml_act_required IN_LIST _rgsml_act_sources)
        message(FATAL_ERROR
            "M17 product activation missing Project MASTER Qt dependency: ${_rgsml_act_required}")
    endif()
endforeach()
unset(_rgsml_act_required)
qt_add_executable(rgsml_terminal_product_activation_tests
    ${_rgsml_act_sources})
target_include_directories(rgsml_terminal_product_activation_tests
    PRIVATE
        "${CMAKE_SOURCE_DIR}/app/src"
        "${CMAKE_SOURCE_DIR}/platform/windows/include"
        "${CMAKE_SOURCE_DIR}/analysis/src")
target_link_libraries(rgsml_terminal_product_activation_tests
    PRIVATE
        rgsml_core
        rgsml_dsp
        rgsml_render
        rgsml_platform
        rgsml_analysis
        Qt6::Core
        Qt6::Test)
rgsml_apply_project_warnings(rgsml_terminal_product_activation_tests)
add_test(NAME rgsml_terminal_product_activation_tests
    COMMAND rgsml_terminal_product_activation_tests)
set_tests_properties(rgsml_terminal_product_activation_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "app;mastering_chain;m17;audible_activation;qualified_candidate")
unset(_rgsml_act_a)
unset(_rgsml_act_b)
unset(_rgsml_act_sources)
