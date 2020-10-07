
if (CMAKE_VERSION VERSION_LESS 3.1.0)
    message(FATAL_ERROR "Qt5 requires at least CMake version 3.1.0")
endif()

if (NOT Qt5_FIND_COMPONENTS)
    set(Qt5_NOT_FOUND_MESSAGE "The Qt5 package requires at least one component")
    set(Qt5_FOUND False)
    return()
endif()

set(_Qt5_FIND_PARTS_REQUIRED)
if (Qt5_FIND_REQUIRED)
    set(_Qt5_FIND_PARTS_REQUIRED REQUIRED)
endif()
set(_Qt5_FIND_PARTS_QUIET)
if (Qt5_FIND_QUIETLY)
    set(_Qt5_FIND_PARTS_QUIET QUIET)
endif()

get_filename_component(_qt5_install_prefix "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(_Qt5_NOTFOUND_MESSAGE)

include(${CMAKE_CURRENT_LIST_DIR}/Qt5ModuleLocation.cmake)

foreach(module ${Qt5_FIND_COMPONENTS})
    find_package(Qt5${module}
        ${_Qt5_FIND_PARTS_QUIET}
        ${_Qt5_FIND_PARTS_REQUIRED}
        PATHS ${_qt5_module_paths} NO_DEFAULT_PATH
    )
    if (NOT Qt5${module}_FOUND)
        string(CONFIGURE ${_qt5_module_location_template} _expected_module_location @ONLY)

        if (Qt5_FIND_REQUIRED_${module})
            set(_Qt5_NOTFOUND_MESSAGE "${_Qt5_NOTFOUND_MESSAGE}Failed to find Qt5 component \"${module}\" config file at \"${_expected_module_location}\"\n")
        elseif(NOT Qt5_FIND_QUIETLY)
            message(WARNING "Failed to find Qt5 component \"${module}\" config file at \"${_expected_module_location}\"")
        endif()

        unset(_expected_module_location)
    endif()
endforeach()

if (_Qt5_NOTFOUND_MESSAGE)
    set(Qt5_NOT_FOUND_MESSAGE "${_Qt5_NOTFOUND_MESSAGE}")
    set(Qt5_FOUND False)
endif()

_qt5_Core_check_file_exists("${CMAKE_CURRENT_LIST_DIR}/Qt5ConfigVersion.cmake")
