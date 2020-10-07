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

#ifndef QSOCKS5SOCKETENGINE_P_H
#define QSOCKS5SOCKETENGINE_P_H

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

#include <QtNetwork/private/qtnetworkglobal_p.h>
#include "qabstractsocketengine_p.h"
#include "qnetworkproxy.h"

QT_REQUIRE_CONFIG(socks5);

QT_BEGIN_NAMESPACE

class QSocks5SocketEnginePrivate;

class Q_AUTOTEST_EXPORT QSocks5SocketEngine : public QAbstractSocketEngine
{
    Q_OBJECT
public:
    QSocks5SocketEngine(QObject *parent = nullptr);
    ~QSocks5SocketEngine();

    bool initialize(QAbstractSocket::SocketType type, QAbstractSocket::NetworkLayerProtocol protocol = QAbstractSocket::IPv4Protocol) override;
    bool initialize(qintptr socketDescriptor, QAbstractSocket::SocketState socketState = QAbstractSocket::ConnectedState) override;

    void setProxy(const QNetworkProxy &networkProxy);

    qintptr socketDescriptor() const override;

    bool isValid() const override;

    bool connectInternal();
    bool connectToHost(const QHostAddress &address, quint16 port) override;
    bool connectToHostByName(const QString &name, quint16 port) override;
    bool bind(const QHostAddress &address, quint16 port) override;
    bool listen() override;
    int accept() override;
    void close() override;

    qint64 bytesAvailable() const override;

    qint64 read(char *data, qint64 maxlen) override;
    qint64 write(const char *data, qint64 len) override;

#ifndef QT_NO_UDPSOCKET
#ifndef QT_NO_NETWORKINTERFACE
    bool joinMulticastGroup(const QHostAddress &groupAddress,
                            const QNetworkInterface &interface) override;
    bool leaveMulticastGroup(const QHostAddress &groupAddress,
                             const QNetworkInterface &interface) override;
    QNetworkInterface multicastInterface() const override;
    bool setMulticastInterface(const QNetworkInterface &iface) override;
#endif // QT_NO_NETWORKINTERFACE

    bool hasPendingDatagrams() const override;
    qint64 pendingDatagramSize() const override;
#endif // QT_NO_UDPSOCKET

    qint64 readDatagram(char *data, qint64 maxlen, QIpPacketHeader * = nullptr,
                        PacketHeaderOptions = WantNone) override;
    qint64 writeDatagram(const char *data, qint64 len, const QIpPacketHeader &) override;
    qint64 bytesToWrite() const override;

    int option(SocketOption option) const override;
    bool setOption(SocketOption option, int value) override;

    bool waitForRead(int msecs = 30000, bool *timedOut = nullptr) override;
    bool waitForWrite(int msecs = 30000, bool *timedOut = nullptr) override;
    bool waitForReadOrWrite(bool *readyToRead, bool *readyToWrite,
                            bool checkRead, bool checkWrite,
                            int msecs = 30000, bool *timedOut = nullptr) override;

    bool isReadNotificationEnabled() const override;
    void setReadNotificationEnabled(bool enable) override;
    bool isWriteNotificationEnabled() const override;
    void setWriteNotificationEnabled(bool enable) override;
    bool isExceptionNotificationEnabled() const override;
    void setExceptionNotificationEnabled(bool enable) override;

private:
    Q_DECLARE_PRIVATE(QSocks5SocketEngine)
    Q_DISABLE_COPY_MOVE(QSocks5SocketEngine)
    Q_PRIVATE_SLOT(d_func(), void _q_controlSocketConnected())
    Q_PRIVATE_SLOT(d_func(), void _q_controlSocketReadNotification())
    Q_PRIVATE_SLOT(d_func(), void _q_controlSocketErrorOccurred(QAbstractSocket::SocketError))
#ifndef QT_NO_UDPSOCKET
    Q_PRIVATE_SLOT(d_func(), void _q_udpSocketReadNotification())
#endif
    Q_PRIVATE_SLOT(d_func(), void _q_controlSocketBytesWritten())
    Q_PRIVATE_SLOT(d_func(), void _q_emitPendingReadNotification())
    Q_PRIVATE_SLOT(d_func(), void _q_emitPendingWriteNotification())
    Q_PRIVATE_SLOT(d_func(), void _q_emitPendingConnectionNotification())
    Q_PRIVATE_SLOT(d_func(), void _q_controlSocketDisconnected())
    Q_PRIVATE_SLOT(d_func(), void _q_controlSocketStateChanged(QAbstractSocket::SocketState))

};


class QTcpSocket;

