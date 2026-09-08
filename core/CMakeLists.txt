add_library(rgsml_core STATIC
    src/build_info.cpp
    src/checked_integer.cpp
    src/error.cpp
    src/frame_time.cpp
    src/processing_state.cpp
    src/rational.cpp
    src/uuid.cpp
)

target_include_directories(rgsml_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

rgsml_apply_project_warnings(rgsml_core)
