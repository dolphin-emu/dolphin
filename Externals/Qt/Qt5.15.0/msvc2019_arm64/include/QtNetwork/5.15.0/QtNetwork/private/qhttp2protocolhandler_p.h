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

#ifndef QHTTP2PROTOCOLHANDLER_P_H
#define QHTTP2PROTOCOLHANDLER_P_H

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

#include <private/qhttpnetworkconnectionchannel_p.h>
#include <private/qabstractprotocolhandler_p.h>
#include <private/qhttpnetworkrequest_p.h>

#include <access/qhttp2configuration.h>

#include <private/http2protocol_p.h>
#include <private/http2streams_p.h>
#include <private/http2frames_p.h>
#include <private/hpacktable_p.h>
#include <private/hpack_p.h>

#include <QtCore/qnamespace.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qglobal.h>
#include <QtCore/qobject.h>
#include <QtCore/qflags.h>
#include <QtCore/qhash.h>

#include <vector>
#include <limits>
#include <deque>
#include <set>

QT_REQUIRE_CONFIG(http);

QT_BEGIN_NAMESPACE

class QHttp2ProtocolHandler : public QObject, public QAbstractProtocolHandler
{
    Q_OBJECT

public:
    QHttp2ProtocolHandler(QHttpNetworkConnectionChannel *channel);

    QHttp2ProtocolHandler(const QHttp2ProtocolHandler &rhs) = delete;
    QHttp2ProtocolHandler(QHttp2ProtocolHandler &&rhs) = delete;

    QHttp2ProtocolHandler &operator = (const QHttp2ProtocolHandler &rhs) = delete;
    QHttp2ProtocolHandler &operator = (QHttp2ProtocolHandler &&rhs) = delete;

    Q_INVOKABLE void handleConnectionClosure();
    Q_INVOKABLE void ensureClientPrefaceSent();

private slots:
    void _q_uploadDataReadyRead();
    void _q_replyDestroyed(QObject* reply);
    void _q_uploadDataDestroyed(QObject* uploadData);

private:
    using Stream = Http2::Stream;

    void _q_readyRead() override;
    Q_INVOKABLE void _q_receiveReply() override;
    Q_INVOKABLE bool sendRequest() override;

    bool sendClientPreface();
    bool sendSETTINGS_ACK();
    bool sendHEADERS(Stream &stream);
    bool sendDATA(Stream &stream);
    Q_INVOKABLE bool sendWINDOW_UPDATE(quint32 streamID, quint32 delta);
    bool sendRST_STREAM(quint32 streamID, quint32 errorCoder);
    bool sendGOAWAY(quint32 errorCode);

    void handleDATA();
    void handleHEADERS();
    void handlePRIORITY();
    void handleRST_STREAM();
    void handleSETTINGS();
    void handlePUSH_PROMISE();
    void handlePING();
    void handleGOAWAY();
    void handleWINDOW_UPDATE();
    void handleCONTINUATION();

    void handleContinuedHEADERS();

    bool acceptSetting(Http2::Settings identifier, quint32 newValue);

    void updateStream(Stream &stream, const HPack::HttpHeader &headers,
                      Qt::ConnectionType connectionType = Qt::DirectConnection);
    void updateStream(Stream &stream, const Http2::Frame &dataFrame,
                      Qt::ConnectionType connectionType = Qt::DirectConnection);
    void finishStream(Stream &stream, Qt::ConnectionType connectionType = Qt::DirectConnection);
    // Error code send by a peer (GOAWAY/RST_STREAM):
    void finishStreamWithError(Stream &stream, quint32 errorCode);
    // Locally encountered error:
    void finishStreamWithError(Stream &stream, QNetworkReply::NetworkError error,
                               const QString &message);

    // Stream's lifecycle management:
    quint32 createNewStream(const HttpMessagePair &message, bool uploadDone = false);
    void addToSuspended(Stream &stream);
    void markAsReset(quint32 streamID);
    quint32 popStreamToResume();
    void removeFromSuspended(quint32 streamID);
    void deleteActiveStream(quint32 streamID);
    bool streamWasReset(quint32 streamID) const;

