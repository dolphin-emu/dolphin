/**
 * \file config.h
 *
 * \brief Configuration options (set of defines)
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
 *
 * This set of compile-time options may be used to enable
 * or disable features selectively, and reduce the global
 * memory footprint.
 */
#ifndef POLARSSL_CONFIG_H
#define POLARSSL_CONFIG_H

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_DEPRECATE)
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

/**
 * \name SECTION: System support
 *
 * This section sets system specific settings.
 * \{
 */

/**
 * \def POLARSSL_HAVE_INT8
 *
 * The system uses 8-bit wide native integers.
 *
 * Uncomment if native integers are 8-bit wide.
 */
//#define POLARSSL_HAVE_INT8

/**
 * \def POLARSSL_HAVE_INT16
 *
 * The system uses 16-bit wide native integers.
 *
 * Uncomment if native integers are 16-bit wide.
 */
//#define POLARSSL_HAVE_INT16

/**
 * \def POLARSSL_HAVE_LONGLONG
 *
 * The compiler supports the 'long long' type.
 * (Only used on 32-bit platforms)
 */
#define POLARSSL_HAVE_LONGLONG

/**
 * \def POLARSSL_HAVE_ASM
 *
 * The compiler has support for asm().
 *
 * Requires support for asm() in compiler.
 *
 * Used in:
 *      library/timing.c
 *      library/padlock.c
 *      include/polarssl/bn_mul.h
 *
 * Comment to disable the use of assembly code.
 */
#define POLARSSL_HAVE_ASM

/**
 * \def POLARSSL_HAVE_SSE2
 *
 * CPU supports SSE2 instruction set.
 *
 * Uncomment if the CPU supports SSE2 (IA-32 specific).
 */
//#define POLARSSL_HAVE_SSE2

/**
 * \def POLARSSL_HAVE_TIME
 *
 * System has time.h and time() / localtime()  / gettimeofday().
 *
 * Comment if your system does not support time functions
 */
#define POLARSSL_HAVE_TIME

/**
 * \def POLARSSL_HAVE_IPV6
 *
 * System supports the basic socket interface for IPv6 (RFC 3493),
 * specifically getaddrinfo(), freeaddrinfo() and struct sockaddr_storage.
 *
 * Note: on Windows/MingW, XP or higher is required.
 *
 * Comment if your system does not support the IPv6 socket interface
 */
#define POLARSSL_HAVE_IPV6
/* \} name SECTION: System support */

/**
 * \name SECTION: PolarSSL feature support
 *
 * This section sets support for features that are or are not needed
 * within the modules that are enabled.
 * \{
 */

/**
 * \def POLARSSL_XXX_ALT
 *
 * Uncomment a macro to let PolarSSL use your alternate core implementation of
 * a symmetric or hash algorithm (e.g. platform specific assembly optimized
 * implementations). Keep in mind that the function prototypes should remain
 * the same.
 *
 * Example: In case you uncomment POLARSSL_AES_ALT, PolarSSL will no longer
 * provide the "struct aes_context" definition and omit the base function
 * declarations and implementations. "aes_alt.h" will be included from
 * "aes.h" to include the new function definitions.
 *
 * Uncomment a macro to enable alternate implementation for core algorithm
 * functions
 */
//#define POLARSSL_AES_ALT
//#define POLARSSL_ARC4_ALT
//#define POLARSSL_BLOWFISH_ALT
//#define POLARSSL_CAMELLIA_ALT
//#define POLARSSL_DES_ALT
//#define POLARSSL_XTEA_ALT
//#define POLARSSL_MD2_ALT
//#define POLARSSL_MD4_ALT
//#define POLARSSL_MD5_ALT
//#define POLARSSL_RIPEMD160_ALT
//#define POLARSSL_SHA1_ALT
//#define POLARSSL_SHA256_ALT
//#define POLARSSL_SHA512_ALT

/**
 * \def POLARSSL_AES_ROM_TABLES
 *
 * Store the AES tables in ROM.
 *
 * Uncomment this macro to store the AES tables in ROM.
 *
 */
//#define POLARSSL_AES_ROM_TABLES

/**
 * \def POLARSSL_CIPHER_MODE_CBC
 *
 * Enable Cipher Block Chaining mode (CBC) for symmetric ciphers.
 */
#define POLARSSL_CIPHER_MODE_CBC

/**
 * \def POLARSSL_CIPHER_MODE_CFB
 *
 * Enable Cipher Feedback mode (CFB) for symmetric ciphers.
 */
#define POLARSSL_CIPHER_MODE_CFB

/**
 * \def POLARSSL_CIPHER_MODE_CTR
 *
 * Enable Counter Block Cipher mode (CTR) for symmetric ciphers.
 */
#define POLARSSL_CIPHER_MODE_CTR

/**
 * \def POLARSSL_CIPHER_NULL_CIPHER
 *
 * Enable NULL cipher.
 * Warning: Only do so when you know what you are doing. This allows for
 * encryption or channels without any security!
 *
 * Requires POLARSSL_ENABLE_WEAK_CIPHERSUITES as well to enable
 * the following ciphersuites:
 *      TLS_ECDH_ECDSA_WITH_NULL_SHA
 *      TLS_ECDH_RSA_WITH_NULL_SHA
 *      TLS_ECDHE_ECDSA_WITH_NULL_SHA
 *      TLS_ECDHE_RSA_WITH_NULL_SHA
 *      TLS_ECDHE_PSK_WITH_NULL_SHA384
 *      TLS_ECDHE_PSK_WITH_NULL_SHA256
 *      TLS_ECDHE_PSK_WITH_NULL_SHA
 *      TLS_DHE_PSK_WITH_NULL_SHA384
 *      TLS_DHE_PSK_WITH_NULL_SHA256
 *      TLS_DHE_PSK_WITH_NULL_SHA
 *      TLS_RSA_WITH_NULL_SHA256
 *      TLS_RSA_WITH_NULL_SHA
 *      TLS_RSA_WITH_NULL_MD5
 *      TLS_RSA_PSK_WITH_NULL_SHA384
 *      TLS_RSA_PSK_WITH_NULL_SHA256
 *      TLS_RSA_PSK_WITH_NULL_SHA
 *      TLS_PSK_WITH_NULL_SHA384
 *      TLS_PSK_WITH_NULL_SHA256
 *      TLS_PSK_WITH_NULL_SHA
 *
 * Uncomment this macro to enable the NULL cipher and ciphersuites
#define POLARSSL_CIPHER_NULL_CIPHER
 */

/**
 * \def POLARSSL_CIPHER_PADDING_XXX
 *
 * Uncomment or comment macros to add support for specific padding modes
 * in the cipher layer with cipher modes that support padding (e.g. CBC)
 *
 * If you disable all padding modes, only full blocks can be used with CBC.
 *
 * Enable padding modes in the cipher layer.
 */
#define POLARSSL_CIPHER_PADDING_PKCS7
#define POLARSSL_CIPHER_PADDING_ONE_AND_ZEROS
#define POLARSSL_CIPHER_PADDING_ZEROS_AND_LEN
#define POLARSSL_CIPHER_PADDING_ZEROS

/**
 * \def POLARSSL_ENABLE_WEAK_CIPHERSUITES
 *
 * Enable weak ciphersuites in SSL / TLS.
 * Warning: Only do so when you know what you are doing. This allows for
 * channels with virtually no security at all!
 *
 * This enables the following ciphersuites:
 *      TLS_RSA_WITH_DES_CBC_SHA
 *      TLS_DHE_RSA_WITH_DES_CBC_SHA
 *
 * Uncomment this macro to enable weak ciphersuites
#define POLARSSL_ENABLE_WEAK_CIPHERSUITES
 */

/**
 * \def POLARSSL_ECP_XXXX_ENABLED
 *
 * Enables specific curves within the Elliptic Curve module.
 * By default all supported curves are enabled.
 *
 * Comment macros to disable the curve and functions for it
 */
#define POLARSSL_ECP_DP_SECP192R1_ENABLED
#define POLARSSL_ECP_DP_SECP224R1_ENABLED
#define POLARSSL_ECP_DP_SECP256R1_ENABLED
#define POLARSSL_ECP_DP_SECP384R1_ENABLED
#define POLARSSL_ECP_DP_SECP521R1_ENABLED
#define POLARSSL_ECP_DP_SECP192K1_ENABLED
#define POLARSSL_ECP_DP_SECP224K1_ENABLED
#define POLARSSL_ECP_DP_SECP256K1_ENABLED
#define POLARSSL_ECP_DP_BP256R1_ENABLED
#define POLARSSL_ECP_DP_BP384R1_ENABLED
#define POLARSSL_ECP_DP_BP512R1_ENABLED
//#define POLARSSL_ECP_DP_M221_ENABLED  // Not implemented yet!
#define POLARSSL_ECP_DP_M255_ENABLED
//#define POLARSSL_ECP_DP_M383_ENABLED  // Not implemented yet!
//#define POLARSSL_ECP_DP_M511_ENABLED  // Not implemented yet!

/**
 * \def POLARSSL_ECP_NIST_OPTIM
 *
 * Enable specific 'modulo p' routines for each NIST prime.
 * Depending on the prime and architecture, makes operations 4 to 8 times
 * faster on the corresponding curve.
 *
 * Comment this macro to disable NIST curves optimisation.
 */
#define POLARSSL_ECP_NIST_OPTIM

/**
 * \def POLARSSL_ECDSA_DETERMINISTIC
 *
 * Enable deterministic ECDSA (RFC 6979).
 * Standard ECDSA is "fragile" in the sense that lack of entropy when signing
 * may result in a compromise of the long-term signing key. This is avoided by
 * the deterministic variant.
 *
 * Requires: POLARSSL_MD_C
 *
 * Comment this macro to disable deterministic ECDSA.
 */
#define POLARSSL_ECDSA_DETERMINISTIC

