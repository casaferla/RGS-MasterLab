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

    if(Qt6_VERSION VERSION_GREATER_EQUAL "6.8")
        qt_generate_deploy_qml_app_script(
            TARGET ${target}
            OUTPUT_SCRIPT deploy_script
            NO_UNSUPPORTED_PLATFORM_ERROR
            DEPLOY_TOOL_OPTIONS --qmldir "${CMAKE_SOURCE_DIR}/ui/qml"
        )
        install(SCRIPT "${deploy_script}")
    endif()
endfunction()
