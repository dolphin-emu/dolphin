/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#ifndef QFILESYSTEMWATCHER_WIN_P_H
#define QFILESYSTEMWATCHER_WIN_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qfilesystemwatcher_p.h"

#include <QtCore/qdatetime.h>
#include <QtCore/qthread.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qhash.h>
#include <QtCore/qmutex.h>
#include <QtCore/qvector.h>

QT_BEGIN_NAMESPACE

class QWindowsFileSystemWatcherEngineThread;
class QWindowsRemovableDriveListener;

// Even though QWindowsFileSystemWatcherEngine is derived of QThread
// via QFileSystemWatcher, it does not start a thread.
// Instead QWindowsFileSystemWatcher creates QWindowsFileSystemWatcherEngineThreads
// to do the actually watching.
class QWindowsFileSystemWatcherEngine : public QFileSystemWatcherEngine
{
    Q_OBJECT
public:
    explicit QWindowsFileSystemWatcherEngine(QObject *parent);
    ~QWindowsFileSystemWatcherEngine();

    QStringList addPaths(const QStringList &paths, QStringList *files, QStringList *directories) override;
    QStringList removePaths(const QStringList &paths, QStringList *files, QStringList *directories) override;

    class Handle
    {
    public:
        Qt::HANDLE handle;
        uint flags;

        Handle();
    };

    class PathInfo {
    public:
        QString absolutePath;
        QString path;
        bool isDir;

        // fileinfo bits
        uint ownerId;
        uint groupId;
        QFile::Permissions permissions;
        QDateTime lastModified;

        PathInfo &operator=(const QFileInfo &fileInfo)
                           {
            ownerId = fileInfo.ownerId();
            groupId = fileInfo.groupId();
            permissions = fileInfo.permissions();
            lastModified = fileInfo.lastModified();
            return *this;
        }

        bool operator!=(const QFileInfo &fileInfo) const
        {
            return (ownerId != fileInfo.ownerId()
                    || groupId != fileInfo.groupId()
                    || permissions != fileInfo.permissions()
                    || lastModified != fileInfo.lastModified());
        }
    };

signals:
    void driveLockForRemoval(const QString &);
    void driveLockForRemovalFailed(const QString &);
    void driveRemoved(const QString &);

private:
    QList<QWindowsFileSystemWatcherEngineThread *> threads;
#ifndef Q_OS_WINRT
    QWindowsRemovableDriveListener *m_driveListener = nullptr;
#endif
};

class QFileSystemWatcherPathKey : public QString
{
public:
    QFileSystemWatcherPathKey() {}
    explicit QFileSystemWatcherPathKey(const QString &other) : QString(other) {}
    QFileSystemWatcherPathKey(const QFileSystemWatcherPathKey &other) : QString(other) {}
    bool operator==(const QFileSystemWatcherPathKey &other) const { return !compare(other, Qt::CaseInsensitive); }
};

Q_DECLARE_TYPEINFO(QFileSystemWatcherPathKey, Q_MOVABLE_TYPE);

inline uint qHash(const QFileSystemWatcherPathKey &key) { return qHash(key.toCaseFolded()); }

class QWindowsFileSystemWatcherEngineThread : public QThread
{
    Q_OBJECT

public:
    typedef QHash<QFileSystemWatcherPathKey, QWindowsFileSystemWatcherEngine::Handle> HandleForDirHash;
    typedef QHash<QFileSystemWatcherPathKey, QWindowsFileSystemWatcherEngine::PathInfo> PathInfoHash;

    QWindowsFileSystemWatcherEngineThread();
    ~QWindowsFileSystemWatcherEngineThread();
    void run() override;
    void stop();
    void wakeup();

    QMutex mutex;
    QVector<Qt::HANDLE> handles;
    int msg;

    HandleForDirHash handleForDir;

    QHash<Qt::HANDLE, PathInfoHash> pathInfoForHandle;

Q_SIGNALS:
    void fileChanged(const QString &path, bool removed);
    void directoryChanged(const QString &path, bool removed);
};

QT_END_NAMESPACE

#endif // QFILESYSTEMWATCHER_WIN_P_H
