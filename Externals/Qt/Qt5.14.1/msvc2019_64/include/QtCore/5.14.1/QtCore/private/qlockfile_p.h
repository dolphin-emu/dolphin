/****************************************************************************
**
** Copyright (C) 2013 David Faure <faure+bluesystems@kde.org>
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

#ifndef QLOCKFILE_P_H
#define QLOCKFILE_P_H

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

#include <QtCore/private/qglobal_p.h>
#include <QtCore/qlockfile.h>
#include <QtCore/qfile.h>

#include <qplatformdefs.h>

#ifdef Q_OS_WIN
#include <io.h>
#include <qt_windows.h>
#endif

QT_BEGIN_NAMESPACE

class QLockFilePrivate
{
public:
    QLockFilePrivate(const QString &fn)
        : fileName(fn),
#ifdef Q_OS_WIN
          fileHandle(INVALID_HANDLE_VALUE),
#else
          fileHandle(-1),
#endif
          staleLockTime(30 * 1000), // 30 seconds
          lockError(QLockFile::NoError),
          isLocked(false)
    {
    }
    QLockFile::LockError tryLock_sys();
    bool removeStaleLock();
    QByteArray lockFileContents() const;
    // Returns \c true if the lock belongs to dead PID, or is old.
    // The attempt to delete it will tell us if it was really stale or not, though.
    bool isApparentlyStale() const;

    // used in dbusmenu
    Q_CORE_EXPORT static QString processNameByPid(qint64 pid);
    static bool isProcessRunning(qint64 pid, const QString &appname);

    QString fileName;
#ifdef Q_OS_WIN
    Qt::HANDLE fileHandle;
#else
    int fileHandle;
#endif
    int staleLockTime; // "int milliseconds" is big enough for 24 days
    QLockFile::LockError lockError;
    bool isLocked;

    static int getLockFileHandle(QLockFile *f)
    {
        int fd;
#ifdef Q_OS_WIN
        // Use of this function on Windows WILL leak a file descriptor.
        fd = _open_osfhandle(intptr_t(f->d_func()->fileHandle), 0);
#else
        fd = f->d_func()->fileHandle;
#endif
        QT_LSEEK(fd, 0, SEEK_SET);
        return fd;
    }
};

QT_END_NAMESPACE

#endif /* QLOCKFILE_P_H */
