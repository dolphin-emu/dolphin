/*
 *  SSL/TLS stress testing program
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

#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "polarssl/config.h"

#include "polarssl/net.h"
#include "polarssl/ssl.h"
#include "polarssl/entropy.h"
#include "polarssl/ctr_drbg.h"
#include "polarssl/timing.h"
#include "polarssl/certs.h"

#define OPMODE_NONE             0
#define OPMODE_CLIENT           1
#define OPMODE_SERVER           2

#define IOMODE_BLOCK            0
#define IOMODE_NONBLOCK         1

#define COMMAND_READ            1
#define COMMAND_WRITE           2
#define COMMAND_BOTH            3

#define DFL_OPMODE              OPMODE_NONE
#define DFL_IOMODE              IOMODE_BLOCK
#define DFL_SERVER_NAME         "localhost"
#define DFL_SERVER_PORT         4433
#define DFL_COMMAND             COMMAND_READ
#define DFL_BUFFER_SIZE         1024
#define DFL_MAX_BYTES           0
#define DFL_DEBUG_LEVEL         0
#define DFL_CONN_TIMEOUT        0
#define DFL_MAX_CONNECTIONS     0
#define DFL_SESSION_REUSE       1
#define DFL_SESSION_LIFETIME    86400
#define DFL_FORCE_CIPHER        0

/*
 * server-specific data
 */
const char *dhm_G = "4";
const char *dhm_P = 
"E4004C1F94182000103D883A448B3F802CE4B44A83301270002C20D0321CFD00" \
"11CCEF784C26A400F43DFB901BCA7538F2C6B176001CF5A0FD16D2C48B1D0C1C" \
"F6AC8E1DA6BCC3B4E1F96B0564965300FFA1D0B601EB2800F489AA512C4B248C" \
"01F76949A60BB7F00A40B1EAB64BDD48E8A700D60B7F1200FA8E77B0A979DABF";

int server_fd = -1;

/*
 * global options
 */
struct options
{
    int opmode;                 /* operation mode (client or server)    */
    int iomode;                 /* I/O mode (blocking or non-blocking)  */
    const char *server_name;    /* hostname of the server (client only) */
    int server_port;            /* port on which the ssl service runs   */
    int command;                /* what to do: read or write operation  */
    int buffer_size;            /* size of the send/receive buffer      */
    int max_bytes;              /* max. # of bytes before a reconnect   */
    int debug_level;            /* level of debugging                   */
    int conn_timeout;           /* max. delay before a reconnect        */
    int max_connections;        /* max. number of reconnections         */
    int session_reuse;          /* flag to reuse the keying material    */
    int session_lifetime;       /* if reached, session data is expired  */
    int force_ciphersuite[2];   /* protocol/ciphersuite to use, or all  */
};

/*
 * Although this PRNG has good statistical properties (eg. passes
 * DIEHARD), it is not cryptographically secure.
 */
unsigned long int lcppm5( unsigned long int *state )
{
    unsigned long int u, v;

    u = v = state[4] ^ 1;
    state[u & 3] ^= u;
    u ^= (v << 12) ^ (v >> 12);
    u ^= v * state[0]; v >>= 8;
    u ^= v * state[1]; v >>= 8;
    u ^= v * state[2]; v >>= 8;
    u ^= v * state[3];
    u &= 0xFFFFFFFF;
    state[4] = u;

    return( u );
}

void my_debug( void *ctx, int level, const char *str )
{
    if( level < ((struct options *) ctx)->debug_level )
        fprintf( stderr, "%s", str );
}

#if !defined(POLARSSL_BIGNUM_C) || !defined(POLARSSL_ENTROPY_C) ||  \
    !defined(POLARSSL_SSL_TLS_C) || !defined(POLARSSL_SSL_SRV_C) || \
    !defined(POLARSSL_SSL_CLI_C) || !defined(POLARSSL_NET_C) ||     \
    !defined(POLARSSL_RSA_C) || !defined(POLARSSL_CTR_DRBG_C)
int main( int argc, char *argv[] )
{
    ((void) argc);
    ((void) argv);

    printf("POLARSSL_BIGNUM_C and/or POLARSSL_ENTROPY_C and/or "
           "POLARSSL_SSL_TLS_C and/or POLARSSL_SSL_SRV_C and/or "
           "POLARSSL_SSL_CLI_C and/or POLARSSL_NET_C and/or "
           "POLARSSL_RSA_C and/or POLARSSL_CTR_DRBG_C not defined.\n");
    return( 0 );
}
#else
/*
 * perform a single SSL connection
 */
