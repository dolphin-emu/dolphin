find_path(HIDAPI_INCLUDE_DIR NAMES hidapi.h PATH_SUFFIXES hidapi)
find_library(HIDAPI_LIBRARY  NAMES hidapi hidapi-hidraw hidapi-libusb)
set(HIDAPI_LIBRARIES  ${HIDAPI_LIBRARY})
set(HIDAPI_INCLUDE_DIRS ${HIDAPI_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(HIDAPI DEFAULT_MSG HIDAPI_LIBRARY HIDAPI_INCLUDE_DIR)

if(HIDAPI_FOUND AND NOT TARGET Hidapi::Hidapi)
    add_library(Hidapi::Hidapi UNKNOWN IMPORTED)
    set_target_properties(Hidapi::Hidapi PROPERTIES
      IMPORTED_LOCATION ${HIDAPI_LIBRARIES}
      INTERFACE_INCLUDE_DIRECTORIES ${HIDAPI_INCLUDE_DIRS}
    )
endif()

mark_as_advanced(HIDAPI_INCLUDE_DIR HIDAPI_LIBRARY)
