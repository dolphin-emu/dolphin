/**
 * \file ssl.h
 *
 * \brief SSL/TLS functions.
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
#ifndef POLARSSL_SSL_H
#define POLARSSL_SSL_H

#include <time.h>

#include "net.h"
#include "rsa.h"
#include "md5.h"
#include "sha1.h"
#include "sha2.h"
#include "sha4.h"
#include "x509.h"
#include "config.h"

#if defined(POLARSSL_DHM_C)
#include "dhm.h"
#endif

#if defined(POLARSSL_ZLIB_SUPPORT)
#include "zlib.h"
#endif

#if defined(_MSC_VER) && !defined(inline)
#define inline _inline
#else
#if defined(__ARMCC_VERSION) && !defined(inline)
#define inline __inline
#endif /* __ARMCC_VERSION */
#endif /*_MSC_VER */

/*
 * SSL Error codes
 */
#define POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE               -0x7080  /**< The requested feature is not available. */
#define POLARSSL_ERR_SSL_BAD_INPUT_DATA                    -0x7100  /**< Bad input parameters to function. */
#define POLARSSL_ERR_SSL_INVALID_MAC                       -0x7180  /**< Verification of the message MAC failed. */
#define POLARSSL_ERR_SSL_INVALID_RECORD                    -0x7200  /**< An invalid SSL record was received. */
#define POLARSSL_ERR_SSL_CONN_EOF                          -0x7280  /**< The connection indicated an EOF. */
#define POLARSSL_ERR_SSL_UNKNOWN_CIPHER                    -0x7300  /**< An unknown cipher was received. */
#define POLARSSL_ERR_SSL_NO_CIPHER_CHOSEN                  -0x7380  /**< The server has no ciphersuites in common with the client. */
#define POLARSSL_ERR_SSL_NO_SESSION_FOUND                  -0x7400  /**< No session to recover was found. */
#define POLARSSL_ERR_SSL_NO_CLIENT_CERTIFICATE             -0x7480  /**< No client certification received from the client, but required by the authentication mode. */
#define POLARSSL_ERR_SSL_CERTIFICATE_TOO_LARGE             -0x7500  /**< Our own certificate(s) is/are too large to send in an SSL message.*/
#define POLARSSL_ERR_SSL_CERTIFICATE_REQUIRED              -0x7580  /**< The own certificate is not set, but needed by the server. */
#define POLARSSL_ERR_SSL_PRIVATE_KEY_REQUIRED              -0x7600  /**< The own private key is not set, but needed. */
#define POLARSSL_ERR_SSL_CA_CHAIN_REQUIRED                 -0x7680  /**< No CA Chain is set, but required to operate. */
#define POLARSSL_ERR_SSL_UNEXPECTED_MESSAGE                -0x7700  /**< An unexpected message was received from our peer. */
#define POLARSSL_ERR_SSL_FATAL_ALERT_MESSAGE               -0x7780  /**< A fatal alert message was received from our peer. */
#define POLARSSL_ERR_SSL_PEER_VERIFY_FAILED                -0x7800  /**< Verification of our peer failed. */
#define POLARSSL_ERR_SSL_PEER_CLOSE_NOTIFY                 -0x7880  /**< The peer notified us that the connection is going to be closed. */
#define POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO               -0x7900  /**< Processing of the ClientHello handshake message failed. */
#define POLARSSL_ERR_SSL_BAD_HS_SERVER_HELLO               -0x7980  /**< Processing of the ServerHello handshake message failed. */
#define POLARSSL_ERR_SSL_BAD_HS_CERTIFICATE                -0x7A00  /**< Processing of the Certificate handshake message failed. */
#define POLARSSL_ERR_SSL_BAD_HS_CERTIFICATE_REQUEST        -0x7A80  /**< Processing of the CertificateRequest handshake message failed. */
#define POLARSSL_ERR_SSL_BAD_HS_SERVER_KEY_EXCHANGE        -0x7B00  /**< Processing of the ServerKeyExchange handshake message failed. */
#define POLARSSL_ERR_SSL_BAD_HS_SERVER_HELLO_DONE          -0x7B80  /**< Processing of the ServerHelloDone handshake message failed. */
#define POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE        -0x7C00  /**< Processing of the ClientKeyExchange handshake message failed. */
#define POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE_DHM_RP -0x7C80  /**< Processing of the ClientKeyExchange handshake message failed in DHM Read Public. */
#define POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE_DHM_CS -0x7D00  /**< Processing of the ClientKeyExchange handshake message failed in DHM Calculate Secret. */
#define POLARSSL_ERR_SSL_BAD_HS_CERTIFICATE_VERIFY         -0x7D80  /**< Processing of the CertificateVerify handshake message failed. */
#define POLARSSL_ERR_SSL_BAD_HS_CHANGE_CIPHER_SPEC         -0x7E00  /**< Processing of the ChangeCipherSpec handshake message failed. */
#define POLARSSL_ERR_SSL_BAD_HS_FINISHED                   -0x7E80  /**< Processing of the Finished handshake message failed. */
#define POLARSSL_ERR_SSL_MALLOC_FAILED                     -0x7F00  /**< Memory allocation failed */
#define POLARSSL_ERR_SSL_HW_ACCEL_FAILED                   -0x7F80  /**< Hardware acceleration function returned with error */
#define POLARSSL_ERR_SSL_HW_ACCEL_FALLTHROUGH              -0x6F80  /**< Hardware acceleration function skipped / left alone data */
#define POLARSSL_ERR_SSL_COMPRESSION_FAILED                -0x6F00  /**< Processing of the compression / decompression failed */
#define POLARSSL_ERR_SSL_BAD_HS_PROTOCOL_VERSION           -0x6E80  /**< Handshake protocol not within min/max boundaries */

/*
 * Various constants
 */