/**
 * \def POLARSSL_KEY_EXCHANGE_PSK_ENABLED
 *
 * Enable the PSK based ciphersuite modes in SSL / TLS.
 *
 * This enables the following ciphersuites (if other requisites are
 * enabled as well):
 *      TLS_PSK_WITH_AES_256_GCM_SHA384
 *      TLS_PSK_WITH_AES_256_CBC_SHA384
 *      TLS_PSK_WITH_AES_256_CBC_SHA
 *      TLS_PSK_WITH_CAMELLIA_256_GCM_SHA384
 *      TLS_PSK_WITH_CAMELLIA_256_CBC_SHA384
 *      TLS_PSK_WITH_AES_128_GCM_SHA256
 *      TLS_PSK_WITH_AES_128_CBC_SHA256
 *      TLS_PSK_WITH_AES_128_CBC_SHA
 *      TLS_PSK_WITH_CAMELLIA_128_GCM_SHA256
 *      TLS_PSK_WITH_CAMELLIA_128_CBC_SHA256
 *      TLS_PSK_WITH_3DES_EDE_CBC_SHA
 *      TLS_PSK_WITH_RC4_128_SHA
 */
#define POLARSSL_KEY_EXCHANGE_PSK_ENABLED

/**
 * \def POLARSSL_KEY_EXCHANGE_DHE_PSK_ENABLED
 *
 * Enable the DHE-PSK based ciphersuite modes in SSL / TLS.
 *
 * Requires: POLARSSL_DHM_C
 *
 * This enables the following ciphersuites (if other requisites are
 * enabled as well):
 *      TLS_DHE_PSK_WITH_AES_256_GCM_SHA384
 *      TLS_DHE_PSK_WITH_AES_256_CBC_SHA384
 *      TLS_DHE_PSK_WITH_AES_256_CBC_SHA
 *      TLS_DHE_PSK_WITH_CAMELLIA_256_GCM_SHA384
 *      TLS_DHE_PSK_WITH_CAMELLIA_256_CBC_SHA384
 *      TLS_DHE_PSK_WITH_AES_128_GCM_SHA256
 *      TLS_DHE_PSK_WITH_AES_128_CBC_SHA256
 *      TLS_DHE_PSK_WITH_AES_128_CBC_SHA
 *      TLS_DHE_PSK_WITH_CAMELLIA_128_GCM_SHA256
 *      TLS_DHE_PSK_WITH_CAMELLIA_128_CBC_SHA256
 *      TLS_DHE_PSK_WITH_3DES_EDE_CBC_SHA
 *      TLS_DHE_PSK_WITH_RC4_128_SHA
 */
#define POLARSSL_KEY_EXCHANGE_DHE_PSK_ENABLED

/**
 * \def POLARSSL_KEY_EXCHANGE_ECDHE_PSK_ENABLED
 *
 * Enable the ECDHE-PSK based ciphersuite modes in SSL / TLS.
 *
 * Requires: POLARSSL_ECDH_C
 *
 * This enables the following ciphersuites (if other requisites are
 * enabled as well):
 *      TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA384
 *      TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA
 *      TLS_ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384
 *      TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256
 *      TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA
 *      TLS_ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256
 *      TLS_ECDHE_PSK_WITH_3DES_EDE_CBC_SHA
 *      TLS_ECDHE_PSK_WITH_RC4_128_SHA
 */
#define POLARSSL_KEY_EXCHANGE_ECDHE_PSK_ENABLED

/**
 * \def POLARSSL_KEY_EXCHANGE_RSA_PSK_ENABLED
 *
 * Enable the RSA-PSK based ciphersuite modes in SSL / TLS.
 *
 * Requires: POLARSSL_RSA_C, POLARSSL_PKCS1_V15,
 *           POLARSSL_X509_CRT_PARSE_C
 *
 * This enables the following ciphersuites (if other requisites are
 * enabled as well):
 *      TLS_RSA_PSK_WITH_AES_256_GCM_SHA384
 *      TLS_RSA_PSK_WITH_AES_256_CBC_SHA384
 *      TLS_RSA_PSK_WITH_AES_256_CBC_SHA
 *      TLS_RSA_PSK_WITH_CAMELLIA_256_GCM_SHA384
 *      TLS_RSA_PSK_WITH_CAMELLIA_256_CBC_SHA384
 *      TLS_RSA_PSK_WITH_AES_128_GCM_SHA256
 *      TLS_RSA_PSK_WITH_AES_128_CBC_SHA256
 *      TLS_RSA_PSK_WITH_AES_128_CBC_SHA
 *      TLS_RSA_PSK_WITH_CAMELLIA_128_GCM_SHA256
 *      TLS_RSA_PSK_WITH_CAMELLIA_128_CBC_SHA256
 *      TLS_RSA_PSK_WITH_3DES_EDE_CBC_SHA
 *      TLS_RSA_PSK_WITH_RC4_128_SHA
 */
#define POLARSSL_KEY_EXCHANGE_RSA_PSK_ENABLED

/**
 * \def POLARSSL_KEY_EXCHANGE_RSA_ENABLED
 *
 * Enable the RSA-only based ciphersuite modes in SSL / TLS.
 *
 * Requires: POLARSSL_RSA_C, POLARSSL_PKCS1_V15,
 *           POLARSSL_X509_CRT_PARSE_C
 *
 * This enables the following ciphersuites (if other requisites are
 * enabled as well):
 *      TLS_RSA_WITH_AES_256_GCM_SHA384
 *      TLS_RSA_WITH_AES_256_CBC_SHA256
 *      TLS_RSA_WITH_AES_256_CBC_SHA
 *      TLS_RSA_WITH_CAMELLIA_256_GCM_SHA384
 *      TLS_RSA_WITH_CAMELLIA_256_CBC_SHA256
 *      TLS_RSA_WITH_CAMELLIA_256_CBC_SHA
 *      TLS_RSA_WITH_AES_128_GCM_SHA256
 *      TLS_RSA_WITH_AES_128_CBC_SHA256
 *      TLS_RSA_WITH_AES_128_CBC_SHA
 *      TLS_RSA_WITH_CAMELLIA_128_GCM_SHA256
 *      TLS_RSA_WITH_CAMELLIA_128_CBC_SHA256
 *      TLS_RSA_WITH_CAMELLIA_128_CBC_SHA
 *      TLS_RSA_WITH_3DES_EDE_CBC_SHA
 *      TLS_RSA_WITH_RC4_128_SHA
 *      TLS_RSA_WITH_RC4_128_MD5
 */
#define POLARSSL_KEY_EXCHANGE_RSA_ENABLED

/**
 * \def POLARSSL_KEY_EXCHANGE_DHE_RSA_ENABLED
 *
 * Enable the DHE-RSA based ciphersuite modes in SSL / TLS.
 *
 * Requires: POLARSSL_DHM_C, POLARSSL_RSA_C, POLARSSL_PKCS1_V15,
 *           POLARSSL_X509_CRT_PARSE_C
 *
 * This enables the following ciphersuites (if other requisites are
 * enabled as well):
 *      TLS_DHE_RSA_WITH_AES_256_GCM_SHA384
 *      TLS_DHE_RSA_WITH_AES_256_CBC_SHA256
 *      TLS_DHE_RSA_WITH_AES_256_CBC_SHA
 *      TLS_DHE_RSA_WITH_CAMELLIA_256_GCM_SHA384
 *      TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256
 *      TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA
 *      TLS_DHE_RSA_WITH_AES_128_GCM_SHA256
 *      TLS_DHE_RSA_WITH_AES_128_CBC_SHA256
 *      TLS_DHE_RSA_WITH_AES_128_CBC_SHA
 *      TLS_DHE_RSA_WITH_CAMELLIA_128_GCM_SHA256
 *      TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256
 *      TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA
 *      TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA
 */
#define POLARSSL_KEY_EXCHANGE_DHE_RSA_ENABLED

/**
 * \def POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED
 *
 * Enable the ECDHE-RSA based ciphersuite modes in SSL / TLS.
 *
 * Requires: POLARSSL_ECDH_C, POLARSSL_RSA_C, POLARSSL_PKCS1_V15,
 *           POLARSSL_X509_CRT_PARSE_C
 *
 * This enables the following ciphersuites (if other requisites are
 * enabled as well):
 *      TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
 *      TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384
 *      TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA
 *      TLS_ECDHE_RSA_WITH_CAMELLIA_256_GCM_SHA384
 *      TLS_ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384
 *      TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
 *      TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256
 *      TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA
 *      TLS_ECDHE_RSA_WITH_CAMELLIA_128_GCM_SHA256
 *      TLS_ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256
 *      TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA
 *      TLS_ECDHE_RSA_WITH_RC4_128_SHA
 */
#define POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED

/**
 * \def POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
 *
 * Enable the ECDHE-ECDSA based ciphersuite modes in SSL / TLS.
 *
 * Requires: POLARSSL_ECDH_C, POLARSSL_ECDSA_C, POLARSSL_X509_CRT_PARSE_C,
 *
 * This enables the following ciphersuites (if other requisites are
 * enabled as well):
 *      TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384
 *      TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384
 *      TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA
 *      TLS_ECDHE_ECDSA_WITH_CAMELLIA_256_GCM_SHA384
 *      TLS_ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384
 *      TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
 *      TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256
 *      TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA
 *      TLS_ECDHE_ECDSA_WITH_CAMELLIA_128_GCM_SHA256
 *      TLS_ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256
 *      TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA
 *      TLS_ECDHE_ECDSA_WITH_RC4_128_SHA
 */
#define POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED

/**
 * \def POLARSSL_KEY_EXCHANGE_ECDH_ECDSA_ENABLED
 *
 * Enable the ECDH-ECDSA based ciphersuite modes in SSL / TLS.
 *
 * Requires: POLARSSL_ECDH_C, POLARSSL_X509_CRT_PARSE_C
 *
 * This enables the following ciphersuites (if other requisites are
 * enabled as well):
 *      TLS_ECDH_ECDSA_WITH_RC4_128_SHA
 *      TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA
 *      TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA
 *      TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA
 *      TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256
 *      TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384
 *      TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256
 *      TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384
 *      TLS_ECDH_ECDSA_WITH_CAMELLIA_128_CBC_SHA256
 *      TLS_ECDH_ECDSA_WITH_CAMELLIA_256_CBC_SHA384
 *      TLS_ECDH_ECDSA_WITH_CAMELLIA_128_GCM_SHA256
 *      TLS_ECDH_ECDSA_WITH_CAMELLIA_256_GCM_SHA384
 */
