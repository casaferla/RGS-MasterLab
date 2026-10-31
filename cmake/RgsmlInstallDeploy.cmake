include_guard(GLOBAL)

include(GNUInstallDirs)

function(rgsml_configure_install_deploy target manifest)
    install(CODE "
        if(NOT IS_ABSOLUTE \"\${CMAKE_INSTALL_PREFIX}\")
            get_filename_component(
                CMAKE_INSTALL_PREFIX
                \"\${CMAKE_INSTALL_PREFIX}\"
                ABSOLUTE
                BASE_DIR \"${CMAKE_SOURCE_DIR}\"
            )
        endif()
    ")

    install(
        TARGETS ${target}
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
    install(
        FILES "${manifest}"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/RGSMasterLab"
    )

    # Every installed product includes both frozen playback FIR files
    # OUTSIDE the EXE. Private original Q0 Final SRC coefficients are NOT
    # published by public CI; authenticated local FQ provisioning remains.
    install(
        FILES ${RGSML_PLAYBACK_FIR_INSTALL_FILES}
        DESTINATION "${CMAKE_INSTALL_BINDIR}/resources/fir/playback"
    )

    qt_generate_deploy_qml_app_script(
        TARGET ${target}
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
        DEPLOY_TOOL_OPTIONS --qmldir "${CMAKE_SOURCE_DIR}/ui/qml"
    )
    install(SCRIPT "${deploy_script}")
endfunction()
