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

#ifndef QNETWORKREPLYHTTPIMPL_P_H
#define QNETWORKREPLYHTTPIMPL_P_H

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
#include "qnetworkrequest.h"
#include "qnetworkreply.h"

#include "QtCore/qpointer.h"
#include "QtCore/qdatetime.h"
#include "QtCore/qsharedpointer.h"
#include "QtCore/qscopedpointer.h"
#include "QtCore/qtimer.h"
#include "qatomic.h"

#include <QtNetwork/QNetworkCacheMetaData>
#include <private/qhttpnetworkrequest_p.h>
#include <private/qnetworkreply_p.h>
#include <QtNetwork/QNetworkProxy>
#include <QtNetwork/QNetworkSession> // ### Qt6: Remove include

#ifndef QT_NO_SSL
#include <QtNetwork/QSslConfiguration>
#endif

QT_REQUIRE_CONFIG(http);

QT_BEGIN_NAMESPACE

class QIODevice;

class QNetworkReplyHttpImplPrivate;
class QNetworkReplyHttpImpl: public QNetworkReply
{
    Q_OBJECT
public:
    QNetworkReplyHttpImpl(QNetworkAccessManager* const, const QNetworkRequest&, QNetworkAccessManager::Operation&, QIODevice* outgoingData);
    virtual ~QNetworkReplyHttpImpl();

    void close() override;
    void abort() override;
    qint64 bytesAvailable() const override;
    bool isSequential () const override;
    qint64 size() const override;
    qint64 readData(char*, qint64) override;
    void setReadBufferSize(qint64 size) override;
    bool canReadLine () const override;

    Q_DECLARE_PRIVATE(QNetworkReplyHttpImpl)
    Q_PRIVATE_SLOT(d_func(), void _q_startOperation())
    Q_PRIVATE_SLOT(d_func(), bool start(const QNetworkRequest &))
    Q_PRIVATE_SLOT(d_func(), void _q_cacheLoadReadyRead())
    Q_PRIVATE_SLOT(d_func(), void _q_bufferOutgoingData())
    Q_PRIVATE_SLOT(d_func(), void _q_bufferOutgoingDataFinished())
    Q_PRIVATE_SLOT(d_func(), void _q_transferTimedOut())
#ifndef QT_NO_BEARERMANAGEMENT // ### Qt6: Remove section
    Q_PRIVATE_SLOT(d_func(), void _q_networkSessionConnected())
    Q_PRIVATE_SLOT(d_func(), void _q_networkSessionFailed())
    Q_PRIVATE_SLOT(d_func(), void _q_networkSessionStateChanged(QNetworkSession::State))
    Q_PRIVATE_SLOT(d_func(), void _q_networkSessionUsagePoliciesChanged(QNetworkSession::UsagePolicies))
#endif
    Q_PRIVATE_SLOT(d_func(), void _q_finished())
    Q_PRIVATE_SLOT(d_func(), void _q_error(QNetworkReply::NetworkError, const QString &))

    // From reply
    Q_PRIVATE_SLOT(d_func(), void replyDownloadData(QByteArray))
    Q_PRIVATE_SLOT(d_func(), void replyFinished())
    Q_PRIVATE_SLOT(d_func(), void replyDownloadMetaData(QList<QPair<QByteArray,QByteArray> >,
                                                        int, QString, bool, QSharedPointer<char>,
                                                        qint64, qint64, bool))
    Q_PRIVATE_SLOT(d_func(), void replyDownloadProgressSlot(qint64,qint64))
    Q_PRIVATE_SLOT(d_func(), void httpAuthenticationRequired(const QHttpNetworkRequest &, QAuthenticator *))
    Q_PRIVATE_SLOT(d_func(), void httpError(QNetworkReply::NetworkError, const QString &))
#ifndef QT_NO_SSL
    Q_PRIVATE_SLOT(d_func(), void replyEncrypted())
    Q_PRIVATE_SLOT(d_func(), void replySslErrors(const QList<QSslError> &, bool *, QList<QSslError> *))
    Q_PRIVATE_SLOT(d_func(), void replySslConfigurationChanged(const QSslConfiguration&))
    Q_PRIVATE_SLOT(d_func(), void replyPreSharedKeyAuthenticationRequiredSlot(QSslPreSharedKeyAuthenticator *))
#endif
#ifndef QT_NO_NETWORKPROXY
    Q_PRIVATE_SLOT(d_func(), void proxyAuthenticationRequired(const QNetworkProxy &proxy, QAuthenticator *auth))
#endif