#define POLARSSL_KEY_EXCHANGE_ECDH_ECDSA_ENABLED

/**
 * \def POLARSSL_KEY_EXCHANGE_ECDH_RSA_ENABLED
 *
 * Enable the ECDH-RSA based ciphersuite modes in SSL / TLS.
 *
 * Requires: POLARSSL_ECDH_C, POLARSSL_X509_CRT_PARSE_C
 *
 * This enables the following ciphersuites (if other requisites are
 * enabled as well):
 *      TLS_ECDH_RSA_WITH_RC4_128_SHA
 *      TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA
 *      TLS_ECDH_RSA_WITH_AES_128_CBC_SHA
 *      TLS_ECDH_RSA_WITH_AES_256_CBC_SHA
 *      TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256
 *      TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384
 *      TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256
 *      TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384
 *      TLS_ECDH_RSA_WITH_CAMELLIA_128_CBC_SHA256
 *      TLS_ECDH_RSA_WITH_CAMELLIA_256_CBC_SHA384
 *      TLS_ECDH_RSA_WITH_CAMELLIA_128_GCM_SHA256
 *      TLS_ECDH_RSA_WITH_CAMELLIA_256_GCM_SHA384
 */
#define POLARSSL_KEY_EXCHANGE_ECDH_RSA_ENABLED

/**
 * \def POLARSSL_ERROR_STRERROR_BC
 *
 * Make available the backward compatible error_strerror() next to the
 * current polarssl_strerror().
 *
 * Disable if you run into name conflicts and want to really remove the
 * error_strerror()
 */
#define POLARSSL_ERROR_STRERROR_BC

/**
 * \def POLARSSL_ERROR_STRERROR_DUMMY
 *
 * Enable a dummy error function to make use of polarssl_strerror() in
 * third party libraries easier.
 *
 * Disable if you run into name conflicts and want to really remove the
 * polarssl_strerror()
 */
#define POLARSSL_ERROR_STRERROR_DUMMY

/**
 * \def POLARSSL_GENPRIME
 *
 * Enable the prime-number generation code.
 *
 * Requires: POLARSSL_BIGNUM_C
 */
#define POLARSSL_GENPRIME

/**
 * \def POLARSSL_FS_IO
 *
 * Enable functions that use the filesystem.
 */
#define POLARSSL_FS_IO

/**
 * \def POLARSSL_NO_DEFAULT_ENTROPY_SOURCES
 *
 * Do not add default entropy sources. These are the platform specific,
 * hardclock and HAVEGE based poll functions.
 *
 * This is useful to have more control over the added entropy sources in an 
 * application.
 *
 * Uncomment this macro to prevent loading of default entropy functions.
 */
//#define POLARSSL_NO_DEFAULT_ENTROPY_SOURCES

/**
 * \def POLARSSL_NO_PLATFORM_ENTROPY
 *
 * Do not use built-in platform entropy functions.
 * This is useful if your platform does not support
 * standards like the /dev/urandom or Windows CryptoAPI.
 *
 * Uncomment this macro to disable the built-in platform entropy functions.
 */
//#define POLARSSL_NO_PLATFORM_ENTROPY

/**
 * \def POLARSSL_MEMORY_DEBUG
 *
 * Enable debugging of buffer allocator memory issues. Automatically prints
 * (to stderr) all (fatal) messages on memory allocation issues. Enables
 * function for 'debug output' of allocated memory.
 *
 * Requires: POLARSSL_MEMORY_BUFFER_ALLOC_C
 *           fprintf()
 *
 * Uncomment this macro to let the buffer allocator print out error messages.
 */
//#define POLARSSL_MEMORY_DEBUG

/**
 * \def POLARSSL_MEMORY_BACKTRACE
 *
 * Include backtrace information with each allocated block.
 *
 * Requires: POLARSSL_MEMORY_BUFFER_ALLOC_C
 *           GLIBC-compatible backtrace() an backtrace_symbols() support
 *
 * Uncomment this macro to include backtrace information
 */
//#define POLARSSL_MEMORY_BACKTRACE

/**
 * \def POLARSSL_PKCS1_V15
 *
 * Enable support for PKCS#1 v1.5 encoding.
 *
 * Requires: POLARSSL_RSA_C
 *
 * This enables support for PKCS#1 v1.5 operations.
 */
#define POLARSSL_PKCS1_V15

/**
 * \def POLARSSL_PKCS1_V21
 *
 * Enable support for PKCS#1 v2.1 encoding.
 *
 * Requires: POLARSSL_MD_C, POLARSSL_RSA_C
 *
 * This enables support for RSAES-OAEP and RSASSA-PSS operations.
 */
#define POLARSSL_PKCS1_V21

/**
 * \def POLARSSL_RSA_NO_CRT
 *
 * Do not use the Chinese Remainder Theorem for the RSA private operation.
 *
 * Uncomment this macro to disable the use of CRT in RSA.
 *
 */
//#define POLARSSL_RSA_NO_CRT

/**
 * \def POLARSSL_SELF_TEST
 *
 * Enable the checkup functions (*_self_test).
 */
#define POLARSSL_SELF_TEST

/**
 * \def POLARSSL_SSL_ALL_ALERT_MESSAGES
 *
 * Enable sending of alert messages in case of encountered errors as per RFC.
 * If you choose not to send the alert messages, PolarSSL can still communicate
 * with other servers, only debugging of failures is harder.
 *
 * The advantage of not sending alert messages, is that no information is given
 * about reasons for failures thus preventing adversaries of gaining intel.
 *
 * Enable sending of all alert messages
 */
#define POLARSSL_SSL_ALERT_MESSAGES

/**
 * \def POLARSSL_SSL_DEBUG_ALL
 *
 * Enable the debug messages in SSL module for all issues.
 * Debug messages have been disabled in some places to prevent timing
 * attacks due to (unbalanced) debugging function calls.
 *
 * If you need all error reporting you should enable this during debugging,
 * but remove this for production servers that should log as well.
 *
 * Uncomment this macro to report all debug messages on errors introducing
 * a timing side-channel.
 *
 */
//#define POLARSSL_SSL_DEBUG_ALL

/**
 * \def POLARSSL_SSL_HW_RECORD_ACCEL
 *
 * Enable hooking functions in SSL module for hardware acceleration of
 * individual records.
 *
 * Uncomment this macro to enable hooking functions.
 */
//#define POLARSSL_SSL_HW_RECORD_ACCEL

/**
 * \def POLARSSL_SSL_SRV_SUPPORT_SSLV2_CLIENT_HELLO
 *
 * Enable support for receiving and parsing SSLv2 Client Hello messages for the
 * SSL Server module (POLARSSL_SSL_SRV_C).
 *
 * Comment this macro to disable support for SSLv2 Client Hello messages.
 */
#define POLARSSL_SSL_SRV_SUPPORT_SSLV2_CLIENT_HELLO

/**
 * \def POLARSSL_SSL_SRV_RESPECT_CLIENT_PREFERENCE
 *
 * Pick the ciphersuite according to the client's preferences rather than ours
 * in the SSL Server module (POLARSSL_SSL_SRV_C).
 *
 * Uncomment this macro to respect client's ciphersuite order
 */
//#define POLARSSL_SSL_SRV_RESPECT_CLIENT_PREFERENCE

/**
 * \def POLARSSL_SSL_MAX_FRAGMENT_LENGTH
 *
 * Enable support for RFC 6066 max_fragment_length extension in SSL.
 *
 * Comment this macro to disable support for the max_fragment_length extension
 */
#define POLARSSL_SSL_MAX_FRAGMENT_LENGTH

/**
 * \def POLARSSL_SSL_PROTO_SSL3
 *
 * Enable support for SSL 3.0.
 *
 * Requires: POLARSSL_MD5_C
 *           POLARSSL_SHA1_C
 *
 * Comment this macro to disable support for SSL 3.0
 */
#define POLARSSL_SSL_PROTO_SSL3

/**
 * \def POLARSSL_SSL_PROTO_TLS1
 *
 * Enable support for TLS 1.0.
 *
 * Requires: POLARSSL_MD5_C
 *           POLARSSL_SHA1_C
 *
 * Comment this macro to disable support for TLS 1.0
 */
#define POLARSSL_SSL_PROTO_TLS1

/**
 * \def POLARSSL_SSL_PROTO_TLS1_1
 *
 * Enable support for TLS 1.1.
 *
 * Requires: POLARSSL_MD5_C
 *           POLARSSL_SHA1_C
 *
 * Comment this macro to disable support for TLS 1.1
 */
#define POLARSSL_SSL_PROTO_TLS1_1

/**
 * \def POLARSSL_SSL_PROTO_TLS1_2
 *
 * Enable support for TLS 1.2.
 *
 * Requires: POLARSSL_SHA1_C or POLARSSL_SHA256_C or POLARSSL_SHA512_C
 *           (Depends on ciphersuites)
 *
 * Comment this macro to disable support for TLS 1.2
 */
#define POLARSSL_SSL_PROTO_TLS1_2

/**
 * \def POLARSSL_SSL_SESSION_TICKETS
 *
 * Enable support for RFC 5077 session tickets in SSL.
 *
 * Requires: POLARSSL_AES_C
 *           POLARSSL_SHA256_C
 *           POLARSSL_CIPHER_MODE_CBC
 *
 * Comment this macro to disable support for SSL session tickets
 */
#define POLARSSL_SSL_SESSION_TICKETS

