if (CMAKE_VERSION VERSION_LESS 3.1.0)
    message(FATAL_ERROR "Qt 5 Gui module requires at least CMake version 3.1.0")
endif()

get_filename_component(_qt5Gui_install_prefix "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

# For backwards compatibility only. Use Qt5Gui_VERSION instead.
set(Qt5Gui_VERSION_STRING 5.15.0)

set(Qt5Gui_LIBRARIES Qt5::Gui)

macro(_qt5_Gui_check_file_exists file)
    if(NOT EXISTS "${file}" )
        message(FATAL_ERROR "The imported target \"Qt5::Gui\" references the file
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


macro(_populate_Gui_target_properties Configuration LIB_LOCATION IMPLIB_LOCATION
      IsDebugAndRelease)
    set_property(TARGET Qt5::Gui APPEND PROPERTY IMPORTED_CONFIGURATIONS ${Configuration})

    set(imported_location "${_qt5Gui_install_prefix}/bin/${LIB_LOCATION}")
    _qt5_Gui_check_file_exists(${imported_location})
    set(_deps
        ${_Qt5Gui_LIB_DEPENDENCIES}
    )
    set(_static_deps
    )

    set_target_properties(Qt5::Gui PROPERTIES
        "IMPORTED_LOCATION_${Configuration}" ${imported_location}
        # For backward compatibility with CMake < 2.8.12
        "IMPORTED_LINK_INTERFACE_LIBRARIES_${Configuration}" "${_deps};${_static_deps}"
    )
    set_property(TARGET Qt5::Gui APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                 "${_deps}"
    )


    set(imported_implib "${_qt5Gui_install_prefix}/lib/${IMPLIB_LOCATION}")
    _qt5_Gui_check_file_exists(${imported_implib})
    if(NOT "${IMPLIB_LOCATION}" STREQUAL "")
        set_target_properties(Qt5::Gui PROPERTIES
        "IMPORTED_IMPLIB_${Configuration}" ${imported_implib}
        )
    endif()
endmacro()

if (NOT TARGET Qt5::Gui)

    set(_Qt5Gui_OWN_INCLUDE_DIRS "${_qt5Gui_install_prefix}/include/" "${_qt5Gui_install_prefix}/include/QtGui")
    set(Qt5Gui_PRIVATE_INCLUDE_DIRS
        "${_qt5Gui_install_prefix}/include/QtGui/5.15.0"
        "${_qt5Gui_install_prefix}/include/QtGui/5.15.0/QtGui"
    )
    include("${CMAKE_CURRENT_LIST_DIR}/ExtraSourceIncludes.cmake" OPTIONAL)

    foreach(_dir ${_Qt5Gui_OWN_INCLUDE_DIRS})
        _qt5_Gui_check_file_exists(${_dir})
    endforeach()

    # Only check existence of private includes if the Private component is
    # specified.
    list(FIND Qt5Gui_FIND_COMPONENTS Private _check_private)
    if (NOT _check_private STREQUAL -1)
        foreach(_dir ${Qt5Gui_PRIVATE_INCLUDE_DIRS})
            _qt5_Gui_check_file_exists(${_dir})
        endforeach()
    endif()

    set(Qt5Gui_INCLUDE_DIRS ${_Qt5Gui_OWN_INCLUDE_DIRS})

    set(Qt5Gui_DEFINITIONS -DQT_GUI_LIB)
    set(Qt5Gui_COMPILE_DEFINITIONS QT_GUI_LIB)
    set(_Qt5Gui_MODULE_DEPENDENCIES "Core")


    set(Qt5Gui_OWN_PRIVATE_INCLUDE_DIRS ${Qt5Gui_PRIVATE_INCLUDE_DIRS})

    set(_Qt5Gui_FIND_DEPENDENCIES_REQUIRED)
    if (Qt5Gui_FIND_REQUIRED)
        set(_Qt5Gui_FIND_DEPENDENCIES_REQUIRED REQUIRED)
    endif()
    set(_Qt5Gui_FIND_DEPENDENCIES_QUIET)
    if (Qt5Gui_FIND_QUIETLY)
        set(_Qt5Gui_DEPENDENCIES_FIND_QUIET QUIET)
    endif()
    set(_Qt5Gui_FIND_VERSION_EXACT)
    if (Qt5Gui_FIND_VERSION_EXACT)
        set(_Qt5Gui_FIND_VERSION_EXACT EXACT)
    endif()

    set(Qt5Gui_EXECUTABLE_COMPILE_FLAGS "")

    foreach(_module_dep ${_Qt5Gui_MODULE_DEPENDENCIES})
        if (NOT Qt5${_module_dep}_FOUND)
            find_package(Qt5${_module_dep}
                5.15.0 ${_Qt5Gui_FIND_VERSION_EXACT}
                ${_Qt5Gui_DEPENDENCIES_FIND_QUIET}
                ${_Qt5Gui_FIND_DEPENDENCIES_REQUIRED}
                PATHS "${CMAKE_CURRENT_LIST_DIR}/.." NO_DEFAULT_PATH
            )
        endif()

        if (NOT Qt5${_module_dep}_FOUND)
            set(Qt5Gui_FOUND False)
            return()
        endif()

        list(APPEND Qt5Gui_INCLUDE_DIRS "${Qt5${_module_dep}_INCLUDE_DIRS}")
        list(APPEND Qt5Gui_PRIVATE_INCLUDE_DIRS "${Qt5${_module_dep}_PRIVATE_INCLUDE_DIRS}")
        list(APPEND Qt5Gui_DEFINITIONS ${Qt5${_module_dep}_DEFINITIONS})
        list(APPEND Qt5Gui_COMPILE_DEFINITIONS ${Qt5${_module_dep}_COMPILE_DEFINITIONS})
        list(APPEND Qt5Gui_EXECUTABLE_COMPILE_FLAGS ${Qt5${_module_dep}_EXECUTABLE_COMPILE_FLAGS})
    endforeach()
    list(REMOVE_DUPLICATES Qt5Gui_INCLUDE_DIRS)
    list(REMOVE_DUPLICATES Qt5Gui_PRIVATE_INCLUDE_DIRS)
    list(REMOVE_DUPLICATES Qt5Gui_DEFINITIONS)
    list(REMOVE_DUPLICATES Qt5Gui_COMPILE_DEFINITIONS)
    list(REMOVE_DUPLICATES Qt5Gui_EXECUTABLE_COMPILE_FLAGS)

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
    if(TARGET Qt5::Gui)
        return()
    endif()

    set(_Qt5Gui_LIB_DEPENDENCIES "Qt5::Core")


    add_library(Qt5::Gui SHARED IMPORTED)

    # Add a versionless target, for compatibility with Qt6.
    if(NOT "${QT_NO_CREATE_VERSIONLESS_TARGETS}" AND NOT TARGET Qt::Gui)
        add_library(Qt::Gui INTERFACE IMPORTED)
        set_target_properties(Qt::Gui PROPERTIES
            INTERFACE_LINK_LIBRARIES "Qt5::Gui"
        )
    endif()


    set_property(TARGET Qt5::Gui PROPERTY
      INTERFACE_INCLUDE_DIRECTORIES ${_Qt5Gui_OWN_INCLUDE_DIRS})
    set_property(TARGET Qt5::Gui PROPERTY
      INTERFACE_COMPILE_DEFINITIONS QT_GUI_LIB)

    set_property(TARGET Qt5::Gui PROPERTY INTERFACE_QT_ENABLED_FEATURES accessibility;action;opengles2;angle;clipboard;colornames;cssparser;cursor;desktopservices;imageformat_xpm;draganddrop;opengl;imageformatplugin;highdpiscaling;im;image_heuristic_mask;image_text;imageformat_jpeg;imageformat_png;imageformat_ppm;imageformat_xbm;movie;pdf;picture;sessionmanager;shortcut;standarditemmodel;systemtrayicon;tabletevent;texthtmlparser;textmarkdownreader;textmarkdownwriter;textodfwriter;validator;whatsthis;wheelevent)
    set_property(TARGET Qt5::Gui PROPERTY INTERFACE_QT_DISABLED_FEATURES dynamicgl;combined-angle-lib;imageformat_bmp;opengles3;opengles31;opengles32;openvg;system-textmarkdownreader;vulkan)

    # Qt 6 forward compatible properties.
    set_property(TARGET Qt5::Gui
                 PROPERTY QT_ENABLED_PUBLIC_FEATURES
                 accessibility;action;opengles2;angle;clipboard;colornames;cssparser;cursor;desktopservices;imageformat_xpm;draganddrop;opengl;imageformatplugin;highdpiscaling;im;image_heuristic_mask;image_text;imageformat_jpeg;imageformat_png;imageformat_ppm;imageformat_xbm;movie;pdf;picture;sessionmanager;shortcut;standarditemmodel;systemtrayicon;tabletevent;texthtmlparser;textmarkdownreader;textmarkdownwriter;textodfwriter;validator;whatsthis;wheelevent)
    set_property(TARGET Qt5::Gui
                 PROPERTY QT_DISABLED_PUBLIC_FEATURES
                 dynamicgl;combined-angle-lib;imageformat_bmp;opengles3;opengles31;opengles32;openvg;system-textmarkdownreader;vulkan)
    set_property(TARGET Qt5::Gui
                 PROPERTY QT_ENABLED_PRIVATE_FEATURES
                 dxguid;direct3d9;angle_d3d11_qdtd;dxgi;direct3d11;direct2d;direct2d1_1;dxgi1_2;direct3d11_1;directwrite;directwrite1;directwrite2;egl;freetype;gif;harfbuzz;ico;jpeg;multiprocess;png;raster-64bit;tuiotouch;vkgen)
    set_property(TARGET Qt5::Gui
                 PROPERTY QT_DISABLED_PRIVATE_FEATURES
                 xcb;accessibility-atspi-bridge;directfb;drm_atomic;egl_x11;eglfs;eglfs_brcm;eglfs_egldevice;eglfs_gbm;eglfs_mali;eglfs_openwfd;eglfs_rcar;eglfs_viv;eglfs_viv_wl;eglfs_vsp2;eglfs_x11;evdev;fontconfig;integrityfb;integrityhid;kms;libinput;libinput-axis-api;linuxfb;mtdev;system-freetype;system-harfbuzz;system-jpeg;system-png;tslib;vnc;vsp2;xlib;xcb-xlib;xkbcommon;xkbcommon-x11)

    set_property(TARGET Qt5::Gui PROPERTY INTERFACE_QT_PLUGIN_TYPES "platforms;platforms/darwin;xcbglintegrations;platformthemes;platforminputcontexts;generic;iconengines;imageformats;egldeviceintegrations")

    set(_Qt5Gui_PRIVATE_DIRS_EXIST TRUE)
    foreach (_Qt5Gui_PRIVATE_DIR ${Qt5Gui_OWN_PRIVATE_INCLUDE_DIRS})
        if (NOT EXISTS ${_Qt5Gui_PRIVATE_DIR})
            set(_Qt5Gui_PRIVATE_DIRS_EXIST FALSE)
        endif()
    endforeach()

    if (_Qt5Gui_PRIVATE_DIRS_EXIST)
        add_library(Qt5::GuiPrivate INTERFACE IMPORTED)
        set_property(TARGET Qt5::GuiPrivate PROPERTY
            INTERFACE_INCLUDE_DIRECTORIES ${Qt5Gui_OWN_PRIVATE_INCLUDE_DIRS}
        )
        set(_Qt5Gui_PRIVATEDEPS)
        foreach(dep ${_Qt5Gui_LIB_DEPENDENCIES})
            if (TARGET ${dep}Private)
                list(APPEND _Qt5Gui_PRIVATEDEPS ${dep}Private)
            endif()
        endforeach()
        set_property(TARGET Qt5::GuiPrivate PROPERTY
            INTERFACE_LINK_LIBRARIES Qt5::Gui ${_Qt5Gui_PRIVATEDEPS}
        )

        # Add a versionless target, for compatibility with Qt6.
        if(NOT "${QT_NO_CREATE_VERSIONLESS_TARGETS}" AND NOT TARGET Qt::GuiPrivate)
            add_library(Qt::GuiPrivate INTERFACE IMPORTED)
            set_target_properties(Qt::GuiPrivate PROPERTIES
                INTERFACE_LINK_LIBRARIES "Qt5::GuiPrivate"
            )
        endif()
    endif()

    _populate_Gui_target_properties(RELEASE "Qt5Gui.dll" "Qt5Gui.lib" TRUE)



    _populate_Gui_target_properties(DEBUG "Qt5Guid.dll" "Qt5Guid.lib" TRUE)



    # In Qt 5.15 the glob pattern was relaxed to also catch plugins not literally named Plugin.
    # Define QT5_STRICT_PLUGIN_GLOB or ModuleName_STRICT_PLUGIN_GLOB to revert to old behavior.
    if (QT5_STRICT_PLUGIN_GLOB OR Qt5Gui_STRICT_PLUGIN_GLOB)
        file(GLOB pluginTargets "${CMAKE_CURRENT_LIST_DIR}/Qt5Gui_*Plugin.cmake")
    else()
        file(GLOB pluginTargets "${CMAKE_CURRENT_LIST_DIR}/Qt5Gui_*.cmake")
    endif()

    macro(_populate_Gui_plugin_properties Plugin Configuration PLUGIN_LOCATION
          IsDebugAndRelease)
        set_property(TARGET Qt5::${Plugin} APPEND PROPERTY IMPORTED_CONFIGURATIONS ${Configuration})

        set(imported_location "${_qt5Gui_install_prefix}/plugins/${PLUGIN_LOCATION}")
        _qt5_Gui_check_file_exists(${imported_location})
        set_target_properties(Qt5::${Plugin} PROPERTIES
            "IMPORTED_LOCATION_${Configuration}" ${imported_location}
        )

    endmacro()

    if (pluginTargets)
        foreach(pluginTarget ${pluginTargets})
            include(${pluginTarget})
        endforeach()
    endif()

    include("${CMAKE_CURRENT_LIST_DIR}/Qt5GuiConfigExtras.cmake")


    _qt5_Gui_check_file_exists("${CMAKE_CURRENT_LIST_DIR}/Qt5GuiConfigVersion.cmake")
endif()
