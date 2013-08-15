/**
 * @file
 * Hashing module documentation file.
 */

/**
 * @addtogroup hashing_module Hashing module
 * 
 * The Hashing module provides one-way hashing functions. Such functions can be
 * used for creating a hash message authentication code (HMAC) when sending a 
 * message. Such a HMAC can be used in combination with a private key
 * for authentication, which is a message integrity control. 
 * The following hashing-algorithms are provided:
 * - MD2, MD4, MD5 128-bit one-way hash functions by Ron Rivest (see 
 *   \c md2_hmac(), \c md4_hmac() and \c md5_hmac()).
 * - SHA-1, SHA-256, SHA-384/512 160-bit or more one-way hash functions by 
 *   NIST and NSA (see\c sha1_hmac(), \c sha2_hmac() and \c sha4_hmac()).
 *
 * This module provides one-way hashing which can be used for authentication.
 */
