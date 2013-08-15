/*
 *  \brief  Generic file encryption program using generic wrappers for configured
 *          security.
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

#if defined(_WIN32)
#include <windows.h>
#if !defined(_WIN32_WCE)
#include <io.h>
#endif
#else
#include <sys/types.h>
#include <unistd.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "polarssl/config.h"

#include "polarssl/cipher.h"
#include "polarssl/md.h"

#define MODE_ENCRYPT    0
#define MODE_DECRYPT    1

#define USAGE   \
    "\n  crypt_and_hash <mode> <input filename> <output filename> <cipher> <md> <key>\n" \
    "\n   <mode>: 0 = encrypt, 1 = decrypt\n" \
    "\n  example: crypt_and_hash 0 file file.aes AES-128-CBC SHA1 hex:E76B2413958B00E193\n" \
    "\n"

#if !defined(POLARSSL_CIPHER_C) || !defined(POLARSSL_MD_C)
int main( int argc, char *argv[] )
{
    ((void) argc);
    ((void) argv);

    printf("POLARSSL_CIPHER_C and/or POLARSSL_MD_C not defined.\n");
    return( 0 );
}
#else
int main( int argc, char *argv[] )
{
    int ret = 1, i, n;
    int mode, lastn;
    size_t keylen, ilen, olen;
    FILE *fkey, *fin = NULL, *fout = NULL;

    char *p;
    unsigned char IV[16];
    unsigned char key[512];
    unsigned char digest[POLARSSL_MD_MAX_SIZE];
    unsigned char buffer[1024];
    unsigned char output[1024];

    const cipher_info_t *cipher_info;
    const md_info_t *md_info;
    cipher_context_t cipher_ctx;
    md_context_t md_ctx;
#if defined(_WIN32_WCE)
    long filesize, offset;
#elif defined(_WIN32)
       LARGE_INTEGER li_size;
    __int64 filesize, offset;
#else
      off_t filesize, offset;
#endif

    memset( &cipher_ctx, 0, sizeof( cipher_context_t ));
    memset( &md_ctx, 0, sizeof( md_context_t ));

    /*
     * Parse the command-line arguments.
     */
    if( argc != 7 )
    {
        const int *list;

        printf( USAGE );

        printf( "Available ciphers:\n" );
        list = cipher_list();
        while( *list )
        {
            cipher_info = cipher_info_from_type( *list );
            printf( "  %s\n", cipher_info->name );
            list++;
        }

        printf( "\nAvailable message digests:\n" );
        list = md_list();
        while( *list )
        {
            md_info = md_info_from_type( *list );
            printf( "  %s\n", md_info->name );
            list++;
        }

#if defined(_WIN32)
        printf( "\n  Press Enter to exit this program.\n" );
        fflush( stdout ); getchar();
#endif

        goto exit;
    }

    mode = atoi( argv[1] );

    if( mode != MODE_ENCRYPT && mode != MODE_DECRYPT )
    {
        fprintf( stderr, "invalid operation mode\n" );
        goto exit;
    }

    if( strcmp( argv[2], argv[3] ) == 0 )
    {
        fprintf( stderr, "input and output filenames must differ\n" );
        goto exit;
    }

    if( ( fin = fopen( argv[2], "rb" ) ) == NULL )
    {
        fprintf( stderr, "fopen(%s,rb) failed\n", argv[2] );
        goto exit;
    }

    if( ( fout = fopen( argv[3], "wb+" ) ) == NULL )
    {
        fprintf( stderr, "fopen(%s,wb+) failed\n", argv[3] );
        goto exit;
    }

    /*
     * Read the Cipher and MD from the command line
     */
    cipher_info = cipher_info_from_string( argv[4] );
    if( cipher_info == NULL )
    {
        fprintf( stderr, "Cipher '%s' not found\n", argv[4] );
        goto exit;
    }
    cipher_init_ctx( &cipher_ctx, cipher_info);

    md_info = md_info_from_string( argv[5] );
    if( md_info == NULL )
    {
        fprintf( stderr, "Message Digest '%s' not found\n", argv[5] );
        goto exit;
    }
    md_init_ctx( &md_ctx, md_info);

    /*
     * Read the secret key and clean the command line.
     */
    if( ( fkey = fopen( argv[6], "rb" ) ) != NULL )
    {
        keylen = fread( key, 1, sizeof( key ), fkey );
        fclose( fkey );
    }
    else
    {
        if( memcmp( argv[6], "hex:", 4 ) == 0 )
        {
            p = &argv[6][4];
            keylen = 0;

            while( sscanf( p, "%02X", &n ) > 0 &&
                   keylen < (int) sizeof( key ) )
            {
                key[keylen++] = (unsigned char) n;
                p += 2;
            }
        }
        else
        {
            keylen = strlen( argv[6] );

            if( keylen > (int) sizeof( key ) )
                keylen = (int) sizeof( key );

            memcpy( key, argv[6], keylen );
        }
    }

    memset( argv[6], 0, strlen( argv[6] ) );

