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

#ifndef HTTP2PROTOCOL_P_H
#define HTTP2PROTOCOL_P_H

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

#include <QtNetwork/qnetworkreply.h>
#include <QtCore/qloggingcategory.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qglobal.h>
#include <QtCore/qmap.h>

// Different HTTP/2 constants/values as defined by RFC 7540.

QT_BEGIN_NAMESPACE

class QHttpNetworkRequest;
class QHttp2Configuration;
class QHttpNetworkReply;
class QByteArray;
class QString;

namespace Http2
{

enum class Settings : quint16
{
    HEADER_TABLE_SIZE_ID = 0x1,
    ENABLE_PUSH_ID = 0x2,
    MAX_CONCURRENT_STREAMS_ID = 0x3,
    INITIAL_WINDOW_SIZE_ID = 0x4,
    MAX_FRAME_SIZE_ID = 0x5,
    MAX_HEADER_LIST_SIZE_ID = 0x6
};

enum class FrameType : uchar
{
    DATA = 0x0,
    HEADERS = 0x1,
    PRIORITY = 0x2,
    RST_STREAM = 0x3,
    SETTINGS = 0x4,
    PUSH_PROMISE = 0x5,
    PING = 0x6,
    GOAWAY = 0x7,
    WINDOW_UPDATE = 0x8,
    CONTINUATION = 0x9,
    // ATTENTION: enumerators must be sorted.
    // We use LAST_FRAME_TYPE to check if
    // frame type is known, if not - this frame
    // must be ignored, HTTP/2 5.1).
    LAST_FRAME_TYPE
};

enum class FrameFlag : uchar
{
    EMPTY = 0x0, // Valid for any frame type.
    ACK = 0x1, // Valid for PING, SETTINGS
    END_STREAM = 0x1, // Valid for HEADERS, DATA
    END_HEADERS = 0x4, // Valid for PUSH_PROMISE, HEADERS,
    PADDED = 0x8, // Valid for PUSH_PROMISE, HEADERS, DATA
    PRIORITY = 0x20 // Valid for HEADERS,
};

Q_DECLARE_FLAGS(FrameFlags, FrameFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(FrameFlags)

enum Http2PredefinedParameters
{
    // Old-style enum, so we
    // can use as Http2::frameHeaderSize for example.
    clientPrefaceLength = 24, // HTTP/2, 3.5
    connectionStreamID = 0, // HTTP/2, 5.1.1
    frameHeaderSize = 9, // HTTP/2, 4.1

    // The initial allowed payload size. We would use it as an
    // upper limit for a frame payload we send, until our peer
    // updates us with a larger SETTINGS_MAX_FRAME_SIZE.

    // The initial maximum payload size that an HTTP/2 frame
    // can contain is 16384. It's also the minimal size that
    // can be advertised  via 'SETTINGS' frames. A real frame
    // can have a payload smaller than 16384.
    minPayloadLimit = 16384, // HTTP/2 6.5.2
    // The maximum allowed payload size.
    maxPayloadSize = (1 << 24) - 1, // HTTP/2 6.5.2

    defaultSessionWindowSize = 65535, // HTTP/2 6.5.2
    // Using 1000 (rather arbitrarily), just to
    // impose *some* upper limit:
    maxPeerConcurrentStreams  = 1000,
    maxConcurrentStreams = 100 // HTTP/2, 6.5.2
};

// These are ints, const, they have internal linkage, it's ok to have them in
// headers - no ODR violation.
const quint32 lastValidStreamID((quint32(1) << 31) - 1); // HTTP/2, 5.1.1

// The default size of 64K is too small and limiting: if we use it, we end up
// sending WINDOW_UPDATE frames on a stream/session all the time, for each
// 2 DATE frames of size 16K (also default) we'll send a WINDOW_UPDATE frame
// for a given stream and have a download speed order of magnitude lower than
// our own HTTP/1.1 protocol handler. We choose a bigger window size: normally,
// HTTP/2 servers are not afraid to immediately set it to the possible max,
// we do the same and split this window size between our concurrent streams.
const qint32 maxSessionReceiveWindowSize((quint32(1) << 31) - 1);
const qint32 qtDefaultStreamReceiveWindowSize = maxSessionReceiveWindowSize / maxConcurrentStreams;

struct Frame configurationToSettingsFrame(const QHttp2Configuration &configuration);
QByteArray settingsFrameToBase64(const Frame &settingsFrame);
void appendProtocolUpgradeHeaders(const QHttp2Configuration &configuration, QHttpNetworkRequest *request);

extern const Q_AUTOTEST_EXPORT char Http2clientPreface[clientPrefaceLength];

enum class FrameStatus
{
    protocolError,
    sizeError,
    incompleteFrame,
    goodFrame
};

enum Http2Error
{
    // Old-style enum to avoid excessive name
    // qualification ...
    // NB:
    // I use the last enumerator to check
    // that errorCode (quint32) is valid,
    // so it needs to be the highest-numbered!
    // HTTP/2 7:
    HTTP2_NO_ERROR = 0x0,
    PROTOCOL_ERROR = 0x1,
    INTERNAL_ERROR = 0x2,
    FLOW_CONTROL_ERROR = 0x3,
    SETTINGS_TIMEOUT = 0x4,
    STREAM_CLOSED = 0x5,
    FRAME_SIZE_ERROR = 0x6,
    REFUSE_STREAM = 0x7,
    CANCEL = 0x8,
    COMPRESSION_ERROR = 0x9,
    CONNECT_ERROR = 0xa,
    ENHANCE_YOUR_CALM = 0xb,
    INADEQUATE_SECURITY = 0xc,
    HTTP_1_1_REQUIRED = 0xd
};

void qt_error(quint32 errorCode, QNetworkReply::NetworkError &error, QString &errorString);
QString qt_error_string(quint32 errorCode);
QNetworkReply::NetworkError qt_error(quint32 errorCode);
bool is_protocol_upgraded(const QHttpNetworkReply &reply);

} // namespace Http2

Q_DECLARE_LOGGING_CATEGORY(QT_HTTP2)

QT_END_NAMESPACE

Q_DECLARE_METATYPE(Http2::Settings)

#endif