#define SSL_MAJOR_VERSION_3             3
#define SSL_MINOR_VERSION_0             0   /*!< SSL v3.0 */
#define SSL_MINOR_VERSION_1             1   /*!< TLS v1.0 */
#define SSL_MINOR_VERSION_2             2   /*!< TLS v1.1 */
#define SSL_MINOR_VERSION_3             3   /*!< TLS v1.2 */

#define SSL_IS_CLIENT                   0
#define SSL_IS_SERVER                   1
#define SSL_COMPRESS_NULL               0
#define SSL_COMPRESS_DEFLATE            1

#define SSL_VERIFY_NONE                 0
#define SSL_VERIFY_OPTIONAL             1
#define SSL_VERIFY_REQUIRED             2

#define SSL_INITIAL_HANDSHAKE           0
#define SSL_RENEGOTIATION               1

#define SSL_LEGACY_RENEGOTIATION        0
#define SSL_SECURE_RENEGOTIATION        1

#define SSL_RENEGOTIATION_DISABLED      0
#define SSL_RENEGOTIATION_ENABLED       1

#define SSL_LEGACY_NO_RENEGOTIATION     0
#define SSL_LEGACY_ALLOW_RENEGOTIATION  1
#define SSL_LEGACY_BREAK_HANDSHAKE      2

/*
 * Size of the input / output buffer.
 * Note: the RFC defines the default size of SSL / TLS messages. If you
 * change the value here, other clients / servers may not be able to
 * communicate with you anymore. Only change this value if you control
 * both sides of the connection and have it reduced at both sides!
 */
#if !defined(POLARSSL_CONFIG_OPTIONS)
#define SSL_MAX_CONTENT_LEN         16384   /**< Size of the input / output buffer */
#endif /* !POLARSSL_CONFIG_OPTIONS */

/*
 * Allow an extra 512 bytes for the record header
 * and encryption overhead (counter + MAC + padding)
 * and allow for a maximum of 1024 of compression expansion if
 * enabled.
 */
#if defined(POLARSSL_ZLIB_SUPPORT)
#define SSL_COMPRESSION_ADD          1024
#else
#define SSL_COMPRESSION_ADD             0
#endif

#define SSL_BUFFER_LEN (SSL_MAX_CONTENT_LEN + SSL_COMPRESSION_ADD + 512)

/*
 * Supported ciphersuites (Official IANA names)
 */
#define TLS_RSA_WITH_NULL_MD5                    0x01   /**< Weak! */
#define TLS_RSA_WITH_NULL_SHA                    0x02   /**< Weak! */
#define TLS_RSA_WITH_NULL_SHA256                 0x3B   /**< Weak! */
#define TLS_RSA_WITH_DES_CBC_SHA                 0x09   /**< Weak! Not in TLS 1.2 */
#define TLS_DHE_RSA_WITH_DES_CBC_SHA             0x15   /**< Weak! Not in TLS 1.2 */

#define TLS_RSA_WITH_RC4_128_MD5                 0x04
#define TLS_RSA_WITH_RC4_128_SHA                 0x05

#define TLS_RSA_WITH_3DES_EDE_CBC_SHA            0x0A
#define TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA        0x16

#define TLS_RSA_WITH_AES_128_CBC_SHA             0x2F
#define TLS_DHE_RSA_WITH_AES_128_CBC_SHA         0x33
#define TLS_RSA_WITH_AES_256_CBC_SHA             0x35
#define TLS_DHE_RSA_WITH_AES_256_CBC_SHA         0x39
#define TLS_RSA_WITH_AES_128_CBC_SHA256          0x3C   /**< TLS 1.2 */
#define TLS_RSA_WITH_AES_256_CBC_SHA256          0x3D   /**< TLS 1.2 */
#define TLS_DHE_RSA_WITH_AES_128_CBC_SHA256      0x67   /**< TLS 1.2 */
#define TLS_DHE_RSA_WITH_AES_256_CBC_SHA256      0x6B   /**< TLS 1.2 */

#define TLS_RSA_WITH_CAMELLIA_128_CBC_SHA        0x41
#define TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA    0x45
#define TLS_RSA_WITH_CAMELLIA_256_CBC_SHA        0x84
#define TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA    0x88
#define TLS_RSA_WITH_CAMELLIA_128_CBC_SHA256     0xBA   /**< TLS 1.2 */
#define TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256 0xBE   /**< TLS 1.2 */
#define TLS_RSA_WITH_CAMELLIA_256_CBC_SHA256     0xC0   /**< TLS 1.2 */
#define TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256 0xC4   /**< TLS 1.2 */

#define TLS_RSA_WITH_AES_128_GCM_SHA256          0x9C
#define TLS_RSA_WITH_AES_256_GCM_SHA384          0x9D
#define TLS_DHE_RSA_WITH_AES_128_GCM_SHA256      0x9E
#define TLS_DHE_RSA_WITH_AES_256_GCM_SHA384      0x9F

#define SSL_EMPTY_RENEGOTIATION_INFO    0xFF   /**< renegotiation info ext */ 

/*
 * Supported Signature and Hash algorithms (For TLS 1.2)
 */
#define SSL_HASH_NONE                0
#define SSL_HASH_MD5                 1
#define SSL_HASH_SHA1                2
#define SSL_HASH_SHA224              3
#define SSL_HASH_SHA256              4
#define SSL_HASH_SHA384              5
#define SSL_HASH_SHA512              6

#define SSL_SIG_RSA                  1

/*
 * Client Certificate Types
 */
#define SSL_CERT_TYPE_RSA_SIGN       1

/*
 * Message, alert and handshake types
 */
#define SSL_MSG_CHANGE_CIPHER_SPEC     20
#define SSL_MSG_ALERT                  21
#define SSL_MSG_HANDSHAKE              22
#define SSL_MSG_APPLICATION_DATA       23

#define SSL_ALERT_LEVEL_WARNING         1
#define SSL_ALERT_LEVEL_FATAL           2

