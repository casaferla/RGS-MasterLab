cmake_minimum_required(VERSION 3.25)
project(RgsmlWarningsPolicyFixture LANGUAGES CXX)

include("${RGSML_WARNINGS_MODULE}")

add_library(project_probe STATIC probe.cpp)
add_library(unconfigured_probe STATIC probe.cpp)
rgsml_apply_project_warnings(project_probe)

get_target_property(project_options project_probe COMPILE_OPTIONS)
get_target_property(unconfigured_options unconfigured_probe COMPILE_OPTIONS)
get_directory_property(directory_options COMPILE_OPTIONS)
if(NOT unconfigured_options)
    set(unconfigured_options "<none>")
endif()
if(NOT directory_options)
    set(directory_options "<none>")
endif()

file(WRITE "${CMAKE_BINARY_DIR}/warnings-result.txt"
    "project=${project_options}\n"
    "unconfigured=${unconfigured_options}\n"
    "directory=${directory_options}\n"
    "cxx_flags=${CMAKE_CXX_FLAGS}\n"
)
