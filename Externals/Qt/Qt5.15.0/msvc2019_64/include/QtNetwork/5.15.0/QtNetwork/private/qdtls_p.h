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

#ifndef QDTLS_P_H
#define QDTLS_P_H

#include <private/qtnetworkglobal_p.h>

#include "qdtls.h"

#include <private/qsslconfiguration_p.h>
#include <private/qobject_p.h>

#include <QtNetwork/qabstractsocket.h>
#include <QtNetwork/qhostaddress.h>
#include <QtNetwork/qsslsocket.h>
#include <QtNetwork/qsslcipher.h>
#include <QtNetwork/qssl.h>

#include <QtCore/qcryptographichash.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qstring.h>

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

QT_REQUIRE_CONFIG(dtls);

QT_BEGIN_NAMESPACE

class QHostAddress;

class QDtlsBasePrivate : public QObjectPrivate
{
public:

    void setDtlsError(QDtlsError code, const QString &description)
    {
        errorCode = code;
        errorDescription = description;
    }

    void clearDtlsError()
    {
        errorCode = QDtlsError::NoError;
        errorDescription.clear();
    }

    void setConfiguration(const QSslConfiguration &configuration);
    QSslConfiguration configuration() const;

    bool setCookieGeneratorParameters(QCryptographicHash::Algorithm alg,
                                      const QByteArray &secret);

    static bool isDtlsProtocol(QSsl::SslProtocol protocol);

    QHostAddress remoteAddress;
    quint16 remotePort = 0;
    quint16 mtuHint = 0;

    QDtlsError errorCode = QDtlsError::NoError;
    QString errorDescription;
    QSslConfigurationPrivate dtlsConfiguration;
    QSslSocket::SslMode mode = QSslSocket::SslClientMode;
    QSslCipher sessionCipher;
    QSsl::SslProtocol sessionProtocol = QSsl::UnknownProtocol;
    QString peerVerificationName;
    QByteArray secret;

#ifdef QT_CRYPTOGRAPHICHASH_ONLY_SHA1
    QCryptographicHash::Algorithm hashAlgorithm = QCryptographicHash::Sha1;
#else
    QCryptographicHash::Algorithm hashAlgorithm = QCryptographicHash::Sha256;
#endif
};

class QDtlsClientVerifierPrivate : public QDtlsBasePrivate
{
public:

    QByteArray verifiedClientHello;

    virtual bool verifyClient(QUdpSocket *socket, const QByteArray &dgram,
                              const QHostAddress &address, quint16 port) = 0;
};

class QDtlsPrivate : public QDtlsBasePrivate
{
public:

    virtual bool startHandshake(QUdpSocket *socket, const QByteArray &dgram) = 0;
    virtual bool handleTimeout(QUdpSocket *socket) = 0;
    virtual bool continueHandshake(QUdpSocket *socket, const QByteArray &dgram) = 0;
    virtual bool resumeHandshake(QUdpSocket *socket) = 0;
    virtual void abortHandshake(QUdpSocket *socket) = 0;
    virtual void sendShutdownAlert(QUdpSocket *socket) = 0;

    virtual qint64 writeDatagramEncrypted(QUdpSocket *socket, const QByteArray &dgram) = 0;
    virtual QByteArray decryptDatagram(QUdpSocket *socket, const QByteArray &dgram) = 0;

    QDtls::HandshakeState handshakeState = QDtls::HandshakeNotStarted;

    QVector<QSslError> tlsErrors;
    QVector<QSslError> tlsErrorsToIgnore;

    bool connectionEncrypted = false;
};

QT_END_NAMESPACE

#endif // QDTLS_P_H
