/*
 *  Debugging routines
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

#if defined(POLARSSL_DEBUG_C)

#include "polarssl/debug.h"

#include <stdarg.h>
#include <stdlib.h>

#if defined _MSC_VER && !defined  snprintf
#define  snprintf  _snprintf
#endif

#if defined _MSC_VER && !defined vsnprintf
#define vsnprintf _vsnprintf
#endif

char *debug_fmt( const char *format, ... )
{
    va_list argp;
    static char str[512];
    int maxlen = sizeof( str ) - 1;

    va_start( argp, format );
    vsnprintf( str, maxlen, format, argp );
    va_end( argp );

    str[maxlen] = '\0';
    return( str );
}

void debug_print_msg( const ssl_context *ssl, int level,
                      const char *file, int line, const char *text )
{
    char str[512];
    int maxlen = sizeof( str ) - 1;

    if( ssl->f_dbg == NULL )
        return;

    snprintf( str, maxlen, "%s(%04d): %s\n", file, line, text );
    str[maxlen] = '\0';
    ssl->f_dbg( ssl->p_dbg, level, str );
}

void debug_print_ret( const ssl_context *ssl, int level,
                      const char *file, int line,
                      const char *text, int ret )
{
    char str[512];
    int maxlen = sizeof( str ) - 1;

    if( ssl->f_dbg == NULL )
        return;

    snprintf( str, maxlen, "%s(%04d): %s() returned %d (0x%x)\n",
              file, line, text, ret, ret );

    str[maxlen] = '\0';
    ssl->f_dbg( ssl->p_dbg, level, str );
}

void debug_print_buf( const ssl_context *ssl, int level,
                      const char *file, int line, const char *text,
                      unsigned char *buf, size_t len )
{
    char str[512];
    size_t i, maxlen = sizeof( str ) - 1;

    if( ssl->f_dbg == NULL )
        return;

    snprintf( str, maxlen, "%s(%04d): dumping '%s' (%d bytes)\n",
              file, line, text, (unsigned int) len );

    str[maxlen] = '\0';
    ssl->f_dbg( ssl->p_dbg, level, str );

    for( i = 0; i < len; i++ )
    {
        if( i >= 4096 )
            break;

        if( i % 16 == 0 )
        {
            if( i > 0 )
                ssl->f_dbg( ssl->p_dbg, level, "\n" );

            snprintf( str, maxlen, "%s(%04d): %04x: ", file, line,
                      (unsigned int) i );

            str[maxlen] = '\0';
            ssl->f_dbg( ssl->p_dbg, level, str );
        }

        snprintf( str, maxlen, " %02x", (unsigned int) buf[i] );

        str[maxlen] = '\0';
        ssl->f_dbg( ssl->p_dbg, level, str );
    }

    if( len > 0 )
        ssl->f_dbg( ssl->p_dbg, level, "\n" );
}

void debug_print_mpi( const ssl_context *ssl, int level,
                      const char *file, int line,
                      const char *text, const mpi *X )
{
    char str[512];
    int j, k, maxlen = sizeof( str ) - 1, zeros = 1;
    size_t i, n;

    if( ssl->f_dbg == NULL || X == NULL )
        return;

    for( n = X->n - 1; n > 0; n-- )
        if( X->p[n] != 0 )
            break;

    for( j = ( sizeof(t_uint) << 3 ) - 1; j >= 0; j-- )
        if( ( ( X->p[n] >> j ) & 1 ) != 0 )
            break;

    snprintf( str, maxlen, "%s(%04d): value of '%s' (%d bits) is:\n",
              file, line, text, 
              (int) ( ( n * ( sizeof(t_uint) << 3 ) ) + j + 1 ) );

    str[maxlen] = '\0';
    ssl->f_dbg( ssl->p_dbg, level, str );

    for( i = n + 1, j = 0; i > 0; i-- )
    {
        if( zeros && X->p[i - 1] == 0 )
            continue;

        for( k = sizeof( t_uint ) - 1; k >= 0; k-- )
        {
            if( zeros && ( ( X->p[i - 1] >> (k << 3) ) & 0xFF ) == 0 )
                continue;
            else
                zeros = 0;

            if( j % 16 == 0 )
            {
                if( j > 0 )
                    ssl->f_dbg( ssl->p_dbg, level, "\n" );

                snprintf( str, maxlen, "%s(%04d): ", file, line );

                str[maxlen] = '\0';
                ssl->f_dbg( ssl->p_dbg, level, str );
            }

            snprintf( str, maxlen, " %02x", (unsigned int)
                      ( X->p[i - 1] >> (k << 3) ) & 0xFF );

            str[maxlen] = '\0';
            ssl->f_dbg( ssl->p_dbg, level, str );

            j++;
        }

    }

    if( zeros == 1 )
    {
        snprintf( str, maxlen, "%s(%04d): ", file, line );

        str[maxlen] = '\0';
        ssl->f_dbg( ssl->p_dbg, level, str );
        ssl->f_dbg( ssl->p_dbg, level, " 00" );
    }

    ssl->f_dbg( ssl->p_dbg, level, "\n" );
}

void debug_print_crt( const ssl_context *ssl, int level,
                      const char *file, int line,
                      const char *text, const x509_cert *crt )
{
    char str[1024], prefix[64];
    int i = 0, maxlen = sizeof( prefix ) - 1;

    if( ssl->f_dbg == NULL || crt == NULL )
        return;

    snprintf( prefix, maxlen, "%s(%04d): ", file, line );
    prefix[maxlen] = '\0';
    maxlen = sizeof( str ) - 1;

    while( crt != NULL )
    {
        char buf[1024];
        x509parse_cert_info( buf, sizeof( buf ) - 1, prefix, crt );

        snprintf( str, maxlen, "%s(%04d): %s #%d:\n%s",
                  file, line, text, ++i, buf );

        str[maxlen] = '\0';
        ssl->f_dbg( ssl->p_dbg, level, str );

        debug_print_mpi( ssl, level, file, line,
                         "crt->rsa.N", &crt->rsa.N );

        debug_print_mpi( ssl, level, file, line,
                         "crt->rsa.E", &crt->rsa.E );

        crt = crt->next;
    }
}

#endif
