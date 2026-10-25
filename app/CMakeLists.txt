qt_add_executable(rgsml_app
    src/audition_source_selector.cpp
    src/audition_source_selector.hpp
    src/compressor_view_model.cpp
    src/compressor_view_model.hpp
    src/dsp_module_adapter.cpp
    src/dsp_module_adapter.hpp
    src/dsp_chain_adapter_model.cpp
    src/dsp_chain_adapter_model.hpp
    src/eq_view_model.cpp
    src/eq_view_model.hpp
    src/gain_view_model.cpp
    src/gain_view_model.hpp
    src/stereo_ms_view_model.cpp
    src/stereo_ms_view_model.hpp
    src/terminal_candidate_gate.cpp
    src/terminal_candidate_gate.hpp
    src/terminal_playback_frame_map.cpp
    src/terminal_playback_frame_map.hpp
    src/terminal_source_control_bridge.cpp
    src/terminal_source_control_bridge.hpp
    src/terminal_audio_session_gate.cpp
    src/terminal_audio_session_gate.hpp
    src/terminal_private_publication_coordinator.cpp
    src/terminal_private_publication_coordinator.hpp
    src/terminal_private_render_controller.cpp
    src/terminal_private_render_controller.hpp
    src/terminal_private_product_session.cpp
    src/terminal_private_product_session.hpp
    src/terminal_monitor_view_model.cpp
    src/terminal_monitor_view_model.hpp
    src/terminal_product_activation_coordinator.cpp
    src/terminal_product_activation_coordinator.hpp
    src/terminal_consumed_output_binding.cpp
    src/terminal_consumed_output_binding.hpp
    src/terminal_private_gr_view_model.cpp
    src/terminal_private_gr_view_model.hpp
    src/terminal_post_limiter_true_peak_coordinator.cpp
    src/terminal_post_limiter_true_peak_coordinator.hpp
    src/terminal_limiter_state.cpp
    src/terminal_limiter_state.hpp
    src/terminal_limiter_view_model.cpp
    src/terminal_limiter_view_model.hpp
    src/mastering_preview_controller.cpp
    src/mastering_preview_controller.hpp
    src/audition_region_view_model.cpp
    src/audition_region_view_model.hpp
    src/gold_selection_view_model.cpp
    src/gold_selection_view_model.hpp
    src/live_spectrum_view_model.cpp
    src/live_spectrum_view_model.hpp
    src/mastering_chain_state.cpp
    src/mastering_chain_state.hpp
    src/main.cpp
    src/project_session_view_model.cpp
    src/project_session_view_model.hpp
    src/playback_transport_view_model.cpp
    src/playback_transport_view_model.hpp
    src/prepare_memory_advice_view_model.cpp
    src/prepare_memory_advice_view_model.hpp
    src/source_selection_view_model.cpp
    src/source_selection_view_model.hpp
    src/source_waveform_view_model.cpp
    src/source_waveform_view_model.hpp
    src/true_peak_static_binding.cpp
    src/true_peak_static_binding.hpp
    src/true_peak_static_coordinator.cpp
    src/true_peak_static_coordinator.hpp
    src/true_peak_live_input_binding.cpp
    src/true_peak_live_input_binding.hpp
    src/true_peak_live_coordinator.cpp
    src/true_peak_live_coordinator.hpp
    src/windows_window_chrome_helper.cpp
    src/windows_window_chrome_helper.hpp
)

set_target_properties(rgsml_app PROPERTIES
    OUTPUT_NAME RGSMasterLab
    WIN32_EXECUTABLE TRUE
)

# Approved RGS–ML C branding only: embed the application Explorer/taskbar
# icon in the Windows PE executable without replacing other metadata.
if(WIN32)
    target_sources(rgsml_app PRIVATE resources/rgsml_app_icon.rc)
endif()

target_include_directories(rgsml_app PRIVATE ${CMAKE_SOURCE_DIR}/analysis/src)

target_link_libraries(rgsml_app
    PRIVATE
        rgsml_core
        rgsml_project
        rgsml_audio
        rgsml_analysis
        rgsml_dsp
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