/**
 * \def POLARSSL_SSL_SERVER_NAME_INDICATION
 *
 * Enable support for RFC 6066 server name indication (SNI) in SSL.
 *
 * Comment this macro to disable support for server name indication in SSL
 */
#define POLARSSL_SSL_SERVER_NAME_INDICATION

/**
 * \def POLARSSL_SSL_TRUNCATED_HMAC
 *
 * Enable support for RFC 6066 truncated HMAC in SSL.
 *
 * Comment this macro to disable support for truncated HMAC in SSL
 */
#define POLARSSL_SSL_TRUNCATED_HMAC

/**
 * \def POLARSSL_THREADING_ALT
 *
 * Provide your own alternate threading implementation.
 *
 * Requires: POLARSSL_THREADING_C
 *
 * Uncomment this to allow your own alternate threading implementation.
 */
//#define POLARSSL_THREADING_ALT

/**
 * \def POLARSSL_THREADING_PTHREAD
 *
 * Enable the pthread wrapper layer for the threading layer.
 *
 * Requires: POLARSSL_THREADING_C
 *
 * Uncomment this to enable pthread mutexes.
 */
//#define POLARSSL_THREADING_PTHREAD

/**
 * \def POLARSSL_X509_ALLOW_EXTENSIONS_NON_V3
 *
 * If set, the X509 parser will not break-off when parsing an X509 certificate
 * and encountering an extension in a v1 or v2 certificate.
 *
 * Uncomment to prevent an error.
 */
//#define POLARSSL_X509_ALLOW_EXTENSIONS_NON_V3

/**
 * \def POLARSSL_X509_ALLOW_UNSUPPORTED_CRITICAL_EXTENSION
 *
 * If set, the X509 parser will not break-off when parsing an X509 certificate
 * and encountering an unknown critical extension.
 *
 * Uncomment to prevent an error.
 */
//#define POLARSSL_X509_ALLOW_UNSUPPORTED_CRITICAL_EXTENSION

/**
 * \def POLARSSL_ZLIB_SUPPORT
 *
 * If set, the SSL/TLS module uses ZLIB to support compression and
 * decompression of packet data.
 *
 * Used in: library/ssl_tls.c
 *          library/ssl_cli.c
 *          library/ssl_srv.c
 *
 * This feature requires zlib library and headers to be present.
 *
 * Uncomment to enable use of ZLIB
 */
//#define POLARSSL_ZLIB_SUPPORT
/* \} name SECTION: PolarSSL feature support */

/**
 * \name SECTION: PolarSSL modules
 *
 * This section enables or disables entire modules in PolarSSL
 * \{
 */

/**
 * \def POLARSSL_AESNI_C
 *
 * Enable AES-NI support on x86-64.
 *
 * Module:  library/aesni.c
 * Caller:  library/aes.c
 *
 * Requires: POLARSSL_HAVE_ASM
 *
 * This modules adds support for the AES-NI instructions on x86-64
 */
#define POLARSSL_AESNI_C

/**
 * \def POLARSSL_AES_C
 *
 * Enable the AES block cipher.
 *
 * Module:  library/aes.c
 * Caller:  library/ssl_tls.c
 *          library/pem.c
 *          library/ctr_drbg.c
 *
 * This module enables the following ciphersuites (if other requisites are
 * enabled as well):
 *      TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA
 *      TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA
 *      TLS_ECDH_RSA_WITH_AES_128_CBC_SHA
 *      TLS_ECDH_RSA_WITH_AES_256_CBC_SHA
 *      TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256
 *      TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384
 *      TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256
 *      TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384
 *      TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256
 *      TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384
 *      TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256
 *      TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384
 *      TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384
 *      TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
 *      TLS_DHE_RSA_WITH_AES_256_GCM_SHA384
 *      TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384
 *      TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384
 *      TLS_DHE_RSA_WITH_AES_256_CBC_SHA256
 *      TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA
 *      TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA
 *      TLS_DHE_RSA_WITH_AES_256_CBC_SHA
 *      TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
 *      TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
 *      TLS_DHE_RSA_WITH_AES_128_GCM_SHA256
 *      TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256
 *      TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256
 *      TLS_DHE_RSA_WITH_AES_128_CBC_SHA256
 *      TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA
 *      TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA
 *      TLS_DHE_RSA_WITH_AES_128_CBC_SHA
 *      TLS_DHE_PSK_WITH_AES_256_GCM_SHA384
 *      TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA384
 *      TLS_DHE_PSK_WITH_AES_256_CBC_SHA384
 *      TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA
 *      TLS_DHE_PSK_WITH_AES_256_CBC_SHA
 *      TLS_DHE_PSK_WITH_AES_128_GCM_SHA256
 *      TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256
 *      TLS_DHE_PSK_WITH_AES_128_CBC_SHA256
 *      TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA
 *      TLS_DHE_PSK_WITH_AES_128_CBC_SHA
 *      TLS_RSA_WITH_AES_256_GCM_SHA384
 *      TLS_RSA_WITH_AES_256_CBC_SHA256
 *      TLS_RSA_WITH_AES_256_CBC_SHA
 *      TLS_RSA_WITH_AES_128_GCM_SHA256
 *      TLS_RSA_WITH_AES_128_CBC_SHA256
 *      TLS_RSA_WITH_AES_128_CBC_SHA
 *      TLS_RSA_PSK_WITH_AES_256_GCM_SHA384
 *      TLS_RSA_PSK_WITH_AES_256_CBC_SHA384
 *      TLS_RSA_PSK_WITH_AES_256_CBC_SHA
 *      TLS_RSA_PSK_WITH_AES_128_GCM_SHA256
 *      TLS_RSA_PSK_WITH_AES_128_CBC_SHA256
 *      TLS_RSA_PSK_WITH_AES_128_CBC_SHA
 *      TLS_PSK_WITH_AES_256_GCM_SHA384
 *      TLS_PSK_WITH_AES_256_CBC_SHA384
 *      TLS_PSK_WITH_AES_256_CBC_SHA
 *      TLS_PSK_WITH_AES_128_GCM_SHA256
 *      TLS_PSK_WITH_AES_128_CBC_SHA256
 *      TLS_PSK_WITH_AES_128_CBC_SHA
 *
 * PEM_PARSE uses AES for decrypting encrypted keys.
 */
#define POLARSSL_AES_C

/**
 * \def POLARSSL_ARC4_C
 *
 * Enable the ARCFOUR stream cipher.
 *
 * Module:  library/arc4.c
 * Caller:  library/ssl_tls.c
 *
 * This module enables the following ciphersuites (if other requisites are
 * enabled as well):
 *      TLS_ECDH_ECDSA_WITH_RC4_128_SHA
 *      TLS_ECDH_RSA_WITH_RC4_128_SHA
 *      TLS_ECDHE_ECDSA_WITH_RC4_128_SHA
 *      TLS_ECDHE_RSA_WITH_RC4_128_SHA
 *      TLS_ECDHE_PSK_WITH_RC4_128_SHA
 *      TLS_DHE_PSK_WITH_RC4_128_SHA
 *      TLS_RSA_WITH_RC4_128_SHA
 *      TLS_RSA_WITH_RC4_128_MD5
 *      TLS_RSA_PSK_WITH_RC4_128_SHA
 *      TLS_PSK_WITH_RC4_128_SHA
 */
#define POLARSSL_ARC4_C

/**
 * \def POLARSSL_ASN1_PARSE_C
 *
 * Enable the generic ASN1 parser.
 *
 * Module:  library/asn1.c
 * Caller:  library/x509.c
 *          library/dhm.c
 *          library/pkcs12.c
 *          library/pkcs5.c
 *          library/pkparse.c
 */
#define POLARSSL_ASN1_PARSE_C

/**
 * \def POLARSSL_ASN1_WRITE_C
 *
 * Enable the generic ASN1 writer.
 *
 * Module:  library/asn1write.c
 * Caller:  library/ecdsa.c
 *          library/pkwrite.c
 *          library/x509_create.c
 *          library/x509write_crt.c
 *          library/x509write_csr.c
 */
#define POLARSSL_ASN1_WRITE_C

/**
 * \def POLARSSL_BASE64_C
 *
 * Enable the Base64 module.
 *
 * Module:  library/base64.c
 * Caller:  library/pem.c
 *
 * This module is required for PEM support (required by X.509).
 */
#define POLARSSL_BASE64_C

/**
 * \def POLARSSL_BIGNUM_C
 *
 * Enable the multi-precision integer library.
 *
 * Module:  library/bignum.c
 * Caller:  library/dhm.c
 *          library/ecp.c
 *          library/rsa.c
 *          library/ssl_tls.c
 *
 * This module is required for RSA and DHM support.
 */
#define POLARSSL_BIGNUM_C

/**
 * \def POLARSSL_BLOWFISH_C
 *
 * Enable the Blowfish block cipher.
 *
 * Module:  library/blowfish.c
 */
#define POLARSSL_BLOWFISH_C

