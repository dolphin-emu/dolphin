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

#ifndef QHOSTINFO_P_H
#define QHOSTINFO_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of the QHostInfo class.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include <QtNetwork/private/qtnetworkglobal_p.h>
#include "QtCore/qcoreapplication.h"
#include "private/qcoreapplication_p.h"
#include "private/qmetaobject_p.h"
#include "QtNetwork/qhostinfo.h"
#include "QtCore/qmutex.h"
#include "QtCore/qwaitcondition.h"
#include "QtCore/qobject.h"
#include "QtCore/qpointer.h"
#include "QtCore/qthread.h"
#if QT_CONFIG(thread)
#include "QtCore/qthreadpool.h"
#endif
#include "QtCore/qrunnable.h"
#include "QtCore/qlist.h"
#include "QtCore/qqueue.h"
#include <QElapsedTimer>
#include <QCache>

#include <QNetworkSession>
#include <QSharedPointer>

#include <atomic>

QT_BEGIN_NAMESPACE


class QHostInfoResult : public QObject
{
    Q_OBJECT
public:
    QHostInfoResult(const QObject *receiver, QtPrivate::QSlotObjectBase *slotObj)
        : receiver(receiver), slotObj(slotObj),
          withContextObject(slotObj && receiver)
    {
        if (receiver)
            moveToThread(receiver->thread());
    }

    void postResultsReady(const QHostInfo &info);

Q_SIGNALS:
    void resultsReady(const QHostInfo &info);

protected:
    bool event(QEvent *event) override;

private:
    QHostInfoResult(const QHostInfoResult *other)
        : receiver(other->receiver), slotObj(other->slotObj),
          withContextObject(other->withContextObject)
    {
        // cleanup if the application terminates before results are delivered
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
                this, &QObject::deleteLater);
        // maintain thread affinity
        moveToThread(other->thread());
    }

    QPointer<const QObject> receiver = nullptr;
    QtPrivate::QSlotObjectBase *slotObj = nullptr;
    const bool withContextObject = false;
};

class QHostInfoAgent
{
public:
    static QHostInfo fromName(const QString &hostName);
private:
    static QHostInfo lookup(const QString &hostName);
    static QHostInfo reverseLookup(const QHostAddress &address);
};

class QHostInfoPrivate
{
public:
    inline QHostInfoPrivate()
        : err(QHostInfo::NoError),
          errorStr(QLatin1String(QT_TRANSLATE_NOOP("QHostInfo", "Unknown error"))),
          lookupId(0)
    {
    }
    static int lookupHostImpl(const QString &name,
                              const QObject *receiver,
                              QtPrivate::QSlotObjectBase *slotObj,
                              const char *member);

    QHostInfo::HostInfoError err;
    QString errorStr;
    QList<QHostAddress> addrs;
    QString hostName;
    int lookupId;
};

// These functions are outside of the QHostInfo class and strictly internal.
// Do NOT use them outside of QAbstractSocket.
QHostInfo Q_NETWORK_EXPORT qt_qhostinfo_lookup(const QString &name, QObject *receiver, const char *member, bool *valid, int *id);
void Q_AUTOTEST_EXPORT qt_qhostinfo_clear_cache();
void Q_AUTOTEST_EXPORT qt_qhostinfo_enable_cache(bool e);
void Q_AUTOTEST_EXPORT qt_qhostinfo_cache_inject(const QString &hostname, const QHostInfo &resolution);

class QHostInfoCache
{
public:
    QHostInfoCache();
    const int max_age; // seconds

    QHostInfo get(const QString &name, bool *valid);
    void put(const QString &name, const QHostInfo &info);
    void clear();

    bool isEnabled() { return enabled.load(std::memory_order_relaxed); }
    // this function is currently only used for the auto tests
    // and not usable by public API
    void setEnabled(bool e) { enabled.store(e, std::memory_order_relaxed); }
private:
    std::atomic<bool> enabled;
    struct QHostInfoCacheElement {
        QHostInfo info;
        QElapsedTimer age;
    };
    QCache<QString,QHostInfoCacheElement> cache;
    QMutex mutex;
};

// the following classes are used for the (normal) case: We use multiple threads to lookup DNS

class QHostInfoRunnable : public QRunnable
{
public:
    QHostInfoRunnable(const QString &hn, int i, const QObject *receiver,
                      QtPrivate::QSlotObjectBase *slotObj);
    void run() override;

    QString toBeLookedUp;
    int id;
    QHostInfoResult resultEmitter;
};


class QHostInfoLookupManager
{
public:
    QHostInfoLookupManager();
    ~QHostInfoLookupManager();

    void clear();

    // called from QHostInfo
    void scheduleLookup(QHostInfoRunnable *r);
    void abortLookup(int id);

    // called from QHostInfoRunnable
    void lookupFinished(QHostInfoRunnable *r);
    bool wasAborted(int id);

    QHostInfoCache cache;

    friend class QHostInfoRunnable;
protected:
#if QT_CONFIG(thread)
    QList<QHostInfoRunnable*> currentLookups; // in progress
    QList<QHostInfoRunnable*> postponedLookups; // postponed because in progress for same host
#endif
    QQueue<QHostInfoRunnable*> scheduledLookups; // not yet started
    QList<QHostInfoRunnable*> finishedLookups; // recently finished
    QList<int> abortedLookups; // ids of aborted lookups

#if QT_CONFIG(thread)
    QThreadPool threadPool;
#endif
    QMutex mutex;

    bool wasDeleted;

private:
    void rescheduleWithMutexHeld();
};

QT_END_NAMESPACE

#endif // QHOSTINFO_P_H
