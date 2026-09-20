add_library(rgsml_project STATIC
    src/project_snapshot.cpp
    src/internal/canonical_json.cpp
    src/internal/minizip_resource_stream.cpp
    src/internal/raw_zip_validator.cpp
    src/project_repository.cpp
)
target_include_directories(rgsml_project PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(rgsml_project PUBLIC rgsml_core
    PRIVATE rgsml_vendor_json MINIZIP::minizip)
set_target_properties(rgsml_project PROPERTIES RGSML_DEPENDENCY_LAYER PROJECT)
rgsml_apply_project_warnings(rgsml_project)
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/RgsmlDependencyRules.cmake")
rgsml_assert_target_dependencies(TARGET rgsml_project
    FORBIDDEN_LAYERS QT QML PLATFORM AUDIO DSP RENDER UI)
