/*
 *  Memory allocation layer
 *
 *  Copyright (C) 2006-2013, Brainspark B.V.
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

#if defined(POLARSSL_MEMORY_C)

#include "polarssl/memory.h"

#if !defined(POLARSSL_MEMORY_STDMALLOC)
static void *memory_malloc_uninit( size_t len )
{
    ((void) len);
    return( NULL );
}

#define POLARSSL_MEMORY_STDMALLOC   memory_malloc_uninit
#endif /* !POLARSSL_MEMORY_STDMALLOC */

#if !defined(POLARSSL_MEMORY_STDFREE)
static void memory_free_uninit( void *ptr )
{
    ((void) ptr);
}

#define POLARSSL_MEMORY_STDFREE     memory_free_uninit
#endif /* !POLARSSL_MEMORY_STDFREE */

void * (*polarssl_malloc)( size_t ) = POLARSSL_MEMORY_STDMALLOC;
void (*polarssl_free)( void * )     = POLARSSL_MEMORY_STDFREE;

int memory_set_own( void * (*malloc_func)( size_t ),
                    void (*free_func)( void * ) )
{
    polarssl_malloc = malloc_func;
    polarssl_free = free_func;

    return( 0 );
}

#endif /* POLARSSL_MEMORY_C */
