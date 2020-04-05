/****************************************************************************
**
** Copyright (C) 2015 Intel Corporation.
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

#ifndef QNETWORKDATAGRAM_P_H
#define QNETWORKDATAGRAM_P_H

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

#include <QtNetwork/private/qtnetworkglobal_p.h>
#include <QtNetwork/qhostaddress.h>

QT_BEGIN_NAMESPACE

class QIpPacketHeader
{
public:
    QIpPacketHeader(const QHostAddress &dstAddr = QHostAddress(), quint16 port = 0)
        : destinationAddress(dstAddr), ifindex(0), hopLimit(-1), streamNumber(-1),
          destinationPort(port), endOfRecord(false)
    {}

    void clear()
    {
        senderAddress.clear();
        destinationAddress.clear();
        ifindex = 0;
        hopLimit = -1;
        streamNumber = -1;
        endOfRecord = false;
    }

    QHostAddress senderAddress;
    QHostAddress destinationAddress;

    uint ifindex;
    int hopLimit;
    int streamNumber;
    quint16 senderPort;
    quint16 destinationPort;
    bool endOfRecord;
};

class QNetworkDatagramPrivate
{
public:
    QNetworkDatagramPrivate(const QByteArray &data = QByteArray(),
                            const QHostAddress &dstAddr = QHostAddress(), quint16 port = 0)
        : data(data), header(dstAddr, port)
    {}
    QNetworkDatagramPrivate(const QByteArray &data, const QIpPacketHeader &header)
        : data(data), header(header)
    {}

    QByteArray data;
    QIpPacketHeader header;
};

QT_END_NAMESPACE

#endif // QNETWORKDATAGRAM_P_H
