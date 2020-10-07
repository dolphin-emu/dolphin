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

#ifndef QCORE_UNIX_P_H
#define QCORE_UNIX_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of Qt code on Unix. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qplatformdefs.h"
#include <QtCore/private/qglobal_p.h>
#include "qatomic.h"
#include "qbytearray.h"

#ifndef Q_OS_UNIX
# error "qcore_unix_p.h included on a non-Unix system"
#endif

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef Q_OS_NACL
#elif !defined (Q_OS_VXWORKS)
# if !defined(Q_OS_HPUX) || defined(__ia64)
#  include <sys/select.h>
# endif
#  include <sys/time.h>
#else
#  include <selectLib.h>
#endif

#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

#if !defined(QT_POSIX_IPC) && !defined(QT_NO_SHAREDMEMORY) && !defined(Q_OS_ANDROID)
#  include <sys/ipc.h>
#endif

#if defined(Q_OS_VXWORKS)
#  include <ioLib.h>
#endif

#ifdef QT_NO_NATIVE_POLL
#  include "qpoll_p.h"
#else
#  include <poll.h>
#endif

struct sockaddr;

#define EINTR_LOOP(var, cmd)                    \
    do {                                        \
        var = cmd;                              \
    } while (var == -1 && errno == EINTR)

QT_BEGIN_NAMESPACE

Q_DECLARE_TYPEINFO(pollfd, Q_PRIMITIVE_TYPE);

// Internal operator functions for timespecs
inline timespec &normalizedTimespec(timespec &t)
{
    while (t.tv_nsec >= 1000000000) {
        ++t.tv_sec;
        t.tv_nsec -= 1000000000;
    }
    while (t.tv_nsec < 0) {
        --t.tv_sec;
        t.tv_nsec += 1000000000;
    }
    return t;
}
inline bool operator<(const timespec &t1, const timespec &t2)
{ return t1.tv_sec < t2.tv_sec || (t1.tv_sec == t2.tv_sec && t1.tv_nsec < t2.tv_nsec); }
inline bool operator==(const timespec &t1, const timespec &t2)
{ return t1.tv_sec == t2.tv_sec && t1.tv_nsec == t2.tv_nsec; }
inline bool operator!=(const timespec &t1, const timespec &t2)
{ return !(t1 == t2); }
inline timespec &operator+=(timespec &t1, const timespec &t2)
{
    t1.tv_sec += t2.tv_sec;
    t1.tv_nsec += t2.tv_nsec;
    return normalizedTimespec(t1);
}
inline timespec operator+(const timespec &t1, const timespec &t2)
{
    timespec tmp;
    tmp.tv_sec = t1.tv_sec + t2.tv_sec;
    tmp.tv_nsec = t1.tv_nsec + t2.tv_nsec;
    return normalizedTimespec(tmp);
}
inline timespec operator-(const timespec &t1, const timespec &t2)
{
    timespec tmp;
    tmp.tv_sec = t1.tv_sec - (t2.tv_sec - 1);
    tmp.tv_nsec = t1.tv_nsec - (t2.tv_nsec + 1000000000);
    return normalizedTimespec(tmp);
}
inline timespec operator*(const timespec &t1, int mul)
{
    timespec tmp;
    tmp.tv_sec = t1.tv_sec * mul;
    tmp.tv_nsec = t1.tv_nsec * mul;
    return normalizedTimespec(tmp);
}
inline timeval timespecToTimeval(const timespec &ts)
{
    timeval tv;
    tv.tv_sec = ts.tv_sec;
    tv.tv_usec = ts.tv_nsec / 1000;
    return tv;
}


inline void qt_ignore_sigpipe()
{
    // Set to ignore SIGPIPE once only.
    static QBasicAtomicInt atom = Q_BASIC_ATOMIC_INITIALIZER(0);
    if (!atom.loadRelaxed()) {
        // More than one thread could turn off SIGPIPE at the same time
        // But that's acceptable because they all would be doing the same
        // action
        struct sigaction noaction;
        memset(&noaction, 0, sizeof(noaction));
        noaction.sa_handler = SIG_IGN;
        ::sigaction(SIGPIPE, &noaction, nullptr);
        atom.storeRelaxed(1);
    }
}

#if defined(Q_PROCESSOR_X86_32) && defined(__GLIBC__)
#  if !__GLIBC_PREREQ(2, 22)
Q_CORE_EXPORT int qt_open64(const char *pathname, int flags, mode_t);
#    undef QT_OPEN
#    define QT_OPEN qt_open64
#  endif
#endif

// don't call QT_OPEN or ::open
// call qt_safe_open
static inline int qt_safe_open(const char *pathname, int flags, mode_t mode = 0777)
{
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
    int fd;
    EINTR_LOOP(fd, QT_OPEN(pathname, flags, mode));

#ifndef O_CLOEXEC
    if (fd != -1)
        ::fcntl(fd, F_SETFD, FD_CLOEXEC);
#endif

    return fd;
}
#undef QT_OPEN
#define QT_OPEN         qt_safe_open

#ifndef Q_OS_VXWORKS // no POSIX pipes in VxWorks
// don't call ::pipe
// call qt_safe_pipe
static inline int qt_safe_pipe(int pipefd[2], int flags = 0)
{
    Q_ASSERT((flags & ~O_NONBLOCK) == 0);

#ifdef QT_THREADSAFE_CLOEXEC
    // use pipe2
    flags |= O_CLOEXEC;
    return ::pipe2(pipefd, flags); // pipe2 is documented not to return EINTR
#else
    int ret = ::pipe(pipefd);
    if (ret == -1)
        return -1;

    ::fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
    ::fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);

    // set non-block too?
    if (flags & O_NONBLOCK) {
        ::fcntl(pipefd[0], F_SETFL, ::fcntl(pipefd[0], F_GETFL) | O_NONBLOCK);
        ::fcntl(pipefd[1], F_SETFL, ::fcntl(pipefd[1], F_GETFL) | O_NONBLOCK);
    }

    return 0;
