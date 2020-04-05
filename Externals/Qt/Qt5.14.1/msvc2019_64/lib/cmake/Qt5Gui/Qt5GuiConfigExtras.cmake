

set(Qt5Gui_EGL_INCLUDE_DIRS "${_qt5Gui_install_prefix}/include//QtANGLE")

_qt5_Gui_check_file_exists(${Qt5Gui_EGL_INCLUDE_DIRS})

list(APPEND Qt5Gui_INCLUDE_DIRS ${Qt5Gui_EGL_INCLUDE_DIRS})
set_property(TARGET Qt5::Gui APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${Qt5Gui_EGL_INCLUDE_DIRS})
set(Qt5Gui_OPENGL_INCLUDE_DIRS ${Qt5Gui_EGL_INCLUDE_DIRS})

macro(_populate_qt5gui_gl_target_properties TargetName Configuration LIB_LOCATION IMPLIB_LOCATION)
    set_property(TARGET Qt5::${TargetName} APPEND PROPERTY IMPORTED_CONFIGURATIONS ${Configuration})

    set(imported_location "${_qt5Gui_install_prefix}/bin/${LIB_LOCATION}")

    set(imported_implib "${_qt5Gui_install_prefix}/lib/${IMPLIB_LOCATION}")

    set_target_properties(Qt5::${TargetName} PROPERTIES
        "IMPORTED_LOCATION_${Configuration}" ${imported_location}
        "IMPORTED_IMPLIB_${Configuration}" ${imported_implib}
    )
endmacro()

add_library(Qt5::Gui_EGL SHARED IMPORTED)
_populate_qt5gui_gl_target_properties(Gui_EGL RELEASE libEGL.dll libEGL.lib)
add_library(Qt5::Gui_GLESv2 SHARED IMPORTED)
_populate_qt5gui_gl_target_properties(Gui_GLESv2 RELEASE libGLESv2.dll libGLESv2.lib)

set_property(TARGET Qt5::Gui_EGL APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${Qt5Gui_EGL_INCLUDE_DIRS})
set_property(TARGET Qt5::Gui_GLESv2 APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${Qt5Gui_OPENGL_INCLUDE_DIRS})

_populate_qt5gui_gl_target_properties(Gui_EGL DEBUG libEGLd.dll libEGLd.lib)
_populate_qt5gui_gl_target_properties(Gui_GLESv2 DEBUG libGLESv2d.dll libGLESv2d.lib)

set(Qt5Gui_EGL_LIBRARIES Qt5::Gui_EGL)
set(Qt5Gui_OPENGL_LIBRARIES Qt5::Gui_GLESv2)


set(Qt5Gui_OPENGL_IMPLEMENTATION GLESv2)

get_target_property(_configs Qt5::Gui IMPORTED_CONFIGURATIONS)
foreach(_config ${_configs})
    set_property(TARGET Qt5::Gui APPEND PROPERTY
        IMPORTED_LINK_DEPENDENT_LIBRARIES_${_config}
        ${Qt5Gui_EGL_LIBRARIES} ${Qt5Gui_OPENGL_LIBRARIES}
    )
endforeach()
unset(_configs)
