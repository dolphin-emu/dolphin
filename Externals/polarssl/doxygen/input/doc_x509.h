/**
 * @file
 * X.509 module documentation file.
 */

/**
 * @addtogroup x509_module X.509 module
 * 
 * The X.509 module provides X.509 support which includes:
 * - X.509 certificate (CRT) reading (see \c x509parse_crt() and
 *   \c x509parse_crtfile()).
 * - X.509 certificate revocation list (CRL) reading (see \c x509parse_crl()
 *   and\c x509parse_crlfile()).
 * - X.509 (RSA) private key reading (see \c x509parse_key() and
 *   \c x509parse_keyfile()).
 * - X.509 certificate signature verification (see \c x509parse_verify())
 *
 * This module can be used to build a certificate authority (CA) chain and
 * verify its signature. It is also used to get a (RSA) private key for signing
 * and decryption.
 */
