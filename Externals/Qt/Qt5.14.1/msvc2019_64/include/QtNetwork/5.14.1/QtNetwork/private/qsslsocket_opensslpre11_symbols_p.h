/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Copyright (C) 2014 BlackBerry Limited. All rights reserved.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNetwork module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

/****************************************************************************
**
** In addition, as a special exception, the copyright holders listed above give
** permission to link the code of its release of Qt with the OpenSSL project's
** "OpenSSL" library (or modified versions of the "OpenSSL" library that use the
** same license as the original version), and distribute the linked executables.
**
** You must comply with the GNU General Public License version 2 in all
** respects for all of the code used other than the "OpenSSL" code.  If you
** modify this file, you may extend this exception to your version of the file,
** but you are not obligated to do so.  If you do not wish to do so, delete
** this exception statement from your version of this file.
**
****************************************************************************/


#ifndef QSSLSOCKET_OPENSSLPRE11_SYMBOLS_P_H
#define QSSLSOCKET_OPENSSLPRE11_SYMBOLS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

// Note: this file does not have QT_BEGIN_NAMESPACE/QT_END_NAMESPACE, it's done
// in qsslsocket_openssl_symbols_p.h.

#ifndef QSSLSOCKET_OPENSSL_SYMBOLS_P_H
#error "You are not supposed to use this header file, include qsslsocket_openssl_symbols_p.h instead"
#endif

unsigned char * q_ASN1_STRING_data(ASN1_STRING *a);
BIO *q_BIO_new_file(const char *filename, const char *mode);
void q_ERR_clear_error();
Q_AUTOTEST_EXPORT BIO *q_BIO_new(BIO_METHOD *a);
Q_AUTOTEST_EXPORT BIO_METHOD *q_BIO_s_mem();
int q_CRYPTO_num_locks();
void q_CRYPTO_set_locking_callback(void (*a)(int, int, const char *, int));
void q_CRYPTO_set_id_callback(unsigned long (*a)());
void q_CRYPTO_free(void *a);
int q_CRYPTO_set_ex_data(CRYPTO_EX_DATA *ad, int idx, void *val);
void *q_CRYPTO_get_ex_data(const CRYPTO_EX_DATA *ad, int idx);
unsigned long q_ERR_peek_last_error();
void q_ERR_free_strings();
void q_EVP_CIPHER_CTX_cleanup(EVP_CIPHER_CTX *a);
void q_EVP_CIPHER_CTX_init(EVP_CIPHER_CTX *a);

typedef _STACK STACK;

// The typedef we use to make our pre 1.1 code look more like 1.1 (less ifdefs).
typedef STACK OPENSSL_STACK;

// We resolve q_sk_ functions, but use q_OPENSSL_sk_ macros in code to reduce
// the amount of #ifdefs.
int q_sk_num(STACK *a);
#define q_OPENSSL_sk_num(a) q_sk_num(a)
void q_sk_pop_free(STACK *a, void (*b)(void *));
#define q_OPENSSL_sk_pop_free(a, b) q_sk_pop_free(a, b)
STACK *q_sk_new_null();
#define q_OPENSSL_sk_new_null() q_sk_new_null()

void q_sk_free(STACK *a);

// Just a name alias (not a function call expression) since in code we take an
// address of this:
#define q_OPENSSL_sk_free q_sk_free

void *q_sk_value(STACK *a, int b);
void q_sk_push(STACK *st, void *data);

#define q_OPENSSL_sk_value(a, b) q_sk_value(a, b)
#define q_OPENSSL_sk_push(st, data) q_sk_push(st, data)

SSL_CTX *q_SSL_CTX_new(const SSL_METHOD *a);

int q_SSL_library_init();
void q_SSL_load_error_strings();

#if OPENSSL_VERSION_NUMBER >= 0x10001000L
int q_SSL_get_ex_new_index(long argl, void *argp, CRYPTO_EX_new *new_func, CRYPTO_EX_dup *dup_func, CRYPTO_EX_free *free_func);
#endif

const SSL_METHOD *q_SSLv23_client_method();
const SSL_METHOD *q_TLSv1_client_method();
const SSL_METHOD *q_TLSv1_1_client_method();
const SSL_METHOD *q_TLSv1_2_client_method();
const SSL_METHOD *q_SSLv23_server_method();
const SSL_METHOD *q_TLSv1_server_method();
const SSL_METHOD *q_TLSv1_1_server_method();
const SSL_METHOD *q_TLSv1_2_server_method();

STACK_OF(X509) *q_X509_STORE_CTX_get_chain(X509_STORE_CTX *ctx);