#define SSL_ALERT_MSG_CLOSE_NOTIFY           0  /* 0x00 */
#define SSL_ALERT_MSG_UNEXPECTED_MESSAGE    10  /* 0x0A */
#define SSL_ALERT_MSG_BAD_RECORD_MAC        20  /* 0x14 */
#define SSL_ALERT_MSG_DECRYPTION_FAILED     21  /* 0x15 */
#define SSL_ALERT_MSG_RECORD_OVERFLOW       22  /* 0x16 */
#define SSL_ALERT_MSG_DECOMPRESSION_FAILURE 30  /* 0x1E */
#define SSL_ALERT_MSG_HANDSHAKE_FAILURE     40  /* 0x28 */
#define SSL_ALERT_MSG_NO_CERT               41  /* 0x29 */
#define SSL_ALERT_MSG_BAD_CERT              42  /* 0x2A */
#define SSL_ALERT_MSG_UNSUPPORTED_CERT      43  /* 0x2B */
#define SSL_ALERT_MSG_CERT_REVOKED          44  /* 0x2C */
#define SSL_ALERT_MSG_CERT_EXPIRED          45  /* 0x2D */
#define SSL_ALERT_MSG_CERT_UNKNOWN          46  /* 0x2E */
#define SSL_ALERT_MSG_ILLEGAL_PARAMETER     47  /* 0x2F */
#define SSL_ALERT_MSG_UNKNOWN_CA            48  /* 0x30 */
#define SSL_ALERT_MSG_ACCESS_DENIED         49  /* 0x31 */
#define SSL_ALERT_MSG_DECODE_ERROR          50  /* 0x32 */
#define SSL_ALERT_MSG_DECRYPT_ERROR         51  /* 0x33 */
#define SSL_ALERT_MSG_EXPORT_RESTRICTION    60  /* 0x3C */
#define SSL_ALERT_MSG_PROTOCOL_VERSION      70  /* 0x46 */
#define SSL_ALERT_MSG_INSUFFICIENT_SECURITY 71  /* 0x47 */
#define SSL_ALERT_MSG_INTERNAL_ERROR        80  /* 0x50 */
#define SSL_ALERT_MSG_USER_CANCELED         90  /* 0x5A */
#define SSL_ALERT_MSG_NO_RENEGOTIATION     100  /* 0x64 */
#define SSL_ALERT_MSG_UNSUPPORTED_EXT      110  /* 0x6E */
#define SSL_ALERT_MSG_UNRECOGNIZED_NAME    112  /* 0x70 */

#define SSL_HS_HELLO_REQUEST            0
#define SSL_HS_CLIENT_HELLO             1
#define SSL_HS_SERVER_HELLO             2
#define SSL_HS_CERTIFICATE             11
#define SSL_HS_SERVER_KEY_EXCHANGE     12
#define SSL_HS_CERTIFICATE_REQUEST     13
#define SSL_HS_SERVER_HELLO_DONE       14
#define SSL_HS_CERTIFICATE_VERIFY      15
#define SSL_HS_CLIENT_KEY_EXCHANGE     16
#define SSL_HS_FINISHED                20

/*
 * TLS extensions
 */
#define TLS_EXT_SERVERNAME              0
#define TLS_EXT_SERVERNAME_HOSTNAME     0

#define TLS_EXT_SIG_ALG                13

#define TLS_EXT_RENEGOTIATION_INFO 0xFF01


/*
 * Generic function pointers for allowing external RSA private key
 * implementations.
 */
typedef int (*rsa_decrypt_func)( void *ctx, int mode, size_t *olen,
                        const unsigned char *input, unsigned char *output,
                        size_t output_max_len ); 
typedef int (*rsa_sign_func)( void *ctx,
                     int (*f_rng)(void *, unsigned char *, size_t), void *p_rng,
                     int mode, int hash_id, unsigned int hashlen,
                     const unsigned char *hash, unsigned char *sig );
typedef size_t (*rsa_key_len_func)( void *ctx );

/*
 * SSL state machine
 */
typedef enum
{
    SSL_HELLO_REQUEST,
    SSL_CLIENT_HELLO,
    SSL_SERVER_HELLO,
    SSL_SERVER_CERTIFICATE,
    SSL_SERVER_KEY_EXCHANGE,
    SSL_CERTIFICATE_REQUEST,
    SSL_SERVER_HELLO_DONE,
    SSL_CLIENT_CERTIFICATE,
    SSL_CLIENT_KEY_EXCHANGE,
    SSL_CERTIFICATE_VERIFY,
    SSL_CLIENT_CHANGE_CIPHER_SPEC,
    SSL_CLIENT_FINISHED,
    SSL_SERVER_CHANGE_CIPHER_SPEC,
    SSL_SERVER_FINISHED,
    SSL_FLUSH_BUFFERS,
    SSL_HANDSHAKE_WRAPUP,
    SSL_HANDSHAKE_OVER
}
ssl_states;

typedef struct _ssl_session ssl_session;
typedef struct _ssl_context ssl_context;
typedef struct _ssl_transform ssl_transform;
typedef struct _ssl_handshake_params ssl_handshake_params;

/*
 * This structure is used for storing current session data.
 */
struct _ssl_session
{
    time_t start;               /*!< starting time      */
    int ciphersuite;            /*!< chosen ciphersuite */
    int compression;            /*!< chosen compression */
    size_t length;              /*!< session id length  */
    unsigned char id[32];       /*!< session identifier */
    unsigned char master[48];   /*!< the master secret  */
    x509_cert *peer_cert;       /*!< peer X.509 cert chain */
};

/*
 * This structure contains a full set of runtime transform parameters
 * either in negotiation or active.
 */
struct _ssl_transform
{
    /*
     * Session specific crypto layer
     */
    unsigned int keylen;                /*!<  symmetric key length    */
    size_t minlen;                      /*!<  min. ciphertext length  */
    size_t ivlen;                       /*!<  IV length               */
    size_t fixed_ivlen;                 /*!<  Fixed part of IV (AEAD) */
    size_t maclen;                      /*!<  MAC length              */

