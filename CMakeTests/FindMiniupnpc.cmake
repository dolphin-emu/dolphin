# Locate miniupnp library
# This module defines
#  MINIUPNP_FOUND, if false, do not try to link to miniupnp
#  MINIUPNP_LIBRARY, the miniupnp variant
#  MINIUPNP_INCLUDE_DIR, where to find miniupnpc.h and family)
#  MINIUPNPC_VERSION_PRE1_6 --> set if we detect the version of miniupnpc is
#                               pre 1.6
#  MINIUPNPC_VERSION_PRE1_5 --> set if we detect the version of miniupnpc is
#                               pre 1.5
#
# Note that the expected include convention is
#  #include "miniupnpc.h"
# and not
#  #include <miniupnpc/miniupnpc.h>
# This is because, the miniupnpc location is not standardized and may exist
# in locations other than miniupnpc/

#=============================================================================
# Copyright 2011 Mark Vejvoda
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distributed this file outside of CMake, substitute the full
#  License text for the above reference.)

if (MINIUPNP_INCLUDE_DIR AND MINIUPNP_LIBRARY)
	# Already in cache, be silent
	set(MINIUPNP_FIND_QUIETLY TRUE)
endif (MINIUPNP_INCLUDE_DIR AND MINIUPNP_LIBRARY)

find_path(MINIUPNP_INCLUDE_DIR miniupnpc.h
	PATH_SUFFIXES miniupnpc)
find_library(MINIUPNP_LIBRARY miniupnpc)

if (MINIUPNP_INCLUDE_DIR AND MINIUPNP_LIBRARY)
	set (MINIUPNP_FOUND TRUE)
endif ()

