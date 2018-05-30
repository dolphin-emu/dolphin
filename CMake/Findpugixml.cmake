find_path(pugixml_INCLUDE_DIRS pugixml.hpp)
find_library(pugixml_LIBRARIES NAMES pugixml)
mark_as_advanced(pugixml_INCLUDE_DIRS pugixml_LIBRARIES)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(pugixml REQUIRED_VARS pugixml_LIBRARIES pugixml_INCLUDE_DIRS)

if(pugixml_FOUND AND NOT TARGET pugixml)
  add_library(pugixml UNKNOWN IMPORTED)
  set_target_properties(pugixml PROPERTIES
    IMPORTED_LOCATION "${pugixml_LIBRARIES}"
    INTERFACE_INCLUDE_DIRECTORIES "${pugixml_INCLUDE_DIRS}"
  )
endif()
