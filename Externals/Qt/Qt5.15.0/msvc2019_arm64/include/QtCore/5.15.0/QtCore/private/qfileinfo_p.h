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

#ifndef QFILEINFO_P_H
#define QFILEINFO_P_H

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

#include "qfileinfo.h"
#include "qdatetime.h"
#include "qatomic.h"
#include "qshareddata.h"
#include "qfilesystemengine_p.h"

#include <QtCore/private/qabstractfileengine_p.h>
#include <QtCore/private/qfilesystementry_p.h>
#include <QtCore/private/qfilesystemmetadata_p.h>

#include <memory>

QT_BEGIN_NAMESPACE

class QFileInfoPrivate : public QSharedData
{
public:
    enum {
        // note: cachedFlags is only 30-bits wide
        CachedFileFlags         = 0x01,
        CachedLinkTypeFlag      = 0x02,
        CachedBundleTypeFlag    = 0x04,
        CachedSize              = 0x08,
        CachedATime             = 0x10,
        CachedBTime             = 0x20,
        CachedMCTime            = 0x40,
        CachedMTime             = 0x80,
        CachedPerms             = 0x100
    };

    inline QFileInfoPrivate()
        : QSharedData(), fileEngine(nullptr),
        cachedFlags(0),
        isDefaultConstructed(true),
        cache_enabled(true), fileFlags(0), fileSize(0)
    {}
    inline QFileInfoPrivate(const QFileInfoPrivate &copy)
        : QSharedData(copy),
        fileEntry(copy.fileEntry),
        metaData(copy.metaData),
        fileEngine(QFileSystemEngine::resolveEntryAndCreateLegacyEngine(fileEntry, metaData)),
        cachedFlags(0),
#ifndef QT_NO_FSFILEENGINE
        isDefaultConstructed(false),
#else
        isDefaultConstructed(!fileEngine),
#endif
        cache_enabled(copy.cache_enabled), fileFlags(0), fileSize(0)
    {}
    inline QFileInfoPrivate(const QString &file)
        : fileEntry(QDir::fromNativeSeparators(file)),
        fileEngine(QFileSystemEngine::resolveEntryAndCreateLegacyEngine(fileEntry, metaData)),
        cachedFlags(0),
#ifndef QT_NO_FSFILEENGINE
        isDefaultConstructed(file.isEmpty()),
#else
        isDefaultConstructed(!fileEngine),
#endif
        cache_enabled(true), fileFlags(0), fileSize(0)
    {
    }

    inline QFileInfoPrivate(const QFileSystemEntry &file, const QFileSystemMetaData &data)
        : QSharedData(),
        fileEntry(file),
        metaData(data),
        fileEngine(QFileSystemEngine::resolveEntryAndCreateLegacyEngine(fileEntry, metaData)),
        cachedFlags(0),
        isDefaultConstructed(false),
        cache_enabled(true), fileFlags(0), fileSize(0)
    {
        //If the file engine is not null, this maybe a "mount point" for a custom file engine
        //in which case we can't trust the metadata
        if (fileEngine)
            metaData = QFileSystemMetaData();
    }

    inline QFileInfoPrivate(const QFileSystemEntry &file, const QFileSystemMetaData &data, std::unique_ptr<QAbstractFileEngine> engine)
        : fileEntry(file),
        metaData(data),
        fileEngine{std::move(engine)},
        cachedFlags(0),
#ifndef QT_NO_FSFILEENGINE
        isDefaultConstructed(false),
#else
        isDefaultConstructed(!fileEngine),
#endif
        cache_enabled(true), fileFlags(0), fileSize(0)
    {
    }

    inline void clearFlags() const {
        fileFlags = 0;
        cachedFlags = 0;
        if (fileEngine)
            (void)fileEngine->fileFlags(QAbstractFileEngine::Refresh);
    }
    inline void clear() {
        metaData.clear();
        clearFlags();
        for (int i = QAbstractFileEngine::NFileNames - 1 ; i >= 0 ; --i)
            fileNames[i].clear();
        fileOwners[1].clear();
        fileOwners[0].clear();
    }

    uint getFileFlags(QAbstractFileEngine::FileFlags) const;
    QDateTime &getFileTime(QAbstractFileEngine::FileTime) const;
    QString getFileName(QAbstractFileEngine::FileName) const;
    QString getFileOwner(QAbstractFileEngine::FileOwner own) const;

    QFileSystemEntry fileEntry;
    mutable QFileSystemMetaData metaData;

    std::unique_ptr<QAbstractFileEngine> const fileEngine;

    mutable QString fileNames[QAbstractFileEngine::NFileNames];
    mutable QString fileOwners[2];  // QAbstractFileEngine::FileOwner: OwnerUser and OwnerGroup
    mutable QDateTime fileTimes[4]; // QAbstractFileEngine::FileTime: BirthTime, MetadataChangeTime, ModificationTime, AccessTime

    mutable uint cachedFlags : 30;
    bool const isDefaultConstructed : 1; // QFileInfo is a default constructed instance
    bool cache_enabled : 1;
    mutable uint fileFlags;
    mutable qint64 fileSize;
    inline bool getCachedFlag(uint c) const
    { return cache_enabled ? (cachedFlags & c) : 0; }
    inline void setCachedFlag(uint c) const
    { if (cache_enabled) cachedFlags |= c; }

    template <typename Ret, typename FSLambda, typename EngineLambda>
    Ret checkAttribute(Ret defaultValue, QFileSystemMetaData::MetaDataFlags fsFlags, const FSLambda &fsLambda,
                       const EngineLambda &engineLambda) const
    {
        if (isDefaultConstructed)
            return defaultValue;
        if (fileEngine)
            return engineLambda();
        if (!cache_enabled || !metaData.hasFlags(fsFlags)) {
            QFileSystemEngine::fillMetaData(fileEntry, metaData, fsFlags);
            // ignore errors, fillMetaData will have cleared the flags
        }
        return fsLambda();
    }

    template <typename Ret, typename FSLambda, typename EngineLambda>
    Ret checkAttribute(QFileSystemMetaData::MetaDataFlags fsFlags, const FSLambda &fsLambda,
                       const EngineLambda &engineLambda) const
    {
        return checkAttribute(Ret(), fsFlags, fsLambda, engineLambda);
    }
};

QT_END_NAMESPACE

#endif // QFILEINFO_P_H