static int ssl_test( struct options *opt )
{
    int ret, i;
    int client_fd;
    int bytes_to_read;
    int bytes_to_write;
    int offset_to_read = 0;
    int offset_to_write = 0;

    long int nb_read;
    long int nb_written;

    unsigned long read_state[5];
    unsigned long write_state[5];

    unsigned char *read_buf = NULL;
    unsigned char *write_buf = NULL;

    const char *pers = "ssl_test";

    struct hr_time t;
    entropy_context entropy;
    ctr_drbg_context ctr_drbg;
    ssl_context ssl;
    x509_cert srvcert;
    rsa_context rsa;

    ret = 1;

    entropy_init( &entropy );
    if( ( ret = ctr_drbg_init( &ctr_drbg, entropy_func, &entropy,
                               (const unsigned char *) pers,
                               strlen( pers ) ) ) != 0 )
    {
        printf( "  ! ctr_drbg_init returned %d\n", ret );
        goto exit;
    }

    get_timer( &t, 1 );

    memset( read_state, 0, sizeof( read_state ) );
    memset( write_state, 0, sizeof( write_state ) );

    memset( &srvcert, 0, sizeof( x509_cert ) );
    memset( &rsa, 0, sizeof( rsa_context ) );

    if( opt->opmode == OPMODE_CLIENT )
    {
        if( ( ret = net_connect( &client_fd, opt->server_name,
                                             opt->server_port ) ) != 0 )
        {
            printf( "  ! net_connect returned %d\n\n", ret );
            return( ret );
        }

        if( ( ret = ssl_init( &ssl ) ) != 0 )
        {
            printf( "  ! ssl_init returned %d\n\n", ret );
            return( ret );
        }

        ssl_set_endpoint( &ssl, SSL_IS_CLIENT );
    }

    if( opt->opmode == OPMODE_SERVER )
    {
#if !defined(POLARSSL_CERTS_C)
        printf("POLARSSL_CERTS_C not defined.\n");
        goto exit;
#else
        ret =  x509parse_crt( &srvcert, (const unsigned char *) test_srv_crt,
                              strlen( test_srv_crt ) );
        if( ret != 0 )
        {
            printf( "  !  x509parse_crt returned %d\n\n", ret );
            goto exit;
        }

        ret =  x509parse_crt( &srvcert, (const unsigned char *) test_ca_crt,
                              strlen( test_ca_crt ) );
        if( ret != 0 )
        {
            printf( "  !  x509parse_crt returned %d\n\n", ret );
            goto exit;
        }

        ret =  x509parse_key( &rsa, (const unsigned char *) test_srv_key,
                              strlen( test_srv_key ), NULL, 0 );
        if( ret != 0 )
        {
            printf( "  !  x509parse_key returned %d\n\n", ret );
            goto exit;
        }
#endif

        if( server_fd < 0 )
        {
            if( ( ret = net_bind( &server_fd, NULL,
                                   opt->server_port ) ) != 0 )
            {
                printf( "  ! net_bind returned %d\n\n", ret );
                return( ret );
            }
        }

        if( ( ret = net_accept( server_fd, &client_fd, NULL ) ) != 0 )
        {
            printf( "  ! net_accept returned %d\n\n", ret );
            return( ret );
        }

        if( ( ret = ssl_init( &ssl ) ) != 0 )
        {
            printf( "  ! ssl_init returned %d\n\n", ret );
            return( ret );
        }

        ssl_set_endpoint( &ssl, SSL_IS_SERVER );
        ssl_set_dh_param( &ssl, dhm_P, dhm_G );
        ssl_set_ca_chain( &ssl, srvcert.next, NULL, NULL );
        ssl_set_own_cert( &ssl, &srvcert, &rsa );
    }

    ssl_set_authmode( &ssl, SSL_VERIFY_NONE );

    ssl_set_rng( &ssl, ctr_drbg_random, &ctr_drbg );
    ssl_set_dbg( &ssl, my_debug, opt );
    ssl_set_bio( &ssl, net_recv, &client_fd,
                       net_send, &client_fd );

    if( opt->force_ciphersuite[0] == DFL_FORCE_CIPHER )
          ssl_set_ciphersuites( &ssl, ssl_default_ciphersuites );
    else  ssl_set_ciphersuites( &ssl, opt->force_ciphersuite );

    if( opt->iomode == IOMODE_NONBLOCK )
        net_set_nonblock( client_fd );

     read_buf = (unsigned char *) malloc( opt->buffer_size );
    write_buf = (unsigned char *) malloc( opt->buffer_size );

    if( read_buf == NULL || write_buf == NULL )
    {
        printf( "  ! malloc(%d bytes) failed\n\n", opt->buffer_size );
        goto exit;
    }

    nb_read = bytes_to_read = 0;
    nb_written = bytes_to_write = 0;

    while( 1 )
    {
        if( opt->command & COMMAND_WRITE )
        {
            if( bytes_to_write == 0 )
            {
                while( bytes_to_write == 0 )
                    bytes_to_write = rand() % opt->buffer_size;

                for( i = 0; i < bytes_to_write; i++ )
                    write_buf[i] = (unsigned char) lcppm5( write_state );

                offset_to_write = 0;
            }

            ret = ssl_write( &ssl, write_buf + offset_to_write,
                             bytes_to_write );

            if( ret >= 0 )
            {
                nb_written += ret;
                bytes_to_write  -= ret;
                offset_to_write += ret;
            }

            if( ret == POLARSSL_ERR_SSL_PEER_CLOSE_NOTIFY ||
                ret == POLARSSL_ERR_NET_CONN_RESET )
            {
                ret = 0;
                goto exit;
            }

            if( ret < 0 && ret != POLARSSL_ERR_NET_WANT_READ &&
                ret != POLARSSL_ERR_NET_WANT_WRITE )
            {
                printf( "  ! ssl_write returned %d\n\n", ret );
                break;
            }
        }

        if( opt->command & COMMAND_READ )
        {
            if( bytes_to_read == 0 )
            {
                bytes_to_read = rand() % opt->buffer_size;
                offset_to_read = 0;
            }

            ret = ssl_read( &ssl, read_buf + offset_to_read,
                            bytes_to_read );

            if( ret >= 0 )
            {
                for( i = 0; i < ret; i++ )
                {
                    if( read_buf[offset_to_read + i] !=
                        (unsigned char) lcppm5( read_state ) )
                    {
                        ret = 1;
                        printf( "  ! plaintext mismatch\n\n" );
                        goto exit;
                    }
                }

                nb_read += ret;
                bytes_to_read -= ret;
                offset_to_read += ret;
            }

            if( ret == POLARSSL_ERR_SSL_PEER_CLOSE_NOTIFY ||
                ret == POLARSSL_ERR_NET_CONN_RESET )
            {
                ret = 0;
                goto exit;
            }

            if( ret < 0 && ret != POLARSSL_ERR_NET_WANT_READ &&
                ret != POLARSSL_ERR_NET_WANT_WRITE )
            {
                printf( "  ! ssl_read returned %d\n\n", ret );
                break;
            }
        }

        ret = 0;

        if( opt->max_bytes != 0 &&
            ( opt->max_bytes <= nb_read ||
              opt->max_bytes <= nb_written ) )
            break;

        if( opt->conn_timeout != 0 &&
            opt->conn_timeout <= (int) get_timer( &t, 0 ) )
            break;
    }

exit:

    fflush( stdout );

    if( read_buf != NULL )
        free( read_buf );

    if( write_buf != NULL )
        free( write_buf );

    ssl_close_notify( &ssl );
    x509_free( &srvcert );
    rsa_free( &rsa );
    ssl_free( &ssl );
    net_close( client_fd );

    return( ret );
}

