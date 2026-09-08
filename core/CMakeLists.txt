add_library(rgsml_core STATIC
    src/build_info.cpp
)

target_include_directories(rgsml_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

rgsml_enable_warnings(rgsml_core)
