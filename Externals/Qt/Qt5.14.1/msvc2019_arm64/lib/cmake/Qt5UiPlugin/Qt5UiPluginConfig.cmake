
if (CMAKE_VERSION VERSION_LESS 3.1.0)
    message(FATAL_ERROR "Qt 5 UiPlugin module requires at least CMake version 3.1.0")
endif()

get_filename_component(_qt5UiPlugin_install_prefix "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)


macro(_qt5_UiPlugin_check_file_exists file)
    if(NOT EXISTS "${file}" )
        message(FATAL_ERROR "The imported target \"Qt5::UiPlugin\" references the file
   \"${file}\"
but this file does not exist.  Possible reasons include:
* The file was deleted, renamed, or moved to another location.
* An install or uninstall procedure did not complete successfully.
* The installation package was faulty and contained
   \"${CMAKE_CURRENT_LIST_FILE}\"
but not all the files it references.
")
    endif()
endmacro()



if (NOT TARGET Qt5::UiPlugin)

    set(_Qt5UiPlugin_OWN_INCLUDE_DIRS "${_qt5UiPlugin_install_prefix}/include/" "${_qt5UiPlugin_install_prefix}/include/QtUiPlugin")
    set(Qt5UiPlugin_PRIVATE_INCLUDE_DIRS "")
    include("${CMAKE_CURRENT_LIST_DIR}/ExtraSourceIncludes.cmake" OPTIONAL)

    foreach(_dir ${_Qt5UiPlugin_OWN_INCLUDE_DIRS})
        _qt5_UiPlugin_check_file_exists(${_dir})
    endforeach()

    # Only check existence of private includes if the Private component is
    # specified.
    list(FIND Qt5UiPlugin_FIND_COMPONENTS Private _check_private)
    if (NOT _check_private STREQUAL -1)
        foreach(_dir ${Qt5UiPlugin_PRIVATE_INCLUDE_DIRS})
            _qt5_UiPlugin_check_file_exists(${_dir})
        endforeach()
    endif()

    set(_Qt5UiPlugin_MODULE_DEPENDENCIES "Widgets;Gui;Core")


    set(Qt5UiPlugin_OWN_PRIVATE_INCLUDE_DIRS ${Qt5UiPlugin_PRIVATE_INCLUDE_DIRS})

    set(_Qt5UiPlugin_FIND_DEPENDENCIES_REQUIRED)
    if (Qt5UiPlugin_FIND_REQUIRED)
        set(_Qt5UiPlugin_FIND_DEPENDENCIES_REQUIRED REQUIRED)
    endif()
    set(_Qt5UiPlugin_FIND_DEPENDENCIES_QUIET)
    if (Qt5UiPlugin_FIND_QUIETLY)
        set(_Qt5UiPlugin_DEPENDENCIES_FIND_QUIET QUIET)
    endif()
    set(_Qt5UiPlugin_FIND_VERSION_EXACT)
    if (Qt5UiPlugin_FIND_VERSION_EXACT)
        set(_Qt5UiPlugin_FIND_VERSION_EXACT EXACT)
    endif()


    foreach(_module_dep ${_Qt5UiPlugin_MODULE_DEPENDENCIES})
        if (NOT Qt5${_module_dep}_FOUND)
            find_package(Qt5${_module_dep}
                5.14.1 ${_Qt5UiPlugin_FIND_VERSION_EXACT}
                ${_Qt5UiPlugin_DEPENDENCIES_FIND_QUIET}
                ${_Qt5UiPlugin_FIND_DEPENDENCIES_REQUIRED}
                PATHS "${CMAKE_CURRENT_LIST_DIR}/.." NO_DEFAULT_PATH
            )
        endif()

        if (NOT Qt5${_module_dep}_FOUND)
            set(Qt5UiPlugin_FOUND False)
            return()
        endif()

    endforeach()

    # It can happen that the same FooConfig.cmake file is included when calling find_package()
    # on some Qt component. An example of that is when using a Qt static build with auto inclusion
    # of plugins:
    #
    # Qt5WidgetsConfig.cmake -> Qt5GuiConfig.cmake -> Qt5Gui_QSvgIconPlugin.cmake ->
    # Qt5SvgConfig.cmake -> Qt5WidgetsConfig.cmake ->
    # finish processing of second Qt5WidgetsConfig.cmake ->
    # return to first Qt5WidgetsConfig.cmake ->
    # add_library cannot create imported target Qt5::Widgets.
    #
    # Make sure to return early in the original Config inclusion, because the target has already
    # been defined as part of the second inclusion.
    if(TARGET Qt5::UiPlugin)
        return()
    endif()

    set(_Qt5UiPlugin_LIB_DEPENDENCIES "Qt5::Widgets;Qt5::Gui;Qt5::Core")


    add_library(Qt5::UiPlugin INTERFACE IMPORTED)

    set_property(TARGET Qt5::UiPlugin PROPERTY
      INTERFACE_INCLUDE_DIRECTORIES ${_Qt5UiPlugin_OWN_INCLUDE_DIRS})
    set_property(TARGET Qt5::UiPlugin PROPERTY
      INTERFACE_COMPILE_DEFINITIONS QT_UIPLUGIN_LIB)

    set_property(TARGET Qt5::UiPlugin PROPERTY INTERFACE_QT_ENABLED_FEATURES )
    set_property(TARGET Qt5::UiPlugin PROPERTY INTERFACE_QT_DISABLED_FEATURES )

    set_property(TARGET Qt5::UiPlugin PROPERTY INTERFACE_QT_PLUGIN_TYPES "")

    set(_Qt5UiPlugin_PRIVATE_DIRS_EXIST TRUE)
    foreach (_Qt5UiPlugin_PRIVATE_DIR ${Qt5UiPlugin_OWN_PRIVATE_INCLUDE_DIRS})
        if (NOT EXISTS ${_Qt5UiPlugin_PRIVATE_DIR})
            set(_Qt5UiPlugin_PRIVATE_DIRS_EXIST FALSE)
        endif()
    endforeach()

    if (_Qt5UiPlugin_PRIVATE_DIRS_EXIST)
        add_library(Qt5::UiPluginPrivate INTERFACE IMPORTED)
        set_property(TARGET Qt5::UiPluginPrivate PROPERTY
            INTERFACE_INCLUDE_DIRECTORIES ${Qt5UiPlugin_OWN_PRIVATE_INCLUDE_DIRS}
        )
        set(_Qt5UiPlugin_PRIVATEDEPS)
        foreach(dep ${_Qt5UiPlugin_LIB_DEPENDENCIES})
            if (TARGET ${dep}Private)
                list(APPEND _Qt5UiPlugin_PRIVATEDEPS ${dep}Private)
            endif()
        endforeach()
        set_property(TARGET Qt5::UiPluginPrivate PROPERTY
            INTERFACE_LINK_LIBRARIES Qt5::UiPlugin ${_Qt5UiPlugin_PRIVATEDEPS}
        )
    endif()

    set_target_properties(Qt5::UiPlugin PROPERTIES
        INTERFACE_LINK_LIBRARIES "${_Qt5UiPlugin_LIB_DEPENDENCIES}"
    )


    file(GLOB pluginTargets "${CMAKE_CURRENT_LIST_DIR}/Qt5UiPlugin_*Plugin.cmake")

    macro(_populate_UiPlugin_plugin_properties Plugin Configuration PLUGIN_LOCATION
          IsDebugAndRelease)
        set_property(TARGET Qt5::${Plugin} APPEND PROPERTY IMPORTED_CONFIGURATIONS ${Configuration})

        set(imported_location "${_qt5UiPlugin_install_prefix}/plugins/${PLUGIN_LOCATION}")
        _qt5_UiPlugin_check_file_exists(${imported_location})
        set_target_properties(Qt5::${Plugin} PROPERTIES
            "IMPORTED_LOCATION_${Configuration}" ${imported_location}
        )

    endmacro()

    if (pluginTargets)
        foreach(pluginTarget ${pluginTargets})
            include(${pluginTarget})
        endforeach()
    endif()





_qt5_UiPlugin_check_file_exists("${CMAKE_CURRENT_LIST_DIR}/Qt5UiPluginConfigVersion.cmake")

endif()
