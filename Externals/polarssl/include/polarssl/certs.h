/**
 * \file certs.h
 *
 * \brief Sample certificates and DHM parameters for testing
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
#ifndef POLARSSL_CERTS_H
#define POLARSSL_CERTS_H

#ifdef __cplusplus
extern "C" {
#endif

extern const char test_ca_crt[];
extern const char test_ca_key[];
extern const char test_ca_pwd[];
extern const char test_srv_crt[];
extern const char test_srv_key[];
extern const char test_cli_crt[];
extern const char test_cli_key[];
extern const char test_dhm_params[];

#ifdef __cplusplus
}
#endif

#endif /* certs.h */
