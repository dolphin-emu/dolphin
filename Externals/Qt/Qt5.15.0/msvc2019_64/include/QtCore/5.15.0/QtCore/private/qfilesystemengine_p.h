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

#ifndef QFILESYSTEMENGINE_P_H
#define QFILESYSTEMENGINE_P_H

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

#include "qfile.h"
#include "qfilesystementry_p.h"
#include "qfilesystemmetadata_p.h"
#include <QtCore/private/qsystemerror_p.h>

QT_BEGIN_NAMESPACE

#define Q_RETURN_ON_INVALID_FILENAME(message, result) \
    { \
        QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC).warning(message); \
        errno = EINVAL; \
        return (result); \
    }

inline bool qIsFilenameBroken(const QByteArray &name)
{
    return name.contains('\0');
}

inline bool qIsFilenameBroken(const QString &name)
{
    return name.contains(QLatin1Char('\0'));
}

inline bool qIsFilenameBroken(const QFileSystemEntry &entry)
{
    return qIsFilenameBroken(entry.nativeFilePath());
}

#define Q_CHECK_FILE_NAME(name, result) \
    do { \
        if (Q_UNLIKELY((name).isEmpty())) \
            Q_RETURN_ON_INVALID_FILENAME("Empty filename passed to function", (result)); \
        if (Q_UNLIKELY(qIsFilenameBroken(name))) \
            Q_RETURN_ON_INVALID_FILENAME("Broken filename passed to function", (result)); \
    } while (false)

class Q_AUTOTEST_EXPORT QFileSystemEngine
{
public:
    static bool isCaseSensitive()
    {
#ifndef Q_OS_WIN
        return true;
#else
        return false;
#endif
    }

    static QFileSystemEntry getLinkTarget(const QFileSystemEntry &link, QFileSystemMetaData &data);
    static QFileSystemEntry canonicalName(const QFileSystemEntry &entry, QFileSystemMetaData &data);
    static QFileSystemEntry absoluteName(const QFileSystemEntry &entry);
    static QByteArray id(const QFileSystemEntry &entry);
    static QString resolveUserName(const QFileSystemEntry &entry, QFileSystemMetaData &data);
    static QString resolveGroupName(const QFileSystemEntry &entry, QFileSystemMetaData &data);

#if defined(Q_OS_UNIX)
    static QString resolveUserName(uint userId);
    static QString resolveGroupName(uint groupId);
#endif

#if defined(Q_OS_DARWIN)
    static QString bundleName(const QFileSystemEntry &entry);
#else
    static QString bundleName(const QFileSystemEntry &entry) { Q_UNUSED(entry) return QString(); }
#endif

    static bool fillMetaData(const QFileSystemEntry &entry, QFileSystemMetaData &data,
                             QFileSystemMetaData::MetaDataFlags what);
#if defined(Q_OS_UNIX)
    static bool cloneFile(int srcfd, int dstfd, const QFileSystemMetaData &knownData);
    static bool fillMetaData(int fd, QFileSystemMetaData &data); // what = PosixStatFlags
    static QByteArray id(int fd);
    static bool setFileTime(int fd, const QDateTime &newDate,
                            QAbstractFileEngine::FileTime whatTime, QSystemError &error);
    static bool setPermissions(int fd, QFile::Permissions permissions, QSystemError &error,
                               QFileSystemMetaData *data = nullptr);
#endif
#if defined(Q_OS_WIN)

    static bool uncListSharesOnServer(const QString &server, QStringList *list); //Used also by QFSFileEngineIterator::hasNext()
    static bool fillMetaData(int fd, QFileSystemMetaData &data,
                             QFileSystemMetaData::MetaDataFlags what);
    static bool fillMetaData(HANDLE fHandle, QFileSystemMetaData &data,
                             QFileSystemMetaData::MetaDataFlags what);
    static bool fillPermissions(const QFileSystemEntry &entry, QFileSystemMetaData &data,
                                QFileSystemMetaData::MetaDataFlags what);
    static QByteArray id(HANDLE fHandle);
    static bool setFileTime(HANDLE fHandle, const QDateTime &newDate,
                            QAbstractFileEngine::FileTime whatTime, QSystemError &error);
    static QString owner(const QFileSystemEntry &entry, QAbstractFileEngine::FileOwner own);
    static QString nativeAbsoluteFilePath(const QString &path);
#endif
    //homePath, rootPath and tempPath shall return clean paths
    static QString homePath();
    static QString rootPath();
    static QString tempPath();

    static bool createDirectory(const QFileSystemEntry &entry, bool createParents);
    static bool removeDirectory(const QFileSystemEntry &entry, bool removeEmptyParents);

    static bool createLink(const QFileSystemEntry &source, const QFileSystemEntry &target, QSystemError &error);

    static bool copyFile(const QFileSystemEntry &source, const QFileSystemEntry &target, QSystemError &error);
    static bool moveFileToTrash(const QFileSystemEntry &source, QFileSystemEntry &newLocation, QSystemError &error);
    static bool renameFile(const QFileSystemEntry &source, const QFileSystemEntry &target, QSystemError &error);
    static bool renameOverwriteFile(const QFileSystemEntry &source, const QFileSystemEntry &target, QSystemError &error);
    static bool removeFile(const QFileSystemEntry &entry, QSystemError &error);

    static bool setPermissions(const QFileSystemEntry &entry, QFile::Permissions permissions, QSystemError &error,
                               QFileSystemMetaData *data = nullptr);

    // unused, therefore not implemented
    static bool setFileTime(const QFileSystemEntry &entry, const QDateTime &newDate,
                            QAbstractFileEngine::FileTime whatTime, QSystemError &error);

    static bool setCurrentPath(const QFileSystemEntry &entry);
    static QFileSystemEntry currentPath();

    static QAbstractFileEngine *resolveEntryAndCreateLegacyEngine(QFileSystemEntry &entry,
                                                                  QFileSystemMetaData &data);
private:
    static QString slowCanonicalized(const QString &path);
#if defined(Q_OS_WIN)
    static void clearWinStatData(QFileSystemMetaData &data);
#endif
};

QT_END_NAMESPACE

#endif // include guard