class QSocks5Authenticator
{
public:
    QSocks5Authenticator();
    virtual ~QSocks5Authenticator();
    virtual char methodId();
    virtual bool beginAuthenticate(QTcpSocket *socket, bool *completed);
    virtual bool continueAuthenticate(QTcpSocket *socket, bool *completed);

    virtual bool seal(const QByteArray &buf, QByteArray *sealedBuf);
    virtual bool unSeal(const QByteArray &sealedBuf, QByteArray *buf);
    virtual bool unSeal(QTcpSocket *sealedSocket, QByteArray *buf);

    virtual QString errorString() { return QString(); }
};

class QSocks5PasswordAuthenticator : public QSocks5Authenticator
{
public:
    QSocks5PasswordAuthenticator(const QString &userName, const QString &password);
    char methodId() override;
    bool beginAuthenticate(QTcpSocket *socket, bool *completed) override;
    bool continueAuthenticate(QTcpSocket *socket, bool *completed) override;

    QString errorString() override;

private:
    QString userName;
    QString password;
};

struct QSocks5Data;
struct QSocks5ConnectData;
struct QSocks5UdpAssociateData;
struct QSocks5BindData;

class QSocks5SocketEnginePrivate : public QAbstractSocketEnginePrivate
{
    Q_DECLARE_PUBLIC(QSocks5SocketEngine)
public:
    QSocks5SocketEnginePrivate();
    ~QSocks5SocketEnginePrivate();

   enum Socks5State
    {
        Uninitialized = 0,
        ConnectError,
        AuthenticationMethodsSent,
        Authenticating,
        AuthenticatingError,
        RequestMethodSent,
        RequestError,
        Connected,
        UdpAssociateSuccess,
        BindSuccess,
        ControlSocketError,
        SocksError,
        HostNameLookupError
    };
    Socks5State socks5State;

    enum Socks5Mode
    {
        NoMode,
        ConnectMode,
        BindMode,
        UdpAssociateMode
    };
    Socks5Mode mode;

    enum Socks5Error
    {
        SocksFailure = 0x01,
        ConnectionNotAllowed = 0x02,
        NetworkUnreachable = 0x03,
        HostUnreachable = 0x04,
        ConnectionRefused = 0x05,
        TTLExpired = 0x06,
        CommandNotSupported = 0x07,
        AddressTypeNotSupported = 0x08,
        LastKnownError = AddressTypeNotSupported,
        UnknownError
    };

    void initialize(Socks5Mode socks5Mode);

    void setErrorState(Socks5State state, const QString &extraMessage = QString());
    void setErrorState(Socks5State state, Socks5Error socks5error);

    void reauthenticate();
    void parseAuthenticationMethodReply();
    void parseAuthenticatingReply();
    void sendRequestMethod();
    void parseRequestMethodReply();
    void parseNewConnection();

    bool waitForConnected(int msecs, bool *timedOut);

    void _q_controlSocketConnected();
    void _q_controlSocketReadNotification();
    void _q_controlSocketErrorOccurred(QAbstractSocket::SocketError);
#ifndef QT_NO_UDPSOCKET
    void _q_udpSocketReadNotification();
#endif
    void _q_controlSocketBytesWritten();
    void _q_controlSocketDisconnected();
    void _q_controlSocketStateChanged(QAbstractSocket::SocketState);

    QNetworkProxy proxyInfo;

    bool readNotificationEnabled, writeNotificationEnabled, exceptNotificationEnabled;

    qintptr socketDescriptor;

    QSocks5Data *data;
    QSocks5ConnectData *connectData;
#ifndef QT_NO_UDPSOCKET
    QSocks5UdpAssociateData *udpData;
#endif
    QSocks5BindData *bindData;
    QString peerName;
    QByteArray receivedHeaderFragment;

    mutable bool readNotificationActivated;
    mutable bool writeNotificationActivated;

    bool readNotificationPending;
    void _q_emitPendingReadNotification();
    void emitReadNotification();
    bool writeNotificationPending;
    void _q_emitPendingWriteNotification();
    void emitWriteNotification();
    bool connectionNotificationPending;
    void _q_emitPendingConnectionNotification();
    void emitConnectionNotification();
};

class Q_AUTOTEST_EXPORT QSocks5SocketEngineHandler : public QSocketEngineHandler
{
public:
    virtual QAbstractSocketEngine *createSocketEngine(QAbstractSocket::SocketType socketType,
                                                      const QNetworkProxy &, QObject *parent) override;
    virtual QAbstractSocketEngine *createSocketEngine(qintptr socketDescriptor, QObject *parent) override;
};

QT_END_NAMESPACE

#endif // QSOCKS5SOCKETENGINE_H
