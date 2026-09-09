add_library(rgsml_platform STATIC
    windows/src/windows_resource_reader.cpp
)

target_include_directories(rgsml_platform
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/windows/include
)

target_link_libraries(rgsml_platform
    PUBLIC
        rgsml_core
    PRIVATE
        Qt6::Core
)

set_property(TARGET rgsml_platform PROPERTY RGSML_DEPENDENCY_LAYER PLATFORM)
rgsml_apply_project_warnings(rgsml_platform)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/RgsmlDependencyRules.cmake")
rgsml_assert_target_dependencies(
    TARGET rgsml_platform
    FORBIDDEN_LAYERS AUDIO DSP QML UI
)
