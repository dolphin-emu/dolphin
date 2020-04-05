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

#ifndef QFILESYSTEMWATCHER_FSEVENTS_P_H
#define QFILESYSTEMWATCHER_FSEVENTS_P_H

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

#include <QtCore/qmutex.h>
#include <QtCore/qhash.h>
#include <QtCore/qthread.h>
#include <QtCore/qvector.h>
#include <QtCore/qsocketnotifier.h>

#include <dispatch/dispatch.h>
#include <CoreServices/CoreServices.h>

QT_REQUIRE_CONFIG(filesystemwatcher);

QT_BEGIN_NAMESPACE

class QFseventsFileSystemWatcherEngine : public QFileSystemWatcherEngine
{
    Q_OBJECT
public:
    ~QFseventsFileSystemWatcherEngine();

    static QFseventsFileSystemWatcherEngine *create(QObject *parent);

    QStringList addPaths(const QStringList &paths, QStringList *files, QStringList *directories);
    QStringList removePaths(const QStringList &paths, QStringList *files, QStringList *directories);

    void processEvent(ConstFSEventStreamRef streamRef, size_t numEvents, char **eventPaths, const FSEventStreamEventFlags eventFlags[], const FSEventStreamEventId eventIds[]);

Q_SIGNALS:
    void emitFileChanged(const QString &path, bool removed);
    void emitDirectoryChanged(const QString &path, bool removed);
    void scheduleStreamRestart();

private slots:
    void doEmitFileChanged(const QString &path, bool removed);
    void doEmitDirectoryChanged(const QString &path, bool removed);
    bool restartStream();

private:
    struct Info {
        QString origPath;
        timespec ctime;
        mode_t mode;
        QString watchedPath;

        Info(): mode(0)
        {
            ctime.tv_sec = 0;
            ctime.tv_nsec = 0;
        }

        Info(const QString &origPath, const timespec &ctime, mode_t mode, const QString &watchedPath)
            : origPath(origPath)
            , ctime(ctime)
            , mode(mode)
            , watchedPath(watchedPath)
        {}
    };
    typedef QHash<QString, Info> InfoByName;
    typedef QHash<QString, InfoByName> FilesByPath;
    struct DirInfo {
        Info dirInfo;
        InfoByName entries;
    };
    typedef QHash<QString, DirInfo> DirsByName;
    typedef QHash<QString, qint64> PathRefCounts;

    struct WatchingState {
        // These fields go hand-in-hand. FSEvents watches paths, and there is no use in watching
        // the same path multiple times. So, the "refcount" on a path is the number of watched
        // files that have the same path, plus the number of directories that have the same path.
        //
        // If the stream fails to start after adding files/directories, the watcher will try to
        // keep watching files/directories that it was already watching. It does that by restoring
        // the previous WatchingState and restarting the stream.
        FilesByPath watchedFiles;
        DirsByName watchedDirectories;
        PathRefCounts watchedPaths;
    };

    QFseventsFileSystemWatcherEngine(QObject *parent);
    bool startStream();
    void stopStream(bool isStopped = false);
    InfoByName scanForDirEntries(const QString &path);
    bool derefPath(const QString &watchedPath);
    bool checkDir(DirsByName::iterator &it);
    bool rescanDirs(const QString &path);
    bool rescanFiles(InfoByName &filesInPath);
    bool rescanFiles(const QString &path);

    QMutex lock;
    dispatch_queue_t queue;
    FSEventStreamRef stream;
    FSEventStreamEventId lastReceivedEvent;
    WatchingState watchingState;
};

QT_END_NAMESPACE

#endif // QFILESYSTEMWATCHER_FSEVENTS_P_H
