# - Try to find GNUTLS 
# Find GNUTLS headers, libraries and the answer to all questions.
#
#  GNUTLS_FOUND               True if gnutls got found
#  GNUTLS_INCLUDE_DIRS        Location of gnutls headers 
#  GNUTLS_LIBRARIES           List of libaries to use gnutls 
#
# Copyright (c) 2007 Bjoern Ricks <b.ricks@fh-osnabrueck.de>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

INCLUDE( FindPkgConfig )

IF ( GNUTLS_FIND_REQUIRED )
	SET( _pkgconfig_REQUIRED "REQUIRED" )
ELSE( GNUTLS_FIND_REQUIRED )
	SET( _pkgconfig_REQUIRED "" )
ENDIF ( GNUTLS_FIND_REQUIRED )

IF ( GNUTLS_MIN_VERSION )
	PKG_SEARCH_MODULE( GNUTLS ${_pkgconfig_REQUIRED} gnutls>=${GNUTLS_MIN_VERSION} )
ELSE ( GNUTLS_MIN_VERSION )
	PKG_SEARCH_MODULE( GNUTLS ${_pkgconfig_REQUIRED} gnutls )
ENDIF ( GNUTLS_MIN_VERSION )


IF( NOT GNUTLS_FOUND AND NOT PKG_CONFIG_FOUND )
	FIND_PATH( GNUTLS_INCLUDE_DIRS gnutls/gnutls.h )
	FIND_LIBRARY( GNUTLS_LIBRARIES gnutls)

	# Report results
	IF ( GNUTLS_LIBRARIES AND GNUTLS_INCLUDE_DIRS )	
		SET( GNUTLS_FOUND 1 )
		IF ( NOT GNUTLS_FIND_QUIETLY )
			MESSAGE( STATUS "Found gnutls: ${GNUTLS_LIBRARIES}" )
		ENDIF ( NOT GNUTLS_FIND_QUIETLY )
	ELSE ( GNUTLS_LIBRARIES AND GNUTLS_INCLUDE_DIRS )	
		IF ( GNUTLS_FIND_REQUIRED )
			MESSAGE( SEND_ERROR "Could NOT find gnutls" )
		ELSE ( GNUTLS_FIND_REQUIRED )
			IF ( NOT GNUTLS_FIND_QUIETLY )
				MESSAGE( STATUS "Could NOT find gnutls" )	
			ENDIF ( NOT GNUTLS_FIND_QUIETLY )
		ENDIF ( GNUTLS_FIND_REQUIRED )
	ENDIF ( GNUTLS_LIBRARIES AND GNUTLS_INCLUDE_DIRS )
ENDIF( NOT GNUTLS_FOUND AND NOT PKG_CONFIG_FOUND )

MARK_AS_ADVANCED( GNUTLS_LIBRARIES GNUTLS_INCLUDE_DIRS )
