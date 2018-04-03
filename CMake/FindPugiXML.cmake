# - Find PugiXML library
# Find the native PugiXML includes and library
#
# This module defines
#  PugiXML_INCLUDE_DIR, where to find PugiXML.h, etc.
#  PugiXML_LIBRARIES, libraries to link against to use PugiXML.
#  PugiXML_FOUND, if false, do not try to use PugiXML.

find_path(PugiXML_INCLUDE_DIR pugixml.hpp)
find_library(PugiXML_LIBRARIES NAMES pugixml)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PugiXML REQUIRED_VARS PugiXML_LIBRARIES PugiXML_INCLUDE_DIR)

mark_as_advanced(PugiXML_INCLUDE_DIR PugiXML_LIBRARY)
