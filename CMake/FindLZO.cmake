find_path(LZO_INCLUDE_DIR lzo/lzo1x.h)
find_library(LZO_LIBRARY lzo2)
mark_as_advanced(LZO_INCLUDE_DIR LZO_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LZO DEFAULT_MSG
	LZO_INCLUDE_DIR LZO_LIBRARY)

if(LZO_FOUND AND NOT TARGET LZO::LZO)
  add_library(LZO::LZO UNKNOWN IMPORTED)
  set_target_properties(LZO::LZO PROPERTIES
    IMPORTED_LOCATION "${LZO_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${LZO_INCLUDE_DIR}"
  )
endif()
