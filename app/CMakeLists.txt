qt_add_executable(rgsml_app
    src/audition_source_selector.cpp
    src/audition_source_selector.hpp
    src/eq_view_model.cpp
    src/eq_view_model.hpp
    src/audition_region_view_model.cpp
    src/audition_region_view_model.hpp
    src/gold_selection_view_model.cpp
    src/gold_selection_view_model.hpp
    src/main.cpp
    src/project_session_view_model.cpp
    src/project_session_view_model.hpp
    src/playback_transport_view_model.cpp
    src/playback_transport_view_model.hpp
    src/source_selection_view_model.cpp
    src/source_selection_view_model.hpp
    src/source_waveform_view_model.cpp
    src/source_waveform_view_model.hpp
)

set_target_properties(rgsml_app PROPERTIES
    OUTPUT_NAME RGSMasterLab
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(rgsml_app
    PRIVATE
        rgsml_core
        rgsml_project
        rgsml_audio
        rgsml_platform
        rgsml_render
        rgsml_ui
        rgsml_uiplugin
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
)

rgsml_apply_project_warnings(rgsml_app)
