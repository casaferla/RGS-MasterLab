# Only the separate Ninja pilot reads this overlay. The canonical VS FQ,
# frozen M16 math and audio source files remain unchanged.
# M16 audio/CMakeLists.txt optimizes the playback SRC TU (/O2) in Debug and
# sets VS_SETTINGS BasicRuntimeChecks=Default for this particular source.
# Ninja ignores VS_SETTINGS and otherwise combines /O2 with /RTC1 => D8016.
# Append /RTC- after /O2 for this one TU only; leave /RTC1 for other sources.
if(NOT MSVC OR NOT CMAKE_GENERATOR MATCHES "^Ninja")
  message(FATAL_ERROR "MSVC Ninja FQ pilot only.")
endif()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL
  set_property SOURCE "${CMAKE_SOURCE_DIR}/audio/src/playback_sample_rate_adapter.cpp"
  TARGET_DIRECTORY rgsml_audio APPEND PROPERTY COMPILE_OPTIONS
  "$<$<CONFIG:Debug>:/RTC->")