    Q_PRIVATE_SLOT(d_func(), void resetUploadDataSlot(bool *r))
    Q_PRIVATE_SLOT(d_func(), void wantUploadDataSlot(qint64))
    Q_PRIVATE_SLOT(d_func(), void sentUploadDataSlot(qint64,qint64))
    Q_PRIVATE_SLOT(d_func(), void uploadByteDeviceReadyReadSlot())
    Q_PRIVATE_SLOT(d_func(), void emitReplyUploadProgress(qint64, qint64))
    Q_PRIVATE_SLOT(d_func(), void _q_cacheSaveDeviceAboutToClose())
    Q_PRIVATE_SLOT(d_func(), void _q_metaDataChanged())
    Q_PRIVATE_SLOT(d_func(), void onRedirected(const QUrl &, int, int))
    Q_PRIVATE_SLOT(d_func(), void followRedirect())

#ifndef QT_NO_SSL
protected:
    void ignoreSslErrors() override;
    void ignoreSslErrorsImplementation(const QList<QSslError> &errors) override;
    void setSslConfigurationImplementation(const QSslConfiguration &configuration) override;
    void sslConfigurationImplementation(QSslConfiguration &configuration) const override;
#endif

signals:
    // To HTTP thread:
    void startHttpRequest();
    void abortHttpRequest();
    void readBufferSizeChanged(qint64 size);
    void readBufferFreed(qint64 size);

    void startHttpRequestSynchronously();

    void haveUploadData(const qint64 pos, const QByteArray &dataArray, bool dataAtEnd, qint64 dataSize);
};

class QNetworkReplyHttpImplPrivate: public QNetworkReplyPrivate
{
#if QT_CONFIG(bearermanagement) // ### Qt6: Remove section
    bool startWaitForSession(QSharedPointer<QNetworkSession> &session);
#endif

public:

    static QHttpNetworkRequest::Priority convert(const QNetworkRequest::Priority& prio);

    QNetworkReplyHttpImplPrivate();
    ~QNetworkReplyHttpImplPrivate();

    bool start(const QNetworkRequest &newHttpRequest);
    void _q_startOperation();

    void _q_cacheLoadReadyRead();

    void _q_bufferOutgoingData();
    void _q_bufferOutgoingDataFinished();

    void _q_cacheSaveDeviceAboutToClose();

    void _q_transferTimedOut();
    void setupTransferTimeout();

#ifndef QT_NO_BEARERMANAGEMENT // ### Qt6: Remove section
    void _q_networkSessionConnected();
    void _q_networkSessionFailed();
    void _q_networkSessionStateChanged(QNetworkSession::State);
    void _q_networkSessionUsagePoliciesChanged(QNetworkSession::UsagePolicies);
#endif
    void _q_finished();

    void finished();
    void error(QNetworkReply::NetworkError code, const QString &errorString);
    void _q_error(QNetworkReply::NetworkError code, const QString &errorString);
    void _q_metaDataChanged();

    void checkForRedirect(const int statusCode);

    // incoming from user
    QNetworkAccessManager *manager;
    QNetworkAccessManagerPrivate *managerPrivate;
    QHttpNetworkRequest httpRequest; // There is also a copy in the HTTP thread
    bool synchronous;

    State state;

    // from http thread
    int statusCode;
    QString reasonPhrase;

    // upload
    QNonContiguousByteDevice* createUploadByteDevice();
    QSharedPointer<QNonContiguousByteDevice> uploadByteDevice;
    qint64 uploadByteDevicePosition;
    bool uploadDeviceChoking; // if we couldn't readPointer() any data at the moment
    QIODevice *outgoingData;
    QSharedPointer<QRingBuffer> outgoingDataBuffer;
    void emitReplyUploadProgress(qint64 bytesSent, qint64 bytesTotal); // dup?
    void onRedirected(const QUrl &redirectUrl, int httpStatus, int maxRedirectsRemainig);
    void followRedirect();
    qint64 bytesUploaded;


