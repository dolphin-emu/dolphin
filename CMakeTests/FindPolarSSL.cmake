# Locate polarssl library
# This module defines
#  POLARSSL_FOUND
#  POLARSSL_LIBRARY
#  POLARSSL_INCLUDE_DIR
#  POLARSSL_WORKS, this is true if polarssl is found and contains the methods
#  needed by dolphin-emu

# validate cached values (but use them as hints)
set(POLARSSL_INCLUDE_DIR_HINT POLARSSL_INCLUDE_DIR)
set(POLARSSL_LIBRARY_HINT POLARSSL_LIBRARY)
unset(POLARSSL_INCLUDE_DIR CACHE)
unset(POLARSSL_LIBRARY CACHE)
find_path(POLARSSL_INCLUDE_DIR polarssl/ssl.h HINTS ${POLARSSL_INCLUDE_DIR_HINT})
find_library(POLARSSL_LIBRARY polarssl HINTS ${POLARSSL_LIBRARY_HINT})

if(POLARSSL_INCLUDE_DIR STREQUAL POLARSSL_INCLUDE_DIR_HINT AND
   POLARSSL_LIBRARY     STREQUAL POLARSSL_LIBRARY_HINT)
	# using cached values, be silent
	set(POLARSSL_FIND_QUIETLY TRUE)
endif()

if (POLARSSL_INCLUDE_DIR AND POLARSSL_LIBRARY)
	set (POLARSSL_FOUND TRUE)
endif ()

if (POLARSSL_FOUND)
	if (NOT POLARSSL_FIND_QUIETLY)
		message (STATUS "Found the polarssl libraries at ${POLARSSL_LIBRARY}")
		message (STATUS "Found the polarssl headers at ${POLARSSL_INCLUDE_DIR}")
	endif (NOT POLARSSL_FIND_QUIETLY)

	set(CMAKE_REQUIRED_INCLUDES ${POLARSSL_INCLUDE_DIR})
	set(CMAKE_REQUIRED_LIBRARIES ${POLARSSL_LIBRARY})
	unset(POLARSSL_WORKS CACHE)
	check_cxx_source_compiles("
	#include <polarssl/ctr_drbg.h>
	#include <polarssl/entropy.h>
	#include <polarssl/net.h>
	#include <polarssl/ssl.h>
	#include <polarssl/version.h>

	#if POLARSSL_VERSION_NUMBER < 0x01030000
	#error \"Shared PolarSSL version is too old\"
	#endif

	int main()
	{
		ssl_context ctx;
		ssl_session session;
		entropy_context entropy;
		ctr_drbg_context ctr_drbg;
		x509_crt cacert;
		x509_crt clicert;
		pk_context pk;

		ssl_init(&ctx);
		entropy_init(&entropy);

		const char* pers = \"dolphin-emu\";
		ctr_drbg_init(&ctr_drbg, entropy_func,
		                    &entropy,
		                    (const unsigned char*)pers,
		                    strlen(pers));
	
		ssl_set_rng(&ctx, ctr_drbg_random, &ctr_drbg);
		ssl_set_session(&ctx, &session);

		ssl_close_notify(&ctx);
		ssl_session_free(&session);
		ssl_free(&ctx);
		entropy_free(&entropy);

		return 0;
	}"
	POLARSSL_WORKS)
else ()
	message (STATUS "Could not find polarssl")
endif ()

mark_as_advanced(POLARSSL_INCLUDE_DIR POLARSSL_LIBRARY)