#endif
}

#endif // Q_OS_VXWORKS

// don't call dup or fcntl(F_DUPFD)
static inline int qt_safe_dup(int oldfd, int atleast = 0, int flags = FD_CLOEXEC)
{
    Q_ASSERT(flags == FD_CLOEXEC || flags == 0);

#ifdef F_DUPFD_CLOEXEC
    int cmd = F_DUPFD;
    if (flags & FD_CLOEXEC)
        cmd = F_DUPFD_CLOEXEC;
    return ::fcntl(oldfd, cmd, atleast);
#else
    // use F_DUPFD
    int ret = ::fcntl(oldfd, F_DUPFD, atleast);

    if (flags && ret != -1)
        ::fcntl(ret, F_SETFD, flags);
    return ret;
#endif
}

// don't call dup2
// call qt_safe_dup2
static inline int qt_safe_dup2(int oldfd, int newfd, int flags = FD_CLOEXEC)
{
    Q_ASSERT(flags == FD_CLOEXEC || flags == 0);

    int ret;
#ifdef QT_THREADSAFE_CLOEXEC
    // use dup3
    EINTR_LOOP(ret, ::dup3(oldfd, newfd, flags ? O_CLOEXEC : 0));
    return ret;
#else
    EINTR_LOOP(ret, ::dup2(oldfd, newfd));
    if (ret == -1)
        return -1;

    if (flags)
        ::fcntl(newfd, F_SETFD, flags);
    return 0;
#endif
}

static inline qint64 qt_safe_read(int fd, void *data, qint64 maxlen)
{
    qint64 ret = 0;
    EINTR_LOOP(ret, QT_READ(fd, data, maxlen));
    return ret;
}
#undef QT_READ
#define QT_READ qt_safe_read

static inline qint64 qt_safe_write(int fd, const void *data, qint64 len)
{
    qint64 ret = 0;
    EINTR_LOOP(ret, QT_WRITE(fd, data, len));
    return ret;
}
#undef QT_WRITE
#define QT_WRITE qt_safe_write

static inline qint64 qt_safe_write_nosignal(int fd, const void *data, qint64 len)
{
    qt_ignore_sigpipe();
    return qt_safe_write(fd, data, len);
}

static inline int qt_safe_close(int fd)
{
    int ret;
    EINTR_LOOP(ret, QT_CLOSE(fd));
    return ret;
}
#undef QT_CLOSE
#define QT_CLOSE qt_safe_close

// - VxWorks & iOS/tvOS/watchOS don't have processes
#if QT_CONFIG(process)
static inline int qt_safe_execve(const char *filename, char *const argv[],
                                 char *const envp[])
{
    int ret;
    EINTR_LOOP(ret, ::execve(filename, argv, envp));
    return ret;
}

static inline int qt_safe_execv(const char *path, char *const argv[])
{
    int ret;
    EINTR_LOOP(ret, ::execv(path, argv));
    return ret;
}

static inline int qt_safe_execvp(const char *file, char *const argv[])
{
    int ret;
    EINTR_LOOP(ret, ::execvp(file, argv));
    return ret;
}

static inline pid_t qt_safe_waitpid(pid_t pid, int *status, int options)
{
    int ret;
    EINTR_LOOP(ret, ::waitpid(pid, status, options));
    return ret;
}
#endif // QT_CONFIG(process)

#if !defined(_POSIX_MONOTONIC_CLOCK)
#  define _POSIX_MONOTONIC_CLOCK -1
#endif

// in qelapsedtimer_mac.cpp or qtimestamp_unix.cpp
timespec qt_gettime() noexcept;
void qt_nanosleep(timespec amount);
QByteArray qt_readlink(const char *path);

/* non-static */
inline bool qt_haveLinuxProcfs()
{
#ifdef Q_OS_LINUX
#  ifdef QT_LINUX_ALWAYS_HAVE_PROCFS
    return true;
#  else
    static const bool present = (access("/proc/version", F_OK) == 0);
    return present;
#  endif
#else
    return false;
#endif
}

Q_CORE_EXPORT int qt_safe_poll(struct pollfd *fds, nfds_t nfds, const struct timespec *timeout_ts);

static inline int qt_poll_msecs(struct pollfd *fds, nfds_t nfds, int timeout)
{
    timespec ts, *pts = nullptr;

    if (timeout >= 0) {
        ts.tv_sec = timeout / 1000;
        ts.tv_nsec = (timeout % 1000) * 1000 * 1000;
        pts = &ts;
    }

    return qt_safe_poll(fds, nfds, pts);
}

static inline struct pollfd qt_make_pollfd(int fd, short events)
{
    struct pollfd pfd = { fd, events, 0 };
    return pfd;
}

// according to X/OPEN we have to define semun ourselves
// we use prefix as on some systems sem.h will have it
struct semid_ds;
union qt_semun {
    int val;                    /* value for SETVAL */
    struct semid_ds *buf;       /* buffer for IPC_STAT, IPC_SET */
    unsigned short *array;      /* array for GETALL, SETALL */
};

QT_END_NAMESPACE

#endif
