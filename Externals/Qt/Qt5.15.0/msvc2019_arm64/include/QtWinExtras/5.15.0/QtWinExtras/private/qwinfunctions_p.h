/****************************************************************************
 **
 ** Copyright (C) 2016 Ivan Vizir <define-true-false@yandex.com>
 ** Contact: https://www.qt.io/licensing/
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
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

#ifndef QWINFUNCTIONS_P_H
#define QWINFUNCTIONS_P_H

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

#include <QtCore/qstring.h>
#include <QtCore/qt_windows.h>
#include <uxtheme.h>
#include <dwmapi.h>

QT_BEGIN_NAMESPACE

enum qt_DWMWINDOWATTRIBUTE // Not present in MinGW 4.9
{
    qt_DWMWA_DISALLOW_PEEK = 11,
    qt_DWMWA_EXCLUDED_FROM_PEEK = 12,
};

namespace QtDwmApiDll
{
    template <class T> static T windowAttribute(HWND hwnd, DWORD attribute, T defaultValue);
    template <class T> static void setWindowAttribute(HWND hwnd, DWORD attribute, T value);

    inline bool booleanWindowAttribute(HWND hwnd, DWORD attribute)
        { return QtDwmApiDll::windowAttribute<BOOL>(hwnd, attribute, FALSE) != FALSE; }

    inline void setBooleanWindowAttribute(HWND hwnd, DWORD attribute, bool value)
        { setWindowAttribute<BOOL>(hwnd, attribute, BOOL(value ? TRUE : FALSE)); }
};

inline void qt_qstringToNullTerminated(const QString &src, wchar_t *dst)
{
    dst[src.toWCharArray(dst)] = 0;
}

inline wchar_t *qt_qstringToNullTerminated(const QString &src)
{
    auto *buffer = new wchar_t[src.length() + 1];
    qt_qstringToNullTerminated(src, buffer);
    return buffer;
}

template <class T>
T QtDwmApiDll::windowAttribute(HWND hwnd, DWORD attribute, T defaultValue)
{
    T value;
    if (FAILED(DwmGetWindowAttribute(hwnd, attribute, &value, sizeof(value))))
        value = defaultValue;
    return value;
}

template <class T>
void QtDwmApiDll::setWindowAttribute(HWND hwnd, DWORD attribute, T value)
{
    DwmSetWindowAttribute(hwnd, attribute, &value, sizeof(value));
}

QT_END_NAMESPACE

#endif // QWINFUNCTIONS_P_H
