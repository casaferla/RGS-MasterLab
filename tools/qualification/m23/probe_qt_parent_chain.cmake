# M23 NONRELEASE diagnostic. Same exact QTest EXE, test cwd and -v2/-o args.
# Only the direct CMake child vs CMake -> PowerShell -> QTest is varied.
if(NOT DEFINED RGSML_GUI_EXE OR NOT EXISTS "${RGSML_GUI_EXE}")
  message(FATAL_ERROR "GUI_PARENT_CHAIN_EXE_MISSING")
endif()
if(NOT DEFINED RGSML_GUI_LOG OR RGSML_GUI_LOG STREQUAL "")
  message(FATAL_ERROR "GUI_PARENT_CHAIN_LOG_MISSING")
endif()
if(NOT DEFINED RGSML_PARENT_MODE)
  message(FATAL_ERROR "GUI_PARENT_CHAIN_MODE_MISSING")
endif()
if(RGSML_PARENT_MODE STREQUAL "CMAKE_DIRECT")
  execute_process(
    COMMAND "${RGSML_GUI_EXE}" -v2 -o "${RGSML_GUI_LOG},txt"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
elseif(RGSML_PARENT_MODE STREQUAL "CMAKE_PWSH_CHILD")
  if(NOT DEFINED RGSML_PWSH_LAUNCHER OR NOT EXISTS "${RGSML_PWSH_LAUNCHER}")
    message(FATAL_ERROR "GUI_PARENT_CHAIN_PWSH_LAUNCHER_MISSING")
  endif()
  execute_process(
    COMMAND pwsh -NoProfile -NonInteractive -File "${RGSML_PWSH_LAUNCHER}"
      -QtExe "${RGSML_GUI_EXE}" -QtLog "${RGSML_GUI_LOG}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
else()
  message(FATAL_ERROR "GUI_PARENT_CHAIN_UNKNOWN_MODE")
endif()
message(STATUS "M23_PARENT_MODE=${RGSML_PARENT_MODE}")
message(STATUS "M23_QTEST_EXIT=${_result}")
# No diagnostic can make the strict original M23 CTest green.
