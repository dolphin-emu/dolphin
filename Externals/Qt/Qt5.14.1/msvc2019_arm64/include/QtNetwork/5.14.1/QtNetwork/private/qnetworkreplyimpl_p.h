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

#ifndef QNETWORKREPLYIMPL_P_H
#define QNETWORKREPLYIMPL_P_H

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
#include "qnetworkreply.h"
#include "qnetworkreply_p.h"
#include "qnetworkaccessmanager.h"
#include "qnetworkproxy.h"
#include "QtCore/qmap.h"
#include "QtCore/qqueue.h"
#include "QtCore/qbuffer.h"
#include "private/qringbuffer_p.h"
#include "private/qbytedata_p.h"
#include <QSharedPointer>
#include <QtNetwork/QNetworkSession>

QT_BEGIN_NAMESPACE

class QAbstractNetworkCache;
class QNetworkAccessBackend;

class QNetworkReplyImplPrivate;
class QNetworkReplyImpl: public QNetworkReply
{
    Q_OBJECT
public:
    QNetworkReplyImpl(QObject *parent = nullptr);
    ~QNetworkReplyImpl();
    virtual void abort() override;

    // reimplemented from QNetworkReply / QIODevice
    virtual void close() override;
    virtual qint64 bytesAvailable() const override;
    virtual void setReadBufferSize(qint64 size) override;

    virtual qint64 readData(char *data, qint64 maxlen) override;
    virtual bool event(QEvent *) override;

    Q_DECLARE_PRIVATE(QNetworkReplyImpl)
    Q_PRIVATE_SLOT(d_func(), void _q_startOperation())
    Q_PRIVATE_SLOT(d_func(), void _q_copyReadyRead())
    Q_PRIVATE_SLOT(d_func(), void _q_copyReadChannelFinished())
    Q_PRIVATE_SLOT(d_func(), void _q_bufferOutgoingData())
    Q_PRIVATE_SLOT(d_func(), void _q_bufferOutgoingDataFinished())
#ifndef QT_NO_BEARERMANAGEMENT
    Q_PRIVATE_SLOT(d_func(), void _q_networkSessionConnected())
    Q_PRIVATE_SLOT(d_func(), void _q_networkSessionFailed())
    Q_PRIVATE_SLOT(d_func(), void _q_networkSessionStateChanged(QNetworkSession::State))
    Q_PRIVATE_SLOT(d_func(), void _q_networkSessionUsagePoliciesChanged(QNetworkSession::UsagePolicies))
#endif

#ifndef QT_NO_SSL
protected:
    void sslConfigurationImplementation(QSslConfiguration &configuration) const override;
    void setSslConfigurationImplementation(const QSslConfiguration &configuration) override;
    virtual void ignoreSslErrors() override;
    virtual void ignoreSslErrorsImplementation(const QList<QSslError> &errors) override;
#endif
};

class QNetworkReplyImplPrivate: public QNetworkReplyPrivate
{
public:
    enum InternalNotifications {
        NotifyDownstreamReadyWrite,
        NotifyCloseDownstreamChannel,
        NotifyCopyFinished
    };

    QNetworkReplyImplPrivate();

    void _q_startOperation();
    void _q_copyReadyRead();
    void _q_copyReadChannelFinished();
    void _q_bufferOutgoingData();
    void _q_bufferOutgoingDataFinished();
#ifndef QT_NO_BEARERMANAGEMENT
    void _q_networkSessionConnected();
    void _q_networkSessionFailed();
    void _q_networkSessionStateChanged(QNetworkSession::State);
    void _q_networkSessionUsagePoliciesChanged(QNetworkSession::UsagePolicies);
#endif

    void setup(QNetworkAccessManager::Operation op, const QNetworkRequest &request,
               QIODevice *outgoingData);

    void pauseNotificationHandling();
    void resumeNotificationHandling();
    void backendNotify(InternalNotifications notification);
    void handleNotifications();
    void createCache();
    void completeCacheSave();

    // callbacks from the backend (through the manager):
    void setCachingEnabled(bool enable);
    bool isCachingEnabled() const;
    void consume(qint64 count);
    void emitUploadProgress(qint64 bytesSent, qint64 bytesTotal);
    qint64 nextDownstreamBlockSize() const;

    void initCacheSaveDevice();
    void appendDownstreamDataSignalEmissions();
    void appendDownstreamData(QByteDataBuffer &data);
    void appendDownstreamData(QIODevice *data);
    void appendDownstreamData(const QByteArray &data);

    void setDownloadBuffer(QSharedPointer<char> sp, qint64 size);
    char* getDownloadBuffer(qint64 size);
    void appendDownstreamDataDownloadBuffer(qint64, qint64);

    void finished();
    void error(QNetworkReply::NetworkError code, const QString &errorString);
    void metaDataChanged();
    void redirectionRequested(const QUrl &target);
    void encrypted();
    void sslErrors(const QList<QSslError> &errors);

    QNetworkAccessBackend *backend;
    QIODevice *outgoingData;
    QSharedPointer<QRingBuffer> outgoingDataBuffer;
    QIODevice *copyDevice;
    QAbstractNetworkCache *networkCache() const;

    bool migrateBackend();

    bool cacheEnabled;
    QIODevice *cacheSaveDevice;

    std::vector<InternalNotifications> pendingNotifications;
    bool notificationHandlingPaused;

    QUrl urlForLastAuthentication;
#ifndef QT_NO_NETWORKPROXY
    QNetworkProxy lastProxyAuthentication;
    QList<QNetworkProxy> proxyList;
#endif

    qint64 bytesDownloaded;
    qint64 lastBytesDownloaded;
    qint64 bytesUploaded;
    qint64 preMigrationDownloaded;

    QString httpReasonPhrase;
    int httpStatusCode;

    State state;

    // Only used when the "zero copy" style is used.
    // Please note that the whole "zero copy" download buffer API is private right now. Do not use it.
    qint64 downloadBufferReadPosition;
    qint64 downloadBufferCurrentSize;
    qint64 downloadBufferMaximumSize;
    QSharedPointer<char> downloadBufferPointer;
    char* downloadBuffer;

    Q_DECLARE_PUBLIC(QNetworkReplyImpl)
};
Q_DECLARE_TYPEINFO(QNetworkReplyImplPrivate::InternalNotifications, Q_PRIMITIVE_TYPE);

class QDisabledNetworkReply : public QNetworkReply
{
    Q_OBJECT

public:
    QDisabledNetworkReply(QObject *parent, const QNetworkRequest &req,
                          QNetworkAccessManager::Operation op);
    ~QDisabledNetworkReply();

    void abort() override { }
protected:
    qint64 readData(char *, qint64) override { return -1; }
};

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QSharedPointer<char>)

#endif
