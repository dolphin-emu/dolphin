/*
 *  Translate error code to error string
 *
 *  Copyright (C) 2006-2012, Brainspark B.V.
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

#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "polarssl/config.h"

#include "polarssl/error.h"

#define USAGE \
    "\n usage: strerror <errorcode>\n"

#if !defined(POLARSSL_ERROR_C) && !defined(POLARSSL_ERROR_STRERROR_DUMMY)
int main( int argc, char *argv[] )
{
    ((void) argc);
    ((void) argv);

    printf("POLARSSL_ERROR_C and/or POLARSSL_ERROR_STRERRO_DUMMY not defined.\n");
    return( 0 );
}
#else
int main( int argc, char *argv[] )
{
    int ret;

    if( argc != 2 )
    {
        printf( USAGE );
        return( 0 );
    }

    ret = atoi( argv[1] );
    if( ret > 0 )
        ret = - ret;

    if( ret != 0 )
    {
        char error_buf[200];
        error_strerror( ret, error_buf, 200 );
        printf("Last error was: %d - %s\n\n", ret, error_buf );
    }

#if defined(_WIN32)
    printf( "  + Press Enter to exit this program.\n" );
    fflush( stdout ); getchar();
#endif

    return( ret );
}
#endif /* POLARSSL_ERROR_C */
