/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Olivier Goffart <ogoffart@woboq.com>
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

#ifndef QREADWRITELOCK_P_H
#define QREADWRITELOCK_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of the implementation.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/private/qglobal_p.h>
#include <QtCore/qhash.h>
#include <QtCore/qwaitcondition.h>

QT_REQUIRE_CONFIG(thread);

QT_BEGIN_NAMESPACE

class QReadWriteLockPrivate
{
public:
    explicit QReadWriteLockPrivate(bool isRecursive = false)
        : recursive(isRecursive) {}

    QMutex mutex;
    QWaitCondition writerCond;
    QWaitCondition readerCond;
    int readerCount = 0;
    int writerCount = 0;
    int waitingReaders = 0;
    int waitingWriters = 0;
    const bool recursive;

    //Called with the mutex locked
    bool lockForWrite(int timeout);
    bool lockForRead(int timeout);
    void unlock();

    //memory management
    int id = 0;
    void release();
    static QReadWriteLockPrivate *allocate();

    // Recusive mutex handling
    Qt::HANDLE currentWriter = {};
    QHash<Qt::HANDLE, int> currentReaders;

    // called with the mutex unlocked
    bool recursiveLockForWrite(int timeout);
    bool recursiveLockForRead(int timeout);
    void recursiveUnlock();
};

QT_END_NAMESPACE

#endif // QREADWRITELOCK_P_H