#define USAGE \
    "\n usage: ssl_test opmode=<> command=<>...\n"               \
    "\n acceptable parameters:\n"                                \
    "    opmode=client/server        default: <none>\n"          \
    "    iomode=block/nonblock       default: block\n"           \
    "    server_name=%%s              default: localhost\n"      \
    "    server_port=%%d              default: 4433\n"           \
    "    command=read/write/both     default: read\n"            \
    "    buffer_size=%%d (bytes)      default: 1024\n"           \
    "    max_bytes=%%d (bytes)        default: 0 (no limit)\n"   \
    "    debug_level=%%d              default: 0 (disabled)\n"   \
    "    conn_timeout=%%d (ms)        default: 0 (no timeout)\n" \
    "    max_connections=%%d          default: 0 (no limit)\n"   \
    "    session_reuse=on/off        default: on (enabled)\n"    \
    "    session_lifetime=%%d (s)     default: 86400\n"          \
    "    force_ciphersuite=<name>    default: all enabled\n"     \
    " acceptable ciphersuite names:\n" 

int main( int argc, char *argv[] )
{
    int i, j, n;
    const int *list;
    int ret = 1;
    int nb_conn;
    char *p, *q;
    struct options opt;

    if( argc == 1 )
    {
    usage:
        printf( USAGE );
        
        list = ssl_list_ciphersuites();
        while( *list )
        {
            printf("    %s\n", ssl_get_ciphersuite_name( *list ) );
            list++;
        }
        printf("\n");
        goto exit;
    }

    opt.opmode                  = DFL_OPMODE;
    opt.iomode                  = DFL_IOMODE;
    opt.server_name             = DFL_SERVER_NAME;
    opt.server_port             = DFL_SERVER_PORT;
    opt.command                 = DFL_COMMAND;
    opt.buffer_size             = DFL_BUFFER_SIZE;
    opt.max_bytes               = DFL_MAX_BYTES;
    opt.debug_level             = DFL_DEBUG_LEVEL;
    opt.conn_timeout            = DFL_CONN_TIMEOUT;
    opt.max_connections         = DFL_MAX_CONNECTIONS;
    opt.session_reuse           = DFL_SESSION_REUSE;
    opt.session_lifetime        = DFL_SESSION_LIFETIME;
    opt.force_ciphersuite[0]    = DFL_FORCE_CIPHER;

    for( i = 1; i < argc; i++ )
    {
        n = strlen( argv[i] );

        for( j = 0; j < n; j++ )
        {
            if( argv[i][j] >= 'A' && argv[i][j] <= 'Z' )
                argv[i][j] |= 0x20;
        }

        p = argv[i];
        if( ( q = strchr( p, '=' ) ) == NULL )
            continue;
        *q++ = '\0';

        if( strcmp( p, "opmode" ) == 0 )
        {
            if( strcmp( q, "client" ) == 0 )
                opt.opmode = OPMODE_CLIENT;
            else
            if( strcmp( q, "server" ) == 0 )
                opt.opmode = OPMODE_SERVER;
            else goto usage;
        }

        if( strcmp( p, "iomode" ) == 0 )
        {
            if( strcmp( q, "block" ) == 0 )
                opt.iomode = IOMODE_BLOCK;
            else
            if( strcmp( q, "nonblock" ) == 0 )
                opt.iomode = IOMODE_NONBLOCK;
            else goto usage;
        }

        if( strcmp( p, "server_name" ) == 0 )
            opt.server_name = q;

        if( strcmp( p, "server_port" ) == 0 )
        {
            opt.server_port = atoi( q );
            if( opt.server_port < 1 || opt.server_port > 65535 )
                goto usage;
        }

        if( strcmp( p, "command" ) == 0 )
        {
            if( strcmp( q, "read" ) == 0 )
                opt.command = COMMAND_READ;
            else
            if( strcmp( q, "write" ) == 0 )
                opt.command = COMMAND_WRITE;
            else
            if( strcmp( q, "both" ) == 0 )
            {
                opt.iomode  = IOMODE_NONBLOCK;
                opt.command = COMMAND_BOTH;
            }
            else goto usage;
        }

        if( strcmp( p, "buffer_size" ) == 0 )
        {
            opt.buffer_size = atoi( q );
            if( opt.buffer_size < 1 || opt.buffer_size > 1048576 )
                goto usage;
        }

        if( strcmp( p, "max_bytes" ) == 0 )
            opt.max_bytes = atoi( q );

        if( strcmp( p, "debug_level" ) == 0 )
            opt.debug_level = atoi( q );

        if( strcmp( p, "conn_timeout" ) == 0 )
            opt.conn_timeout = atoi( q );

        if( strcmp( p, "max_connections" ) == 0 )
            opt.max_connections = atoi( q );

        if( strcmp( p, "session_reuse" ) == 0 )
        {
            if( strcmp( q, "on" ) == 0 )
                opt.session_reuse = 1;
            else
            if( strcmp( q, "off" ) == 0 )
                opt.session_reuse = 0;
            else
                goto usage;
        }

        if( strcmp( p, "session_lifetime" ) == 0 )
            opt.session_lifetime = atoi( q );

        if( strcmp( p, "force_ciphersuite" ) == 0 )
        {
            opt.force_ciphersuite[0] = -1;

            opt.force_ciphersuite[0] = ssl_get_ciphersuite_id( q );

            if( opt.force_ciphersuite[0] <= 0 )
                goto usage;

            opt.force_ciphersuite[1] = 0;
        }
    }

    switch( opt.opmode )
    {
        case OPMODE_CLIENT:
            break;

        case OPMODE_SERVER:
            break;

        default:
            goto usage;
    }

    nb_conn = 0;

    do {
        nb_conn++;
        ret = ssl_test( &opt );
        if( opt.max_connections != 0 &&
            opt.max_connections <= nb_conn )
            break;
    }
    while( ret == 0 );

exit:

#if defined(_WIN32)
    printf( "  Press Enter to exit this program.\n" );
    fflush( stdout ); getchar();
#endif

    return( ret );
}
#endif /* POLARSSL_BIGNUM_C && POLARSSL_ENTROPY_C && POLARSSL_SSL_TLS_C &&
          POLARSSL_SSL_SRV_C && POLARSSL_SSL_CLI_C && POLARSSL_NET_C &&
          POLARSSL_RSA_C && POLARSSL_CTR_DRBG_C */
