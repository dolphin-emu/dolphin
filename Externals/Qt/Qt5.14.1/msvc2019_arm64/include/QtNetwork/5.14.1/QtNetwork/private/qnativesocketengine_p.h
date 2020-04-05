/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Intel Corporation.
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

#ifndef QNATIVESOCKETENGINE_P_H
#define QNATIVESOCKETENGINE_P_H

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
#include "QtNetwork/qhostaddress.h"
#include "QtNetwork/qnetworkinterface.h"
#include "private/qabstractsocketengine_p.h"
#ifndef Q_OS_WIN
#  include "qplatformdefs.h"
#  include <netinet/in.h>
#else
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <mswsock.h>
#endif

QT_BEGIN_NAMESPACE

#ifdef Q_OS_WIN
#  define QT_SOCKLEN_T int
#  define QT_SOCKOPTLEN_T int

// The following definitions are copied from the MinGW header mswsock.h which
// was placed in the public domain. The WSASendMsg and WSARecvMsg functions
// were introduced with Windows Vista, so some Win32 headers are lacking them.
// There are no known versions of Windows CE or Embedded that contain them.
#  ifndef WSAID_WSARECVMSG
typedef INT (WINAPI *LPFN_WSARECVMSG)(SOCKET s, LPWSAMSG lpMsg,
                                      LPDWORD lpdwNumberOfBytesRecvd,
                                      LPWSAOVERLAPPED lpOverlapped,
                                      LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
#    define WSAID_WSARECVMSG {0xf689d7c8,0x6f1f,0x436b,{0x8a,0x53,0xe5,0x4f,0xe3,0x51,0xc3,0x22}}
#  endif // !WSAID_WSARECVMSG
#  ifndef WSAID_WSASENDMSG
typedef struct {
  LPWSAMSG lpMsg;
  DWORD dwFlags;
  LPDWORD lpNumberOfBytesSent;
  LPWSAOVERLAPPED lpOverlapped;
  LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine;
} WSASENDMSG, *LPWSASENDMSG;

typedef INT (WSAAPI *LPFN_WSASENDMSG)(SOCKET s, LPWSAMSG lpMsg, DWORD dwFlags,
                                      LPDWORD lpNumberOfBytesSent,
                                      LPWSAOVERLAPPED lpOverlapped,
                                      LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);

#    define WSAID_WSASENDMSG {0xa441e712,0x754f,0x43ca,{0x84,0xa7,0x0d,0xee,0x44,0xcf,0x60,0x6d}}
#  endif // !WSAID_WSASENDMSG
#endif // Q_OS_WIN

union qt_sockaddr {
    sockaddr a;
    sockaddr_in a4;
    sockaddr_in6 a6;
};

namespace {
namespace SetSALen {
    template <typename T> void set(T *sa, typename std::enable_if<(&T::sa_len, true), QT_SOCKLEN_T>::type len)
    { sa->sa_len = len; }
    template <typename T> void set(T *sin6, typename std::enable_if<(&T::sin6_len, true), QT_SOCKLEN_T>::type len)
    { sin6->sin6_len = len; }
    template <typename T> void set(T *, ...) {}
}
}

class QNativeSocketEnginePrivate;
#ifndef QT_NO_NETWORKINTERFACE
class QNetworkInterface;
#endif

class Q_AUTOTEST_EXPORT QNativeSocketEngine : public QAbstractSocketEngine
{
    Q_OBJECT
public:
    QNativeSocketEngine(QObject *parent = nullptr);
    ~QNativeSocketEngine();

    bool initialize(QAbstractSocket::SocketType type, QAbstractSocket::NetworkLayerProtocol protocol = QAbstractSocket::IPv4Protocol) override;
    bool initialize(qintptr socketDescriptor, QAbstractSocket::SocketState socketState = QAbstractSocket::ConnectedState) override;

    qintptr socketDescriptor() const override;

    bool isValid() const override;

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
                            const QNetworkInterface &iface) override;
    bool leaveMulticastGroup(const QHostAddress &groupAddress,
                             const QNetworkInterface &iface) override;
    QNetworkInterface multicastInterface() const override;
    bool setMulticastInterface(const QNetworkInterface &iface) override;
#endif

    bool hasPendingDatagrams() const override;
    qint64 pendingDatagramSize() const override;
#endif // QT_NO_UDPSOCKET

    qint64 readDatagram(char *data, qint64 maxlen, QIpPacketHeader * = nullptr,
                        PacketHeaderOptions = WantNone) override;
    qint64 writeDatagram(const char *data, qint64 len, const QIpPacketHeader &) override;
    qint64 bytesToWrite() const override;

#if 0   // currently unused
    qint64 receiveBufferSize() const;
    void setReceiveBufferSize(qint64 bufferSize);

    qint64 sendBufferSize() const;
    void setSendBufferSize(qint64 bufferSize);
#endif

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

public Q_SLOTS:
    // non-virtual override;
    void connectionNotification();

private:
    Q_DECLARE_PRIVATE(QNativeSocketEngine)
    Q_DISABLE_COPY_MOVE(QNativeSocketEngine)
};

class QSocketNotifier;

class QNativeSocketEnginePrivate : public QAbstractSocketEnginePrivate
{
    Q_DECLARE_PUBLIC(QNativeSocketEngine)
public:
    QNativeSocketEnginePrivate();
    ~QNativeSocketEnginePrivate();

