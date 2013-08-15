/**
 * @file
 * SSL/TLS communication module documentation file.
 */

/**
 * @addtogroup ssltls_communication_module SSL/TLS communication module
 * 
 * The SSL/TLS communication module provides the means to create an SSL/TLS
 * communication channel. 
 * The basic provisions are:
 * - initialise an SSL/TLS context (see \c ssl_init()).
 * - perform an SSL/TLS handshake (see \c ssl_handshake()).
 * - read/write (see \c ssl_read() and \c ssl_write()).
 * - notify a peer that conection is being closed (see \c ssl_close_notify()).
 *
 *
 * Many aspects of such a channel are set through parameters and callback
 * functions:
 * - the endpoint role: client or server.
 * - the authentication mode. Should verification take place.
 * - the Host-to-host communication channel. A TCP/IP module is provided.
 * - the random number generator (RNG).
 * - the ciphers to use for encryption/decryption.
 * - session control functions.
 * - X.509 parameters for certificate-handling and key exchange.
 * 
 *
 * This module can be used to create an SSL/TLS server and client and to provide a basic
 * framework to setup and communicate through an SSL/TLS communication channel.\n
 * Note that you need to provide for several aspects yourself as mentioned above.
 */
