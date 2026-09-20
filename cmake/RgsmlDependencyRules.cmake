include_guard(GLOBAL)

function(rgsml_assert_target_dependencies)
    cmake_parse_arguments(ARG "" "TARGET" "FORBIDDEN_LAYERS" ${ARGN})

    if(NOT ARG_TARGET)
        message(FATAL_ERROR "RGSML_DEPENDENCY_RULE_CONFIGURATION: TARGET is required")
    endif()
    if(NOT TARGET "${ARG_TARGET}")
        message(FATAL_ERROR
            "RGSML_DEPENDENCY_RULE_CONFIGURATION: target '${ARG_TARGET}' does not exist")
    endif()
    if(NOT ARG_FORBIDDEN_LAYERS)
        set(ARG_FORBIDDEN_LAYERS QT QML PLATFORM DSP UI AUDIO PROJECT)
    endif()

    set(pending_targets "${ARG_TARGET}")
    set(visited_targets)

    while(pending_targets)
        list(POP_FRONT pending_targets current_target)
        if(current_target IN_LIST visited_targets)
            continue()
        endif()
        list(APPEND visited_targets "${current_target}")

        get_property(current_layer TARGET "${current_target}" PROPERTY RGSML_DEPENDENCY_LAYER)
        if(NOT current_target STREQUAL ARG_TARGET
            AND current_layer
            AND current_layer IN_LIST ARG_FORBIDDEN_LAYERS)
            message(FATAL_ERROR
                "RGSML_DEPENDENCY_RULE_VIOLATION: target '${ARG_TARGET}' reaches forbidden dependency '${current_target}' in layer '${current_layer}'")
        endif()

        foreach(link_property LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
            get_target_property(link_dependencies "${current_target}" "${link_property}")
            if(NOT link_dependencies OR link_dependencies MATCHES "-NOTFOUND$")
                continue()
            endif()

            foreach(link_dependency IN LISTS link_dependencies)
                if(link_dependency MATCHES "Qt[0-9]+::" AND "QT" IN_LIST ARG_FORBIDDEN_LAYERS)
                    message(FATAL_ERROR
                        "RGSML_DEPENDENCY_RULE_VIOLATION: target '${ARG_TARGET}' reaches forbidden dependency '${link_dependency}' in layer 'QT'")
                endif()

                set(candidate "${link_dependency}")
                if(candidate MATCHES "^\\$<LINK_ONLY:([^>]+)>$")
                    set(candidate "${CMAKE_MATCH_1}")
                elseif(candidate MATCHES "^\\$<BUILD_INTERFACE:([^>]+)>$")
                    set(candidate "${CMAKE_MATCH_1}")
                endif()

                if(TARGET "${candidate}" AND NOT candidate IN_LIST visited_targets)
                    list(APPEND pending_targets "${candidate}")
                endif()
            endforeach()
        endforeach()
    endwhile()
endfunction()
