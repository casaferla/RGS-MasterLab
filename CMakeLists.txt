cmake_minimum_required(VERSION 3.25)

project(RGSMasterLab VERSION 0.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/RgsmlOptions.cmake)
include(cmake/RgsmlWarnings.cmake)

set(RGSML_QT_ROOT "$ENV{RGSML_QT_ROOT}" CACHE PATH "Prefix of the local Qt kit")
if(RGSML_QT_ROOT)
    file(TO_CMAKE_PATH "${RGSML_QT_ROOT}" RGSML_QT_ROOT_CMAKE)
    list(PREPEND CMAKE_PREFIX_PATH "${RGSML_QT_ROOT_CMAKE}")
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
if(RGSML_BUILD_TESTS)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
endif()

qt_standard_project_setup(REQUIRES 6.8)

add_subdirectory(core)
add_subdirectory(ui)
add_subdirectory(app)

if(RGSML_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()