/**
 * \def POLARSSL_CAMELLIA_C
 *
 * Enable the Camellia block cipher.
 *
 * Module:  library/camellia.c
 * Caller:  library/ssl_tls.c
 *
 * This module enables the following ciphersuites (if other requisites are
 * enabled as well):
 *      TLS_ECDH_ECDSA_WITH_CAMELLIA_128_CBC_SHA256
 *      TLS_ECDH_ECDSA_WITH_CAMELLIA_256_CBC_SHA384
 *      TLS_ECDH_RSA_WITH_CAMELLIA_128_CBC_SHA256
 *      TLS_ECDH_RSA_WITH_CAMELLIA_256_CBC_SHA384
 *      TLS_ECDH_ECDSA_WITH_CAMELLIA_128_GCM_SHA256
 *      TLS_ECDH_ECDSA_WITH_CAMELLIA_256_GCM_SHA384
 *      TLS_ECDH_RSA_WITH_CAMELLIA_128_GCM_SHA256
 *      TLS_ECDH_RSA_WITH_CAMELLIA_256_GCM_SHA384
 *      TLS_ECDHE_ECDSA_WITH_CAMELLIA_256_GCM_SHA384
 *      TLS_ECDHE_RSA_WITH_CAMELLIA_256_GCM_SHA384
 *      TLS_DHE_RSA_WITH_CAMELLIA_256_GCM_SHA384
 *      TLS_ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384
 *      TLS_ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384
 *      TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256
 *      TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA
 *      TLS_ECDHE_ECDSA_WITH_CAMELLIA_128_GCM_SHA256
 *      TLS_ECDHE_RSA_WITH_CAMELLIA_128_GCM_SHA256
 *      TLS_DHE_RSA_WITH_CAMELLIA_128_GCM_SHA256
 *      TLS_ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256
 *      TLS_ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256
 *      TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256
 *      TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA
 *      TLS_DHE_PSK_WITH_CAMELLIA_256_GCM_SHA384
 *      TLS_ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384
 *      TLS_DHE_PSK_WITH_CAMELLIA_256_CBC_SHA384
 *      TLS_DHE_PSK_WITH_CAMELLIA_128_GCM_SHA256
 *      TLS_DHE_PSK_WITH_CAMELLIA_128_CBC_SHA256
 *      TLS_ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256
 *      TLS_RSA_WITH_CAMELLIA_256_GCM_SHA384
 *      TLS_RSA_WITH_CAMELLIA_256_CBC_SHA256
 *      TLS_RSA_WITH_CAMELLIA_256_CBC_SHA
 *      TLS_RSA_WITH_CAMELLIA_128_GCM_SHA256
 *      TLS_RSA_WITH_CAMELLIA_128_CBC_SHA256
 *      TLS_RSA_WITH_CAMELLIA_128_CBC_SHA
 *      TLS_RSA_PSK_WITH_CAMELLIA_256_GCM_SHA384
 *      TLS_RSA_PSK_WITH_CAMELLIA_256_CBC_SHA384
 *      TLS_RSA_PSK_WITH_CAMELLIA_128_GCM_SHA256
 *      TLS_RSA_PSK_WITH_CAMELLIA_128_CBC_SHA256
 *      TLS_PSK_WITH_CAMELLIA_256_GCM_SHA384
 *      TLS_PSK_WITH_CAMELLIA_256_CBC_SHA384
 *      TLS_PSK_WITH_CAMELLIA_128_GCM_SHA256
 *      TLS_PSK_WITH_CAMELLIA_128_CBC_SHA256
 */
#define POLARSSL_CAMELLIA_C

/**
 * \def POLARSSL_CERTS_C
 *
 * Enable the test certificates.
 *
 * Module:  library/certs.c
 * Caller:
 *
 * Requires: POLARSSL_PEM_PARSE_C
 *
 * This module is used for testing (ssl_client/server).
 */
#define POLARSSL_CERTS_C

/**
 * \def POLARSSL_CIPHER_C
 *
 * Enable the generic cipher layer.
 *
 * Module:  library/cipher.c
 * Caller:  library/ssl_tls.c
 *
 * Uncomment to enable generic cipher wrappers.
 */
#define POLARSSL_CIPHER_C

/**
 * \def POLARSSL_CTR_DRBG_C
 *
 * Enable the CTR_DRBG AES-256-based random generator.
 *
 * Module:  library/ctr_drbg.c
 * Caller:
 *
 * Requires: POLARSSL_AES_C
 *
 * This module provides the CTR_DRBG AES-256 random number generator.
 */
#define POLARSSL_CTR_DRBG_C

/**
 * \def POLARSSL_DEBUG_C
 *
 * Enable the debug functions.
 *
 * Module:  library/debug.c
 * Caller:  library/ssl_cli.c
 *          library/ssl_srv.c
 *          library/ssl_tls.c
 *
 * This module provides debugging functions.
 */
#define POLARSSL_DEBUG_C

/**
 * \def POLARSSL_DES_C
 *
 * Enable the DES block cipher.
 *
 * Module:  library/des.c
 * Caller:  library/pem.c
 *          library/ssl_tls.c
 *
 * This module enables the following ciphersuites (if other requisites are
 * enabled as well):
 *      TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA
 *      TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA
 *      TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA
 *      TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA
 *      TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA
 *      TLS_ECDHE_PSK_WITH_3DES_EDE_CBC_SHA
 *      TLS_DHE_PSK_WITH_3DES_EDE_CBC_SHA
 *      TLS_RSA_WITH_3DES_EDE_CBC_SHA
 *      TLS_RSA_PSK_WITH_3DES_EDE_CBC_SHA
 *      TLS_PSK_WITH_3DES_EDE_CBC_SHA
 *
 * PEM_PARSE uses DES/3DES for decrypting encrypted keys.
 */
#define POLARSSL_DES_C

/**
 * \def POLARSSL_DHM_C
 *
 * Enable the Diffie-Hellman-Merkle module.
 *
 * Module:  library/dhm.c
 * Caller:  library/ssl_cli.c
 *          library/ssl_srv.c
 *
 * This module is used by the following key exchanges:
 *      DHE-RSA, DHE-PSK
 */
#define POLARSSL_DHM_C

/**
 * \def POLARSSL_ECDH_C
 *
 * Enable the elliptic curve Diffie-Hellman library.
 *
 * Module:  library/ecdh.c
 * Caller:  library/ssl_cli.c
 *          library/ssl_srv.c
 *
 * This module is used by the following key exchanges:
 *      ECDHE-ECDSA, ECDHE-RSA, DHE-PSK
 *
 * Requires: POLARSSL_ECP_C
 */
#define POLARSSL_ECDH_C

/**
 * \def POLARSSL_ECDSA_C
 *
 * Enable the elliptic curve DSA library.
 *
 * Module:  library/ecdsa.c
 * Caller:
 *
 * This module is used by the following key exchanges:
 *      ECDHE-ECDSA
 *
 * Requires: POLARSSL_ECP_C, POLARSSL_ASN1_WRITE_C, POLARSSL_ASN1_PARSE_C
 */
#define POLARSSL_ECDSA_C

/**
 * \def POLARSSL_ECP_C
 *
 * Enable the elliptic curve over GF(p) library.
 *
 * Module:  library/ecp.c
 * Caller:  library/ecdh.c
 *          library/ecdsa.c
 *
 * Requires: POLARSSL_BIGNUM_C and at least one POLARSSL_ECP_DP_XXX_ENABLED
 */
#define POLARSSL_ECP_C

/**
 * \def POLARSSL_ENTROPY_C
 *
 * Enable the platform-specific entropy code.
 *
 * Module:  library/entropy.c
 * Caller:
 *
 * Requires: POLARSSL_SHA512_C
 *
 * This module provides a generic entropy pool
 */
#define POLARSSL_ENTROPY_C

/**
 * \def POLARSSL_ERROR_C
 *
 * Enable error code to error string conversion.
 *
 * Module:  library/error.c
 * Caller:
 *
 * This module enables err_strerror().
 */
#define POLARSSL_ERROR_C

/**
 * \def POLARSSL_GCM_C
 *
 * Enable the Galois/Counter Mode (GCM) for AES.
 *
 * Module:  library/gcm.c
 *
 * Requires: POLARSSL_AES_C or POLARSSL_CAMELLIA_C
 *
 * This module enables the AES-GCM and CAMELLIA-GCM ciphersuites, if other
 * requisites are enabled as well.
 */
#define POLARSSL_GCM_C

/**
 * \def POLARSSL_HAVEGE_C
 *
 * Enable the HAVEGE random generator.
 *
 * Warning: the HAVEGE random generator is not suitable for virtualized
 *          environments
 *
 * Warning: the HAVEGE random generator is dependent on timing and specific
 *          processor traits. It is therefore not advised to use HAVEGE as
 *          your applications primary random generator or primary entropy pool
 *          input. As a secondary input to your entropy pool, it IS able add
 *          the (limited) extra entropy it provides.
 *
 * Module:  library/havege.c
 * Caller:
 *
 * Requires: POLARSSL_TIMING_C
 *
 * Uncomment to enable the HAVEGE random generator.
 */
//#define POLARSSL_HAVEGE_C

/**
 * \def POLARSSL_MD_C
 *
 * Enable the generic message digest layer.
 *
 * Module:  library/md.c
 * Caller:
 *
 * Uncomment to enable generic message digest wrappers.
 */
#define POLARSSL_MD_C

/**
 * \def POLARSSL_MD2_C
 *
 * Enable the MD2 hash algorithm.
 *
 * Module:  library/md2.c
 * Caller:
 *
 * Uncomment to enable support for (rare) MD2-signed X.509 certs.
 */
//#define POLARSSL_MD2_C

/**
 * \def POLARSSL_MD4_C
 *
 * Enable the MD4 hash algorithm.
 *
 * Module:  library/md4.c
 * Caller:
 *
 * Uncomment to enable support for (rare) MD4-signed X.509 certs.
 */
//#define POLARSSL_MD4_C

/**
 * \def POLARSSL_MD5_C
 *
 * Enable the MD5 hash algorithm.
 *
 * Module:  library/md5.c
 * Caller:  library/md.c
 *          library/pem.c
 *          library/ssl_tls.c
 *
 * This module is required for SSL/TLS and X.509.
 * PEM_PARSE uses MD5 for decrypting encrypted keys.
 */
#define POLARSSL_MD5_C

/**
 * \def POLARSSL_MEMORY_C
 *
 * Enable the memory allocation layer.
 * By default PolarSSL uses the system-provided malloc() and free().
 * (As long as POLARSSL_MEMORY_STDMALLOC and POLARSSL_MEMORY_STDFREE
 * are defined and unmodified)
 *
 * This allows different allocators (self-implemented or provided)
 *
 * Enable this layer to allow use of alternative memory allocators.
 */
//#define POLARSSL_MEMORY_C

/**
 * \def POLARSSL_MEMORY_BUFFER_ALLOC_C
 *
 * Enable the buffer allocator implementation that makes use of a (stack)
 * based buffer to 'allocate' dynamic memory. (replaces malloc() and free()
 * calls)
 *
 * Module:  library/memory_buffer_alloc.c
 *
 * Requires: POLARSSL_MEMORY_C
 *
 * Enable this module to enable the buffer memory allocator.
 */
