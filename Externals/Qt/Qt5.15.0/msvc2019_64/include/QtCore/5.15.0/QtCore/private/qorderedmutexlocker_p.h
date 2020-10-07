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

#ifndef QORDEREDMUTEXLOCKER_P_H
#define QORDEREDMUTEXLOCKER_P_H

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
#include <QtCore/qmutex.h>

#include <functional>

QT_BEGIN_NAMESPACE

#if QT_CONFIG(thread)

/*
  Locks 2 mutexes in a defined order, avoiding a recursive lock if
  we're trying to lock the same mutex twice.
*/
class QOrderedMutexLocker
{
public:
    QOrderedMutexLocker(QBasicMutex *m1, QBasicMutex *m2)
        : mtx1((m1 == m2) ? m1 : (std::less<QBasicMutex *>()(m1, m2) ? m1 : m2)),
          mtx2((m1 == m2) ?  nullptr : (std::less<QBasicMutex *>()(m1, m2) ? m2 : m1)),
          locked(false)
    {
        relock();
    }
    ~QOrderedMutexLocker()
    {
        unlock();
    }

    void relock()
    {
        if (!locked) {
            if (mtx1) mtx1->lock();
            if (mtx2) mtx2->lock();
            locked = true;
        }
    }

    void unlock()
    {
        if (locked) {
            if (mtx2) mtx2->unlock();
            if (mtx1) mtx1->unlock();
            locked = false;
        }
    }

    static bool relock(QBasicMutex *mtx1, QBasicMutex *mtx2)
    {
        // mtx1 is already locked, mtx2 not... do we need to unlock and relock?
        if (mtx1 == mtx2)
            return false;
        if (std::less<QBasicMutex *>()(mtx1, mtx2)) {
            mtx2->lock();
            return true;
        }
        if (!mtx2->tryLock()) {
            mtx1->unlock();
            mtx2->lock();
            mtx1->lock();
        }
        return true;
    }

private:
    QBasicMutex *mtx1, *mtx2;
    bool locked;
};

class QBasicMutexLocker
{
public:
    inline explicit QBasicMutexLocker(QBasicMutex *m) QT_MUTEX_LOCK_NOEXCEPT
        : m(m), isLocked(true)
    {
        m->lock();
    }
    inline ~QBasicMutexLocker() { if (isLocked) unlock(); }

    inline void unlock() noexcept
    {
        isLocked = false;
        m->unlock();
    }

    inline void relock() QT_MUTEX_LOCK_NOEXCEPT
    {
        isLocked = true;
        m->lock();
    }

private:
    Q_DISABLE_COPY(QBasicMutexLocker)

    QBasicMutex *m;
    bool isLocked;
};

#else

class QOrderedMutexLocker
{
public:
    QOrderedMutexLocker(QBasicMutex *, QBasicMutex *) {}
    ~QOrderedMutexLocker() {}

    void relock() {}
    void unlock() {}

    static bool relock(QBasicMutex *, QBasicMutex *) { return false; }
};

using QBasicMutexLocker = QMutexLocker;

#endif


QT_END_NAMESPACE

#endif
