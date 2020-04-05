/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Copyright (C) 2014 BlackBerry Limited. All rights reserved.
** Copyright (C) 2016 Richard J. Moore <rich@kde.org>
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

#ifndef QSSLSOCKET_OPENSSL11_SYMBOLS_P_H
#define QSSLSOCKET_OPENSSL11_SYMBOLS_P_H

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

const unsigned char * q_ASN1_STRING_get0_data(const ASN1_STRING *x);

Q_AUTOTEST_EXPORT BIO *q_BIO_new(const BIO_METHOD *a);
Q_AUTOTEST_EXPORT const BIO_METHOD *q_BIO_s_mem();

int q_DSA_bits(DSA *a);
int q_EVP_CIPHER_CTX_reset(EVP_CIPHER_CTX *c);
Q_AUTOTEST_EXPORT int q_EVP_PKEY_up_ref(EVP_PKEY *a);
int q_EVP_PKEY_base_id(EVP_PKEY *a);
int q_RSA_bits(RSA *a);
Q_AUTOTEST_EXPORT int q_OPENSSL_sk_num(OPENSSL_STACK *a);
Q_AUTOTEST_EXPORT void q_OPENSSL_sk_pop_free(OPENSSL_STACK *a, void (*b)(void *));
Q_AUTOTEST_EXPORT OPENSSL_STACK *q_OPENSSL_sk_new_null();
Q_AUTOTEST_EXPORT void q_OPENSSL_sk_push(OPENSSL_STACK *st, void *data);
Q_AUTOTEST_EXPORT void q_OPENSSL_sk_free(OPENSSL_STACK *a);
Q_AUTOTEST_EXPORT void * q_OPENSSL_sk_value(OPENSSL_STACK *a, int b);
int q_SSL_session_reused(SSL *a);
unsigned long q_SSL_CTX_set_options(SSL_CTX *ctx, unsigned long op);
int q_OPENSSL_init_ssl(uint64_t opts, const OPENSSL_INIT_SETTINGS *settings);
size_t q_SSL_get_client_random(SSL *a, unsigned char *out, size_t outlen);
size_t q_SSL_SESSION_get_master_key(const SSL_SESSION *session, unsigned char *out, size_t outlen);
int q_CRYPTO_get_ex_new_index(int class_index, long argl, void *argp, CRYPTO_EX_new *new_func, CRYPTO_EX_dup *dup_func, CRYPTO_EX_free *free_func);
const SSL_METHOD *q_TLS_method();
const SSL_METHOD *q_TLS_client_method();
const SSL_METHOD *q_TLS_server_method();
ASN1_TIME *q_X509_getm_notBefore(X509 *a);
ASN1_TIME *q_X509_getm_notAfter(X509 *a);

Q_AUTOTEST_EXPORT void q_X509_up_ref(X509 *a);
long q_X509_get_version(X509 *a);
EVP_PKEY *q_X509_get_pubkey(X509 *a);
void q_X509_STORE_set_verify_cb(X509_STORE *ctx, X509_STORE_CTX_verify_cb verify_cb);
int q_X509_STORE_set_ex_data(X509_STORE *ctx, int idx, void *data);
void *q_X509_STORE_get_ex_data(X509_STORE *r, int idx);
STACK_OF(X509) *q_X509_STORE_CTX_get0_chain(X509_STORE_CTX *ctx);
void q_DH_get0_pqg(const DH *dh, const BIGNUM **p, const BIGNUM **q, const BIGNUM **g);
int q_DH_bits(DH *dh);

# define q_SSL_load_error_strings() q_OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS \
                                                       | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL)

#define q_SKM_sk_num(type, st) ((int (*)(const STACK_OF(type) *))q_OPENSSL_sk_num)(st)
#define q_SKM_sk_value(type, st,i) ((type * (*)(const STACK_OF(type) *, int))q_OPENSSL_sk_value)(st, i)

