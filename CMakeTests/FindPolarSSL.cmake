# Locate polarssl library
# This module defines
#  POLARSSL_FOUND
#  POLARSSL_LIBRARY
#  POLARSSL_INCLUDE_DIR
#  POLARSSL_WORKS, this is true if polarssl is found and contains the methods
#  needed by dolphin-emu

if(POLARSSL_INCLUDE_DIR AND POLARSSL_LIBRARY)
	# Already in cache, be silent
	set(POLARSSL_FIND_QUIETLY TRUE)
endif()

find_path(POLARSSL_INCLUDE_DIR polarssl/ssl.h)
find_library(POLARSSL_LIBRARY polarssl)

if (POLARSSL_INCLUDE_DIR AND POLARSSL_LIBRARY)
	set (POLARSSL_FOUND TRUE)
endif ()

if (POLARSSL_FOUND)
	if (NOT POLARSSL_FIND_QUIETLY)
		message (STATUS "Found the polarssl libraries at ${POLARSSL_LIBRARY}")
		message (STATUS "Found the polarssl headers at ${POLARSSL_INCLUDE_DIR}")
	endif (NOT POLARSSL_FIND_QUIETLY)

	message(STATUS "Checking to see if system version contains necessary methods")

	set(CMAKE_REQUIRED_INCLUDES ${POLARSSL_INCLUDE_DIR})
	set(CMAKE_REQUIRED_LIBRARIES ${POLARSSL_LIBRARY})
	check_cxx_source_compiles("
	#include <polarssl/net.h>
	#include <polarssl/ssl.h>
	#include <polarssl/havege.h>
	int main()
	{
	ssl_context ctx;
	ssl_session session;
	havege_state hs;

	ssl_init(&ctx);
	havege_init(&hs);
	ssl_set_rng(&ctx, havege_random, &hs);
	ssl_set_session(&ctx, &session);

	ssl_close_notify(&ctx);
	ssl_session_free(&session);
	ssl_free(&ctx);

	return 0;
	}"
	POLARSSL_WORKS)

else ()
	message (STATUS "Could not find polarssl")
endif ()

MARK_AS_ADVANCED(POLARSSL_INCLUDE_DIR POLARSSL_LIBRARY)

