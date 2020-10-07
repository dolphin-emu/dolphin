/****************************************************************************
**
** Copyright (C) 2019 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Marc Mutz <marc.mutz@kdab.com>
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

#ifndef QLOCKING_P_H
#define QLOCKING_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of qmutex.cpp, qmutex_unix.cpp, and qmutex_win.cpp.  This header
// file may change from version to version without notice, or even be
// removed.
//
// We mean it.
//

#include <QtCore/qmutex.h>

#include <mutex>

QT_BEGIN_NAMESPACE

//
// This API is bridging the time until we can depend on C++17:
//
// - qt_scoped_lock returns a lock that cannot be unlocked again before the end of the scope
// - qt_unique_lock returns a lock that can be unlock()ed and moved around
// - for compat with QMutexLocker, qt_unique_lock supports passing by pointer.
//   Do NOT use this overload lightly; it's only for cases such as where a Q_GLOBAL_STATIC
//   may have already been deleted. In particular, do NOT port from
//       QMutexLocker locker(&mutex);
//   to
//       auto locker = qt_unique_lock(&mutex);
//   as this will not port automatically to std::unique_lock come C++17!
//
// The intent, come C++17, is to replace
//     qt_scoped_lock(mutex);
//     qt_unique_lock(mutex); // except qt_unique_lock(&mutex)
// with
//     std::scoped_lock(mutex);
//     std::unique_lock(mutex);
// resp. (C++17 meaning CTAD, guaranteed copy elision + scoped_lock available on all platforms),
// so please use these functions only in ways which don't break this mechanical search & replace.
//

namespace {

template <typename Mutex, typename Lock =
#if defined(__cpp_guaranteed_copy_elision) && __cpp_guaranteed_copy_elision >= 201606L
# if defined(__cpp_lib_scoped_lock) && __cpp_lib_scoped_lock >= 201703L
          std::scoped_lock
# else
          std::lock_guard
# endif
#else
          std::unique_lock
#endif
          <typename std::decay<Mutex>::type>
>
Lock qt_scoped_lock(Mutex &mutex)
{
    return Lock(mutex);
}

template <typename Mutex, typename Lock = std::unique_lock<typename std::decay<Mutex>::type>>
Lock qt_unique_lock(Mutex &mutex)
{
    return Lock(mutex);
}

template <typename Mutex, typename Lock = std::unique_lock<typename std::decay<Mutex>::type>>
Lock qt_unique_lock(Mutex *mutex)
{
    return mutex ? Lock(*mutex) : Lock() ;
}

} // unnamed namespace

QT_END_NAMESPACE

#endif // QLOCKING_P_H