    unsigned char iv_enc[16];           /*!<  IV (encryption)         */
    unsigned char iv_dec[16];           /*!<  IV (decryption)         */

    unsigned char mac_enc[32];          /*!<  MAC (encryption)        */
    unsigned char mac_dec[32];          /*!<  MAC (decryption)        */

    uint32_t ctx_enc[136];              /*!<  encryption context      */
    uint32_t ctx_dec[136];              /*!<  decryption context      */

    /*
     * Session specific compression layer
     */
#if defined(POLARSSL_ZLIB_SUPPORT)
    z_stream ctx_deflate;               /*!<  compression context     */
    z_stream ctx_inflate;               /*!<  decompression context   */
#endif
};

/*
 * This structure contains the parameters only needed during handshake.
 */
struct _ssl_handshake_params
{
    /*
     * Handshake specific crypto variables
     */
    int sig_alg;                        /*!<  Signature algorithm     */
    int cert_type;                      /*!<  Requested cert type     */
    int verify_sig_alg;                 /*!<  Signature algorithm for verify */
#if defined(POLARSSL_DHM_C)
    dhm_context dhm_ctx;                /*!<  DHM key exchange        */
#endif

    /*
     * Checksum contexts
     */
     md5_context fin_md5;
    sha1_context fin_sha1;
    sha2_context fin_sha2;
    sha4_context fin_sha4;

    void (*update_checksum)(ssl_context *, unsigned char *, size_t);
    void (*calc_verify)(ssl_context *, unsigned char *);
    void (*calc_finished)(ssl_context *, unsigned char *, int);
    int  (*tls_prf)(unsigned char *, size_t, char *,
                    unsigned char *, size_t,
                    unsigned char *, size_t);

    size_t pmslen;                      /*!<  premaster length        */

    unsigned char randbytes[64];        /*!<  random bytes            */
    unsigned char premaster[POLARSSL_MPI_MAX_SIZE];
                                        /*!<  premaster secret        */

    int resume;                         /*!<  session resume indicator*/
};

struct _ssl_context
{
    /*
     * Miscellaneous
     */
    int state;                  /*!< SSL handshake: current state     */
    int renegotiation;          /*!< Initial or renegotiation         */

    int major_ver;              /*!< equal to  SSL_MAJOR_VERSION_3    */
    int minor_ver;              /*!< either 0 (SSL3) or 1 (TLS1.0)    */

    int max_major_ver;          /*!< max. major version from client   */
    int max_minor_ver;          /*!< max. minor version from client   */
    int min_major_ver;          /*!< min. major version accepted      */
    int min_minor_ver;          /*!< min. minor version accepted      */

    /*
     * Callbacks (RNG, debug, I/O, verification)
     */
    int  (*f_rng)(void *, unsigned char *, size_t);
    void (*f_dbg)(void *, int, const char *);
    int (*f_recv)(void *, unsigned char *, size_t);
    int (*f_send)(void *, const unsigned char *, size_t);
    int (*f_vrfy)(void *, x509_cert *, int, int *);
    int (*f_get_cache)(void *, ssl_session *);
    int (*f_set_cache)(void *, const ssl_session *);
    int (*f_sni)(void *, ssl_context *, const unsigned char *, size_t);

    void *p_rng;                /*!< context for the RNG function     */
    void *p_dbg;                /*!< context for the debug function   */
    void *p_recv;               /*!< context for reading operations   */
    void *p_send;               /*!< context for writing operations   */
    void *p_vrfy;               /*!< context for verification         */
    void *p_get_cache;          /*!< context for cache retrieval      */
    void *p_set_cache;          /*!< context for cache store          */
    void *p_sni;                /*!< context for SNI extension        */
    void *p_hw_data;            /*!< context for HW acceleration      */

    /*
     * Session layer
     */
    ssl_session *session_in;            /*!<  current session data (in)   */
    ssl_session *session_out;           /*!<  current session data (out)  */
    ssl_session *session;               /*!<  negotiated session data     */
    ssl_session *session_negotiate;     /*!<  session data in negotiation */

    ssl_handshake_params *handshake;    /*!<  params required only during
                                              the handshake process        */

    /*
     * Record layer transformations
     */
    ssl_transform *transform_in;        /*!<  current transform params (in)   */
    ssl_transform *transform_out;       /*!<  current transform params (in)   */
    ssl_transform *transform;           /*!<  negotiated transform params     */
    ssl_transform *transform_negotiate; /*!<  transform params in negotiation */

    /*
     * Record layer (incoming data)
     */
    unsigned char *in_ctr;      /*!< 64-bit incoming message counter  */
    unsigned char *in_hdr;      /*!< 5-byte record header (in_ctr+8)  */
    unsigned char *in_msg;      /*!< the message contents (in_hdr+5)  */
    unsigned char *in_offt;     /*!< read offset in application data  */

    int in_msgtype;             /*!< record header: message type      */
    size_t in_msglen;           /*!< record header: message length    */
    size_t in_left;             /*!< amount of data read so far       */

    size_t in_hslen;            /*!< current handshake message length */
    int nb_zero;                /*!< # of 0-length encrypted messages */

    /*
     * Record layer (outgoing data)
     */
    unsigned char *out_ctr;     /*!< 64-bit outgoing message counter  */
    unsigned char *out_hdr;     /*!< 5-byte record header (out_ctr+8) */
    unsigned char *out_msg;     /*!< the message contents (out_hdr+32)*/

    int out_msgtype;            /*!< record header: message type      */
    size_t out_msglen;          /*!< record header: message length    */
    size_t out_left;            /*!< amount of data not yet written   */

    /*
     * PKI layer
     */
    void *rsa_key;                      /*!<  own RSA private key     */
    rsa_decrypt_func rsa_decrypt;       /*!<  function for RSA decrypt*/
    rsa_sign_func rsa_sign;             /*!<  function for RSA sign   */
    rsa_key_len_func rsa_key_len;       /*!<  function for RSA key len*/