#ifdef SSLEAY_MACROS
int     q_i2d_DSAPrivateKey(const DSA *a, unsigned char **pp);
int     q_i2d_RSAPrivateKey(const RSA *a, unsigned char **pp);
RSA *q_d2i_RSAPrivateKey(RSA **a, unsigned char **pp, long length);
DSA *q_d2i_DSAPrivateKey(DSA **a, unsigned char **pp, long length);
#define q_PEM_read_bio_RSAPrivateKey(bp, x, cb, u) \
        (RSA *)q_PEM_ASN1_read_bio( \
        (void *(*)(void**, const unsigned char**, long int))q_d2i_RSAPrivateKey, PEM_STRING_RSA, bp, (void **)x, cb, u)
#define q_PEM_read_bio_DSAPrivateKey(bp, x, cb, u) \
        (DSA *)q_PEM_ASN1_read_bio( \
        (void *(*)(void**, const unsigned char**, long int))q_d2i_DSAPrivateKey, PEM_STRING_DSA, bp, (void **)x, cb, u)
#define q_PEM_write_bio_RSAPrivateKey(bp,x,enc,kstr,klen,cb,u) \
        PEM_ASN1_write_bio((int (*)(void*, unsigned char**))q_i2d_RSAPrivateKey,PEM_STRING_RSA,\
                           bp,(char *)x,enc,kstr,klen,cb,u)
#define q_PEM_write_bio_DSAPrivateKey(bp,x,enc,kstr,klen,cb,u) \
        PEM_ASN1_write_bio((int (*)(void*, unsigned char**))q_i2d_DSAPrivateKey,PEM_STRING_DSA,\
                           bp,(char *)x,enc,kstr,klen,cb,u)
#define q_PEM_read_bio_DHparams(bp, dh, cb, u) \
        (DH *)q_PEM_ASN1_read_bio( \
        (void *(*)(void**, const unsigned char**, long int))q_d2i_DHparams, PEM_STRING_DHPARAMS, bp, (void **)x, cb, u)
#endif // SSLEAY_MACROS

#define q_SSL_CTX_set_options(ctx,op) q_SSL_CTX_ctrl((ctx),SSL_CTRL_OPTIONS,(op),NULL)
#define q_SSL_set_options(ssl,op) q_SSL_ctrl((ssl),SSL_CTRL_OPTIONS,(op),nullptr)
#define q_SKM_sk_num(type, st) ((int (*)(const STACK_OF(type) *))q_sk_num)(st)
#define q_SKM_sk_value(type, st,i) ((type * (*)(const STACK_OF(type) *, int))q_sk_value)(st, i)
#define q_X509_getm_notAfter(x) X509_get_notAfter(x)
#define q_X509_getm_notBefore(x) X509_get_notBefore(x)

// "Forward compatibility" with OpenSSL 1.1 (to save on #if-ery elsewhere):
#define q_X509_get_version(x509) q_ASN1_INTEGER_get((x509)->cert_info->version)
#define q_ASN1_STRING_get0_data(x) q_ASN1_STRING_data(x)
#define q_EVP_PKEY_base_id(pkey) ((pkey)->type)
#define q_X509_get_pubkey(x509) q_X509_PUBKEY_get((x509)->cert_info->key)
#define q_SSL_SESSION_get_ticket_lifetime_hint(s) ((s)->tlsext_tick_lifetime_hint)
#define q_RSA_bits(rsa) q_BN_num_bits((rsa)->n)
#define q_DSA_bits(dsa) q_BN_num_bits((dsa)->p)
#define q_DH_bits(dsa) q_BN_num_bits((dh)->p)
#define q_X509_STORE_set_verify_cb(s,c) X509_STORE_set_verify_cb_func((s),(c))

char *q_CONF_get1_default_config_file();
void q_OPENSSL_add_all_algorithms_noconf();
void q_OPENSSL_add_all_algorithms_conf();

long q_SSLeay();
const char *q_SSLeay_version(int type);

#if QT_CONFIG(dtls)
// DTLS:
extern "C"
{
typedef int (*CookieVerifyCallback)(SSL *, unsigned char *, unsigned);
}

#define q_DTLSv1_listen(ssl, peer) q_SSL_ctrl(ssl, DTLS_CTRL_LISTEN, 0, (void *)peer)

const SSL_METHOD *q_DTLSv1_server_method();
const SSL_METHOD *q_DTLSv1_client_method();
const SSL_METHOD *q_DTLSv1_2_server_method();
const SSL_METHOD *q_DTLSv1_2_client_method();
#endif // dtls

#endif // QSSLSOCKET_OPENSSL_PRE11_SYMBOLS_P_H
