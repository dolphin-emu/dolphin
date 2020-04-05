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

#ifndef QLOCALSERVER_P_H
#define QLOCALSERVER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of the QLocalServer class.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include <QtNetwork/private/qtnetworkglobal_p.h>

#include "qlocalserver.h"
#include "private/qobject_p.h"
#include <qqueue.h>

QT_REQUIRE_CONFIG(localserver);

#if defined(QT_LOCALSOCKET_TCP)
#   include <qtcpserver.h>
#elif defined(Q_OS_WIN)
#   include <qt_windows.h>
#   include <qwineventnotifier.h>
#else
#   include <private/qabstractsocketengine_p.h>
#   include <qsocketnotifier.h>
#endif

QT_BEGIN_NAMESPACE

class QLocalServerPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QLocalServer)

public:
    QLocalServerPrivate() :
#if !defined(QT_LOCALSOCKET_TCP) && !defined(Q_OS_WIN)
            listenSocket(-1), socketNotifier(nullptr),
#endif
            maxPendingConnections(30), error(QAbstractSocket::UnknownSocketError),
            socketOptions(QLocalServer::NoOptions)
    {
    }

    void init();
    bool listen(const QString &name);
    bool listen(qintptr socketDescriptor);
    static bool removeServer(const QString &name);
    void closeServer();
    void waitForNewConnection(int msec, bool *timedOut);
    void _q_onNewConnection();

#if defined(QT_LOCALSOCKET_TCP)

    QTcpServer tcpServer;
    QMap<quintptr, QTcpSocket*> socketMap;
#elif defined(Q_OS_WIN)
    struct Listener {
        HANDLE handle;
        OVERLAPPED overlapped;
        bool connected;
    };

    void setError(const QString &function);
    bool addListener();

    QList<Listener> listeners;
    HANDLE eventHandle;
    QWinEventNotifier *connectionEventNotifier;
#else
    void setError(const QString &function);

    int listenSocket;
    QSocketNotifier *socketNotifier;
#endif

    QString serverName;
    QString fullServerName;
    int maxPendingConnections;
    QQueue<QLocalSocket*> pendingConnections;
    QString errorString;
    QAbstractSocket::SocketError error;
    QLocalServer::SocketOptions socketOptions;
};

QT_END_NAMESPACE

#endif // QLOCALSERVER_P_H