    x509_cert *own_cert;                /*!<  own X.509 certificate   */
    x509_cert *ca_chain;                /*!<  own trusted CA chain    */
    x509_crl *ca_crl;                   /*!<  trusted CA CRLs         */
    const char *peer_cn;                /*!<  expected peer CN        */

    /*
     * User settings
     */
    int endpoint;                       /*!<  0: client, 1: server    */
    int authmode;                       /*!<  verification mode       */
    int client_auth;                    /*!<  flag for client auth.   */
    int verify_result;                  /*!<  verification result     */
    int disable_renegotiation;          /*!<  enable/disable renegotiation   */
    int allow_legacy_renegotiation;     /*!<  allow legacy renegotiation     */
    const int **ciphersuites;           /*!<  allowed ciphersuites / version */

#if defined(POLARSSL_DHM_C)
    mpi dhm_P;                          /*!<  prime modulus for DHM   */
    mpi dhm_G;                          /*!<  generator for DHM       */
#endif

    /*
     * TLS extensions
     */
    unsigned char *hostname;
    size_t         hostname_len;

    /*
     * Secure renegotiation
     */
    int secure_renegotiation;           /*!<  does peer support legacy or
                                              secure renegotiation           */
    size_t verify_data_len;             /*!<  length of verify data stored   */
    char own_verify_data[36];           /*!<  previous handshake verify data */
    char peer_verify_data[36];          /*!<  previous handshake verify data */
};

