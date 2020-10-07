/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Copyright (C) 2015 Intel Corporation.
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

#ifndef QGLOBAL_P_H
#define QGLOBAL_P_H

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

#include "qglobal.h"
#include "qglobal_p.h"      // include self to avoid syncqt warning - no-op

#ifndef QT_BOOTSTRAPPED
#include <QtCore/private/qconfig_p.h>
#include <QtCore/private/qtcore-config_p.h>
#endif

#if defined(__cplusplus)
#ifdef Q_CC_MINGW
#  include <unistd.h> // Define _POSIX_THREAD_SAFE_FUNCTIONS to obtain localtime_r()
#endif
#include <time.h>

QT_BEGIN_NAMESPACE

// These behave as if they consult the environment, so need to share its locking:
Q_CORE_EXPORT void qTzSet();
Q_CORE_EXPORT time_t qMkTime(struct tm *when);

QT_END_NAMESPACE

#if !__has_builtin(__builtin_available)
#include <initializer_list>
#include <QtCore/qoperatingsystemversion.h>
#include <QtCore/qversionnumber.h>

QT_BEGIN_NAMESPACE

struct qt_clang_builtin_available_os_version_data {
    QOperatingSystemVersion::OSType type;
    const char *version;
};

static inline bool qt_clang_builtin_available(
    const std::initializer_list<qt_clang_builtin_available_os_version_data> &versions)
{
    for (auto it = versions.begin(); it != versions.end(); ++it) {
        if (QOperatingSystemVersion::currentType() == it->type) {
            const auto current = QOperatingSystemVersion::current();
            return QVersionNumber(
                current.majorVersion(),
                current.minorVersion(),
                current.microVersion()) >= QVersionNumber::fromString(
                    QString::fromLatin1(it->version));
        }
    }

    // Result is true if the platform is not any of the checked ones; this matches behavior of
    // LLVM __builtin_available and @available constructs
    return true;
}

QT_END_NAMESPACE

#define QT_AVAILABLE_OS_VER(os, ver) \
    QT_PREPEND_NAMESPACE(qt_clang_builtin_available_os_version_data){\
        QT_PREPEND_NAMESPACE(QOperatingSystemVersion)::os, #ver}
#define QT_AVAILABLE_CAT(L, R) QT_AVAILABLE_CAT_(L, R)
#define QT_AVAILABLE_CAT_(L, R) L ## R
#define QT_AVAILABLE_EXPAND(...) QT_AVAILABLE_OS_VER(__VA_ARGS__)
#define QT_AVAILABLE_SPLIT(os_ver) QT_AVAILABLE_EXPAND(QT_AVAILABLE_CAT(QT_AVAILABLE_SPLIT_, os_ver))
#define QT_AVAILABLE_SPLIT_macOS MacOS,
#define QT_AVAILABLE_SPLIT_iOS IOS,
#define QT_AVAILABLE_SPLIT_tvOS TvOS,
#define QT_AVAILABLE_SPLIT_watchOS WatchOS,
#define QT_BUILTIN_AVAILABLE0(e) \
    QT_PREPEND_NAMESPACE(qt_clang_builtin_available)({})
#define QT_BUILTIN_AVAILABLE1(a, e) \
    QT_PREPEND_NAMESPACE(qt_clang_builtin_available)({QT_AVAILABLE_SPLIT(a)})
#define QT_BUILTIN_AVAILABLE2(a, b, e) \
    QT_PREPEND_NAMESPACE(qt_clang_builtin_available)({QT_AVAILABLE_SPLIT(a), \
                                                      QT_AVAILABLE_SPLIT(b)})
#define QT_BUILTIN_AVAILABLE3(a, b, c, e) \
    QT_PREPEND_NAMESPACE(qt_clang_builtin_available)({QT_AVAILABLE_SPLIT(a), \
                                                      QT_AVAILABLE_SPLIT(b), \
                                                      QT_AVAILABLE_SPLIT(c)})
#define QT_BUILTIN_AVAILABLE4(a, b, c, d, e) \
    QT_PREPEND_NAMESPACE(qt_clang_builtin_available)({QT_AVAILABLE_SPLIT(a), \
                                                      QT_AVAILABLE_SPLIT(b), \
                                                      QT_AVAILABLE_SPLIT(c), \
                                                      QT_AVAILABLE_SPLIT(d)})
#define QT_BUILTIN_AVAILABLE_ARG(arg0, arg1, arg2, arg3, arg4, arg5, ...) arg5
#define QT_BUILTIN_AVAILABLE_CHOOSER(...) QT_BUILTIN_AVAILABLE_ARG(__VA_ARGS__, \
    QT_BUILTIN_AVAILABLE4, \
    QT_BUILTIN_AVAILABLE3, \
    QT_BUILTIN_AVAILABLE2, \
    QT_BUILTIN_AVAILABLE1, \
    QT_BUILTIN_AVAILABLE0, )
#define __builtin_available(...) QT_BUILTIN_AVAILABLE_CHOOSER(__VA_ARGS__)(__VA_ARGS__)
#endif // !__has_builtin(__builtin_available)
#endif // defined(__cplusplus)

#endif // QGLOBAL_P_H

