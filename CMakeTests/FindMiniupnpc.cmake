# This file only works for MiniUPnPc 1.7 or later (it requires MINIUPNPC_API_VERSION).
# TODO Find out if any distribution still ships with /usr/include/miniupnpc.h (i.e. not in a separate directory).

find_path(MINIUPNPC_INCLUDE_DIR miniupnpc.h PATH_SUFFIXES miniupnpc)
find_library(MINIUPNPC_LIBRARY miniupnpc)

if(MINIUPNPC_INCLUDE_DIR)
	file(STRINGS "${MINIUPNPC_INCLUDE_DIR}/miniupnpc.h" MINIUPNPC_API_VERSION_STR REGEX "^#define[\t ]+MINIUPNPC_API_VERSION[\t ]+[0-9]+")
	string(REGEX REPLACE "^#define[\t ]+MINIUPNPC_API_VERSION[\t ]+([0-9]+)" "\\1" MINIUPNPC_API_VERSION ${MINIUPNPC_API_VERSION_STR})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MINIUPNPC DEFAULT_MSG MINIUPNPC_INCLUDE_DIR MINIUPNPC_LIBRARY MINIUPNPC_API_VERSION)

set(MINIUPNPC_LIBRARIES ${MINIUPNPC_LIBRARY})
set(MINIUPNPC_INCLUDE_DIRS ${MINIUPNPC_INCLUDE_DIR})
mark_as_advanced(MINIUPNPC_INCLUDE_DIR MINIUPNPC_LIBRARY MINIUPNPC_API_VERSION_STR)
