# Diagnostics only: same QTest executable / working directory / full suite.
# The ONLY within-CMake variation is execute_process output handling.
if(NOT DEFINED RGSML_TEST_EXE OR NOT EXISTS "${RGSML_TEST_EXE}")
    message(FATAL_ERROR "M23_CAPTURE_MODE_MISSING_TEST_EXE")
endif()
if(NOT DEFINED RGSML_QT_LOG OR RGSML_QT_LOG STREQUAL "")
    message(FATAL_ERROR "M23_CAPTURE_MODE_MISSING_QT_LOG")
endif()
if(NOT DEFINED RGSML_CAPTURE_MODE)
    message(FATAL_ERROR "M23_CAPTURE_MODE_MISSING_MODE")
endif()
set(_cmd "${RGSML_TEST_EXE}" -v2 -o "${RGSML_QT_LOG},txt")
if(RGSML_CAPTURE_MODE STREQUAL "NO_CAPTURE")
    execute_process(
        COMMAND ${_cmd}
        RESULT_VARIABLE _result)
elseif(RGSML_CAPTURE_MODE STREQUAL "CAPTURE_VARIABLES")
    execute_process(
        COMMAND ${_cmd}
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err)
elseif(RGSML_CAPTURE_MODE STREQUAL "CAPTURE_FILES")
    execute_process(
        COMMAND ${_cmd}
        RESULT_VARIABLE _result
        OUTPUT_FILE "${RGSML_QT_LOG}.stdout.txt"
        ERROR_FILE "${RGSML_QT_LOG}.stderr.txt")
elseif(RGSML_CAPTURE_MODE STREQUAL "CAPTURE_STDOUT_ONLY")
    execute_process(
        COMMAND ${_cmd}
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _out)
else()
    message(FATAL_ERROR "M23_CAPTURE_MODE_UNKNOWN_${RGSML_CAPTURE_MODE}")
endif()
# No masking: report exact Qt return code; wrapper remains diagnostic only.
message(STATUS "M23_CAPTURE_MODE=${RGSML_CAPTURE_MODE}")
message(STATUS "M23_QT_PROCESS_EXIT=${_result}")