//#define POLARSSL_MEMORY_BUFFER_ALLOC_C

/**
 * \def POLARSSL_NET_C
 *
 * Enable the TCP/IP networking routines.
 *
 * Module:  library/net.c
 *
 * This module provides TCP/IP networking routines.
 */
#define POLARSSL_NET_C

/**
 * \def POLARSSL_OID_C
 *
 * Enable the OID database.
 *
 * Module:  library/oid.c
 * Caller:  library/asn1write.c
 *          library/pkcs5.c
 *          library/pkparse.c
 *          library/pkwrite.c
 *          library/rsa.c
 *          library/x509.c
 *          library/x509_create.c
 *          library/x509_crl.c
 *          library/x509_crt.c
 *          library/x509_csr.c
 *          library/x509write_crt.c
 *          library/x509write_csr.c
 *
 * This modules translates between OIDs and internal values.
 */
#define POLARSSL_OID_C

/**
 * \def POLARSSL_PADLOCK_C
 *
 * Enable VIA Padlock support on x86.
 *
 * Module:  library/padlock.c
 * Caller:  library/aes.c
 *
 * Requires: POLARSSL_HAVE_ASM
 *
 * This modules adds support for the VIA PadLock on x86.
 */
#define POLARSSL_PADLOCK_C

/**
 * \def POLARSSL_PBKDF2_C
 *
 * Enable PKCS#5 PBKDF2 key derivation function.
 * DEPRECATED: Use POLARSSL_PKCS5_C instead
 *
 * Module:  library/pbkdf2.c
 *
 * Requires: POLARSSL_PKCS5_C
 *
 * This module adds support for the PKCS#5 PBKDF2 key derivation function.
 */
#define POLARSSL_PBKDF2_C

/**
 * \def POLARSSL_PEM_PARSE_C
 *
 * Enable PEM decoding / parsing.
 *
 * Module:  library/pem.c
 * Caller:  library/dhm.c
 *          library/pkparse.c
 *          library/x509_crl.c
 *          library/x509_crt.c
 *          library/x509_csr.c
 *
 * Requires: POLARSSL_BASE64_C
 *
 * This modules adds support for decoding / parsing PEM files.
 */
#define POLARSSL_PEM_PARSE_C

/**
 * \def POLARSSL_PEM_WRITE_C
 *
 * Enable PEM encoding / writing.
 *
 * Module:  library/pem.c
 * Caller:  library/pkwrite.c
 *          library/x509write_crt.c
 *          library/x509write_csr.c
 *
 * Requires: POLARSSL_BASE64_C
 *
 * This modules adds support for encoding / writing PEM files.
 */
#define POLARSSL_PEM_WRITE_C

/**
 * \def POLARSSL_PK_C
 *
 * Enable the generic public (asymetric) key layer.
 *
 * Module:  library/pk.c
 * Caller:  library/ssl_tls.c
 *          library/ssl_cli.c
 *          library/ssl_srv.c
 *
 * Requires: POLARSSL_RSA_C or POLARSSL_ECP_C
 *
 * Uncomment to enable generic public key wrappers.
 */
#define POLARSSL_PK_C

/**
 * \def POLARSSL_PK_PARSE_C
 *
 * Enable the generic public (asymetric) key parser.
 *
 * Module:  library/pkparse.c
 * Caller:  library/x509_crt.c
 *          library/x509_csr.c
 *
 * Requires: POLARSSL_PK_C
 *
 * Uncomment to enable generic public key parse functions.
 */
#define POLARSSL_PK_PARSE_C

/**
 * \def POLARSSL_PK_WRITE_C
 *
 * Enable the generic public (asymetric) key writer.
 *
 * Module:  library/pkwrite.c
 * Caller:  library/x509write.c
 *
 * Requires: POLARSSL_PK_C
 *
 * Uncomment to enable generic public key write functions.
 */
#define POLARSSL_PK_WRITE_C

/**
 * \def POLARSSL_PKCS5_C
 *
 * Enable PKCS#5 functions.
 *
 * Module:  library/pkcs5.c
 *
 * Requires: POLARSSL_MD_C
 *
 * This module adds support for the PKCS#5 functions.
 */
#define POLARSSL_PKCS5_C

/**
 * \def POLARSSL_PKCS11_C
 *
 * Enable wrapper for PKCS#11 smartcard support.
 *
 * Module:  library/pkcs11.c
 * Caller:  library/pk.c
 *
 * Requires: POLARSSL_PK_C
 *
 * This module enables SSL/TLS PKCS #11 smartcard support.
 * Requires the presence of the PKCS#11 helper library (libpkcs11-helper)
 */
//#define POLARSSL_PKCS11_C

/**
 * \def POLARSSL_PKCS12_C
 *
 * Enable PKCS#12 PBE functions.
 * Adds algorithms for parsing PKCS#8 encrypted private keys
 *
 * Module:  library/pkcs12.c
 * Caller:  library/pkparse.c
 *
 * Requires: POLARSSL_ASN1_PARSE_C, POLARSSL_CIPHER_C, POLARSSL_MD_C
 * Can use:  POLARSSL_ARC4_C
 *
 * This module enables PKCS#12 functions.
 */
#define POLARSSL_PKCS12_C

/**
 * \def POLARSSL_RIPEMD160_C
 *
 * Enable the RIPEMD-160 hash algorithm.
 *
 * Module:  library/ripemd160.c
 * Caller:  library/md.c
 *
 */
#define POLARSSL_RIPEMD160_C

/**
 * \def POLARSSL_RSA_C
 *
 * Enable the RSA public-key cryptosystem.
 *
 * Module:  library/rsa.c
 * Caller:  library/ssl_cli.c
 *          library/ssl_srv.c
 *          library/ssl_tls.c
 *          library/x509.c
 *
 * This module is used by the following key exchanges:
 *      RSA, DHE-RSA, ECDHE-RSA, RSA-PSK
 *
 * Requires: POLARSSL_BIGNUM_C, POLARSSL_OID_C
 */
#define POLARSSL_RSA_C

/**
 * \def POLARSSL_SHA1_C
 *
 * Enable the SHA1 cryptographic hash algorithm.
 *
 * Module:  library/sha1.c
 * Caller:  library/md.c
 *          library/ssl_cli.c
 *          library/ssl_srv.c
 *          library/ssl_tls.c
 *          library/x509write_crt.c
 *
 * This module is required for SSL/TLS and SHA1-signed certificates.
 */
#define POLARSSL_SHA1_C

/**
 * \def POLARSSL_SHA256_C
 *
 * Enable the SHA-224 and SHA-256 cryptographic hash algorithms.
 * (Used to be POLARSSL_SHA2_C)
 *
 * Module:  library/sha256.c
 * Caller:  library/entropy.c
 *          library/md.c
 *          library/ssl_cli.c
 *          library/ssl_srv.c
 *          library/ssl_tls.c
 *
 * This module adds support for SHA-224 and SHA-256.
 * This module is required for the SSL/TLS 1.2 PRF function.
 */
#define POLARSSL_SHA256_C

/**
 * \def POLARSSL_SHA512_C
 *
 * Enable the SHA-384 and SHA-512 cryptographic hash algorithms.
 * (Used to be POLARSSL_SHA4_C)
 *
 * Module:  library/sha512.c
 * Caller:  library/entropy.c
 *          library/md.c
 *          library/ssl_cli.c
 *          library/ssl_srv.c
 *
 * This module adds support for SHA-384 and SHA-512.
 */
#define POLARSSL_SHA512_C

/**
 * \def POLARSSL_SSL_CACHE_C
 *
 * Enable simple SSL cache implementation.
 *
 * Module:  library/ssl_cache.c
 * Caller:
 *
 * Requires: POLARSSL_SSL_CACHE_C
 */
#define POLARSSL_SSL_CACHE_C

/**
 * \def POLARSSL_SSL_CLI_C
 *
 * Enable the SSL/TLS client code.
 *
 * Module:  library/ssl_cli.c
 * Caller:
 *
 * Requires: POLARSSL_SSL_TLS_C
 *
 * This module is required for SSL/TLS client support.
 */
#define POLARSSL_SSL_CLI_C

/**
 * \def POLARSSL_SSL_SRV_C
 *
 * Enable the SSL/TLS server code.
 *
 * Module:  library/ssl_srv.c
 * Caller:
 *
 * Requires: POLARSSL_SSL_TLS_C
 *
 * This module is required for SSL/TLS server support.
 */
#define POLARSSL_SSL_SRV_C

/**
 * \def POLARSSL_SSL_TLS_C
 *
 * Enable the generic SSL/TLS code.
 *
 * Module:  library/ssl_tls.c
 * Caller:  library/ssl_cli.c
 *          library/ssl_srv.c
 *
 * Requires: POLARSSL_CIPHER_C, POLARSSL_MD_C
 *           and at least one of the POLARSSL_SSL_PROTO_* defines
 *
 * This module is required for SSL/TLS.
 */
#define POLARSSL_SSL_TLS_C

/**
 * \def POLARSSL_THREADING_C
 *
 * Enable the threading abstraction layer.
 * By default PolarSSL assumes it is used in a non-threaded environment or that
 * contexts are not shared between threads. If you do intend to use contexts
 * between threads, you will need to enable this layer to prevent race
 * conditions.
 *
 * Module:  library/threading.c
 *
 * This allows different threading implementations (self-implemented or
 * provided).
 *
 * You will have to enable either POLARSSL_THREADING_ALT or
 * POLARSSL_THREADING_PTHREAD.
 *
 * Enable this layer to allow use of mutexes within PolarSSL
 */
//#define POLARSSL_THREADING_C

/**
 * \def POLARSSL_TIMING_C
 *
 * Enable the portable timing interface.
 *
 * Module:  library/timing.c
 * Caller:  library/havege.c
 *
 * This module is used by the HAVEGE random number generator.
 */
