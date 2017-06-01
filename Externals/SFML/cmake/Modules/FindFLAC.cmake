#
# Try to find FLAC libraries and include paths.
# Once done this will define
#
# FLAC_FOUND
# FLAC_INCLUDE_DIR
# FLAC_LIBRARY
#

find_path(FLAC_INCLUDE_DIR FLAC/all.h)
find_path(FLAC_INCLUDE_DIR FLAC/stream_decoder.h)

find_library(FLAC_LIBRARY NAMES FLAC)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FLAC DEFAULT_MSG FLAC_LIBRARY FLAC_INCLUDE_DIR)

mark_as_advanced(FLAC_INCLUDE_DIR FLAC_LIBRARY)
