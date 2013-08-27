/*
 *  SSL session cache implementation
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
/*
 * These session callbacks use a simple chained list
 * to store and retrieve the session information.
 */

#include "polarssl/config.h"

#if defined(POLARSSL_SSL_CACHE_C)

#include "polarssl/ssl_cache.h"

#include <stdlib.h>

void ssl_cache_init( ssl_cache_context *cache )
{
    memset( cache, 0, sizeof( ssl_cache_context ) );

    cache->timeout = SSL_CACHE_DEFAULT_TIMEOUT;
    cache->max_entries = SSL_CACHE_DEFAULT_MAX_ENTRIES;
}

int ssl_cache_get( void *data, ssl_session *session )
{
    time_t t = time( NULL );
    ssl_cache_context *cache = (ssl_cache_context *) data;
    ssl_cache_entry *cur, *entry;

    cur = cache->chain;
    entry = NULL;

    while( cur != NULL )
    {
        entry = cur;
        cur = cur->next;

        if( cache->timeout != 0 &&
            (int) ( t - entry->timestamp ) > cache->timeout )
            continue;

        if( session->ciphersuite != entry->session.ciphersuite ||
            session->compression != entry->session.compression ||
            session->length != entry->session.length )
            continue;

        if( memcmp( session->id, entry->session.id,
                    entry->session.length ) != 0 )
            continue;

        memcpy( session->master, entry->session.master, 48 );

        /*
         * Restore peer certificate (without rest of the original chain)
         */
        if( entry->peer_cert.p != NULL )
        {
            session->peer_cert = (x509_cert *) malloc( sizeof(x509_cert) );
            if( session->peer_cert == NULL )
                return( 1 );

            memset( session->peer_cert, 0, sizeof(x509_cert) );
            if( x509parse_crt( session->peer_cert, entry->peer_cert.p,
                               entry->peer_cert.len ) != 0 )
            {
                free( session->peer_cert );
                session->peer_cert = NULL;
                return( 1 );
            }
        }

        return( 0 );
    }

    return( 1 );
}

int ssl_cache_set( void *data, const ssl_session *session )
{
    time_t t = time( NULL ), oldest = 0;
    ssl_cache_context *cache = (ssl_cache_context *) data;
    ssl_cache_entry *cur, *prv, *old = NULL;
    int count = 0;

    cur = cache->chain;
    prv = NULL;

    while( cur != NULL )
    {
        count++;

        if( cache->timeout != 0 &&
            (int) ( t - cur->timestamp ) > cache->timeout )
        {
            cur->timestamp = t;
            break; /* expired, reuse this slot, update timestamp */
        }

        if( memcmp( session->id, cur->session.id, cur->session.length ) == 0 )
            break; /* client reconnected, keep timestamp for session id */

        if( oldest == 0 || cur->timestamp < oldest )
        {
            oldest = cur->timestamp;
            old = cur;
        }

        prv = cur;
        cur = cur->next;
    }

    if( cur == NULL )
    {
        /*
         * Reuse oldest entry if max_entries reached
         */
        if( old != NULL && count >= cache->max_entries )
        {
            cur = old;
            memset( &cur->session, 0, sizeof(ssl_session) );
            if( cur->peer_cert.p != NULL )
            {
                free( cur->peer_cert.p );
                memset( &cur->peer_cert, 0, sizeof(x509_buf) );
            }
        }
        else
        {
            cur = (ssl_cache_entry *) malloc( sizeof(ssl_cache_entry) );
            if( cur == NULL )
                return( 1 );

            memset( cur, 0, sizeof(ssl_cache_entry) );

            if( prv == NULL )
                cache->chain = cur;
            else
                prv->next = cur;
        }

        cur->timestamp = t;
    }

    memcpy( &cur->session, session, sizeof( ssl_session ) );
    
    /*
     * Store peer certificate
     */
    if( session->peer_cert != NULL )
    {
        cur->peer_cert.p = (unsigned char *) malloc( session->peer_cert->raw.len );
        if( cur->peer_cert.p == NULL )
            return( 1 );

        memcpy( cur->peer_cert.p, session->peer_cert->raw.p,
                session->peer_cert->raw.len );
        cur->peer_cert.len = session->peer_cert->raw.len;

        cur->session.peer_cert = NULL;
    }

    return( 0 );
}

void ssl_cache_set_timeout( ssl_cache_context *cache, int timeout )
{
    if( timeout < 0 ) timeout = 0;

    cache->timeout = timeout;
}

void ssl_cache_set_max_entries( ssl_cache_context *cache, int max )
{
    if( max < 0 ) max = 0;

    cache->max_entries = max;
}

void ssl_cache_free( ssl_cache_context *cache )
{
    ssl_cache_entry *cur, *prv;

    cur = cache->chain;

    while( cur != NULL )
    {
        prv = cur;
        cur = cur->next;

        ssl_session_free( &prv->session );

        if( prv->peer_cert.p != NULL )
            free( prv->peer_cert.p );

        free( prv );
    }
}

#endif /* POLARSSL_SSL_CACHE_C */
