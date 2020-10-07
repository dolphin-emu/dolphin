/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#ifndef QQMLDATABLOB_P_H
#define QQMLDATABLOB_P_H

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

#include <private/qqmlrefcount_p.h>
#include <private/qqmljsdiagnosticmessage_p.h>
#include <private/qv4compileddata_p.h>

#if QT_CONFIG(qml_network)
#include <QtNetwork/qnetworkreply.h>
#endif

#include <QtQml/qqmlerror.h>
#include <QtQml/qqmlabstracturlinterceptor.h>

#include <QtCore/qdatetime.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qurl.h>

QT_BEGIN_NAMESPACE

class QQmlTypeLoader;
class Q_QML_PRIVATE_EXPORT QQmlDataBlob : public QQmlRefCount
{
public:
    enum Status {
        Null,                    // Prior to QQmlTypeLoader::load()
        Loading,                 // Prior to data being received and dataReceived() being called
        WaitingForDependencies,  // While there are outstanding addDependency()s
        ResolvingDependencies,   // While resolving outstanding dependencies, to detect cycles
        Complete,                // Finished
        Error                    // Error
    };

    enum Type { //Matched in QQmlAbstractUrlInterceptor
        QmlFile = QQmlAbstractUrlInterceptor::QmlFile,
        JavaScriptFile = QQmlAbstractUrlInterceptor::JavaScriptFile,
        QmldirFile = QQmlAbstractUrlInterceptor::QmldirFile
    };

    QQmlDataBlob(const QUrl &, Type, QQmlTypeLoader* manager);
    ~QQmlDataBlob() override;

    void startLoading();

    QQmlTypeLoader *typeLoader() const { return m_typeLoader; }

    Type type() const;

    Status status() const;
    bool isNull() const;
    bool isLoading() const;
    bool isWaiting() const;
    bool isComplete() const;
    bool isError() const;
    bool isCompleteOrError() const;

    qreal progress() const;

    QUrl url() const;
    QString urlString() const;
    QUrl finalUrl() const;
    QString finalUrlString() const;

    QList<QQmlError> errors() const;

    class SourceCodeData {
    public:
        QString readAll(QString *error) const;
        QDateTime sourceTimeStamp() const;
        bool exists() const;
        bool isEmpty() const;
    private:
        friend class QQmlDataBlob;
        friend class QQmlTypeLoader;
        QString inlineSourceCode;
        QFileInfo fileInfo;
        bool hasInlineSourceCode = false;
    };

protected:
    // Can be called from within callbacks
    void setError(const QQmlError &);
    void setError(const QList<QQmlError> &errors);
    void setError(const QQmlJS::DiagnosticMessage &error);
    void setError(const QVector<QQmlError> &errors);
    void setError(const QString &description);
    void addDependency(QQmlDataBlob *);

    // Callbacks made in load thread
    virtual void dataReceived(const SourceCodeData &) = 0;
    virtual void initializeFromCachedUnit(const QV4::CompiledData::Unit*) = 0;
    virtual void done();
#if QT_CONFIG(qml_network)
    virtual void networkError(QNetworkReply::NetworkError);
#endif
    virtual void dependencyError(QQmlDataBlob *);
    virtual void dependencyComplete(QQmlDataBlob *);
    virtual void allDependenciesDone();

    // Callbacks made in main thread
    virtual void downloadProgressChanged(qreal);
    virtual void completed();

protected:
    // Manager that is currently fetching data for me
    QQmlTypeLoader *m_typeLoader;

private:
    friend class QQmlTypeLoader;
    friend class QQmlTypeLoaderThread;

    void tryDone();
    void cancelAllWaitingFor();
    void notifyAllWaitingOnMe();
    void notifyComplete(QQmlDataBlob *);

    struct ThreadData {
    private:
        enum {
            StatusMask = 0x0000FFFF,
            StatusShift = 0,
            ProgressMask = 0x00FF0000,
            ProgressShift = 16,
            AsyncMask = 0x80000000,
            NoMask = 0
        };

    public:
        inline ThreadData()
            : _p(0)
        {
        }

        inline QQmlDataBlob::Status status() const
        {
            return QQmlDataBlob::Status((_p.loadRelaxed() & StatusMask) >> StatusShift);
        }

        inline void setStatus(QQmlDataBlob::Status status)
        {
            while (true) {
                int d = _p.loadRelaxed();
                int nd = (d & ~StatusMask) | ((status << StatusShift) & StatusMask);
                if (d == nd || _p.testAndSetOrdered(d, nd)) return;
            }
        }

        inline bool isAsync() const
        {
            return _p.loadRelaxed() & AsyncMask;
        }

        inline void setIsAsync(bool v)
        {
            while (true) {
                int d = _p.loadRelaxed();
                int nd = (d & ~AsyncMask) | (v ? AsyncMask : NoMask);
                if (d == nd || _p.testAndSetOrdered(d, nd)) return;
            }
        }

        inline quint8 progress() const
        {
            return quint8((_p.loadRelaxed() & ProgressMask) >> ProgressShift);
        }

        inline void setProgress(quint8 v)
        {
            while (true) {
                int d = _p.loadRelaxed();
                int nd = (d & ~ProgressMask) | ((v << ProgressShift) & ProgressMask);
                if (d == nd || _p.testAndSetOrdered(d, nd)) return;
            }
        }

    private:
        QAtomicInt _p;
    };
    ThreadData m_data;

    // m_errors should *always* be written before the status is set to Error.
    // We use the status change as a memory fence around m_errors so that locking
    // isn't required.  Once the status is set to Error (or Complete), m_errors
    // cannot be changed.
    QList<QQmlError> m_errors;

    Type m_type;

    QUrl m_url;
    QUrl m_finalUrl;
    mutable QString m_urlString;
    mutable QString m_finalUrlString;

    // List of QQmlDataBlob's that are waiting for me to complete.
protected:
    QList<QQmlDataBlob *> m_waitingOnMe;
private:

    // List of QQmlDataBlob's that I am waiting for to complete.
    QVector<QQmlRefPointer<QQmlDataBlob>> m_waitingFor;

    int m_redirectCount:30;
    bool m_inCallback:1;
    bool m_isDone:1;
};

QT_END_NAMESPACE

#endif // QQMLDATABLOB_P_H