    bool prefaceSent = false;
    // In the current implementation we send
    // SETTINGS only once, immediately after
    // the client's preface 24-byte message.
    bool waitingForSettingsACK = false;

    static const quint32 maxAcceptableTableSize = 16 * HPack::FieldLookupTable::DefaultSize;
    // HTTP/2 4.3: Header compression is stateful. One compression context and
    // one decompression context are used for the entire connection.
    HPack::Decoder decoder;
    HPack::Encoder encoder;

    QHash<QObject *, int> streamIDs;
    QHash<quint32, Stream> activeStreams;
    std::deque<quint32> suspendedStreams[3]; // 3 for priorities: High, Normal, Low.
    static const std::deque<quint32>::size_type maxRecycledStreams;
    std::deque<quint32> recycledStreams;

    // Peer's max frame size (this min is the default value
    // we start with, that can be updated by SETTINGS frame):
    quint32 maxFrameSize = Http2::minPayloadLimit;

    Http2::FrameReader frameReader;
    Http2::Frame inboundFrame;
    Http2::FrameWriter frameWriter;
    // Temporary storage to assemble HEADERS' block
    // from several CONTINUATION frames ...
    bool continuationExpected = false;
    std::vector<Http2::Frame> continuedFrames;

    // Control flow:

    // This is how many concurrent streams our peer allows us, 100 is the
    // initial value, can be updated by the server's SETTINGS frame(s):
    quint32 maxConcurrentStreams = Http2::maxConcurrentStreams;
    // While we allow sending SETTTINGS_MAX_CONCURRENT_STREAMS to limit our peer,
    // it's just a hint and we do not actually enforce it (and we can continue
    // sending requests and creating streams while maxConcurrentStreams allows).

    // This is our (client-side) maximum possible receive window size, we set
    // it in a ctor from QHttp2Configuration, it does not change after that.
    // The default is 64Kb:
    qint32 maxSessionReceiveWindowSize = Http2::defaultSessionWindowSize;

    // Our session current receive window size, updated in a ctor from
    // QHttp2Configuration. Signed integer since it can become negative
    // (it's still a valid window size).
    qint32 sessionReceiveWindowSize = Http2::defaultSessionWindowSize;
    // Our per-stream receive window size, default is 64 Kb, will be updated
    // from QHttp2Configuration. Again, signed - can become negative.
    qint32 streamInitialReceiveWindowSize = Http2::defaultSessionWindowSize;

    // These are our peer's receive window sizes, they will be updated by the
    // peer's SETTINGS and WINDOW_UPDATE frames, defaults presumed to be 64Kb.
    qint32 sessionSendWindowSize = Http2::defaultSessionWindowSize;
    qint32 streamInitialSendWindowSize = Http2::defaultSessionWindowSize;

    // Our peer's header size limitations. It's unlimited by default, but can
    // be changed via peer's SETTINGS frame.
    quint32 maxHeaderListSize = (std::numeric_limits<quint32>::max)();
    // While we can send SETTINGS_MAX_HEADER_LIST_SIZE value (our limit on
    // the headers size), we never enforce it, it's just a hint to our peer.

    Q_INVOKABLE void resumeSuspendedStreams();
    // Our stream IDs (all odd), the first valid will be 1.
    quint32 nextID = 1;
    quint32 allocateStreamID();
    bool validPeerStreamID() const;
    bool goingAway = false;
    bool pushPromiseEnabled = false;
    quint32 lastPromisedID = Http2::connectionStreamID;
    QHash<QString, Http2::PushPromise> promisedData;
    bool tryReserveStream(const Http2::Frame &pushPromiseFrame,
                          const HPack::HttpHeader &requestHeader);
    void resetPromisedStream(const Http2::Frame &pushPromiseFrame,
                             Http2::Http2Error reason);
    void initReplyFromPushPromise(const HttpMessagePair &message,
                                  const QString &cacheKey);
    // Errors:
    void connectionError(Http2::Http2Error errorCode,
                         const char *message);
    void closeSession();
};

QT_END_NAMESPACE

#endif
