/*
 *  Version information
 *
 *  Copyright (C) 2006-2010, Brainspark B.V.
 *
 *  This file is part of PolarSSL (http://www.polarssl.org)
 *  Lead Maintainer: Paul Bakker <polarssl_maintainer at polarssl.org>
 *
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "polarssl/config.h"

#if defined(POLARSSL_VERSION_C)

#include "polarssl/version.h"
#include <string.h>

const char version[] = POLARSSL_VERSION_STRING;

unsigned int version_get_number()
{
    return POLARSSL_VERSION_NUMBER;
}

void version_get_string( char *string )
{
    memcpy( string, POLARSSL_VERSION_STRING, sizeof( POLARSSL_VERSION_STRING ) );
}

void version_get_string_full( char *string )
{
    memcpy( string, POLARSSL_VERSION_STRING_FULL, sizeof( POLARSSL_VERSION_STRING_FULL ) );
}

#endif /* POLARSSL_VERSION_C */
