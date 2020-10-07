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

#ifndef QFILESYSTEMENTRY_P_H
#define QFILESYSTEMENTRY_P_H

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
#include <QtCore/qstring.h>
#include <QtCore/qbytearray.h>

#if defined(Q_OS_WIN)
#define QFILESYSTEMENTRY_NATIVE_PATH_IS_UTF16
#endif

QT_BEGIN_NAMESPACE

class QFileSystemEntry
{
public:

#ifndef QFILESYSTEMENTRY_NATIVE_PATH_IS_UTF16
    typedef QByteArray NativePath;
#else
    typedef QString NativePath;
#endif
    struct FromNativePath{};
    struct FromInternalPath{};

    QFileSystemEntry();
    explicit QFileSystemEntry(const QString &filePath);

    QFileSystemEntry(const QString &filePath, FromInternalPath dummy);
    QFileSystemEntry(const NativePath &nativeFilePath, FromNativePath dummy);
    QFileSystemEntry(const QString &filePath, const NativePath &nativeFilePath);

    QString filePath() const;
    QString fileName() const;
    QString path() const;
    NativePath nativeFilePath() const;
    QString baseName() const;
    QString completeBaseName() const;
    QString suffix() const;
    QString completeSuffix() const;
    bool isAbsolute() const;
    bool isRelative() const;
    bool isClean() const;

#if defined(Q_OS_WIN)
    bool isDriveRoot() const;
    static bool isDriveRootPath(const QString &path);
#endif
    bool isRoot() const;

    bool isEmpty() const
    {
        return m_filePath.isEmpty() && m_nativeFilePath.isEmpty();
    }
    void clear()
    {
        *this = QFileSystemEntry();
    }

    Q_CORE_EXPORT static bool isRootPath(const QString &path);

private:
    // creates the QString version out of the bytearray version
    void resolveFilePath() const;
    // creates the bytearray version out of the QString version
    void resolveNativeFilePath() const;
    // resolves the separator
    void findLastSeparator() const;
    // resolves the dots and the separator
    void findFileNameSeparators() const;

    mutable QString m_filePath; // always has slashes as separator
    mutable NativePath m_nativeFilePath; // native encoding and separators

    mutable qint16 m_lastSeparator; // index in m_filePath of last separator
    mutable qint16 m_firstDotInFileName; // index after m_filePath for first dot (.)
    mutable qint16 m_lastDotInFileName; // index after m_firstDotInFileName for last dot (.)
};

QT_END_NAMESPACE

#endif // include guard
