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

#ifndef QABSTRACTSOCKET_P_H
#define QABSTRACTSOCKET_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of the QAbstractSocket class.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include <QtNetwork/private/qtnetworkglobal_p.h>
#include "QtNetwork/qabstractsocket.h"
#include "QtCore/qbytearray.h"
#include "QtCore/qlist.h"
#include "QtCore/qtimer.h"
#include "private/qiodevice_p.h"
#include "private/qabstractsocketengine_p.h"
#include "qnetworkproxy.h"

QT_BEGIN_NAMESPACE

class QHostInfo;

class QAbstractSocketPrivate : public QIODevicePrivate, public QAbstractSocketEngineReceiver
{
    Q_DECLARE_PUBLIC(QAbstractSocket)
public:
    QAbstractSocketPrivate();
    virtual ~QAbstractSocketPrivate();

    // from QIODevicePrivate
    qint64 skip(qint64 maxSize) override;

    // from QAbstractSocketEngineReceiver
    inline void readNotification() override { canReadNotification(); }
    inline void writeNotification() override { canWriteNotification(); }
    inline void exceptionNotification() override {}
    inline void closeNotification() override { canCloseNotification(); }
    void connectionNotification() override;
#ifndef QT_NO_NETWORKPROXY
    inline void proxyAuthenticationRequired(const QNetworkProxy &proxy, QAuthenticator *authenticator) override {
        Q_Q(QAbstractSocket);
        emit q->proxyAuthenticationRequired(proxy, authenticator);
    }
#endif

    virtual bool bind(const QHostAddress &address, quint16 port, QAbstractSocket::BindMode mode);

    virtual bool canReadNotification();
    bool canWriteNotification();
    void canCloseNotification();

    // slots
    void _q_connectToNextAddress();
    void _q_startConnecting(const QHostInfo &hostInfo);
    void _q_testConnection();
    void _q_abortConnectionAttempt();

    bool emittedReadyRead;
    bool emittedBytesWritten;

    bool abortCalled;
    bool pendingClose;

    QAbstractSocket::PauseModes pauseMode;

    QString hostName;
    quint16 port;
    QHostAddress host;
    QList<QHostAddress> addresses;

    quint16 localPort;
    quint16 peerPort;
    QHostAddress localAddress;
    QHostAddress peerAddress;
    QString peerName;

    QAbstractSocketEngine *socketEngine;
    qintptr cachedSocketDescriptor;

#ifndef QT_NO_NETWORKPROXY
    QNetworkProxy proxy;
    QNetworkProxy proxyInUse;
    QString protocolTag;
    void resolveProxy(const QString &hostName, quint16 port);
#else
    inline void resolveProxy(const QString &, quint16) { }
#endif
    inline void resolveProxy(quint16 port) { resolveProxy(QString(), port); }

    void resetSocketLayer();
    virtual bool flush();

    bool initSocketLayer(QAbstractSocket::NetworkLayerProtocol protocol);
    virtual void configureCreatedSocket();
    void startConnectingByName(const QString &host);
    void fetchConnectionParameters();
    bool readFromSocket();
    virtual bool writeToSocket();
    void emitReadyRead(int channel = 0);
    void emitBytesWritten(qint64 bytes, int channel = 0);

    void setError(QAbstractSocket::SocketError errorCode, const QString &errorString);
    void setErrorAndEmit(QAbstractSocket::SocketError errorCode, const QString &errorString);

    qint64 readBufferMaxSize;
    bool isBuffered;
    bool hasPendingData;

    QTimer *connectTimer;

    int hostLookupId;

    QAbstractSocket::SocketType socketType;
    QAbstractSocket::SocketState state;

    // Must be kept in sync with QIODevicePrivate::errorString.
    QAbstractSocket::SocketError socketError;

    QAbstractSocket::NetworkLayerProtocol preferredNetworkLayerProtocol;

    bool prePauseReadSocketNotifierState;
    bool prePauseWriteSocketNotifierState;
    bool prePauseExceptionSocketNotifierState;
    static void pauseSocketNotifiers(QAbstractSocket*);
    static void resumeSocketNotifiers(QAbstractSocket*);
    static QAbstractSocketEngine* getSocketEngine(QAbstractSocket*);
};

QT_END_NAMESPACE

#endif // QABSTRACTSOCKET_P_H
