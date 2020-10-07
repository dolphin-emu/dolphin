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

#ifndef HTTP2STREAMS_P_H
#define HTTP2STREAMS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of the Network Access API.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include "http2frames_p.h"
#include "hpack_p.h"

#include <private/qhttpnetworkconnectionchannel_p.h>
#include <private/qhttpnetworkrequest_p.h>

#include <QtCore/qglobal.h>
#include <QtCore/qstring.h>

#include <vector>

QT_REQUIRE_CONFIG(http);

QT_BEGIN_NAMESPACE

class QNonContiguousByteDevice;

namespace Http2
{

struct Q_AUTOTEST_EXPORT Stream
{
    enum StreamState {
        idle,
        open,
        halfClosedLocal,
        halfClosedRemote,
        remoteReserved,
        closed
    };

    Stream();
    // That's a ctor for a client-initiated stream:
    Stream(const HttpMessagePair &message, quint32 streamID, qint32 sendSize,
           qint32 recvSize);
    // That's a reserved stream, created by PUSH_PROMISE from a server:
    Stream(const QString &key, quint32 streamID, qint32 recvSize);

    QHttpNetworkReply *reply() const;
    const QHttpNetworkRequest &request() const;
    QHttpNetworkRequest &request();
    QHttpNetworkRequest::Priority priority() const;
    uchar weight() const;

    QNonContiguousByteDevice *data() const;

    HttpMessagePair httpPair;
    quint32 streamID = 0;
    // Signed as window sizes can become negative:
    qint32 sendWindow = 65535;
    qint32 recvWindow = 65535;

    StreamState state = idle;
    QString key; // for PUSH_PROMISE
};

struct PushPromise
{
    quint32 reservedID = 0;
    // PUSH_PROMISE has its own HEADERS,
    // usually similar to what request has:
    HPack::HttpHeader pushHeader;
    // Response has its own (normal) HEADERS:
    HPack::HttpHeader responseHeader;
    // DATA frames on a promised stream:
    std::vector<Frame> dataFrames;
};

} // namespace Http2

QT_END_NAMESPACE

#endif