if (MINIUPNP_FOUND)
	if (NOT MINIUPNP_FIND_QUIETLY)
		message (STATUS "Found the miniupnpc libraries at ${MINIUPNP_LIBRARY}")
		message (STATUS "Found the miniupnpc headers at ${MINIUPNP_INCLUDE_DIR}")
	endif (NOT MINIUPNP_FIND_QUIETLY)

	message(STATUS "Detecting version of miniupnpc in path: ${MINIUPNP_INCLUDE_DIR}")

	set(CMAKE_REQUIRED_INCLUDES ${MINIUPNP_INCLUDE_DIR})
	set(CMAKE_REQUIRED_LIBRARIES ${MINIUPNP_LIBRARY})
	check_cxx_source_runs("
	#include <miniwget.h>
	#include <miniupnpc.h>
	#include <upnpcommands.h>
	#include <stdio.h>
	int main()
	{
	static struct UPNPUrls urls;
	static struct IGDdatas data;

	GetUPNPUrls (&urls, &data, \"myurl\",0);

	return 0;
	}"
	MINIUPNPC_VERSION_1_7_OR_HIGHER)

IF (NOT MINIUPNPC_VERSION_1_7_OR_HIGHER)
	set(CMAKE_REQUIRED_INCLUDES ${MINIUPNP_INCLUDE_DIR})
	set(CMAKE_REQUIRED_LIBRARIES ${MINIUPNP_LIBRARY})
	check_cxx_source_runs("
	#include <miniwget.h>
	#include <miniupnpc.h>
	#include <upnpcommands.h>
	#include <stdio.h>
	int main()
	{
	struct UPNPDev *devlist = NULL;
	int upnp_delay = 5000;
	const char *upnp_multicastif = NULL;
	const char *upnp_minissdpdsock = NULL;
	int upnp_sameport = 0;
	int upnp_ipv6 = 0;
	int upnp_error = 0;
	devlist = upnpDiscover(upnp_delay, upnp_multicastif, upnp_minissdpdsock, upnp_sameport, upnp_ipv6, &upnp_error);

	return 0;
	}"
	MINIUPNPC_VERSION_PRE1_7)
   ENDIF()

   IF (NOT MINIUPNPC_VERSION_PRE1_7 AND NOT MINIUPNPC_VERSION_1_7_OR_HIGHER)
	   set(CMAKE_REQUIRED_INCLUDES ${MINIUPNP_INCLUDE_DIR})
	   set(CMAKE_REQUIRED_LIBRARIES ${MINIUPNP_LIBRARY})
	   check_cxx_source_runs("
	   #include <miniwget.h>
	   #include <miniupnpc.h>
	   #include <upnpcommands.h>
	   #include <stdio.h>
	   int main()
	   {
	   struct UPNPDev *devlist = NULL;
	   int upnp_delay = 5000;
	   const char *upnp_multicastif = NULL;
	   const char *upnp_minissdpdsock = NULL;
	   int upnp_sameport = 0;
	   int upnp_ipv6 = 0;
	   int upnp_error = 0;
	   devlist = upnpDiscover(upnp_delay, upnp_multicastif, upnp_minissdpdsock, upnp_sameport);

	   return 0;
	   }"
	   MINIUPNPC_VERSION_PRE1_6)

   ENDIF()

   IF (NOT MINIUPNPC_VERSION_PRE1_6 AND NOT MINIUPNPC_VERSION_PRE1_7 AND NOT MINIUPNPC_VERSION_1_7_OR_HIGHER)
	   set(CMAKE_REQUIRED_INCLUDES ${MINIUPNP_INCLUDE_DIR})
	   set(CMAKE_REQUIRED_LIBRARIES ${MINIUPNP_LIBRARY})
	   check_cxx_source_runs("
	   #include <miniwget.h>
	   #include <miniupnpc.h>
	   #include <upnpcommands.h>
	   #include <stdio.h>
	   static struct UPNPUrls urls;
	   static struct IGDdatas data;
	   int main()
	   {
	   char externalIP[16]     = \"\";
	   UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype, externalIP);

	   return 0;
	   }"
	   MINIUPNPC_VERSION_1_5_OR_HIGHER)
	ENDIF()

	IF (NOT MINIUPNPC_VERSION_1_5_OR_HIGHER AND NOT MINIUPNPC_VERSION_PRE1_6 AND NOT MINIUPNPC_VERSION_PRE1_7 AND NOT MINIUPNPC_VERSION_1_7_OR_HIGHER)
		set(CMAKE_REQUIRED_INCLUDES ${MINIUPNP_INCLUDE_DIR})
		set(CMAKE_REQUIRED_LIBRARIES ${MINIUPNP_LIBRARY})
		check_cxx_source_runs("
		#include <miniwget.h>
		#include <miniupnpc.h>
		#include <upnpcommands.h>
		#include <stdio.h>
		static struct UPNPUrls urls;
		static struct IGDdatas data;
		int main()
		{
		char externalIP[16]     = \"\";
		UPNP_GetExternalIPAddress(urls.controlURL, data.servicetype, externalIP);

		return 0;
		}"
		MINIUPNPC_VERSION_PRE1_5)

ENDIF()

IF(MINIUPNPC_VERSION_PRE1_5)
	message(STATUS "Found miniupnpc version is pre v1.5")
ENDIF()
IF(MINIUPNPC_VERSION_PRE1_6)
	message(STATUS "Found miniupnpc version is pre v1.6")
ENDIF()
IF(MINIUPNPC_VERSION_PRE1_7)
	message(STATUS "Found miniupnpc version is pre v1.7")
ENDIF()

IF(NOT MINIUPNPC_VERSION_PRE1_5 AND NOT MINIUPNPC_VERSION_PRE1_6 AND NOT MINIUPNPC_VERSION_PRE1_7)
	message(STATUS "Found miniupnpc version is v1.7 or higher")
ENDIF()

else ()
	message (STATUS "Could not find miniupnp")
endif ()

MARK_AS_ADVANCED(MINIUPNP_INCLUDE_DIR MINIUPNP_LIBRARY)

