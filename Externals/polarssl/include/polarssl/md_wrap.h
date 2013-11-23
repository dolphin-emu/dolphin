/**
 * \file md_wrap.h
 * 
 * \brief Message digest wrappers.
 *
 * \author Adriaan de Jong <dejong@fox-it.com>
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
#ifndef POLARSSL_MD_WRAP_H
#define POLARSSL_MD_WRAP_H

#include "config.h"
#include "md.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(POLARSSL_MD2_C)
extern const md_info_t md2_info;
#endif
#if defined(POLARSSL_MD4_C)
extern const md_info_t md4_info;
#endif
#if defined(POLARSSL_MD5_C)
extern const md_info_t md5_info;
#endif
#if defined(POLARSSL_SHA1_C)
extern const md_info_t sha1_info;
#endif
#if defined(POLARSSL_SHA2_C)
extern const md_info_t sha224_info;
extern const md_info_t sha256_info;
#endif
#if defined(POLARSSL_SHA4_C)
extern const md_info_t sha384_info;
extern const md_info_t sha512_info;
#endif

#ifdef __cplusplus
}
#endif

#endif /* POLARSSL_MD_WRAP_H */