#if defined(_WIN32_WCE)
    filesize = fseek( fin, 0L, SEEK_END );
#else
#if defined(_WIN32)
    /*
     * Support large files (> 2Gb) on Win32
     */
    li_size.QuadPart = 0;
    li_size.LowPart  =
        SetFilePointer( (HANDLE) _get_osfhandle( _fileno( fin ) ),
                        li_size.LowPart, &li_size.HighPart, FILE_END );

    if( li_size.LowPart == 0xFFFFFFFF && GetLastError() != NO_ERROR )
    {
        fprintf( stderr, "SetFilePointer(0,FILE_END) failed\n" );
        goto exit;
    }

    filesize = li_size.QuadPart;
#else
    if( ( filesize = lseek( fileno( fin ), 0, SEEK_END ) ) < 0 )
    {
        perror( "lseek" );
        goto exit;
    }
#endif
#endif

    if( fseek( fin, 0, SEEK_SET ) < 0 )
    {
        fprintf( stderr, "fseek(0,SEEK_SET) failed\n" );
        goto exit;
    }

    if( mode == MODE_ENCRYPT )
    {
        /*
         * Generate the initialization vector as:
         * IV = SHA-256( filesize || filename )[0..15]
         */
        for( i = 0; i < 8; i++ )
            buffer[i] = (unsigned char)( filesize >> ( i << 3 ) );

        p = argv[2];

        md_starts( &md_ctx );
        md_update( &md_ctx, buffer, 8 );
        md_update( &md_ctx, (unsigned char *) p, strlen( p ) );
        md_finish( &md_ctx, digest );

        memcpy( IV, digest, 16 );

        /*
         * The last four bits in the IV are actually used
         * to store the file size modulo the AES block size.
         */
        lastn = (int)( filesize & 0x0F );

        IV[15] = (unsigned char)
            ( ( IV[15] & 0xF0 ) | lastn );

        /*
         * Append the IV at the beginning of the output.
         */
        if( fwrite( IV, 1, 16, fout ) != 16 )
        {
            fprintf( stderr, "fwrite(%d bytes) failed\n", 16 );
            goto exit;
        }

        /*
         * Hash the IV and the secret key together 8192 times
         * using the result to setup the AES context and HMAC.
         */
        memset( digest, 0,  32 );
        memcpy( digest, IV, 16 );

        for( i = 0; i < 8192; i++ )
        {
            md_starts( &md_ctx );
            md_update( &md_ctx, digest, 32 );
            md_update( &md_ctx, key, keylen );
            md_finish( &md_ctx, digest );

        }

        memset( key, 0, sizeof( key ) );

        if( cipher_setkey( &cipher_ctx, digest, cipher_info->key_length,
                           POLARSSL_ENCRYPT ) != 0 )
        {
            fprintf( stderr, "cipher_setkey() returned error\n");
            goto exit;
        }
        if( cipher_reset( &cipher_ctx, IV ) != 0 )
        {
            fprintf( stderr, "cipher_reset() returned error\n");
            goto exit;
        }

        md_hmac_starts( &md_ctx, digest, 32 );

        /*
         * Encrypt and write the ciphertext.
         */
        for( offset = 0; offset < filesize; offset += cipher_get_block_size( &cipher_ctx ) )
        {
            ilen = ( (unsigned int) filesize - offset > cipher_get_block_size( &cipher_ctx ) ) ?
                cipher_get_block_size( &cipher_ctx ) : (unsigned int) ( filesize - offset );

            if( fread( buffer, 1, ilen, fin ) != ilen )
            {
                fprintf( stderr, "fread(%ld bytes) failed\n", (long) n );
                goto exit;
            }

            cipher_update( &cipher_ctx, buffer, ilen, output, &olen );
            md_hmac_update( &md_ctx, output, olen );

            if( fwrite( output, 1, olen, fout ) != olen )
            {
                fprintf( stderr, "fwrite(%ld bytes) failed\n", (long) olen );
                goto exit;
            }
        }

        if( cipher_finish( &cipher_ctx, output, &olen ) != 0 )
        {
            fprintf( stderr, "cipher_finish() returned error\n" );
            goto exit;
        }
        md_hmac_update( &md_ctx, output, olen );

        if( fwrite( output, 1, olen, fout ) != olen )
        {
            fprintf( stderr, "fwrite(%ld bytes) failed\n", (long) olen );
            goto exit;
        }

        /*
         * Finally write the HMAC.
         */
        md_hmac_finish( &md_ctx, digest );

        if( fwrite( digest, 1, md_get_size( md_info ), fout ) != md_get_size( md_info ) )
        {
            fprintf( stderr, "fwrite(%d bytes) failed\n", md_get_size( md_info ) );
            goto exit;
        }
    }

    if( mode == MODE_DECRYPT )
    {
        /*
         *  The encrypted file must be structured as follows:
         *
         *        00 .. 15              Initialization Vector
         *        16 .. 31              AES Encrypted Block #1
         *           ..
         *      N*16 .. (N+1)*16 - 1    AES Encrypted Block #N
         *  (N+1)*16 .. (N+1)*16 + 32   HMAC-SHA-256(ciphertext)
         */
        if( filesize < 16 + md_get_size( md_info ) )
        {
            fprintf( stderr, "File too short to be encrypted.\n" );
            goto exit;
        }

        if( ( ( filesize - md_get_size( md_info ) ) % 
                cipher_get_block_size( &cipher_ctx ) ) != 0 )
        {
            fprintf( stderr, "File content not a multiple of the block size (%d).\n",
                     cipher_get_block_size( &cipher_ctx ));
            goto exit;
        }

        /*
         * Substract the IV + HMAC length.
         */
        filesize -= ( 16 + md_get_size( md_info ) );

        /*
         * Read the IV and original filesize modulo 16.
         */
        if( fread( buffer, 1, 16, fin ) != 16 )
        {
            fprintf( stderr, "fread(%d bytes) failed\n", 16 );
            goto exit;
        }

        memcpy( IV, buffer, 16 );
        lastn = IV[15] & 0x0F;

        /*
         * Hash the IV and the secret key together 8192 times
         * using the result to setup the AES context and HMAC.
         */
        memset( digest, 0,  32 );
        memcpy( digest, IV, 16 );

        for( i = 0; i < 8192; i++ )
        {
            md_starts( &md_ctx );
            md_update( &md_ctx, digest, 32 );
            md_update( &md_ctx, key, keylen );
            md_finish( &md_ctx, digest );
        }

        memset( key, 0, sizeof( key ) );

        cipher_setkey( &cipher_ctx, digest, cipher_info->key_length,
            POLARSSL_DECRYPT );
        cipher_reset( &cipher_ctx, IV);

        md_hmac_starts( &md_ctx, digest, 32 );

        /*
         * Decrypt and write the plaintext.
         */
        for( offset = 0; offset < filesize; offset += cipher_get_block_size( &cipher_ctx ) )
        {
            if( fread( buffer, 1, cipher_get_block_size( &cipher_ctx ), fin ) !=
                (size_t) cipher_get_block_size( &cipher_ctx ) )
            {
                fprintf( stderr, "fread(%d bytes) failed\n",
                    cipher_get_block_size( &cipher_ctx ) );
                goto exit;
            }

            md_hmac_update( &md_ctx, buffer, cipher_get_block_size( &cipher_ctx ) );
            cipher_update( &cipher_ctx, buffer, cipher_get_block_size( &cipher_ctx ),
                output, &olen );

            if( fwrite( output, 1, olen, fout ) != olen )
            {
                fprintf( stderr, "fwrite(%ld bytes) failed\n", (long) olen );
                goto exit;
            }
        }

        /*
         * Write the final block of data
         */
        cipher_finish( &cipher_ctx, output, &olen );

        if( fwrite( output, 1, olen, fout ) != olen )
        {
            fprintf( stderr, "fwrite(%ld bytes) failed\n", (long) olen );
            goto exit;
        }

        /*
         * Verify the message authentication code.
         */
        md_hmac_finish( &md_ctx, digest );

        if( fread( buffer, 1, md_get_size( md_info ), fin ) != md_get_size( md_info ) )
        {
            fprintf( stderr, "fread(%d bytes) failed\n", md_get_size( md_info ) );
            goto exit;
        }

        if( memcmp( digest, buffer, md_get_size( md_info ) ) != 0 )
        {
            fprintf( stderr, "HMAC check failed: wrong key, "
                             "or file corrupted.\n" );
            goto exit;
        }
    }

    ret = 0;

exit:
    if( fin )
        fclose( fin );
    if( fout )
        fclose( fout );

    memset( buffer, 0, sizeof( buffer ) );
    memset( digest, 0, sizeof( digest ) );

    cipher_free_ctx( &cipher_ctx );
    md_free_ctx( &md_ctx );

    return( ret );
}
#endif /* POLARSSL_CIPHER_C && POLARSSL_MD_C */
