find_path(CUBEB_INCLUDE_DIR cubeb.h PATH_SUFFIXES cubeb)
find_library(CUBEB_LIBRARY cubeb)
mark_as_advanced(CUBEB_INCLUDE_DIR CUBEB_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CUBEB DEFAULT_MSG
	CUBEB_INCLUDE_DIR CUBEB_LIBRARY)

if(CUBEB_FOUND AND NOT TARGET CUBEB)
  add_library(cubeb::cubeb UNKNOWN IMPORTED)
  set_target_properties(cubeb::cubeb PROPERTIES
    IMPORTED_LOCATION "${CUBEB_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${CUBEB_INCLUDE_DIR}"
  )
endif()
