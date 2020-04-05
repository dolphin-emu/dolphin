/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QT_EGL_P_H
#define QT_EGL_P_H

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

#ifdef QT_EGL_NO_X11
# ifndef MESA_EGL_NO_X11_HEADERS
#  define MESA_EGL_NO_X11_HEADERS // MESA
# endif
# if !defined(Q_OS_INTEGRITY)
#  define WIN_INTERFACE_CUSTOM   // NV
# endif // Q_OS_INTEGRITY
#endif  // QT_EGL_NO_X11

#ifdef QT_EGL_WAYLAND
# define WAYLAND // NV
#endif // QT_EGL_WAYLAND

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <stdint.h>

QT_BEGIN_NAMESPACE

namespace QtInternal {

template <class FromType, class ToType>
struct QtEglConverter
{
    static inline ToType convert(FromType v)
    { return v; }
};

template <>
struct QtEglConverter<uint32_t, uintptr_t>
{
    static inline uintptr_t convert(uint32_t v)
    { return v; }
};

#if QT_POINTER_SIZE > 4
template <>
struct QtEglConverter<uintptr_t, uint32_t>
{
    static inline uint32_t convert(uintptr_t v)
    { return uint32_t(v); }
};
#endif

template <>
struct QtEglConverter<uint32_t, void *>
{
    static inline void *convert(uint32_t v)
    { return reinterpret_cast<void *>(uintptr_t(v)); }
};

template <>
struct QtEglConverter<void *, uint32_t>
{
    static inline uint32_t convert(void *v)
    { return uintptr_t(v); }
};

} // QtInternal

template <class ToType, class FromType>
static inline ToType qt_egl_cast(FromType from)
{ return QtInternal::QtEglConverter<FromType, ToType>::convert(from); }

QT_END_NAMESPACE

#endif // QT_EGL_P_H
