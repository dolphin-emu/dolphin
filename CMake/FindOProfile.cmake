# - Try to find OProfile
# Once done this will define
# OPROFILE_FOUND - System has OProfile
# OPROFILE_INCLUDE_DIRS - The OProfile include directories
# OPROFILE_LIBRARIES - The libraries needed to use OProfile

find_path(OPROFILE_INCLUDE_DIR opagent.h)

find_library(OPROFILE_LIBRARY opagent
  PATH_SUFFIXES oprofile
)

set(OPROFILE_INCLUDE_DIRS ${OPROFILE_INCLUDE_DIR})
set(OPROFILE_LIBRARIES ${OPROFILE_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OProfile DEFAULT_MSG
  OPROFILE_LIBRARY OPROFILE_INCLUDE_DIR
)

if (OPROFILE_FOUND)
  if (NOT TARGET OProfile::OProfile)
    add_library(OProfile::OProfile UNKNOWN IMPORTED)
    set_target_properties(OProfile::OProfile PROPERTIES
      IMPORTED_LOCATION ${OPROFILE_LIBRARIES}
      INTERFACE_INCLUDE_DIRECTORIES ${OPROFILE_INCLUDE_DIRS}
    )
  endif()
endif()

mark_as_advanced(OPROFILE_INCLUDE_DIR OPROFILE_LIBRARY)
