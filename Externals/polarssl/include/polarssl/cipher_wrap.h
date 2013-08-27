/**
 * \file cipher_wrap.h
 * 
 * \brief Cipher wrappers.
 *
 * \author Adriaan de Jong <dejong@fox-it.com>
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
#ifndef POLARSSL_CIPHER_WRAP_H
#define POLARSSL_CIPHER_WRAP_H

#include "config.h"
#include "cipher.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(POLARSSL_AES_C)

extern const cipher_info_t aes_128_cbc_info;
extern const cipher_info_t aes_192_cbc_info;
extern const cipher_info_t aes_256_cbc_info;

#if defined(POLARSSL_CIPHER_MODE_CFB)
extern const cipher_info_t aes_128_cfb128_info;
extern const cipher_info_t aes_192_cfb128_info;
extern const cipher_info_t aes_256_cfb128_info;
#endif /* POLARSSL_CIPHER_MODE_CFB */

#if defined(POLARSSL_CIPHER_MODE_CTR)
extern const cipher_info_t aes_128_ctr_info;
extern const cipher_info_t aes_192_ctr_info;
extern const cipher_info_t aes_256_ctr_info;
#endif /* POLARSSL_CIPHER_MODE_CTR */

#endif /* defined(POLARSSL_AES_C) */

#if defined(POLARSSL_CAMELLIA_C)

extern const cipher_info_t camellia_128_cbc_info;
extern const cipher_info_t camellia_192_cbc_info;
extern const cipher_info_t camellia_256_cbc_info;

#if defined(POLARSSL_CIPHER_MODE_CFB)
extern const cipher_info_t camellia_128_cfb128_info;
extern const cipher_info_t camellia_192_cfb128_info;
extern const cipher_info_t camellia_256_cfb128_info;
#endif /* POLARSSL_CIPHER_MODE_CFB */

#if defined(POLARSSL_CIPHER_MODE_CTR)
extern const cipher_info_t camellia_128_ctr_info;
extern const cipher_info_t camellia_192_ctr_info;
extern const cipher_info_t camellia_256_ctr_info;
#endif /* POLARSSL_CIPHER_MODE_CTR */

#endif /* defined(POLARSSL_CAMELLIA_C) */

#if defined(POLARSSL_DES_C)

extern const cipher_info_t des_cbc_info;
extern const cipher_info_t des_ede_cbc_info;
extern const cipher_info_t des_ede3_cbc_info;

#endif /* defined(POLARSSL_DES_C) */

#if defined(POLARSSL_BLOWFISH_C)
extern const cipher_info_t blowfish_cbc_info;

#if defined(POLARSSL_CIPHER_MODE_CFB)
extern const cipher_info_t blowfish_cfb64_info;
#endif /* POLARSSL_CIPHER_MODE_CFB */

#if defined(POLARSSL_CIPHER_MODE_CTR)
extern const cipher_info_t blowfish_ctr_info;
#endif /* POLARSSL_CIPHER_MODE_CTR */
#endif /* defined(POLARSSL_BLOWFISH_C) */

#if defined(POLARSSL_CIPHER_NULL_CIPHER)
extern const cipher_info_t null_cipher_info;
#endif /* defined(POLARSSL_CIPHER_NULL_CIPHER) */

#ifdef __cplusplus
}
#endif

#endif /* POLARSSL_CIPHER_WRAP_H */
