find_path(LIBMGBA_INCLUDE_DIR mgba/flags.h)
find_library(LIBMGBA_LIBRARY mgba)
mark_as_advanced(LIBMGBA_INCLUDE_DIR LIBMGBA_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBMGBA DEFAULT_MSG
	LIBMGBA_INCLUDE_DIR LIBMGBA_LIBRARY)

if(LIBMGBA_FOUND AND NOT mGBA::mgba)
  add_library(mGBA::mgba UNKNOWN IMPORTED)
  set_target_properties(mGBA::mgba PROPERTIES
    IMPORTED_LOCATION "${LIBMGBA_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBMGBA_INCLUDE_DIR}"
  )
endif()
