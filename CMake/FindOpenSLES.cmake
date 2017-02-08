# - Try to find OpenSLES
# Once done this will define
#  OPENSLES_FOUND - System has OpenSLES
#  OPENSLES_INCLUDE_DIR - The OpenSLES include directory
#  OPENSLES_LIBRARY - The library needed to use OpenSLES
# An imported target OpenSLES::OpenSLES is also created, prefer this

find_path(OPENSLES_INCLUDE_DIR
  NAMES SLES/OpenSLES.h
)

find_library(OPENSLES_LIBRARY
  NAMES OpenSLES
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenSLES  DEFAULT_MSG
                                  OPENSLES_LIBRARY OPENSLES_INCLUDE_DIR)

if(OpenSLES_FOUND)
  if(NOT TARGET OpenSLES::OpenSLES)
    add_library(OpenSLES::OpenSLES UNKNOWN IMPORTED)
    set_target_properties(OpenSLES::OpenSLES PROPERTIES
      IMPORTED_LOCATION ${OPENSLES_LIBRARY}
      INTERFACE_INCLUDE_DIRECTORIES ${OPENSLES_INCLUDE_DIR}
    )
  endif()
endif()

mark_as_advanced(OPENSLES_INCLUDE_DIR OPENSLES_LIBRARY )
