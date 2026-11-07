cmake_minimum_required(VERSION 3.25)
project(RgsmlAnalysisWorkbenchScreenshot LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick)
qt_standard_project_setup(REQUIRES 6.8)
get_filename_component(RGSML_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
add_executable(rgsml_analysis_workbench_screenshot
    analysis_workbench_screenshot.cpp)
target_compile_definitions(rgsml_analysis_workbench_screenshot PRIVATE
    RGSML_ANALYSIS_QML_SOURCE="${RGSML_ROOT}/ui/qml/M19M21AnalysisWorkbenchPreview.qml")
target_link_libraries(rgsml_analysis_workbench_screenshot PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
if(MSVC)
    target_compile_options(rgsml_analysis_workbench_screenshot PRIVATE /W4 /WX)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rgsml_analysis_workbench_screenshot PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()