    qintptr socketDescriptor;

    QSocketNotifier *readNotifier, *writeNotifier, *exceptNotifier;

#if defined(Q_OS_WIN)
    LPFN_WSASENDMSG sendmsg;
    LPFN_WSARECVMSG recvmsg;
#  endif
    enum ErrorString {
        NonBlockingInitFailedErrorString,
        BroadcastingInitFailedErrorString,
        NoIpV6ErrorString,
        RemoteHostClosedErrorString,
        TimeOutErrorString,
        ResourceErrorString,
        OperationUnsupportedErrorString,
        ProtocolUnsupportedErrorString,
        InvalidSocketErrorString,
        HostUnreachableErrorString,
        NetworkUnreachableErrorString,
        AccessErrorString,
        ConnectionTimeOutErrorString,
        ConnectionRefusedErrorString,
        AddressInuseErrorString,
        AddressNotAvailableErrorString,
        AddressProtectedErrorString,
        DatagramTooLargeErrorString,
        SendDatagramErrorString,
        ReceiveDatagramErrorString,
        WriteErrorString,
        ReadErrorString,
        PortInuseErrorString,
        NotSocketErrorString,
        InvalidProxyTypeString,
        TemporaryErrorString,
        NetworkDroppedConnectionErrorString,
        ConnectionResetErrorString,

        UnknownSocketErrorString = -1
    };

    void setError(QAbstractSocket::SocketError error, ErrorString errorString) const;
    QHostAddress adjustAddressProtocol(const QHostAddress &address) const;

    // native functions
    int option(QNativeSocketEngine::SocketOption option) const;
    bool setOption(QNativeSocketEngine::SocketOption option, int value);

    bool createNewSocket(QAbstractSocket::SocketType type, QAbstractSocket::NetworkLayerProtocol &protocol);

    bool nativeConnect(const QHostAddress &address, quint16 port);
    bool nativeBind(const QHostAddress &address, quint16 port);
    bool nativeListen(int backlog);
    int nativeAccept();
#ifndef QT_NO_NETWORKINTERFACE
    bool nativeJoinMulticastGroup(const QHostAddress &groupAddress,
                                  const QNetworkInterface &iface);
    bool nativeLeaveMulticastGroup(const QHostAddress &groupAddress,
                                   const QNetworkInterface &iface);
    QNetworkInterface nativeMulticastInterface() const;
    bool nativeSetMulticastInterface(const QNetworkInterface &iface);
#endif
    qint64 nativeBytesAvailable() const;

    bool nativeHasPendingDatagrams() const;
    qint64 nativePendingDatagramSize() const;
    qint64 nativeReceiveDatagram(char *data, qint64 maxLength, QIpPacketHeader *header,
                                 QAbstractSocketEngine::PacketHeaderOptions options);
    qint64 nativeSendDatagram(const char *data, qint64 length, const QIpPacketHeader &header);
    qint64 nativeRead(char *data, qint64 maxLength);
    qint64 nativeWrite(const char *data, qint64 length);
    int nativeSelect(int timeout, bool selectForRead) const;
    int nativeSelect(int timeout, bool checkRead, bool checkWrite,
                     bool *selectForRead, bool *selectForWrite) const;

    void nativeClose();

    bool checkProxy(const QHostAddress &address);
    bool fetchConnectionParameters();

#if QT_CONFIG(networkinterface)
    static uint scopeIdFromString(const QString &scopeid)
    { return QNetworkInterface::interfaceIndexFromName(scopeid); }
#endif

    /*! \internal
        Sets \a address and \a port in the \a aa sockaddr structure and the size in \a sockAddrSize.
        The address \a is converted to IPv6 if the current socket protocol is also IPv6.
     */
    void setPortAndAddress(quint16 port, const QHostAddress &address, qt_sockaddr *aa, QT_SOCKLEN_T *sockAddrSize)
    {
        if (address.protocol() == QAbstractSocket::IPv6Protocol
            || address.protocol() == QAbstractSocket::AnyIPProtocol
            || socketProtocol == QAbstractSocket::IPv6Protocol
            || socketProtocol == QAbstractSocket::AnyIPProtocol) {
            memset(&aa->a6, 0, sizeof(sockaddr_in6));
            aa->a6.sin6_family = AF_INET6;
#if QT_CONFIG(networkinterface)
            aa->a6.sin6_scope_id = scopeIdFromString(address.scopeId());
#endif
            aa->a6.sin6_port = htons(port);
            Q_IPV6ADDR tmp = address.toIPv6Address();
            memcpy(&aa->a6.sin6_addr, &tmp, sizeof(tmp));
            *sockAddrSize = sizeof(sockaddr_in6);
            SetSALen::set(&aa->a, sizeof(sockaddr_in6));
        } else {
            memset(&aa->a, 0, sizeof(sockaddr_in));
            aa->a4.sin_family = AF_INET;
            aa->a4.sin_port = htons(port);
            aa->a4.sin_addr.s_addr = htonl(address.toIPv4Address());
            *sockAddrSize = sizeof(sockaddr_in);
            SetSALen::set(&aa->a, sizeof(sockaddr_in));
        }
    }

};

QT_END_NAMESPACE

#endif // QNATIVESOCKETENGINE_P_H