#define q_OPENSSL_add_all_algorithms_conf()  q_OPENSSL_init_crypto(OPENSSL_INIT_ADD_ALL_CIPHERS \
                                                                   | OPENSSL_INIT_ADD_ALL_DIGESTS \
                                                                   | OPENSSL_INIT_LOAD_CONFIG, NULL)
#define  q_OPENSSL_add_all_algorithms_noconf() q_OPENSSL_init_crypto(OPENSSL_INIT_ADD_ALL_CIPHERS \
                                                                    | OPENSSL_INIT_ADD_ALL_DIGESTS, NULL)

int q_OPENSSL_init_crypto(uint64_t opts, const OPENSSL_INIT_SETTINGS *settings);
void q_CRYPTO_free(void *str, const char *file, int line);

long q_OpenSSL_version_num();
const char *q_OpenSSL_version(int type);

unsigned long q_SSL_SESSION_get_ticket_lifetime_hint(const SSL_SESSION *session);
unsigned long q_SSL_set_options(SSL *s, unsigned long op);

#ifdef TLS1_3_VERSION
int q_SSL_CTX_set_ciphersuites(SSL_CTX *ctx, const char *str);
#endif

#if QT_CONFIG(dtls)
// Functions and types required for DTLS support:
extern "C"
{

typedef int (*CookieVerifyCallback)(SSL *, const unsigned char *, unsigned);
typedef int (*DgramWriteCallback) (BIO *, const char *, int);
typedef int (*DgramReadCallback) (BIO *, char *, int);
typedef int (*DgramPutsCallback) (BIO *, const char *);
typedef long (*DgramCtrlCallback) (BIO *, int, long, void *);
typedef int (*DgramCreateCallback) (BIO *);
typedef int (*DgramDestroyCallback) (BIO *);

}

int q_DTLSv1_listen(SSL *s, BIO_ADDR *client);
BIO_ADDR *q_BIO_ADDR_new();
void q_BIO_ADDR_free(BIO_ADDR *ap);

// API we need for a custom dgram BIO:

BIO_METHOD *q_BIO_meth_new(int type, const char *name);
void q_BIO_meth_free(BIO_METHOD *biom);
int q_BIO_meth_set_write(BIO_METHOD *biom, DgramWriteCallback);
int q_BIO_meth_set_read(BIO_METHOD *biom, DgramReadCallback);
int q_BIO_meth_set_puts(BIO_METHOD *biom, DgramPutsCallback);
int q_BIO_meth_set_ctrl(BIO_METHOD *biom, DgramCtrlCallback);
int q_BIO_meth_set_create(BIO_METHOD *biom, DgramCreateCallback);
int q_BIO_meth_set_destroy(BIO_METHOD *biom, DgramDestroyCallback);

#endif // dtls

void q_BIO_set_data(BIO *a, void *ptr);
void *q_BIO_get_data(BIO *a);
void q_BIO_set_init(BIO *a, int init);
int q_BIO_get_shutdown(BIO *a);
void q_BIO_set_shutdown(BIO *a, int shut);

#if QT_CONFIG(ocsp)
const OCSP_CERTID *q_OCSP_SINGLERESP_get0_id(const OCSP_SINGLERESP *x);
#endif // ocsp

#define q_SSL_CTX_set_min_proto_version(ctx, version) \
        q_SSL_CTX_ctrl(ctx, SSL_CTRL_SET_MIN_PROTO_VERSION, version, nullptr)

#define q_SSL_CTX_set_max_proto_version(ctx, version) \
        q_SSL_CTX_ctrl(ctx, SSL_CTRL_SET_MAX_PROTO_VERSION, version, nullptr)

extern "C" {
typedef int (*q_SSL_psk_use_session_cb_func_t)(SSL *, const EVP_MD *, const unsigned char **, size_t *,
                                               SSL_SESSION **);
}
void q_SSL_set_psk_use_session_callback(SSL *s, q_SSL_psk_use_session_cb_func_t);

#endif
