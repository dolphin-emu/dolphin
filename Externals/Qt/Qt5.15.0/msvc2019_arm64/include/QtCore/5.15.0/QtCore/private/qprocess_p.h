/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Intel Corporation.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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

#ifndef QPROCESS_P_H
#define QPROCESS_P_H

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

#include "QtCore/qprocess.h"
#include "QtCore/qstringlist.h"
#include "QtCore/qhash.h"
#include "QtCore/qmap.h"
#include "QtCore/qshareddata.h"
#include "private/qiodevice_p.h"

QT_REQUIRE_CONFIG(processenvironment);

#ifdef Q_OS_UNIX
#include <QtCore/private/qorderedmutexlocker_p.h>
#endif

#ifdef Q_OS_WIN
#include "QtCore/qt_windows.h"
typedef HANDLE Q_PIPE;
#define INVALID_Q_PIPE INVALID_HANDLE_VALUE
#else
typedef int Q_PIPE;
#define INVALID_Q_PIPE -1
#endif

QT_BEGIN_NAMESPACE

class QSocketNotifier;
class QWindowsPipeReader;
class QWindowsPipeWriter;
class QWinEventNotifier;
class QTimer;

#ifdef Q_OS_WIN
class QProcEnvKey : public QString
{
public:
    QProcEnvKey() {}
    explicit QProcEnvKey(const QString &other) : QString(other) {}
    QProcEnvKey(const QProcEnvKey &other) : QString(other) {}
    bool operator==(const QProcEnvKey &other) const { return !compare(other, Qt::CaseInsensitive); }
};

inline bool operator<(const QProcEnvKey &a, const QProcEnvKey &b)
{
    // On windows use case-insensitive ordering because that is how Windows needs the environment
    // block sorted (https://msdn.microsoft.com/en-us/library/windows/desktop/ms682009(v=vs.85).aspx)
    return a.compare(b, Qt::CaseInsensitive) < 0;
}

Q_DECLARE_TYPEINFO(QProcEnvKey, Q_MOVABLE_TYPE);

typedef QString QProcEnvValue;
#else
using QProcEnvKey = QByteArray;

class QProcEnvValue
{
public:
    QProcEnvValue() = default;
    explicit QProcEnvValue(const QString &value) : stringValue(value) {}
    explicit QProcEnvValue(const QByteArray &value) : byteValue(value) {}
    bool operator==(const QProcEnvValue &other) const
    {
        return byteValue.isEmpty() && other.byteValue.isEmpty()
                ? stringValue == other.stringValue
                : bytes() == other.bytes();
    }
    QByteArray bytes() const
    {
        if (byteValue.isEmpty() && !stringValue.isEmpty())
            byteValue = stringValue.toLocal8Bit();
        return byteValue;
    }
    QString string() const
    {
        if (stringValue.isEmpty() && !byteValue.isEmpty())
            stringValue = QString::fromLocal8Bit(byteValue);
        return stringValue;
    }

    mutable QByteArray byteValue;
    mutable QString stringValue;
};
Q_DECLARE_TYPEINFO(QProcEnvValue, Q_MOVABLE_TYPE);
#endif

class QProcessEnvironmentPrivate: public QSharedData
{
public:
    typedef QProcEnvKey Key;
    typedef QProcEnvValue Value;
#ifdef Q_OS_WIN
    inline Key prepareName(const QString &name) const { return Key(name); }
    inline QString nameToString(const Key &name) const { return name; }
    inline Value prepareValue(const QString &value) const { return value; }
    inline QString valueToString(const Value &value) const { return value; }
#else
    struct NameMapMutexLocker : public QMutexLocker
    {
        NameMapMutexLocker(const QProcessEnvironmentPrivate *d) : QMutexLocker(&d->nameMapMutex) {}
    };
    struct OrderedNameMapMutexLocker : public QOrderedMutexLocker
    {
        OrderedNameMapMutexLocker(const QProcessEnvironmentPrivate *d1,
                                  const QProcessEnvironmentPrivate *d2)
            : QOrderedMutexLocker(&d1->nameMapMutex, &d2->nameMapMutex)
        {}
    };

    inline Key prepareName(const QString &name) const
    {
        const NameMapMutexLocker locker(this);
        Key &ent = nameMap[name];
        if (ent.isEmpty())
            ent = name.toLocal8Bit();
        return ent;
    }
    inline QString nameToString(const Key &name) const
    {
        const QString sname = QString::fromLocal8Bit(name);
        {
            const NameMapMutexLocker locker(this);
            nameMap[sname] = name;
        }
        return sname;
    }
    inline Value prepareValue(const QString &value) const { return Value(value); }
    inline QString valueToString(const Value &value) const { return value.string(); }

    QProcessEnvironmentPrivate() : QSharedData() {}
    QProcessEnvironmentPrivate(const QProcessEnvironmentPrivate &other) :
        QSharedData(), vars(other.vars)
    {
        // We don't need to lock our own mutex, as this object is new and
        // consequently not shared. For the same reason, non-const methods
        // do not need a lock, as they detach objects (however, we need to
        // ensure that they really detach before using prepareName()).
        NameMapMutexLocker locker(&other);
        nameMap = other.nameMap;
        // We need to detach our nameMap, so that our mutex can protect it.
        // As we are being detached, it likely would be detached a moment later anyway.
        nameMap.detach();
    }
#endif

