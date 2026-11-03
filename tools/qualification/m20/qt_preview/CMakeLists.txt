cmake_minimum_required(VERSION 3.25)
project(M20IsolatedQtPreview LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick)
qt_standard_project_setup(REQUIRES 6.8)
get_filename_component(M20_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
add_executable(m20_qt_preview_smoke m20_qml_preview_smoke.cpp)
target_include_directories(m20_qt_preview_smoke PRIVATE
  "${M20_REPO_ROOT}/analysis/src"
  "${M20_REPO_ROOT}/analysis/include"
  "${M20_REPO_ROOT}/audio/include"
  "${M20_REPO_ROOT}/core/include"
  "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(m20_qt_preview_smoke PRIVATE
  M20_QML_SOURCE="${M20_REPO_ROOT}/ui/qml/M20BasicTonalStereoPreview.qml")
target_link_libraries(m20_qt_preview_smoke PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
if(MSVC)
  target_compile_options(m20_qt_preview_smoke PRIVATE /W4 /WX /fp:strict)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(m20_qt_preview_smoke PRIVATE
    -Wall -Wextra -Wpedantic -Werror -fno-fast-math -ffp-contract=off)
endif()