#define POLARSSL_TIMING_C

/**
 * \def POLARSSL_VERSION_C
 *
 * Enable run-time version information.
 *
 * Module:  library/version.c
 *
 * This module provides run-time version information.
 */
#define POLARSSL_VERSION_C

/**
 * \def POLARSSL_X509_USE_C
 *
 * Enable X.509 core for using certificates.
 *
 * Module:  library/x509.c
 * Caller:  library/x509_crl.c
 *          library/x509_crt.c
 *          library/x509_csr.c
 *
 * Requires: POLARSSL_ASN1_PARSE_C, POLARSSL_BIGNUM_C, POLARSSL_OID_C,
 *           POLARSSL_PK_PARSE_C
 *
 * This module is required for the X.509 parsing modules.
 */
#define POLARSSL_X509_USE_C

/**
 * \def POLARSSL_X509_CRT_PARSE_C
 *
 * Enable X.509 certificate parsing.
 *
 * Module:  library/x509_crt.c
 * Caller:  library/ssl_cli.c
 *          library/ssl_srv.c
 *          library/ssl_tls.c
 *
 * Requires: POLARSSL_X509_USE_C
 *
 * This module is required for X.509 certificate parsing.
 */
#define POLARSSL_X509_CRT_PARSE_C

/**
 * \def POLARSSL_X509_CRL_PARSE_C
 *
 * Enable X.509 CRL parsing.
 *
 * Module:  library/x509_crl.c
 * Caller:  library/x509_crt.c
 *
 * Requires: POLARSSL_X509_USE_C
 *
 * This module is required for X.509 CRL parsing.
 */
#define POLARSSL_X509_CRL_PARSE_C

/**
 * \def POLARSSL_X509_CSR_PARSE_C
 *
 * Enable X.509 Certificate Signing Request (CSR) parsing.
 *
 * Module:  library/x509_csr.c
 * Caller:  library/x509_crt_write.c
 *
 * Requires: POLARSSL_X509_USE_C
 *
 * This module is used for reading X.509 certificate request.
 */
#define POLARSSL_X509_CSR_PARSE_C

/**
 * \def POLARSSL_X509_CREATE_C
 *
 * Enable X.509 core for creating certificates.
 *
 * Module:  library/x509_create.c
 *
 * Requires: POLARSSL_BIGNUM_C, POLARSSL_OID_C, POLARSSL_PK_WRITE_C
 *
 * This module is the basis for creating X.509 certificates and CSRs.
 */
#define POLARSSL_X509_CREATE_C

/**
 * \def POLARSSL_X509_CRT_WRITE_C
 *
 * Enable creating X.509 certificates.
 *
 * Module:  library/x509_crt_write.c
 *
 * Requires: POLARSSL_CREATE_C
 *
 * This module is required for X.509 certificate creation.
 */
#define POLARSSL_X509_CRT_WRITE_C

/**
 * \def POLARSSL_X509_CSR_WRITE_C
 *
 * Enable creating X.509 Certificate Signing Requests (CSR).
 *
 * Module:  library/x509_csr_write.c
 *
 * Requires: POLARSSL_CREATE_C
 *
 * This module is required for X.509 certificate request writing.
 */
#define POLARSSL_X509_CSR_WRITE_C

/**
 * \def POLARSSL_XTEA_C
 *
 * Enable the XTEA block cipher.
 *
 * Module:  library/xtea.c
 * Caller:
 */
#define POLARSSL_XTEA_C

/* \} name SECTION: PolarSSL modules */

/**
 * \name SECTION: Module configuration options
 *
 * This section allows for the setting of module specific sizes and
 * configuration options. The default values are already present in the
 * relevant header files and should suffice for the regular use cases.
 * Our advice is to enable POLARSSL_CONFIG_OPTIONS and change values here
 * only if you have a good reason and know the consequences.
 *
 * If POLARSSL_CONFIG_OPTIONS is undefined here the options in the module
 * header file take precedence.
 *
 * Please check the respective header file for documentation on these
 * parameters (to prevent duplicate documentation).
 *
 * Uncomment POLARSSL_CONFIG_OPTIONS to enable using the values defined here.
 * \{
 */
//#define POLARSSL_CONFIG_OPTIONS   /**< Enable config.h module value configuration */

#if defined(POLARSSL_CONFIG_OPTIONS)

// MPI / BIGNUM options
//
#define POLARSSL_MPI_WINDOW_SIZE            6 /**< Maximum windows size used. */
#define POLARSSL_MPI_MAX_SIZE             512 /**< Maximum number of bytes for usable MPIs. */

// CTR_DRBG options
//
#define CTR_DRBG_ENTROPY_LEN               48 /**< Amount of entropy used per seed by default (48 with SHA-512, 32 with SHA-256) */
#define CTR_DRBG_RESEED_INTERVAL        10000 /**< Interval before reseed is performed by default */
#define CTR_DRBG_MAX_INPUT                256 /**< Maximum number of additional input bytes */
#define CTR_DRBG_MAX_REQUEST             1024 /**< Maximum number of requested bytes per call */
#define CTR_DRBG_MAX_SEED_INPUT           384 /**< Maximum size of (re)seed buffer */

// ECP options
//
#define POLARSSL_ECP_MAX_BITS             521 /**< Maximum bit size of groups */
#define POLARSSL_ECP_WINDOW_SIZE            6 /**< Maximum window size used */
#define POLARSSL_ECP_FIXED_POINT_OPTIM      1 /**< Enable fixed-point speed-up */

// Entropy options
//
#define ENTROPY_MAX_SOURCES                20 /**< Maximum number of sources supported */
#define ENTROPY_MAX_GATHER                128 /**< Maximum amount requested from entropy sources */

// Memory options
#define MEMORY_ALIGN_MULTIPLE               4 /**< Align on multiples of this value */
#define POLARSSL_MEMORY_STDMALLOC      malloc /**< Default allocator to use, can be undefined */
#define POLARSSL_MEMORY_STDFREE          free /**< Default free to use, can be undefined */

// SSL Cache options
//
#define SSL_CACHE_DEFAULT_TIMEOUT       86400 /**< 1 day  */
#define SSL_CACHE_DEFAULT_MAX_ENTRIES      50 /**< Maximum entries in cache */

// SSL options
//
#define SSL_MAX_CONTENT_LEN             16384 /**< Size of the input / output buffer */
#define SSL_DEFAULT_TICKET_LIFETIME     86400 /**< Lifetime of session tickets (if enabled) */

#endif /* POLARSSL_CONFIG_OPTIONS */

/* \} name */

/*
 * Sanity checks on defines and dependencies
 */
#if defined(POLARSSL_AESNI_C) && !defined(POLARSSL_HAVE_ASM)
#error "POLARSSL_AESNI_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_CERTS_C) && !defined(POLARSSL_PEM_PARSE_C)
#error "POLARSSL_CERTS_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_CTR_DRBG_C) && !defined(POLARSSL_AES_C)
#error "POLARSSL_CTR_DRBG_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_DHM_C) && !defined(POLARSSL_BIGNUM_C)
#error "POLARSSL_DHM_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_ECDH_C) && !defined(POLARSSL_ECP_C)
#error "POLARSSL_ECDH_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_ECDSA_C) &&            \
    ( !defined(POLARSSL_ECP_C) ||           \
      !defined(POLARSSL_ASN1_PARSE_C) ||    \
      !defined(POLARSSL_ASN1_WRITE_C) )
#error "POLARSSL_ECDSA_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_ECDSA_DETERMINISTIC) && !defined(POLARSSL_MD_C)
#error "POLARSSL_ECDSA_DETERMINISTIC defined, but not all prerequisites"
#endif

#if defined(POLARSSL_ECP_C) && ( !defined(POLARSSL_BIGNUM_C) || (   \
    !defined(POLARSSL_ECP_DP_SECP192R1_ENABLED) &&                  \
    !defined(POLARSSL_ECP_DP_SECP224R1_ENABLED) &&                  \
    !defined(POLARSSL_ECP_DP_SECP256R1_ENABLED) &&                  \
    !defined(POLARSSL_ECP_DP_SECP384R1_ENABLED) &&                  \
    !defined(POLARSSL_ECP_DP_SECP521R1_ENABLED) &&                  \
    !defined(POLARSSL_ECP_DP_BP256R1_ENABLED)   &&                  \
    !defined(POLARSSL_ECP_DP_BP384R1_ENABLED)   &&                  \
    !defined(POLARSSL_ECP_DP_BP512R1_ENABLED) ) )
#error "POLARSSL_ECP_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_ENTROPY_C) && (!defined(POLARSSL_SHA512_C) &&      \
                                    !defined(POLARSSL_SHA256_C))
#error "POLARSSL_ENTROPY_C defined, but not all prerequisites"
#endif
#if defined(POLARSSL_ENTROPY_C) && defined(POLARSSL_SHA512_C) &&         \
    defined(POLARSSL_CONFIG_OPTIONS) && (CTR_DRBG_ENTROPY_LEN > 64)
#error "CTR_DRBG_ENTROPY_LEN value too high"
#endif
#if defined(POLARSSL_ENTROPY_C) && !defined(POLARSSL_SHA512_C) &&        \
    defined(POLARSSL_CONFIG_OPTIONS) && (CTR_DRBG_ENTROPY_LEN > 32)
#error "CTR_DRBG_ENTROPY_LEN value too high"
#endif

#if defined(POLARSSL_GCM_C) && (                                        \
        !defined(POLARSSL_AES_C) && !defined(POLARSSL_CAMELLIA_C) )
#error "POLARSSL_GCM_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_HAVEGE_C) && !defined(POLARSSL_TIMING_C)
#error "POLARSSL_HAVEGE_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_KEY_EXCHANGE_ECDH_ECDSA_ENABLED) &&                 \
    ( !defined(POLARSSL_ECDH_C) || !defined(POLARSSL_X509_CRT_PARSE_C) )
