/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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


#ifndef QSSLSOCKET_P_H
#define QSSLSOCKET_P_H

#include "qsslsocket.h"

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
#include <private/qtcpsocket_p.h>
#include "qsslkey.h"
#include "qsslconfiguration_p.h"
#include "qocspresponse.h"
#ifndef QT_NO_OPENSSL
#include <private/qsslcontext_openssl_p.h>
#else
class QSslContext;
#endif

#include <QtCore/qstringlist.h>
#include <QtCore/qvector.h>
#include <private/qringbuffer_p.h>

#if defined(Q_OS_MAC)
#include <Security/SecCertificate.h>
#include <CoreFoundation/CFArray.h>
#elif defined(Q_OS_WIN)
#include <QtCore/qt_windows.h>
#ifndef Q_OS_WINRT
#include <wincrypt.h>
#endif // !Q_OS_WINRT
#ifndef HCRYPTPROV_LEGACY
#define HCRYPTPROV_LEGACY HCRYPTPROV
#endif // !HCRYPTPROV_LEGACY
#endif // Q_OS_WIN

QT_BEGIN_NAMESPACE

#if defined(Q_OS_MACX)
    typedef CFDataRef (*PtrSecCertificateCopyData)(SecCertificateRef);
    typedef OSStatus (*PtrSecTrustSettingsCopyCertificates)(int, CFArrayRef*);
    typedef OSStatus (*PtrSecTrustCopyAnchorCertificates)(CFArrayRef*);
#endif

class QSslSocketPrivate : public QTcpSocketPrivate
{
    Q_DECLARE_PUBLIC(QSslSocket)
public:
    QSslSocketPrivate();
    virtual ~QSslSocketPrivate();

    void init();
    bool verifyProtocolSupported(const char *where);
    bool initialized;

    QSslSocket::SslMode mode;
    bool autoStartHandshake;
    bool connectionEncrypted;
    bool shutdown;
    bool ignoreAllSslErrors;
    QList<QSslError> ignoreErrorsList;
    bool* readyReadEmittedPointer;

    QSslConfigurationPrivate configuration;
    QList<QSslError> sslErrors;
    QSharedPointer<QSslContext> sslContextPointer;

    // if set, this hostname is used for certificate validation instead of the hostname
    // that was used for connecting to.
    QString verificationPeerName;

    bool allowRootCertOnDemandLoading;

    static bool s_loadRootCertsOnDemand;

    static bool supportsSsl();
    static long sslLibraryVersionNumber();
    static QString sslLibraryVersionString();
    static long sslLibraryBuildVersionNumber();
    static QString sslLibraryBuildVersionString();
    static void ensureInitialized();
    static void deinitialize();
    static QList<QSslCipher> defaultCiphers();
    static QList<QSslCipher> supportedCiphers();
    static void setDefaultCiphers(const QList<QSslCipher> &ciphers);
    static void setDefaultSupportedCiphers(const QList<QSslCipher> &ciphers);
    static void resetDefaultCiphers();

    static QVector<QSslEllipticCurve> supportedEllipticCurves();
    static void setDefaultSupportedEllipticCurves(const QVector<QSslEllipticCurve> &curves);
    static void resetDefaultEllipticCurves();

    static QList<QSslCertificate> defaultCaCertificates();
    static QList<QSslCertificate> systemCaCertificates();
    static void setDefaultCaCertificates(const QList<QSslCertificate> &certs);
    static bool addDefaultCaCertificates(const QString &path, QSsl::EncodingFormat format,
                                         QRegExp::PatternSyntax syntax);
    static void addDefaultCaCertificate(const QSslCertificate &cert);
    static void addDefaultCaCertificates(const QList<QSslCertificate> &certs);
    Q_AUTOTEST_EXPORT static bool isMatchingHostname(const QSslCertificate &cert,
                                                     const QString &peerName);
    Q_AUTOTEST_EXPORT static bool isMatchingHostname(const QString &cn, const QString &hostname);

    // The socket itself, including private slots.
    QTcpSocket *plainSocket;
    void createPlainSocket(QIODevice::OpenMode openMode);
    static void pauseSocketNotifiers(QSslSocket*);
    static void resumeSocketNotifiers(QSslSocket*);
    // ### The 2 methods below should be made member methods once the QSslContext class is made public
    static void checkSettingSslContext(QSslSocket*, QSharedPointer<QSslContext>);
    static QSharedPointer<QSslContext> sslContext(QSslSocket *socket);
    bool isPaused() const;
    bool bind(const QHostAddress &address, quint16, QAbstractSocket::BindMode) override;
    void _q_connectedSlot();
    void _q_hostFoundSlot();
    void _q_disconnectedSlot();
    void _q_stateChangedSlot(QAbstractSocket::SocketState);
    void _q_errorSlot(QAbstractSocket::SocketError);
    void _q_readyReadSlot();
    void _q_channelReadyReadSlot(int);
    void _q_bytesWrittenSlot(qint64);
    void _q_channelBytesWrittenSlot(int, qint64);
    void _q_readChannelFinishedSlot();
    void _q_flushWriteBuffer();
    void _q_flushReadBuffer();
    void _q_resumeImplementation();
#if defined(Q_OS_WIN) && !defined(Q_OS_WINRT) && !QT_CONFIG(schannel)
    virtual void _q_caRootLoaded(QSslCertificate,QSslCertificate) = 0;
#endif

    static QList<QByteArray> unixRootCertDirectories(); // used also by QSslContext

    virtual qint64 peek(char *data, qint64 maxSize) override;
    virtual QByteArray peek(qint64 maxSize) override;
    qint64 skip(qint64 maxSize) override;
    bool flush() override;

    // Platform specific functions
    virtual void startClientEncryption() = 0;
    virtual void startServerEncryption() = 0;
    virtual void transmit() = 0;
    virtual void disconnectFromHost() = 0;
    virtual void disconnected() = 0;
    virtual QSslCipher sessionCipher() const = 0;
    virtual QSsl::SslProtocol sessionProtocol() const = 0;
    virtual void continueHandshake() = 0;

    Q_AUTOTEST_EXPORT static bool rootCertOnDemandLoadingSupported();

private:
    static bool ensureLibraryLoaded();
    static void ensureCiphersAndCertsLoaded();
#if defined(Q_OS_ANDROID) && !defined(Q_OS_ANDROID_EMBEDDED)
    static QList<QByteArray> fetchSslCertificateData();
#endif

    static bool s_libraryLoaded;
    static bool s_loadedCiphersAndCerts;
protected:
    bool verifyErrorsHaveBeenIgnored();
    bool paused;
    bool flushTriggered;
    QVector<QOcspResponse> ocspResponses;
};

#if QT_CONFIG(securetransport) || QT_CONFIG(schannel)
// Implemented in qsslsocket_qt.cpp
QByteArray _q_makePkcs12(const QList<QSslCertificate> &certs, const QSslKey &key, const QString &passPhrase);
#endif

QT_END_NAMESPACE

#endif
