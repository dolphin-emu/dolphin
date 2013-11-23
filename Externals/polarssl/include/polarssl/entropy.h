/**
 * \file entropy.h
 *
 * \brief Entropy accumulator implementation
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
#ifndef POLARSSL_ENTROPY_H
#define POLARSSL_ENTROPY_H

#include <string.h>

#include "config.h"

#include "sha4.h"
#if defined(POLARSSL_HAVEGE_C)
#include "havege.h"
#endif

#define POLARSSL_ERR_ENTROPY_SOURCE_FAILED                 -0x003C  /**< Critical entropy source failure. */
#define POLARSSL_ERR_ENTROPY_MAX_SOURCES                   -0x003E  /**< No more sources can be added. */
#define POLARSSL_ERR_ENTROPY_NO_SOURCES_DEFINED            -0x0040  /**< No sources have been added to poll. */

#if !defined(POLARSSL_CONFIG_OPTIONS)
#define ENTROPY_MAX_SOURCES     20      /**< Maximum number of sources supported */
#define ENTROPY_MAX_GATHER      128     /**< Maximum amount requested from entropy sources */
#endif /* !POLARSSL_CONFIG_OPTIONS  */

#define ENTROPY_BLOCK_SIZE      64      /**< Block size of entropy accumulator (SHA-512) */

#define ENTROPY_SOURCE_MANUAL   ENTROPY_MAX_SOURCES

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Entropy poll callback pointer
 *
 * \param data      Callback-specific data pointer
 * \param output    Data to fill
 * \param len       Maximum size to provide
 * \param olen      The actual amount of bytes put into the buffer (Can be 0)
 *
 * \return          0 if no critical failures occurred,
 *                  POLARSSL_ERR_ENTROPY_SOURCE_FAILED otherwise
 */
typedef int (*f_source_ptr)(void *, unsigned char *, size_t, size_t *);

/**
 * \brief           Entropy source state
 */
typedef struct
{
    f_source_ptr    f_source;   /**< The entropy source callback */
    void *          p_source;   /**< The callback data pointer */
    size_t          size;       /**< Amount received */
    size_t          threshold;  /**< Minimum level required before release */
}
source_state;

/**
 * \brief           Entropy context structure
 */
typedef struct 
{
    sha4_context    accumulator;
    int             source_count;
    source_state    source[ENTROPY_MAX_SOURCES];
#if defined(POLARSSL_HAVEGE_C)
    havege_state    havege_data;
#endif
}
entropy_context;

/**
 * \brief           Initialize the context
 *
 * \param ctx       Entropy context to initialize
 */
void entropy_init( entropy_context *ctx );

/**
 * \brief           Adds an entropy source to poll
 *
 * \param ctx       Entropy context
 * \param f_source  Entropy function
 * \param p_source  Function data
 * \param threshold Minimum required from source before entropy is released
 *                  ( with entropy_func() )
 *
 * \return          0 if successful or POLARSSL_ERR_ENTROPY_MAX_SOURCES
 */
int entropy_add_source( entropy_context *ctx,
                        f_source_ptr f_source, void *p_source,
                        size_t threshold );

/**
 * \brief           Trigger an extra gather poll for the accumulator
 *
 * \param ctx       Entropy context
 *
 * \return          0 if successful, or POLARSSL_ERR_ENTROPY_SOURCE_FAILED
 */
int entropy_gather( entropy_context *ctx );

/**
 * \brief           Retrieve entropy from the accumulator (Max ENTROPY_BLOCK_SIZE)
 *
 * \param data      Entropy context
 * \param output    Buffer to fill
 * \param len       Length of buffer
 *
 * \return          0 if successful, or POLARSSL_ERR_ENTROPY_SOURCE_FAILED
 */
int entropy_func( void *data, unsigned char *output, size_t len );

/**
 * \brief           Add data to the accumulator manually
 * 
 * \param ctx       Entropy context
 * \param data      Data to add
 * \param len       Length of data
 *
 * \return          0 if successful
 */
int entropy_update_manual( entropy_context *ctx,
                           const unsigned char *data, size_t len );

#ifdef __cplusplus
}
#endif

#endif /* entropy.h */
