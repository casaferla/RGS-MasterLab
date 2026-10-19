# Companion target for exact private M17 GR projection tests.
# Reuse the already-qualified app-test source closure and Qt environment.
get_target_property(_rgsml_gr_sources rgsml_mastering_chain_state_tests SOURCES)
if(NOT _rgsml_gr_sources)
    message(FATAL_ERROR "M17 private GR test cannot find source closure")
endif()
list(FILTER _rgsml_gr_sources EXCLUDE REGEX "test_mastering_chain_state\\.cpp$")
list(APPEND _rgsml_gr_sources
    "${CMAKE_SOURCE_DIR}/tests/unit/app/test_terminal_private_gr_projection.cpp")
qt_add_executable(rgsml_terminal_private_gr_projection_tests
    ${_rgsml_gr_sources})
target_include_directories(rgsml_terminal_private_gr_projection_tests
    PRIVATE
        "${CMAKE_SOURCE_DIR}/app/src"
        "${CMAKE_SOURCE_DIR}/platform/windows/include")
target_link_libraries(rgsml_terminal_private_gr_projection_tests
    PRIVATE
        rgsml_dsp
        rgsml_core
        rgsml_render
        Qt6::Test)
rgsml_apply_project_warnings(rgsml_terminal_private_gr_projection_tests)
add_test(NAME rgsml_terminal_private_gr_projection_tests
    COMMAND rgsml_terminal_private_gr_projection_tests)
set_tests_properties(rgsml_terminal_private_gr_projection_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    LABELS "app;mastering_chain;m17;private_gr")
unset(_rgsml_gr_sources)
