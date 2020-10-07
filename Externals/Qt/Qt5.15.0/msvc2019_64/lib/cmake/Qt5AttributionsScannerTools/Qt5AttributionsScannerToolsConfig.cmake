if (CMAKE_VERSION VERSION_LESS 2.8.3)
    message(FATAL_ERROR "Qt 5 requires at least CMake version 2.8.3")
endif()

get_filename_component(_qt5_qattributionsscannertools_install_prefix "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(_qt5_AttributionsScannerTools_check_file_exists file)
    if(NOT EXISTS "${file}" )
        message(FATAL_ERROR "The package \"Qt5AttributionsScannerTools\" references the file
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

if (NOT TARGET Qt5::qtattributionsscanner)
    add_executable(Qt5::qtattributionsscanner IMPORTED)

    set(imported_location "${_qt5_qattributionsscannertools_install_prefix}/bin/qtattributionsscanner")
    _qt5_AttributionsScannerTools_check_file_exists(${imported_location})

    set_target_properties(Qt5::qtattributionsscanner PROPERTIES
        IMPORTED_LOCATION ${imported_location}
    )
endif()

# Create versionless tool targets.
foreach(__qt_tool qtattributionsscanner)
    if(NOT "${QT_NO_CREATE_VERSIONLESS_TARGETS}" AND NOT TARGET Qt::${__qt_tool}
       AND TARGET Qt5::${__qt_tool})
        add_executable(Qt::${__qt_tool} IMPORTED)
        get_target_property(__qt_imported_location Qt5::${__qt_tool} IMPORTED_LOCATION)
        set_target_properties(Qt::${__qt_tool}
                              PROPERTIES IMPORTED_LOCATION "${__qt_imported_location}")
    endif()
endforeach()
