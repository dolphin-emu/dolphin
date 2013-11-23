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
#define POLARSSL_HAVE_INT8
 */

/**
 * \def POLARSSL_HAVE_INT16
 *
 * The system uses 16-bit wide native integers.
 *
 * Uncomment if native integers are 16-bit wide.
#define POLARSSL_HAVE_INT16
 */

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
 * The compiler has support for asm()
 *
 * Uncomment to enable the use of assembly code.
 *
 * Requires support for asm() in compiler.
 *
 * Used in:
 *      library/timing.c
 *      library/padlock.c
 *      include/polarssl/bn_mul.h
 *
 */
#define POLARSSL_HAVE_ASM

/**
 * \def POLARSSL_HAVE_SSE2
 *
 * CPU supports SSE2 instruction set.
 *
 * Uncomment if the CPU supports SSE2 (IA-32 specific).
 *
#define POLARSSL_HAVE_SSE2
 */
/* \} name */

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
#define POLARSSL_AES_ALT
#define POLARSSL_ARC4_ALT
#define POLARSSL_BLOWFISH_ALT
#define POLARSSL_CAMELLIA_ALT
#define POLARSSL_DES_ALT
#define POLARSSL_XTEA_ALT
#define POLARSSL_MD2_ALT
#define POLARSSL_MD4_ALT
#define POLARSSL_MD5_ALT
#define POLARSSL_SHA1_ALT
#define POLARSSL_SHA2_ALT
#define POLARSSL_SHA4_ALT
 */

/**
 * \def POLARSSL_AES_ROM_TABLES
 *
 * Store the AES tables in ROM.
 *
 * Uncomment this macro to store the AES tables in ROM.
 *
#define POLARSSL_AES_ROM_TABLES
 */

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
 *      TLS_RSA_WITH_NULL_MD5
 *      TLS_RSA_WITH_NULL_SHA
 *      TLS_RSA_WITH_NULL_SHA256
 *
 * Uncomment this macro to enable the NULL cipher and ciphersuites
#define POLARSSL_CIPHER_NULL_CIPHER
 */

/**
 * \def POLARSSL_ENABLE_WEAK_CIPHERSUITES
 *
 * Enable weak ciphersuites in SSL / TLS
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
 * \def POLARSSL_ERROR_STRERROR_DUMMY
 *
 * Enable a dummy error function to make use of error_strerror() in
 * third party libraries easier.
 *
 * Disable if you run into name conflicts and want to really remove the
 * error_strerror()
 */
#define POLARSSL_ERROR_STRERROR_DUMMY

/**
 * \def POLARSSL_GENPRIME
 *
 * Requires: POLARSSL_BIGNUM_C, POLARSSL_RSA_C
 *
 * Enable the RSA prime-number generation code.
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
#define POLARSSL_NO_DEFAULT_ENTROPY_SOURCES
 */

/**
 * \def POLARSSL_NO_PLATFORM_ENTROPY
 *
 * Do not use built-in platform entropy functions.
 * This is useful if your platform does not support
 * standards like the /dev/urandom or Windows CryptoAPI.
 *
 * Uncomment this macro to disable the built-in platform entropy functions.
#define POLARSSL_NO_PLATFORM_ENTROPY
 */

/**
 * \def POLARSSL_PKCS1_V21
 *
 * Requires: POLARSSL_MD_C, POLARSSL_RSA_C
 *
 * Enable support for PKCS#1 v2.1 encoding.
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
#define POLARSSL_RSA_NO_CRT
 */

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
#define POLARSSL_SSL_DEBUG_ALL
 */

/**
 * \def POLARSSL_SSL_HW_RECORD_ACCEL
 *
 * Enable hooking functions in SSL module for hardware acceleration of
 * individual records.
 *
 * Uncomment this macro to enable hooking functions.
#define POLARSSL_SSL_HW_RECORD_ACCEL
 */

/**
 * \def POLARSSL_SSL_SRV_SUPPORT_SSLV2_CLIENT_HELLO
 *
 * Enable support for receiving and parsing SSLv2 Client Hello messages for the
 * SSL Server module (POLARSSL_SSL_SRV_C)
 *
 * Comment this macro to disable support for SSLv2 Client Hello messages.
 */