#error "POLARSSL_KEY_EXCHANGE_ECDH_ECDSA_ENABLED defined, but not all prerequisites"
#endif

#if defined(POLARSSL_KEY_EXCHANGE_ECDH_RSA_ENABLED) &&                 \
    ( !defined(POLARSSL_ECDH_C) || !defined(POLARSSL_X509_CRT_PARSE_C) )
#error "POLARSSL_KEY_EXCHANGE_ECDH_RSA_ENABLED defined, but not all prerequisites"
#endif

#if defined(POLARSSL_KEY_EXCHANGE_DHE_PSK_ENABLED) && !defined(POLARSSL_DHM_C)
#error "POLARSSL_KEY_EXCHANGE_DHE_PSK_ENABLED defined, but not all prerequisites"
#endif

#if defined(POLARSSL_KEY_EXCHANGE_ECDHE_PSK_ENABLED) &&                     \
    !defined(POLARSSL_ECDH_C)
#error "POLARSSL_KEY_EXCHANGE_ECDHE_PSK_ENABLED defined, but not all prerequisites"
#endif

#if defined(POLARSSL_KEY_EXCHANGE_DHE_RSA_ENABLED) &&                   \
    ( !defined(POLARSSL_DHM_C) || !defined(POLARSSL_RSA_C) ||           \
      !defined(POLARSSL_X509_CRT_PARSE_C) || !defined(POLARSSL_PKCS1_V15) )
#error "POLARSSL_KEY_EXCHANGE_DHE_RSA_ENABLED defined, but not all prerequisites"
#endif

#if defined(POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED) &&                 \
    ( !defined(POLARSSL_ECDH_C) || !defined(POLARSSL_RSA_C) ||          \
      !defined(POLARSSL_X509_CRT_PARSE_C) || !defined(POLARSSL_PKCS1_V15) )
#error "POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED defined, but not all prerequisites"
#endif

#if defined(POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED) &&                 \
    ( !defined(POLARSSL_ECDH_C) || !defined(POLARSSL_ECDSA_C) ||          \
      !defined(POLARSSL_X509_CRT_PARSE_C) )
#error "POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED defined, but not all prerequisites"
#endif

#if defined(POLARSSL_KEY_EXCHANGE_RSA_PSK_ENABLED) &&                   \
    ( !defined(POLARSSL_RSA_C) || !defined(POLARSSL_X509_CRT_PARSE_C) ||\
      !defined(POLARSSL_PKCS1_V15) )
#error "POLARSSL_KEY_EXCHANGE_RSA_PSK_ENABLED defined, but not all prerequisites"
#endif

#if defined(POLARSSL_KEY_EXCHANGE_RSA_ENABLED) &&                       \
    ( !defined(POLARSSL_RSA_C) || !defined(POLARSSL_X509_CRT_PARSE_C) ||\
      !defined(POLARSSL_PKCS1_V15) )
#error "POLARSSL_KEY_EXCHANGE_RSA_ENABLED defined, but not all prerequisites"
#endif

#if defined(POLARSSL_MEMORY_BUFFER_ALLOC_C) && !defined(POLARSSL_MEMORY_C)
#error "POLARSSL_MEMORY_BUFFER_ALLOC_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_PADLOCK_C) && !defined(POLARSSL_HAVE_ASM)
#error "POLARSSL_PADLOCK_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_PBKDF2_C) && !defined(POLARSSL_MD_C)
#error "POLARSSL_PBKDF2_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_PEM_PARSE_C) && !defined(POLARSSL_BASE64_C)
#error "POLARSSL_PEM_PARSE_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_PEM_WRITE_C) && !defined(POLARSSL_BASE64_C)
#error "POLARSSL_PEM_WRITE_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_PK_PARSE_C) && !defined(POLARSSL_PK_C)
#error "POLARSSL_PK_PARSE_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_PK_WRITE_C) && !defined(POLARSSL_PK_C)
#error "POLARSSL_PK_WRITE_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_PKCS11_C) && !defined(POLARSSL_PK_C)
#error "POLARSSL_PKCS11_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_RSA_C) && ( !defined(POLARSSL_BIGNUM_C) ||         \
    !defined(POLARSSL_OID_C) )
#error "POLARSSL_RSA_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_SSL_PROTO_SSL3) && ( !defined(POLARSSL_MD5_C) ||     \
    !defined(POLARSSL_SHA1_C) )
#error "POLARSSL_SSL_PROTO_SSL3 defined, but not all prerequisites"
#endif

#if defined(POLARSSL_SSL_PROTO_TLS1) && ( !defined(POLARSSL_MD5_C) ||     \
    !defined(POLARSSL_SHA1_C) )
#error "POLARSSL_SSL_PROTO_TLS1 defined, but not all prerequisites"
#endif

#if defined(POLARSSL_SSL_PROTO_TLS1_1) && ( !defined(POLARSSL_MD5_C) ||     \
    !defined(POLARSSL_SHA1_C) )
#error "POLARSSL_SSL_PROTO_TLS1_1 defined, but not all prerequisites"
#endif

#if defined(POLARSSL_SSL_PROTO_TLS1_2) && ( !defined(POLARSSL_SHA1_C) &&     \
    !defined(POLARSSL_SHA256_C) && !defined(POLARSSL_SHA512_C) )
#error "POLARSSL_SSL_PROTO_TLS1_2 defined, but not all prerequisites"
#endif

#if defined(POLARSSL_SSL_CLI_C) && !defined(POLARSSL_SSL_TLS_C)
#error "POLARSSL_SSL_CLI_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_SSL_TLS_C) && ( !defined(POLARSSL_CIPHER_C) ||     \
    !defined(POLARSSL_MD_C) )
#error "POLARSSL_SSL_TLS_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_SSL_SRV_C) && !defined(POLARSSL_SSL_TLS_C)
#error "POLARSSL_SSL_SRV_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_SSL_TLS_C) && (!defined(POLARSSL_SSL_PROTO_SSL3) && \
    !defined(POLARSSL_SSL_PROTO_TLS1) && !defined(POLARSSL_SSL_PROTO_TLS1_1) && \
    !defined(POLARSSL_SSL_PROTO_TLS1_2))
#error "POLARSSL_SSL_TLS_C defined, but no protocols are active"
#endif

#if defined(POLARSSL_SSL_TLS_C) && (defined(POLARSSL_SSL_PROTO_SSL3) && \
    defined(POLARSSL_SSL_PROTO_TLS1_1) && !defined(POLARSSL_SSL_PROTO_TLS1))
#error "Illegal protocol selection"
#endif

#if defined(POLARSSL_SSL_TLS_C) && (defined(POLARSSL_SSL_PROTO_TLS1) && \
    defined(POLARSSL_SSL_PROTO_TLS1_2) && !defined(POLARSSL_SSL_PROTO_TLS1_1))
#error "Illegal protocol selection"
#endif

#if defined(POLARSSL_SSL_TLS_C) && (defined(POLARSSL_SSL_PROTO_SSL3) && \
    defined(POLARSSL_SSL_PROTO_TLS1_2) && (!defined(POLARSSL_SSL_PROTO_TLS1) || \
    !defined(POLARSSL_SSL_PROTO_TLS1_1)))
#error "Illegal protocol selection"
#endif

#if defined(POLARSSL_SSL_SESSION_TICKETS) && defined(POLARSSL_SSL_TLS_C) && \
    ( !defined(POLARSSL_AES_C) || !defined(POLARSSL_SHA256_C) ||            \
      !defined(POLARSSL_CIPHER_MODE_CBC) )
#error "POLARSSL_SSL_SESSION_TICKETS_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_THREADING_PTHREAD)
#if !defined(POLARSSL_THREADING_C) || defined(POLARSSL_THREADING_IMPL)
#error "POLARSSL_THREADING_PTHREAD defined, but not all prerequisites"
#endif
#define POLARSSL_THREADING_IMPL
#endif

#if defined(POLARSSL_THREADING_ALT)
#if !defined(POLARSSL_THREADING_C) || defined(POLARSSL_THREADING_IMPL)
#error "POLARSSL_THREADING_ALT defined, but not all prerequisites"
#endif
#define POLARSSL_THREADING_IMPL
#endif

#if defined(POLARSSL_THREADING_C) && !defined(POLARSSL_THREADING_IMPL)
#error "POLARSSL_THREADING_C defined, single threading implementation required"
#endif
#undef POLARSSL_THREADING_IMPL

#if defined(POLARSSL_X509_USE_C) && ( !defined(POLARSSL_BIGNUM_C) ||  \
    !defined(POLARSSL_OID_C) || !defined(POLARSSL_ASN1_PARSE_C) ||      \
    !defined(POLARSSL_PK_PARSE_C) )
#error "POLARSSL_X509_USE_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_X509_CREATE_C) && ( !defined(POLARSSL_BIGNUM_C) ||  \
    !defined(POLARSSL_OID_C) || !defined(POLARSSL_ASN1_WRITE_C) ||       \
    !defined(POLARSSL_PK_WRITE_C) )
#error "POLARSSL_X509_CREATE_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_X509_CRT_PARSE_C) && ( !defined(POLARSSL_X509_USE_C) )
#error "POLARSSL_X509_CRT_PARSE_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_X509_CRL_PARSE_C) && ( !defined(POLARSSL_X509_USE_C) )
#error "POLARSSL_X509_CRL_PARSE_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_X509_CSR_PARSE_C) && ( !defined(POLARSSL_X509_USE_C) )
#error "POLARSSL_X509_CSR_PARSE_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_X509_CRT_WRITE_C) && ( !defined(POLARSSL_X509_CREATE_C) )
#error "POLARSSL_X509_CRT_WRITE_C defined, but not all prerequisites"
#endif

#if defined(POLARSSL_X509_CSR_WRITE_C) && ( !defined(POLARSSL_X509_CREATE_C) )
#error "POLARSSL_X509_CSR_WRITE_C defined, but not all prerequisites"
#endif

#endif /* config.h */
