add_library(rgsml_render STATIC
    src/render_preview.cpp
    src/render_request.cpp
    src/render_result.cpp
)

target_include_directories(rgsml_render
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(rgsml_render
    PUBLIC
        rgsml_core
        rgsml_audio
        rgsml_dsp
)

set_property(TARGET rgsml_render PROPERTY RGSML_DEPENDENCY_LAYER RENDER)
rgsml_apply_project_warnings(rgsml_render)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/RgsmlDependencyRules.cmake")
rgsml_assert_target_dependencies(
    TARGET rgsml_render
    FORBIDDEN_LAYERS QT QML PLATFORM UI PROJECT)