#define POLARSSL_SSL_SRV_SUPPORT_SSLV2_CLIENT_HELLO

/**
 * \def POLARSSL_X509_ALLOW_UNSUPPORTED_CRITICAL_EXTENSION
 *
 * If set, the X509 parser will not break-off when parsing an X509 certificate
 * and encountering an unknown critical extension.
 *
 * Uncomment to prevent an error.
 *
#define POLARSSL_X509_ALLOW_UNSUPPORTED_CRITICAL_EXTENSION
 */

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
#define POLARSSL_ZLIB_SUPPORT
 */
/* \} name */

/**
 * \name SECTION: PolarSSL modules
 *
 * This section enables or disables entire modules in PolarSSL
 * \{
 */

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
 *      TLS_RSA_WITH_AES_128_CBC_SHA
 *      TLS_RSA_WITH_AES_256_CBC_SHA
 *      TLS_DHE_RSA_WITH_AES_128_CBC_SHA
 *      TLS_DHE_RSA_WITH_AES_256_CBC_SHA
 *      TLS_RSA_WITH_AES_128_CBC_SHA256
 *      TLS_RSA_WITH_AES_256_CBC_SHA256
 *      TLS_DHE_RSA_WITH_AES_128_CBC_SHA256
 *      TLS_DHE_RSA_WITH_AES_256_CBC_SHA256
 *      TLS_RSA_WITH_AES_128_GCM_SHA256
 *      TLS_RSA_WITH_AES_256_GCM_SHA384
 *
 * PEM uses AES for decrypting encrypted keys.
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
 * This module enables the following ciphersuites:
 *      TLS_RSA_WITH_RC4_128_MD5
 *      TLS_RSA_WITH_RC4_128_SHA
 */
#define POLARSSL_ARC4_C

/**
 * \def POLARSSL_ASN1_PARSE_C
 *
 * Enable the generic ASN1 parser.
 *
 * Module:  library/asn1.c
 * Caller:  library/x509parse.c
 */
#define POLARSSL_ASN1_PARSE_C

/**
 * \def POLARSSL_ASN1_WRITE_C
 *
 * Enable the generic ASN1 writer.
 *
 * Module:  library/asn1write.c
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
 *          library/rsa.c
 *          library/ssl_tls.c
 *          library/x509parse.c
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
 *      TLS_RSA_WITH_CAMELLIA_128_CBC_SHA
 *      TLS_RSA_WITH_CAMELLIA_256_CBC_SHA
 *      TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA
 *      TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA
 *      TLS_RSA_WITH_CAMELLIA_128_CBC_SHA256
 *      TLS_RSA_WITH_CAMELLIA_256_CBC_SHA256
 *      TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256
 *      TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256
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
 * This module is used for testing (ssl_client/server).
 */
#define POLARSSL_CERTS_C

/**
 * \def POLARSSL_CIPHER_C
 *
 * Enable the generic cipher layer.
 *
 * Module:  library/cipher.c
 * Caller:
 *
 * Uncomment to enable generic cipher wrappers.
 */
#define POLARSSL_CIPHER_C

/**
 * \def POLARSSL_CTR_DRBG_C
 *
 * Enable the CTR_DRBG AES-256-based random generator
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
 *      TLS_RSA_WITH_3DES_EDE_CBC_SHA
 *      TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA
 *
 * PEM uses DES/3DES for decrypting encrypted keys.
 */
#define POLARSSL_DES_C

/**
 * \def POLARSSL_DHM_C
 *
 * Enable the Diffie-Hellman-Merkle key exchange.
 *
 * Module:  library/dhm.c
 * Caller:  library/ssl_cli.c
 *          library/ssl_srv.c
 *
 * This module enables the following ciphersuites (if other requisites are
 * enabled as well):
 *      TLS_DHE_RSA_WITH_DES_CBC_SHA
 *      TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA
 *      TLS_DHE_RSA_WITH_AES_128_CBC_SHA
 *      TLS_DHE_RSA_WITH_AES_256_CBC_SHA
 *      TLS_DHE_RSA_WITH_AES_128_CBC_SHA256
 *      TLS_DHE_RSA_WITH_AES_256_CBC_SHA256
 *      TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA
 *      TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA
 *      TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256
 *      TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256
 *      TLS_DHE_RSA_WITH_AES_128_GCM_SHA256
 *      TLS_DHE_RSA_WITH_AES_256_GCM_SHA384
 */