    // cache
    void createCache();
    void completeCacheSave();
    void setCachingEnabled(bool enable);
    bool isCachingEnabled() const;
    bool isCachingAllowed() const;
    void initCacheSaveDevice();
    QIODevice *cacheLoadDevice;
    bool loadingFromCache;

    QIODevice *cacheSaveDevice;
    bool cacheEnabled; // is this for saving?


    QUrl urlForLastAuthentication;
#ifndef QT_NO_NETWORKPROXY
    QNetworkProxy lastProxyAuthentication;
#endif


    bool migrateBackend();
    bool canResume() const;
    void setResumeOffset(quint64 offset);
    quint64 resumeOffset;
    qint64 preMigrationDownloaded;

    qint64 bytesDownloaded;
    qint64 bytesBuffered;

    QTimer *transferTimeout;

    // Only used when the "zero copy" style is used.
    // Please note that the whole "zero copy" download buffer API is private right now. Do not use it.
    qint64 downloadBufferReadPosition;
    qint64 downloadBufferCurrentSize;
    QSharedPointer<char> downloadBufferPointer;
    char* downloadZerocopyBuffer;

    // Will be increased by HTTP thread:
    QSharedPointer<QAtomicInt> pendingDownloadDataEmissions;
    QSharedPointer<QAtomicInt> pendingDownloadProgressEmissions;


#ifndef QT_NO_SSL
    QScopedPointer<QSslConfiguration> sslConfiguration;
    bool pendingIgnoreAllSslErrors;
    QList<QSslError> pendingIgnoreSslErrorsList;
#endif

    QNetworkRequest redirectRequest;

    bool loadFromCacheIfAllowed(QHttpNetworkRequest &httpRequest);
    void invalidateCache();
    bool sendCacheContents(const QNetworkCacheMetaData &metaData);
    QNetworkCacheMetaData fetchCacheMetaData(const QNetworkCacheMetaData &metaData) const;


    void postRequest(const QNetworkRequest& newHttpRequest);
    QNetworkAccessManager::Operation getRedirectOperation(QNetworkAccessManager::Operation currentOp, int httpStatus);
    QNetworkRequest createRedirectRequest(const QNetworkRequest &originalRequests, const QUrl &url, int maxRedirectsRemainig);
    bool isHttpRedirectResponse() const;

public:
    // From HTTP thread:
    void replyDownloadData(QByteArray);
    void replyFinished();
    void replyDownloadMetaData(const QList<QPair<QByteArray,QByteArray> > &, int, const QString &,
                               bool, QSharedPointer<char>, qint64, qint64, bool);
    void replyDownloadProgressSlot(qint64,qint64);
    void httpAuthenticationRequired(const QHttpNetworkRequest &request, QAuthenticator *auth);
    void httpError(QNetworkReply::NetworkError error, const QString &errorString);
#ifndef QT_NO_SSL
    void replyEncrypted();
    void replySslErrors(const QList<QSslError> &, bool *, QList<QSslError> *);
    void replySslConfigurationChanged(const QSslConfiguration &newSslConfiguration);
    void replyPreSharedKeyAuthenticationRequiredSlot(QSslPreSharedKeyAuthenticator *);
#endif
#ifndef QT_NO_NETWORKPROXY
    void proxyAuthenticationRequired(const QNetworkProxy &proxy, QAuthenticator *auth);
#endif

    // From QNonContiguousByteDeviceThreadForwardImpl in HTTP thread:
    void resetUploadDataSlot(bool *r);
    void wantUploadDataSlot(qint64);
    void sentUploadDataSlot(qint64, qint64);

    // From user's QNonContiguousByteDevice
    void uploadByteDeviceReadyReadSlot();

    Q_DECLARE_PUBLIC(QNetworkReplyHttpImpl)
};

QT_END_NAMESPACE

#endif
