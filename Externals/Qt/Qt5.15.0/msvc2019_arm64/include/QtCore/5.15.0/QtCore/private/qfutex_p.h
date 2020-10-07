/****************************************************************************
**
** Copyright (C) 2017 Intel Corporation.
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

#ifndef QFUTEX_P_H
#define QFUTEX_P_H

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

#include <qglobal.h>

QT_BEGIN_NAMESPACE

namespace QtDummyFutex {
    Q_DECL_CONSTEXPR inline bool futexAvailable() { return false; }
    template <typename Atomic>
    inline bool futexWait(Atomic &, typename Atomic::Type, int = 0)
    { Q_UNREACHABLE(); return false; }
    template <typename Atomic> inline void futexWakeOne(Atomic &)
    { Q_UNREACHABLE(); }
    template <typename Atomic> inline void futexWakeAll(Atomic &)
    { Q_UNREACHABLE(); }
}

QT_END_NAMESPACE

#if defined(Q_OS_LINUX) && !defined(QT_LINUXBASE)
// use Linux mutexes everywhere except for LSB builds
#  include <sys/syscall.h>
#  include <errno.h>
#  include <limits.h>
#  include <unistd.h>
#  include <asm/unistd.h>
#  include <linux/futex.h>
#  define QT_ALWAYS_USE_FUTEX

// if not defined in linux/futex.h
#  define FUTEX_PRIVATE_FLAG        128         // added in v2.6.22

#  if __has_feature(thread_sanitizer) || defined(__SANITIZE_THREAD__)
#    include <sanitizer/tsan_interface.h>
inline void _q_tsan_acquire(void *addr, void *addr2)
{
    __tsan_acquire(addr);
    if (addr2)
        __tsan_acquire(addr2);
}
inline void _q_tsan_release(void *addr, void *addr2)
{
    if (addr2)
        __tsan_release(addr2);
    __tsan_release(addr);
}
#  else
inline void _q_tsan_acquire(void *, void *) {}
inline void _q_tsan_release(void *, void *) {}
#  endif // __has_feature(thread_sanitizer) || defined(__SANITIZE_THREAD__)

QT_BEGIN_NAMESPACE
namespace QtLinuxFutex {
    constexpr inline bool futexAvailable() { return true; }
    inline int _q_futex(int *addr, int op, int val, quintptr val2 = 0,
                        int *addr2 = nullptr, int val3 = 0) noexcept
    {
        // A futex call ensures total ordering on the futex words
        // (in either success or failure of the call). Instruct TSAN accordingly,
        // as TSAN does not understand the futex(2) syscall.
        _q_tsan_release(addr, addr2);

        // we use __NR_futex because some libcs (like Android's bionic) don't
        // provide SYS_futex etc.
        int result = syscall(__NR_futex, addr, op | FUTEX_PRIVATE_FLAG, val, val2, addr2, val3);

        _q_tsan_acquire(addr, addr2);

        return result;
    }
    template <typename T> int *addr(T *ptr)
    {
        int *int_addr = reinterpret_cast<int *>(ptr);
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
        if (sizeof(T) > sizeof(int))
            int_addr++; //We want a pointer to the least significant half
#endif
        return int_addr;
    }

    template <typename Atomic>
    inline void futexWait(Atomic &futex, typename Atomic::Type expectedValue)
    {
        _q_futex(addr(&futex), FUTEX_WAIT, qintptr(expectedValue));
    }
    template <typename Atomic>
    inline bool futexWait(Atomic &futex, typename Atomic::Type expectedValue, qint64 nstimeout)
    {
        struct timespec ts;
        ts.tv_sec = nstimeout / 1000 / 1000 / 1000;
        ts.tv_nsec = nstimeout % (1000 * 1000 * 1000);
        int r = _q_futex(addr(&futex), FUTEX_WAIT, qintptr(expectedValue), quintptr(&ts));
        return r == 0 || errno != ETIMEDOUT;
    }
    template <typename Atomic> inline void futexWakeOne(Atomic &futex)
    {
        _q_futex(addr(&futex), FUTEX_WAKE, 1);
    }
    template <typename Atomic> inline void futexWakeAll(Atomic &futex)
    {
        _q_futex(addr(&futex), FUTEX_WAKE, INT_MAX);
    }
    template <typename Atomic> inline
    void futexWakeOp(Atomic &futex1, int wake1, int wake2, Atomic &futex2, quint32 op)
    {
        _q_futex(addr(&futex1), FUTEX_WAKE_OP, wake1, wake2, addr(&futex2), op);
    }
}
namespace QtFutex = QtLinuxFutex;
QT_END_NAMESPACE

#else

QT_BEGIN_NAMESPACE
namespace QtFutex = QtDummyFutex;
QT_END_NAMESPACE
#endif

#endif // QFUTEX_P_H