#define POLARSSL_DHM_C

/**
 * \def POLARSSL_ENTROPY_C
 *
 * Enable the platform-specific entropy code.
 *
 * Module:  library/entropy.c
 * Caller:
 *
 * Requires: POLARSSL_SHA4_C
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
 * Enable the Galois/Counter Mode (GCM) for AES
 *
 * Module:  library/gcm.c
 *
 * Requires: POLARSSL_AES_C
 *
 * This module enables the following ciphersuites (if other requisites are
 * enabled as well):
 *      TLS_RSA_WITH_AES_128_GCM_SHA256
 *      TLS_RSA_WITH_AES_256_GCM_SHA384
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
#define POLARSSL_HAVEGE_C

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
 * Enable the MD2 hash algorithm
 *
 * Module:  library/md2.c
 * Caller:  library/x509parse.c
 *
 * Uncomment to enable support for (rare) MD2-signed X.509 certs.
 *
#define POLARSSL_MD2_C
 */

/**
 * \def POLARSSL_MD4_C
 *
 * Enable the MD4 hash algorithm
 *
 * Module:  library/md4.c
 * Caller:  library/x509parse.c
 *
 * Uncomment to enable support for (rare) MD4-signed X.509 certs.
 *
#define POLARSSL_MD4_C
 */

/**
 * \def POLARSSL_MD5_C
 *
 * Enable the MD5 hash algorithm
 *
 * Module:  library/md5.c
 * Caller:  library/pem.c
 *          library/ssl_tls.c
 *          library/x509parse.c
 *
 * This module is required for SSL/TLS and X.509.
 * PEM uses MD5 for decrypting encrypted keys.
 */
#define POLARSSL_MD5_C

/**
 * \def POLARSSL_NET_C
 *
 * Enable the TCP/IP networking routines.
 *
 * Module:  library/net.c
 * Caller:
 *
 * This module provides TCP/IP networking routines.
 */
#define POLARSSL_NET_C

/**
 * \def POLARSSL_PADLOCK_C
 *
 * Enable VIA Padlock support on x86.
 *
 * Module:  library/padlock.c
 * Caller:  library/aes.c
 *
 * This modules adds support for the VIA PadLock on x86.
 */
#define POLARSSL_PADLOCK_C

/**
 * \def POLARSSL_PBKDF2_C
 *
 * Enable PKCS#5 PBKDF2 key derivation function
 * DEPRECATED: Use POLARSSL_PKCS5_C instead
 *
 * Module:  library/pbkdf2.c
 *
 * Requires: POLARSSL_PKCS5_C
 *
 * This module adds support for the PKCS#5 PBKDF2 key derivation function.
#define POLARSSL_PBKDF2_C
 */

/**
 * \def POLARSSL_PEM_C
 *
 * Enable PEM decoding
 *
 * Module:  library/pem.c
 * Caller:  library/x509parse.c
 *
 * Requires: POLARSSL_BASE64_C
 *
 * This modules adds support for decoding PEM files.
 */
#define POLARSSL_PEM_C

/**
 * \def POLARSSL_PKCS5_C
 *
 * Enable PKCS#5 functions
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
 * Module:  library/ssl_srv.c
 * Caller:  library/ssl_cli.c
 *          library/ssl_srv.c
 *
 * Requires: POLARSSL_SSL_TLS_C
 *
 * This module enables SSL/TLS PKCS #11 smartcard support.
 * Requires the presence of the PKCS#11 helper library (libpkcs11-helper)
#define POLARSSL_PKCS11_C
 */

/**
 * \def POLARSSL_PKCS12_C
 *
 * Enable PKCS#12 PBE functions
 * Adds algorithms for parsing PKCS#8 encrypted private keys
 *
 * Module:  library/pkcs12.c
 * Caller:  library/x509parse.c
 *
 * Requires: POLARSSL_ASN1_PARSE_C, POLARSSL_CIPHER_C, POLARSSL_MD_C
 * Can use:  POLARSSL_ARC4_C
 *
 * This module enables PKCS#12 functions.
 */
