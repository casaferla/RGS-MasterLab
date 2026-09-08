qt_add_qml_module(rgsml_ui
    STATIC
    URI Rgsml.Ui
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Rgsml/Ui"
    QML_FILES
        qml/Main.qml
)

target_link_libraries(rgsml_ui
    PUBLIC
        Qt6::Quick
        Qt6::QuickControls2
)

rgsml_apply_project_warnings(rgsml_ui)
