/****************************************************************************
**
** Copyright (C) 2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Rafael Roquetto <rafael.roquetto@kdab.com>
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

#ifndef QTRACE_P_H
#define QTRACE_P_H

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

/*
 * The Qt tracepoints API consists of only five macros:
 *
 *     - Q_TRACE(tracepoint, args...)
 *       Fires 'tracepoint' if it is enabled.
 *
 *     - Q_TRACE_EXIT(tracepoint, args...)
 *       Fires 'tracepoint' if it is enabled when the current scope exists.
 *
 *     - Q_TRACE_SCOPE(tracepoint, args...)
 *       Wrapper around Q_TRACE/_EXIT to trace entry and exit. First it traces
 *       `${tracepoint}_entry` and then `${tracepoint}_exit` on scope exit.
 *
 *     - Q_UNCONDITIONAL_TRACE(tracepoint, args...)
 *       Fires 'tracepoint' unconditionally: no check is performed to query
 *       whether 'tracepoint' is enabled.
 *
 *     - Q_TRACE_ENABLED(tracepoint)
 *       Returns 'true' if 'tracepoint' is enabled; false otherwise.
 *
 * When using LTTNG, Q_TRACE, Q_UNCONDITIONAL_TRACE and Q_TRACE_ENABLED map
 * ultimately to tracepoint(), do_tracepoint() and tracepoint_enabled(),
 * respectively, described on the lttng-ust manpage (man 3 lttng-ust).
 *
 * On ETW, Q_TRACE() and Q_UNCONDITIONAL_TRACE() are equivalent, ultimately
 * amounting to a call to TraceLoggingWrite(), whereas Q_TRACE_ENABLED()
 * wraps around TraceLoggingProviderEnabled().
 *
 * A tracepoint provider is defined in a separate file, that follows the
 * following format:
 *
 *     tracepoint_name(arg_type arg_name, ...)
 *
 * For instance:
 *
 *     qcoreapplication_ctor(int argc, const char * const argv)
 *     qcoreapplication_foo(int argc, const char[10] argv)
 *     qcoreapplication_baz(const char[len] some_string, unsigned int len)
 *     qcoreapplication_qstring(const QString &foo)
 *     qcoreapplication_qrect(const QRect &rect)
 *
 * The provider file is then parsed by src/tools/tracegen, which can be
 * switched to output either ETW or LTTNG tracepoint definitions. The provider
 * name is deduced to be basename(provider_file).
 *
 * To use the above (inside qtcore), you need to include
 * <providername_tracepoints_p.h>. After that, the following call becomes
 * possible:
 *
 *     Q_TRACE(qcoreapplication_qrect, myRect);
 *
 * Currently, all C++ primitive non-pointer types are supported for
 * arguments. Additionally, char * is supported, and is assumed to
 * be a NULL-terminated string. Finally, the following subset of Qt types also
 * currently supported:
 *
 *      - QString
 *      - QByteArray
 *      - QUrl
 *      - QRect
 *
 * Dynamic arrays are supported using the syntax illustrated by
 * qcoreapplication_baz above.
 */

#include <QtCore/qglobal.h>
#include <QtCore/qscopeguard.h>

QT_BEGIN_NAMESPACE

#if defined(Q_TRACEPOINT) && !defined(QT_BOOTSTRAPPED)
#  define Q_HAS_TRACEPOINTS 1
#  define Q_TRACE(x, ...) QtPrivate::trace_ ## x(__VA_ARGS__)
#  define Q_TRACE_EXIT(x, ...) \
    const auto qTraceExit_ ## x ## __COUNTER__ = qScopeGuard([&]() { Q_TRACE(x, __VA_ARGS__); });
#  define Q_TRACE_SCOPE(x, ...) \
    Q_TRACE(x ## _entry, __VA_ARGS__); \
    Q_TRACE_EXIT(x ## _exit);
#  define Q_UNCONDITIONAL_TRACE(x, ...) QtPrivate::do_trace_ ## x(__VA_ARGS__)
#  define Q_TRACE_ENABLED(x) QtPrivate::trace_ ## x ## _enabled()
#else
#  define Q_HAS_TRACEPOINTS 0
#  define Q_TRACE(x, ...)
#  define Q_TRACE_EXIT(x, ...)
#  define Q_TRACE_SCOPE(x, ...)
#  define Q_UNCONDITIONAL_TRACE(x, ...)
#  define Q_TRACE_ENABLED(x) false
#endif // defined(Q_TRACEPOINT) && !defined(QT_BOOTSTRAPPED)

QT_END_NAMESPACE

#endif // QTRACE_P_H
