# Try to find libxcb
#
#
# Once done this will define:
#   LIBXCB_FOUND         - True if xcb was found
#   LIBXCB_INCLUDE_DIRS  - Directories containing the headers
#   LIBXCB_LIBRARIES     - List of libraries to link to
#
#   Also for each requested component:
#     LIBXCB_${UPPER_COMPONENT_NAME}_FOUND
#     LIBXCB_${UPPER_COMPONENT_NAME}_INCLUDE_DIRS
#     LIBXCB_${UPPER_COMPONENT_NAME}_LIBRARIES

include(FindPackageHandleStandardArgs)

IF(NOT WIN32)
    IF(LIBXCB_LIBRARIES AND LIBXCB_INCLUDE_DIR)
        set(XCB_FIND_QUIETLY TRUE)
    ENDIF()

    # Find xcb
    FIND_PATH(LIBXCB_INCLUDE_DIR xcb/xcb.h)
    FIND_LIBRARY(LIBXCB_LIBRARY NAMES xcb libxcb)

    # Add xcb info to LIBXCB_LIBRARIES and LIBXCB_INCLUDE_DIRS
    SET(LIBXCB_LIBRARIES ${LIBXCB_LIBRARY})
    SET(LIBXCB_INCLUDE_DIRS ${LIBXCB_INCLUDE_DIR})

    find_package_handle_standard_args(LIBXCB DEFAULT_MSG
        LIBXCB_LIBRARY LIBXCB_INCLUDE_DIR)

    mark_as_advanced(LIBXCB_LIBRARY LIBXCB_INCLUDE_DIR)

    # Check whether we should search for XLIB_XCB
    set(FIND_XLIB_XCB FALSE)
    FOREACH(XCB_COMPONENT ${XCB_FIND_COMPONENTS})
        # Generate upper string of the component name
        string(TOUPPER ${XCB_COMPONENT} XCB_COMPONENT_UPPER)

        IF(${XCB_COMPONENT_UPPER} MATCHES "XLIB_XCB")
            set(FIND_XLIB_XCB TRUE)
        ELSE()
            # XCB_COMPONENTS is generated to be a copy of XCB_FIND_COMPONENTS
            # without XLIB_XCB (for later component search)
            set(XCB_COMPONENTS ${XCB_COMPONENTS} ${XCB_COMPONENT})
        ENDIF()
    ENDFOREACH()

    # Find XLIB_XCB if requested
    IF(FIND_XLIB_XCB)
        FIND_PATH(XLIB_XCB_INCLUDE_DIR X11/Xlib-xcb.h)
        FIND_LIBRARY(XLIB_XCB_LIBRARY NAMES X11-xcb libX11-xcb)

        SET(XLIB_XCB_LIBRARIES ${XLIB_XCB_LIBRARY})
        SET(XLIB_XCB_INCLUDE_DIRS ${XLIB_XCB_INCLUDE_DIR})

        find_package_handle_standard_args(XLIB_XCB DEFAULT_MSG
            XLIB_XCB_LIBRARY LIBXCB_INCLUDE_DIR)

        mark_as_advanced(XLIB_XCB_LIBRARY XLIB_XCB_INCLUDE_DIR)

        # Add xlib_xcb info to LIBXCB_LIBRARIES and LIBXCB_INCLUDE_DIRS
        set(LIBXCB_LIBRARIES ${LIBXCB_LIBRARIES}  ${XLIB_XCB_LIBRARIES})
        set(LIBXCB_INCLUDE_DIRS ${LIBXCB_INCLUDE_DIRS}  ${XLIB_XCB_INCLUDE_DIR})

        if(NOT XLIB_XCB_FOUND)
            message(FATAL_ERROR "XlibXcb library not found")
        endif()
    ELSE()
        # Add component name to the component list
        set(${XCB_COMPONENTS} ${XCB_FIND_COMPONENTS})
    ENDIF()

    # Loop through requested xcb components (does not contain xlib_xcb)
    FOREACH(XCB_COMPONENT ${XCB_COMPONENTS})
        # Generate lower and upper string of the component name
        string(TOLOWER ${XCB_COMPONENT} XCB_COMPONENT_LOWER)
        string(TOUPPER ${XCB_COMPONENT} XCB_COMPONENT_UPPER)

        # Find the specific component
        FIND_LIBRARY(LIBXCB_${XCB_COMPONENT_UPPER}_LIBRARY
            NAMES libxcb-${XCB_COMPONENT_LOWER} xcb-${XCB_COMPONENT_LOWER})

        find_package_handle_standard_args(LIBXCB_${XCB_COMPONENT_UPPER} DEFAULT_MSG
            LIBXCB_${XCB_COMPONENT_UPPER}_LIBRARY LIBXCB_INCLUDE_DIR)

        mark_as_advanced(LIBXCB_${XCB_COMPONENT_UPPER}_LIBRARY)

        # Append the component's library path to LIBXCB_LIBRARIES
        set(LIBXCB_LIBRARIES ${LIBXCB_LIBRARIES} ${LIBXCB_${XCB_COMPONENT_UPPER}_LIBRARY})

        if(NOT LIBXCB_${XCB_COMPONENT_UPPER}_FOUND)
            message(FATAL_ERROR "xcb-${XCB_COMPONENT_LOWER} not found")
        endif()
    ENDFOREACH()

endif()
