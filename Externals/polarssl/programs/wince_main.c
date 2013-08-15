/*
 *  Windows CE console application entry point
 *
 *  Copyright (C) 2006-2011, Brainspark B.V.
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

#if defined(_WIN32_WCE)

#include <windows.h>

extern int main( int, const char ** );

int _tmain( int argc, _TCHAR* targv[] )
{
    char **argv;
    int i;

    argv = ( char ** ) calloc( argc, sizeof( char * ) );

    for ( i = 0; i < argc; i++ ) {
        size_t len;
        len = _tcslen( targv[i] ) + 1;
        argv[i] = ( char * ) calloc( len, sizeof( char ) );
        wcstombs( argv[i], targv[i], len );
    }

    return main( argc, argv );
}

#endif  /* defined(_WIN32_WCE) */