    using Map = QMap<Key, Value>;
    Map vars;

#ifdef Q_OS_UNIX
    typedef QHash<QString, Key> NameHash;
    mutable NameHash nameMap;
    mutable QMutex nameMapMutex;
#endif

    static QProcessEnvironment fromList(const QStringList &list);
    QStringList toList() const;
    QStringList keys() const;
    void insert(const QProcessEnvironmentPrivate &other);
};

template<> Q_INLINE_TEMPLATE void QSharedDataPointer<QProcessEnvironmentPrivate>::detach()
{
    if (d && d->ref.loadRelaxed() == 1)
        return;
    QProcessEnvironmentPrivate *x = (d ? new QProcessEnvironmentPrivate(*d)
                                     : new QProcessEnvironmentPrivate);
    x->ref.ref();
    if (d && !d->ref.deref())
        delete d;
    d = x;
}

#if QT_CONFIG(process)

class QProcessPrivate : public QIODevicePrivate
{
public:
    Q_DECLARE_PUBLIC(QProcess)

    struct Channel {
        enum ProcessChannelType {
            Normal = 0,
            PipeSource = 1,
            PipeSink = 2,
            Redirect = 3
            // if you add "= 4" here, increase the number of bits below
        };

        Channel() : process(nullptr), notifier(nullptr), type(Normal), closed(false), append(false)
        {
            pipe[0] = INVALID_Q_PIPE;
            pipe[1] = INVALID_Q_PIPE;
#ifdef Q_OS_WIN
            reader = 0;
#endif
        }

        void clear();

        Channel &operator=(const QString &fileName)
        {
            clear();
            file = fileName;
            type = fileName.isEmpty() ? Normal : Redirect;
            return *this;
        }

        void pipeTo(QProcessPrivate *other)
        {
            clear();
            process = other;
            type = PipeSource;
        }

        void pipeFrom(QProcessPrivate *other)
        {
            clear();
            process = other;
            type = PipeSink;
        }

        QString file;
        QProcessPrivate *process;
        QSocketNotifier *notifier;
#ifdef Q_OS_WIN
        union {
            QWindowsPipeReader *reader;
            QWindowsPipeWriter *writer;
        };
#endif
        Q_PIPE pipe[2];

        unsigned type : 2;
        bool closed : 1;
        bool append : 1;
    };

    QProcessPrivate();
    virtual ~QProcessPrivate();

    // private slots
    bool _q_canReadStandardOutput();
    bool _q_canReadStandardError();
    bool _q_canWrite();
    bool _q_startupNotification();
    bool _q_processDied();

    QProcess::ProcessChannelMode processChannelMode;
    QProcess::InputChannelMode inputChannelMode;
    QProcess::ProcessError processError;
    QProcess::ProcessState processState;
    QString workingDirectory;
    Q_PID pid;
    int sequenceNumber;

    bool dying;
    bool emittedReadyRead;
    bool emittedBytesWritten;

    Channel stdinChannel;
    Channel stdoutChannel;
    Channel stderrChannel;
    bool openChannel(Channel &channel);
    void closeChannel(Channel *channel);
    void closeWriteChannel();
    bool tryReadFromChannel(Channel *channel); // obviously, only stdout and stderr

    QString program;
    QStringList arguments;
#if defined(Q_OS_WIN)
    QString nativeArguments;
    QProcess::CreateProcessArgumentModifier modifyCreateProcessArgs;
#endif
    QProcessEnvironment environment;

    Q_PIPE childStartedPipe[2];
    void destroyPipe(Q_PIPE pipe[2]);

    QSocketNotifier *startupSocketNotifier;
    QSocketNotifier *deathNotifier;

    int forkfd;

#ifdef Q_OS_WIN
    QTimer *stdinWriteTrigger;
    QWinEventNotifier *processFinishedNotifier;
#endif

    void start(QIODevice::OpenMode mode);
    void startProcess();
#if defined(Q_OS_UNIX)
    void execChild(const char *workingDirectory, char **argv, char **envp);
#endif
    bool processStarted(QString *errorMessage = nullptr);
    void terminateProcess();
    void killProcess();
    void findExitCode();
#ifdef Q_OS_UNIX
    bool waitForDeadChild();
#endif
#ifdef Q_OS_WIN
    bool callCreateProcess(QProcess::CreateProcessArguments *cpargs);
    bool drainOutputPipes();
    void flushPipeWriter();
    qint64 pipeWriterBytesToWrite() const;
#endif

    bool startDetached(qint64 *pPid);

    int exitCode;
    QProcess::ExitStatus exitStatus;
    bool crashed;

    bool waitForStarted(int msecs = 30000);
    bool waitForReadyRead(int msecs = 30000);
    bool waitForBytesWritten(int msecs = 30000);
    bool waitForFinished(int msecs = 30000);

    qint64 bytesAvailableInChannel(const Channel *channel) const;
    qint64 readFromChannel(const Channel *channel, char *data, qint64 maxlen);
    bool writeToStdin();

    void cleanup();
    void setError(QProcess::ProcessError error, const QString &description = QString());
    void setErrorAndEmit(QProcess::ProcessError error, const QString &description = QString());
};

#endif // QT_CONFIG(process)

QT_END_NAMESPACE

#endif // QPROCESS_P_H
