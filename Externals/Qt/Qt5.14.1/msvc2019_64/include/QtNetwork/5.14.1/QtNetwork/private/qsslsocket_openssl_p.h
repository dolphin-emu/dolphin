/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#ifndef QSSLSOCKET_OPENSSL_P_H
#define QSSLSOCKET_OPENSSL_P_H

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

#include <QtNetwork/private/qtnetworkglobal_p.h>
#include "qsslsocket_p.h"

#include <QtCore/qvector.h>
#include <QtCore/qstring.h>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#if defined(OCSP_RESPONSE)
#undef OCSP_RESPONSE
#endif
#if defined(X509_NAME)
#undef X509_NAME
#endif
#endif // Q_OS_WIN

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>
#include <openssl/pkcs7.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/stack.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/x509_vfy.h>
#include <openssl/dsa.h>
#include <openssl/rsa.h>
#include <openssl/crypto.h>
#include <openssl/tls1.h>

#if QT_CONFIG(opensslv11)
#include <openssl/dh.h>
#endif

QT_BEGIN_NAMESPACE

struct QSslErrorEntry {
    int code;
    int depth;

    static QSslErrorEntry fromStoreContext(X509_STORE_CTX *ctx);
};
Q_DECLARE_TYPEINFO(QSslErrorEntry, Q_PRIMITIVE_TYPE);

class QSslSocketBackendPrivate : public QSslSocketPrivate
{
    Q_DECLARE_PUBLIC(QSslSocket)
public:
    QSslSocketBackendPrivate();
    virtual ~QSslSocketBackendPrivate();

    // SSL context
    bool initSslContext();
    void destroySslContext();
    SSL *ssl;
    BIO *readBio;
    BIO *writeBio;
    SSL_SESSION *session;
    QVector<QSslErrorEntry> errorList;
#if OPENSSL_VERSION_NUMBER >= 0x10001000L
    static int s_indexForSSLExtraData; // index used in SSL_get_ex_data to get the matching QSslSocketBackendPrivate
#endif

    bool inSetAndEmitError = false;

    // Platform specific functions
    void startClientEncryption() override;
    void startServerEncryption() override;
    void transmit() override;
    bool startHandshake();
    void disconnectFromHost() override;
    void disconnected() override;
    QSslCipher sessionCipher() const override;
    QSsl::SslProtocol sessionProtocol() const override;
    void continueHandshake() override;
    bool checkSslErrors();
    void storePeerCertificates();
    unsigned int tlsPskClientCallback(const char *hint, char *identity, unsigned int max_identity_len, unsigned char *psk, unsigned int max_psk_len);
    unsigned int tlsPskServerCallback(const char *identity, unsigned char *psk, unsigned int max_psk_len);
#ifdef Q_OS_WIN
    void fetchCaRootForCert(const QSslCertificate &cert);
    void _q_caRootLoaded(QSslCertificate,QSslCertificate) override;
#endif

#if QT_CONFIG(ocsp)
    bool checkOcspStatus();
#endif

    // This decription will go to setErrorAndEmit(SslHandshakeError, ocspErrorDescription)
    QString ocspErrorDescription;
    // These will go to sslErrors()
    QVector<QSslError> ocspErrors;
    QByteArray ocspResponseDer;

    Q_AUTOTEST_EXPORT static long setupOpenSslOptions(QSsl::SslProtocol protocol, QSsl::SslOptions sslOptions);
    static QSslCipher QSslCipher_from_SSL_CIPHER(const SSL_CIPHER *cipher);
    static QList<QSslCertificate> STACKOFX509_to_QSslCertificates(STACK_OF(X509) *x509);
    static QList<QSslError> verify(const QList<QSslCertificate> &certificateChain, const QString &hostName);
    static QString getErrorsFromOpenSsl();
    static bool importPkcs12(QIODevice *device,
                             QSslKey *key, QSslCertificate *cert,
                             QList<QSslCertificate> *caCertificates,
                             const QByteArray &passPhrase);

    static QString msgErrorsDuringHandshake();
};

QT_END_NAMESPACE

#endif
