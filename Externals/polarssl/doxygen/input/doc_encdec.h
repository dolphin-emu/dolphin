/**
 * @file
 * Encryption/decryption module documentation file.
 */

/**
 * @addtogroup encdec_module Encryption/decryption module
 * 
 * The Encryption/decryption module provides encryption/decryption functions. 
 * One can differtiate between symmetric and asymetric algorithms; the 
 * symmetric ones are mostly used for message confidentiality and the asymmetric 
 * ones for key exchange and message integrity.
 * Some symmetric algorithms provide different block cipher modes, mainly 
 * Electronic Code Book (ECB) which is used for short (64-bit) messages and 
 * Cipher Block Chaining (CBC) which provides the structure needed for longer 
 * messages. In addition the Cipher Feedback Mode (CFB-128) stream cipher mode
 * is implemented for specific algorithms.
 *
 * Sometimes the same functions are used for encryption and decryption.
 * The following algorithms are provided:
 * - Symmetric:
 *   - AES (see \c aes_crypt_ecb(), \c aes_crypt_cbc() and \c aes_crypt_cfb128()).
 *   - ARCFOUR (see \c arc4_crypt()).
 *   - Camellia (see \c camellia_crypt_ecb(), \c camellia_crypt_cbc() and \c camellia_crypt_cfb128()).
 *   - DES/3DES (see \c des_crypt_ecb(), \c des_crypt_cbc(), \c des3_crypt_ecb() 
 *     and \c des3_crypt_cbc()).
 *   - XTEA (see \c xtea_crypt_ecb()).
 * - Asymmetric:
 *   - Diffie-Hellman-Merkle (see \c dhm_read_public(), \c dhm_make_public() 
 *     and \c dhm_calc_secret()).
 *   - RSA (see \c rsa_public() and \c rsa_private()).
 *
 * This module provides encryption/decryption which can be used to provide 
 * secrecy.
 * It also provides asymmetric key functions which can be used for 
 * confidentiality, integrity, authentication and non-repudiation. 
 */