#ifdef __cplusplus
extern "C" {
#endif

extern const int ssl_default_ciphersuites[];

#if defined(POLARSSL_SSL_HW_RECORD_ACCEL)
extern int (*ssl_hw_record_init)(ssl_context *ssl,
                const unsigned char *key_enc, const unsigned char *key_dec,
                const unsigned char *iv_enc,  const unsigned char *iv_dec,
                const unsigned char *mac_enc, const unsigned char *mac_dec);
extern int (*ssl_hw_record_reset)(ssl_context *ssl);
extern int (*ssl_hw_record_write)(ssl_context *ssl);
extern int (*ssl_hw_record_read)(ssl_context *ssl);
extern int (*ssl_hw_record_finish)(ssl_context *ssl);
#endif

/**
 * \brief Returns the list of ciphersuites supported by the SSL/TLS module.
 *
 * \return              a statically allocated array of ciphersuites, the last
 *                      entry is 0.
 */
static inline const int *ssl_list_ciphersuites( void )
{
    return ssl_default_ciphersuites;
}

/**
 * \brief               Return the name of the ciphersuite associated with the given
 *                      ID
 *
 * \param ciphersuite_id SSL ciphersuite ID
 *
 * \return              a string containing the ciphersuite name
 */
const char *ssl_get_ciphersuite_name( const int ciphersuite_id );

/**
 * \brief               Return the ID of the ciphersuite associated with the given
 *                      name
 *
 * \param ciphersuite_name SSL ciphersuite name
 *
 * \return              the ID with the ciphersuite or 0 if not found
 */
int ssl_get_ciphersuite_id( const char *ciphersuite_name );

/**
 * \brief          Initialize an SSL context
 *
 * \param ssl      SSL context
 *
 * \return         0 if successful, or POLARSSL_ERR_SSL_MALLOC_FAILED if
 *                 memory allocation failed
 */
int ssl_init( ssl_context *ssl );

/**
 * \brief          Reset an already initialized SSL context for re-use
 *                 while retaining application-set variables, function
 *                 pointers and data.
 *
 * \param ssl      SSL context
 * \return         0 if successful, or POLASSL_ERR_SSL_MALLOC_FAILED,
                   POLARSSL_ERR_SSL_HW_ACCEL_FAILED or
 *                 POLARSSL_ERR_SSL_COMPRESSION_FAILED
 */
int ssl_session_reset( ssl_context *ssl );

/**
 * \brief          Set the current endpoint type
 *
 * \param ssl      SSL context
 * \param endpoint must be SSL_IS_CLIENT or SSL_IS_SERVER
 */
void ssl_set_endpoint( ssl_context *ssl, int endpoint );

/**
 * \brief          Set the certificate verification mode
 *
 * \param ssl      SSL context
 * \param authmode can be:
 *
 *  SSL_VERIFY_NONE:      peer certificate is not checked (default),
 *                        this is insecure and SHOULD be avoided.
 *
 *  SSL_VERIFY_OPTIONAL:  peer certificate is checked, however the
 *                        handshake continues even if verification failed;
 *                        ssl_get_verify_result() can be called after the
 *                        handshake is complete.
 *
 *  SSL_VERIFY_REQUIRED:  peer *must* present a valid certificate,
 *                        handshake is aborted if verification failed.
 */
void ssl_set_authmode( ssl_context *ssl, int authmode );

/**
 * \brief          Set the verification callback (Optional).
 *
 *                 If set, the verify callback is called for each
 *                 certificate in the chain. For implementation
 *                 information, please see \c x509parse_verify()
 *
 * \param ssl      SSL context
 * \param f_vrfy   verification function
 * \param p_vrfy   verification parameter
 */
void ssl_set_verify( ssl_context *ssl,
                     int (*f_vrfy)(void *, x509_cert *, int, int *),
                     void *p_vrfy );

/**
 * \brief          Set the random number generator callback
 *
 * \param ssl      SSL context
 * \param f_rng    RNG function
 * \param p_rng    RNG parameter
 */
void ssl_set_rng( ssl_context *ssl,
                  int (*f_rng)(void *, unsigned char *, size_t),
                  void *p_rng );

/**
 * \brief          Set the debug callback
 *
 * \param ssl      SSL context
 * \param f_dbg    debug function
 * \param p_dbg    debug parameter
 */
void ssl_set_dbg( ssl_context *ssl,
                  void (*f_dbg)(void *, int, const char *),
                  void  *p_dbg );

/**
 * \brief          Set the underlying BIO read and write callbacks
 *
 * \param ssl      SSL context
 * \param f_recv   read callback
 * \param p_recv   read parameter
 * \param f_send   write callback
 * \param p_send   write parameter
 */
void ssl_set_bio( ssl_context *ssl,
        int (*f_recv)(void *, unsigned char *, size_t), void *p_recv,
        int (*f_send)(void *, const unsigned char *, size_t), void *p_send );

/**
 * \brief          Set the session cache callbacks (server-side only)
 *                 If not set, no session resuming is done.
 *
 *                 The session cache has the responsibility to check for stale
 *                 entries based on timeout. See RFC 5246 for recommendations.
 *
 *                 Warning: session.peer_cert is cleared by the SSL/TLS layer on
 *                 connection shutdown, so do not cache the pointer! Either set
 *                 it to NULL or make a full copy of the certificate.
 *
 *                 The get callback is called once during the initial handshake
 *                 to enable session resuming. The get function has the
 *                 following parameters: (void *parameter, ssl_session *session)
 *                 If a valid entry is found, it should fill the master of
 *                 the session object with the cached values and return 0,
 *                 return 1 otherwise. Optionally peer_cert can be set as well
 *                 if it is properly present in cache entry.
 *
 *                 The set callback is called once during the initial handshake
 *                 to enable session resuming after the entire handshake has
 *                 been finished. The set function has the following parameters:
 *                 (void *parameter, const ssl_session *session). The function
 *                 should create a cache entry for future retrieval based on
 *                 the data in the session structure and should keep in mind
 *                 that the ssl_session object presented (and all its referenced
 *                 data) is cleared by the SSL/TLS layer when the connection is
 *                 terminated. It is recommended to add metadata to determine if
 *                 an entry is still valid in the future. Return 0 if
 *                 successfully cached, return 1 otherwise.
 *
 * \param ssl            SSL context
 * \param f_get_cache    session get callback
 * \param p_get_cache    session get parameter
 * \param f_set_cache    session set callback
 * \param p_set_cache    session set parameter
 */
void ssl_set_session_cache( ssl_context *ssl,
        int (*f_get_cache)(void *, ssl_session *), void *p_get_cache,
        int (*f_set_cache)(void *, const ssl_session *), void *p_set_cache );

/**
 * \brief          Request resumption of session (client-side only)
 *                 Session data is copied from presented session structure.
 *
 *                 Warning: session.peer_cert is cleared by the SSL/TLS layer on
 *                 connection shutdown, so do not cache the pointer! Either set
 *                 it to NULL or make a full copy of the certificate when
 *                 storing the session for use in this function.
 *
 * \param ssl      SSL context
 * \param session  session context
 */
void ssl_set_session( ssl_context *ssl, const ssl_session *session );

/**
 * \brief               Set the list of allowed ciphersuites
 *                      (Default: ssl_default_ciphersuites)
 *                      (Overrides all version specific lists)
 *
 * \param ssl           SSL context
 * \param ciphersuites  0-terminated list of allowed ciphersuites
 */
void ssl_set_ciphersuites( ssl_context *ssl, const int *ciphersuites );

/**
 * \brief               Set the list of allowed ciphersuites for a specific
 *                      version of the protocol.
 *                      (Default: ssl_default_ciphersuites)
 *                      (Only useful on the server side)
 *
 * \param ssl           SSL context
 * \param ciphersuites  0-terminated list of allowed ciphersuites
 * \param major         Major version number (only SSL_MAJOR_VERSION_3
 *                      supported)
 * \param minor         Minor version number (SSL_MINOR_VERSION_0,
 *                      SSL_MINOR_VERSION_1 and SSL_MINOR_VERSION_2,
 *                      SSL_MINOR_VERSION_3 supported)
 */
void ssl_set_ciphersuites_for_version( ssl_context *ssl,
                                       const int *ciphersuites,
                                       int major, int minor );

/**
 * \brief          Set the data required to verify peer certificate
 *
 * \param ssl      SSL context
 * \param ca_chain trusted CA chain (meaning all fully trusted top-level CAs)
 * \param ca_crl   trusted CA CRLs
 * \param peer_cn  expected peer CommonName (or NULL)
 */
void ssl_set_ca_chain( ssl_context *ssl, x509_cert *ca_chain,
                       x509_crl *ca_crl, const char *peer_cn );

/**
 * \brief          Set own certificate chain and private key
 *
 *                 Note: own_cert should contain IN order from the bottom
 *                 up your certificate chain. The top certificate (self-signed)
 *                 can be omitted.
 *
 * \param ssl      SSL context
 * \param own_cert own public certificate chain
 * \param rsa_key  own private RSA key
 */
void ssl_set_own_cert( ssl_context *ssl, x509_cert *own_cert,
                       rsa_context *rsa_key );

/**
 * \brief          Set own certificate and alternate non-PolarSSL private
 *                 key and handling callbacks, such as the PKCS#11 wrappers
 *                 or any other external private key handler.
 *                 (see the respective RSA functions in rsa.h for documentation
 *                 of the callback parameters, with the only change being
 *                 that the rsa_context * is a void * in the callbacks)
 *
 *                 Note: own_cert should contain IN order from the bottom
 *                 up your certificate chain. The top certificate (self-signed)
 *                 can be omitted.
 *
 * \param ssl      SSL context
 * \param own_cert own public certificate chain
 * \param rsa_key  alternate implementation private RSA key
 * \param rsa_decrypt_func  alternate implementation of \c rsa_pkcs1_decrypt()
 * \param rsa_sign_func     alternate implementation of \c rsa_pkcs1_sign()
 * \param rsa_key_len_func  function returning length of RSA key in bytes
 */
void ssl_set_own_cert_alt( ssl_context *ssl, x509_cert *own_cert,
                           void *rsa_key,
                           rsa_decrypt_func rsa_decrypt,
                           rsa_sign_func rsa_sign,
                           rsa_key_len_func rsa_key_len );

#if defined(POLARSSL_DHM_C)
/**
 * \brief          Set the Diffie-Hellman public P and G values,
 *                 read as hexadecimal strings (server-side only)
 *                 (Default: POLARSSL_DHM_RFC5114_MODP_1024_[PG])
 *
 * \param ssl      SSL context
 * \param dhm_P    Diffie-Hellman-Merkle modulus
 * \param dhm_G    Diffie-Hellman-Merkle generator
 *
 * \return         0 if successful
 */
int ssl_set_dh_param( ssl_context *ssl, const char *dhm_P, const char *dhm_G );

/**
 * \brief          Set the Diffie-Hellman public P and G values,
 *                 read from existing context (server-side only)
 *
 * \param ssl      SSL context
 * \param dhm_ctx  Diffie-Hellman-Merkle context
 *
 * \return         0 if successful
 */
int ssl_set_dh_param_ctx( ssl_context *ssl, dhm_context *dhm_ctx );
#endif

/**
 * \brief          Set hostname for ServerName TLS extension
 *                 (client-side only)
 *                 
 *
 * \param ssl      SSL context
 * \param hostname the server hostname
 *
 * \return         0 if successful or POLARSSL_ERR_SSL_MALLOC_FAILED
 */
int ssl_set_hostname( ssl_context *ssl, const char *hostname );

/**
 * \brief          Set server side ServerName TLS extension callback
 *                 (optional, server-side only).
 *
 *                 If set, the ServerName callback is called whenever the
 *                 server receives a ServerName TLS extension from the client
 *                 during a handshake. The ServerName callback has the
 *                 following parameters: (void *parameter, ssl_context *ssl,
 *                 const unsigned char *hostname, size_t len). If a suitable
 *                 certificate is found, the callback should set the
 *                 certificate and key to use with ssl_set_own_cert() (and
 *                 possibly adjust the CA chain as well) and return 0. The
 *                 callback should return -1 to abort the handshake at this
 *                 point.
 *
 * \param ssl      SSL context
 * \param f_sni    verification function
 * \param p_sni    verification parameter
 */
void ssl_set_sni( ssl_context *ssl,
                  int (*f_sni)(void *, ssl_context *, const unsigned char *,
                               size_t),
                  void *p_sni );

/**
 * \brief          Set the maximum supported version sent from the client side
 * 
 * \param ssl      SSL context
 * \param major    Major version number (only SSL_MAJOR_VERSION_3 supported)
 * \param minor    Minor version number (SSL_MINOR_VERSION_0,
 *                 SSL_MINOR_VERSION_1 and SSL_MINOR_VERSION_2,
 *                 SSL_MINOR_VERSION_3 supported)
 */
void ssl_set_max_version( ssl_context *ssl, int major, int minor );


/**
 * \brief          Set the minimum accepted SSL/TLS protocol version
 *                 (Default: SSL_MAJOR_VERSION_3, SSL_MINOR_VERSION_0)
 *
 * \param ssl      SSL context
 * \param major    Major version number (only SSL_MAJOR_VERSION_3 supported)
 * \param minor    Minor version number (SSL_MINOR_VERSION_0,
 *                 SSL_MINOR_VERSION_1 and SSL_MINOR_VERSION_2,
 *                 SSL_MINOR_VERSION_3 supported)
 */
void ssl_set_min_version( ssl_context *ssl, int major, int minor );

/**
 * \brief          Enable / Disable renegotiation support for connection when
 *                 initiated by peer
 *                 (Default: SSL_RENEGOTIATION_DISABLED)
 *
 *                 Note: A server with support enabled is more vulnerable for a
 *                 resource DoS by a malicious client. You should enable this on
 *                 a client to enable server-initiated renegotiation.
 *
 * \param ssl      SSL context
 * \param renegotiation     Enable or disable (SSL_RENEGOTIATION_ENABLED or
 *                                             SSL_RENEGOTIATION_DISABLED)
 */
void ssl_set_renegotiation( ssl_context *ssl, int renegotiation );

/**
 * \brief          Prevent or allow legacy renegotiation.
 *                 (Default: SSL_LEGACY_NO_RENEGOTIATION)
 *                 
 *                 SSL_LEGACY_NO_RENEGOTIATION allows connections to
 *                 be established even if the peer does not support
 *                 secure renegotiation, but does not allow renegotiation
 *                 to take place if not secure.
 *                 (Interoperable and secure option)
 *
 *                 SSL_LEGACY_ALLOW_RENEGOTIATION allows renegotiations
 *                 with non-upgraded peers. Allowing legacy renegotiation
 *                 makes the connection vulnerable to specific man in the
 *                 middle attacks. (See RFC 5746)
 *                 (Most interoperable and least secure option)
 *
 *                 SSL_LEGACY_BREAK_HANDSHAKE breaks off connections
 *                 if peer does not support secure renegotiation. Results
 *                 in interoperability issues with non-upgraded peers
 *                 that do not support renegotiation altogether.
 *                 (Most secure option, interoperability issues)
 *
 * \param ssl      SSL context
 * \param allow_legacy  Prevent or allow (SSL_NO_LEGACY_RENEGOTIATION,
 *                                        SSL_ALLOW_LEGACY_RENEGOTIATION or
 *                                        SSL_LEGACY_BREAK_HANDSHAKE)
 */
void ssl_legacy_renegotiation( ssl_context *ssl, int allow_legacy );

/**
 * \brief          Return the number of data bytes available to read
 *
 * \param ssl      SSL context
 *
 * \return         how many bytes are available in the read buffer
 */
size_t ssl_get_bytes_avail( const ssl_context *ssl );

/**
 * \brief          Return the result of the certificate verification
 *
 * \param ssl      SSL context
 *
 * \return         0 if successful, or a combination of:
 *                      BADCERT_EXPIRED
 *                      BADCERT_REVOKED
 *                      BADCERT_CN_MISMATCH
 *                      BADCERT_NOT_TRUSTED
 */
int ssl_get_verify_result( const ssl_context *ssl );

/**
 * \brief          Return the name of the current ciphersuite
 *
 * \param ssl      SSL context
 *
 * \return         a string containing the ciphersuite name
 */
const char *ssl_get_ciphersuite( const ssl_context *ssl );

/**
 * \brief          Return the current SSL version (SSLv3/TLSv1/etc)
 *
 * \param ssl      SSL context
 *
 * \return         a string containing the SSL version
 */
const char *ssl_get_version( const ssl_context *ssl );

/**
 * \brief          Return the peer certificate from the current connection
 *
 *                 Note: Can be NULL in case no certificate was sent during
 *                 the handshake. Different calls for the same connection can
 *                 return the same or different pointers for the same
 *                 certificate and even a different certificate altogether.
 *                 The peer cert CAN change in a single connection if
 *                 renegotiation is performed.
 *
 * \param ssl      SSL context
 *
 * \return         the current peer certificate
 */
const x509_cert *ssl_get_peer_cert( const ssl_context *ssl );

/**
 * \brief          Perform the SSL handshake
 *
 * \param ssl      SSL context
 *
 * \return         0 if successful, POLARSSL_ERR_NET_WANT_READ,
 *                 POLARSSL_ERR_NET_WANT_WRITE, or a specific SSL error code.
 */
int ssl_handshake( ssl_context *ssl );

/**
 * \brief          Perform a single step of the SSL handshake
 *
 *                 Note: the state of the context (ssl->state) will be at
 *                 the following state after execution of this function.
 *                 Do not call this function if state is SSL_HANDSHAKE_OVER.
 *
 * \param ssl      SSL context
 *
 * \return         0 if successful, POLARSSL_ERR_NET_WANT_READ,
 *                 POLARSSL_ERR_NET_WANT_WRITE, or a specific SSL error code.
 */
int ssl_handshake_step( ssl_context *ssl );

/**
 * \brief          Perform an SSL renegotiation on the running connection
 *
 * \param ssl      SSL context
 *
 * \return         0 if succesful, or any ssl_handshake() return value.
 */
int ssl_renegotiate( ssl_context *ssl );

/**
 * \brief          Read at most 'len' application data bytes
 *
 * \param ssl      SSL context
 * \param buf      buffer that will hold the data
 * \param len      how many bytes must be read
 *
 * \return         This function returns the number of bytes read, 0 for EOF,
 *                 or a negative error code.
 */
int ssl_read( ssl_context *ssl, unsigned char *buf, size_t len );

/**
 * \brief          Write exactly 'len' application data bytes
 *
 * \param ssl      SSL context
 * \param buf      buffer holding the data
 * \param len      how many bytes must be written
 *
 * \return         This function returns the number of bytes written,
 *                 or a negative error code.
 *
 * \note           When this function returns POLARSSL_ERR_NET_WANT_WRITE,
 *                 it must be called later with the *same* arguments,
 *                 until it returns a positive value.
 */
int ssl_write( ssl_context *ssl, const unsigned char *buf, size_t len );

/**
 * \brief           Send an alert message
 *
 * \param ssl       SSL context
 * \param level     The alert level of the message
 *                  (SSL_ALERT_LEVEL_WARNING or SSL_ALERT_LEVEL_FATAL)
 * \param message   The alert message (SSL_ALERT_MSG_*)
 *
 * \return          0 if successful, or a specific SSL error code.
 */
int ssl_send_alert_message( ssl_context *ssl,
                            unsigned char level,
                            unsigned char message );
/**
 * \brief          Notify the peer that the connection is being closed
 *
 * \param ssl      SSL context
 */
int ssl_close_notify( ssl_context *ssl );

/**
 * \brief          Free referenced items in an SSL context and clear memory
 *
 * \param ssl      SSL context
 */
void ssl_free( ssl_context *ssl );

/**
 * \brief          Free referenced items in an SSL session including the
 *                 peer certificate and clear memory
 *
 * \param session  SSL session
 */
void ssl_session_free( ssl_session *session );

/**
 * \brief           Free referenced items in an SSL transform context and clear
 *                  memory
 *
 * \param transform SSL transform context
 */
void ssl_transform_free( ssl_transform *transform );

/**
 * \brief           Free referenced items in an SSL handshake context and clear
 *                  memory
 *
 * \param handshake SSL handshake context
 */
void ssl_handshake_free( ssl_handshake_params *handshake );

/*
 * Internal functions (do not call directly)
 */
int ssl_handshake_client_step( ssl_context *ssl );
int ssl_handshake_server_step( ssl_context *ssl );
void ssl_handshake_wrapup( ssl_context *ssl );

int ssl_send_fatal_handshake_failure( ssl_context *ssl );

int ssl_derive_keys( ssl_context *ssl );

int ssl_read_record( ssl_context *ssl );
/**
 * \return         0 if successful, POLARSSL_ERR_SSL_CONN_EOF on EOF or
 *                 another negative error code.
 */
int ssl_fetch_input( ssl_context *ssl, size_t nb_want );

int ssl_write_record( ssl_context *ssl );
int ssl_flush_output( ssl_context *ssl );

int ssl_parse_certificate( ssl_context *ssl );
int ssl_write_certificate( ssl_context *ssl );

int ssl_parse_change_cipher_spec( ssl_context *ssl );
int ssl_write_change_cipher_spec( ssl_context *ssl );

int ssl_parse_finished( ssl_context *ssl );
int ssl_write_finished( ssl_context *ssl );

void ssl_optimize_checksum( ssl_context *ssl, int ciphersuite );

#ifdef __cplusplus
}
#endif

#endif /* ssl.h */
