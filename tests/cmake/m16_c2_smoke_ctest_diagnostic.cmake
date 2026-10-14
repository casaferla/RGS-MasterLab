# M16 C2 test infrastructure diagnostic. No production audio or test assertions changed.
# Invoke the existing Qt smoke executable and retain its second-run output
# even when CTest's own process summary lacks the failed Qt case details.
if(NOT DEFINED RGSML_SMOKE_EXE OR RGSML_SMOKE_EXE STREQUAL "")
    message(FATAL_ERROR "M16_C2_SMOKE_DIAGNOSTIC_MISSING_EXE")
endif()
if(DEFINED ENV{RGSML_GUI01_EVIDENCE_DIR}
        AND NOT "$ENV{RGSML_GUI01_EVIDENCE_DIR}" STREQUAL "")
    set(_evidence_dir "$ENV{RGSML_GUI01_EVIDENCE_DIR}")
else()
    set(_evidence_dir "${CMAKE_CURRENT_BINARY_DIR}")
endif()
file(MAKE_DIRECTORY "${_evidence_dir}")
set(_qt_log "${_evidence_dir}/m16_c2_ctest_smoke_second_invocation.txt")
file(REMOVE "${_qt_log}")

# Qt emits individual assertion outcomes to an explicit file; keep CTest
# status equal to the real Qt executable status, never conceal failures.
execute_process(
    COMMAND "${RGSML_SMOKE_EXE}" -v2 -o "${_qt_log},txt"
    RESULT_VARIABLE _smoke_status
    OUTPUT_VARIABLE _smoke_stdout
    ERROR_VARIABLE _smoke_stderr
)
message(STATUS "M16_C2_CTEST_SMOKE_PROCESS_STATUS=${_smoke_status}")
message(STATUS "M16_C2_CTEST_SMOKE_QT_LOG=${_qt_log}")

if(EXISTS "${_qt_log}")
    file(READ "${_qt_log}" _qt_log_content)
    string(LENGTH "${_qt_log_content}" _qt_length)
else()
    set(_qt_log_content "NO_QT_LOG_CREATED")
    set(_qt_length 0)
endif()

if(NOT "${_smoke_status}" STREQUAL "0" OR NOT EXISTS "${_qt_log}")
    if(_qt_length GREATER 16000)
        math(EXPR _tail_offset "${_qt_length} - 16000")
        string(SUBSTRING "${_qt_log_content}" ${_tail_offset} -1 _qt_tail)
    else()
        set(_qt_tail "${_qt_log_content}")
    endif()
    message(FATAL_ERROR
        "M16_C2_CTEST_SMOKE_FAILED; ORIGINAL_RESULT=${_smoke_status}\n"
        "QT_LOG_LAST_16000_CHARS:\n${_qt_tail}\n"
        "STDOUT:\n${_smoke_stdout}\nSTDERR:\n${_smoke_stderr}")
endif()
message(STATUS "M16_C2_CTEST_SMOKE_PASSED; QT_LOG_BYTES=${_qt_length}")
