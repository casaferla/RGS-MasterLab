qt_add_qml_module(rgsml_ui
    STATIC
    URI Rgsml.Ui
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Rgsml/Ui"
    QML_FILES
        qml/Main.qml
    SOURCES
        src/internal/waveform_geometry.cpp
        src/internal/waveform_geometry.hpp
        src/waveform_item.cpp
        src/waveform_item.hpp
        src/waveform_presentation.cpp
        src/waveform_presentation.hpp
)

target_include_directories(rgsml_ui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(rgsml_ui
    PUBLIC
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickDialogs2
        rgsml_audio
)

set_property(TARGET rgsml_ui PROPERTY RGSML_DEPENDENCY_LAYER UI)

rgsml_apply_project_warnings(rgsml_ui)