#define POLARSSL_PKCS12_C

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
 * Requires: POLARSSL_BIGNUM_C
 *
 * This module is required for SSL/TLS and MD5-signed certificates.
 */
#define POLARSSL_RSA_C

/**
 * \def POLARSSL_SHA1_C
 *
 * Enable the SHA1 cryptographic hash algorithm.
 *
 * Module:  library/sha1.c
 * Caller:  library/ssl_cli.c
 *          library/ssl_srv.c
 *          library/ssl_tls.c
 *          library/x509parse.c
 *
 * This module is required for SSL/TLS and SHA1-signed certificates.
 */
#define POLARSSL_SHA1_C

/**
 * \def POLARSSL_SHA2_C
 *
 * Enable the SHA-224 and SHA-256 cryptographic hash algorithms.
 *
 * Module:  library/sha2.c
 * Caller:  library/md_wrap.c
 *          library/x509parse.c
 *
 * This module adds support for SHA-224 and SHA-256.
 * This module is required for the SSL/TLS 1.2 PRF function.
 */
#define POLARSSL_SHA2_C

/**
 * \def POLARSSL_SHA4_C
 *
 * Enable the SHA-384 and SHA-512 cryptographic hash algorithms.
 *
 * Module:  library/sha4.c
 * Caller:  library/md_wrap.c
 *          library/x509parse.c
 *
 * This module adds support for SHA-384 and SHA-512.
 */
#define POLARSSL_SHA4_C

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
//#define POLARSSL_SSL_SRV_C

/**
 * \def POLARSSL_SSL_TLS_C
 *
 * Enable the generic SSL/TLS code.
 *
 * Module:  library/ssl_tls.c
 * Caller:  library/ssl_cli.c
 *          library/ssl_srv.c
 *
 * Requires: POLARSSL_MD5_C, POLARSSL_SHA1_C, POLARSSL_X509_PARSE_C
 *
 * This module is required for SSL/TLS.
 */
#define POLARSSL_SSL_TLS_C

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
 * \def POLARSSL_X509_PARSE_C
 *
 * Enable X.509 certificate parsing.
 *
 * Module:  library/x509parse.c
 * Caller:  library/ssl_cli.c
 *          library/ssl_srv.c
 *          library/ssl_tls.c
 *
 * Requires: POLARSSL_ASN1_PARSE_C, POLARSSL_BIGNUM_C, POLARSSL_RSA_C
 *
 * This module is required for X.509 certificate parsing.
 */
#define POLARSSL_X509_PARSE_C

/**
 * \def POLARSSL_X509_WRITE_C
 *
 * Enable X.509 buffer writing.
 *
 * Module:  library/x509write.c
 *
 * Requires: POLARSSL_BIGNUM_C, POLARSSL_RSA_C
 *
 * This module is required for X.509 certificate request writing.
 */
#define POLARSSL_X509_WRITE_C

/**
 * \def POLARSSL_XTEA_C
 *
 * Enable the XTEA block cipher.
 *
 * Module:  library/xtea.c
 * Caller:
 */
#define POLARSSL_XTEA_C
/* \} name */

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
#define CTR_DRBG_ENTROPY_LEN               48 /**< Amount of entropy used per seed by default */
#define CTR_DRBG_RESEED_INTERVAL        10000 /**< Interval before reseed is performed by default */
#define CTR_DRBG_MAX_INPUT                256 /**< Maximum number of additional input bytes */
#define CTR_DRBG_MAX_REQUEST             1024 /**< Maximum number of requested bytes per call */
#define CTR_DRBG_MAX_SEED_INPUT           384 /**< Maximum size of (re)seed buffer */

// Entropy options
//
#define ENTROPY_MAX_SOURCES                20 /**< Maximum number of sources supported */
#define ENTROPY_MAX_GATHER                128 /**< Maximum amount requested from entropy sources */

// SSL Cache options
//
#define SSL_CACHE_DEFAULT_TIMEOUT       86400 /**< 1 day  */
#define SSL_CACHE_DEFAULT_MAX_ENTRIES      50 /**< Maximum entries in cache */

// SSL options
//
#define SSL_MAX_CONTENT_LEN             16384 /**< Size of the input / output buffer */

#endif /* POLARSSL_CONFIG_OPTIONS */

/* \} name */
#endif /* config.h */
