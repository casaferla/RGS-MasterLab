qt_add_qml_module(rgsml_ui
    STATIC
    URI Rgsml.Ui
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Rgsml/Ui"
    QML_FILES
        qml/AuditionSourceSelector.qml
        qml/Main.qml
        qml/ParametricEqEditor.qml
        qml/ParametricEqEditorWindow.qml
        qml/ParametricEqGraph.qml
        qml/SegmentedTimeEditor.qml
        qml/StudioButton.qml
        qml/StudioIcon.qml
        qml/StudioIconButton.qml
        qml/StudioNumericField.qml
        qml/StudioSegmentButton.qml
        qml/StudioToggle.qml
        qml/TransportButton.qml
    SOURCES
        src/internal/waveform_geometry.cpp
        src/internal/waveform_geometry.hpp
        src/internal/waveform_viewport.cpp
        src/internal/waveform_viewport.hpp
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

if(MSVC)
    target_compile_options(rgsml_ui PRIVATE /wd4702)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/RgsmlDependencyRules.cmake")
rgsml_assert_target_dependencies(TARGET rgsml_ui FORBIDDEN_LAYERS PROJECT)
