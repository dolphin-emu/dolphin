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

#ifndef QWINFUNCTIONS_H
#define QWINFUNCTIONS_H

#if 0
#pragma qt_class(QtWin)
#endif

#include <QtCore/qobject.h>
#include <QtCore/qt_windows.h>
#include <QtWinExtras/qwinextrasglobal.h>
#ifdef QT_WIDGETS_LIB
#include <QtWidgets/qwidget.h>
#endif

QT_BEGIN_NAMESPACE

class QPixmap;
class QImage;
class QBitmap;
class QColor;
class QWindow;
class QString;
class QMargins;

namespace QtWin
{
    enum HBitmapFormat
    {
        HBitmapNoAlpha,
        HBitmapPremultipliedAlpha,
        HBitmapAlpha
    };

    enum WindowFlip3DPolicy
    {
        FlipDefault,
        FlipExcludeBelow,
        FlipExcludeAbove
    };

    Q_WINEXTRAS_EXPORT HBITMAP createMask(const QBitmap &bitmap);
    Q_WINEXTRAS_EXPORT HBITMAP toHBITMAP(const QPixmap &p, HBitmapFormat format = HBitmapNoAlpha);
    Q_WINEXTRAS_EXPORT QPixmap fromHBITMAP(HBITMAP bitmap, HBitmapFormat format = HBitmapNoAlpha);
    Q_WINEXTRAS_EXPORT HICON toHICON(const QPixmap &p);
    Q_WINEXTRAS_EXPORT HBITMAP imageToHBITMAP(const QImage &image, QtWin::HBitmapFormat format = HBitmapNoAlpha);
    Q_WINEXTRAS_EXPORT QImage imageFromHBITMAP(HDC hdc, HBITMAP bitmap, int width, int height);
    Q_WINEXTRAS_EXPORT QImage imageFromHBITMAP(HBITMAP bitmap, QtWin::HBitmapFormat format = HBitmapNoAlpha);
    Q_WINEXTRAS_EXPORT QPixmap fromHICON(HICON icon);
    Q_WINEXTRAS_EXPORT HRGN toHRGN(const QRegion &region);
    Q_WINEXTRAS_EXPORT QRegion fromHRGN(HRGN hrgn);

    Q_WINEXTRAS_EXPORT QString stringFromHresult(HRESULT hresult);
    Q_WINEXTRAS_EXPORT QString errorStringFromHresult(HRESULT hresult);

    Q_WINEXTRAS_EXPORT QColor colorizationColor(bool *opaqueBlend = nullptr);
    Q_WINEXTRAS_EXPORT QColor realColorizationColor();

    Q_WINEXTRAS_EXPORT void setWindowExcludedFromPeek(QWindow *window, bool exclude);
    Q_WINEXTRAS_EXPORT bool isWindowExcludedFromPeek(QWindow *window);
    Q_WINEXTRAS_EXPORT void setWindowDisallowPeek(QWindow *window, bool disallow);
    Q_WINEXTRAS_EXPORT bool isWindowPeekDisallowed(QWindow *window);
    Q_WINEXTRAS_EXPORT void setWindowFlip3DPolicy(QWindow *window, WindowFlip3DPolicy policy);
    Q_WINEXTRAS_EXPORT WindowFlip3DPolicy windowFlip3DPolicy(QWindow *);

    Q_WINEXTRAS_EXPORT void extendFrameIntoClientArea(QWindow *window, int left, int top, int right, int bottom);
    Q_WINEXTRAS_EXPORT void extendFrameIntoClientArea(QWindow *window, const QMargins &margins);
    Q_WINEXTRAS_EXPORT void resetExtendedFrame(QWindow *window);

    Q_WINEXTRAS_EXPORT void enableBlurBehindWindow(QWindow *window, const QRegion &region);
    Q_WINEXTRAS_EXPORT void enableBlurBehindWindow(QWindow *window);
    Q_WINEXTRAS_EXPORT void disableBlurBehindWindow(QWindow *window);

    Q_WINEXTRAS_EXPORT bool isCompositionEnabled();
    Q_WINEXTRAS_EXPORT void setCompositionEnabled(bool enabled);
    Q_WINEXTRAS_EXPORT bool isCompositionOpaque();

    Q_WINEXTRAS_EXPORT void setCurrentProcessExplicitAppUserModelID(const QString &id);

    Q_WINEXTRAS_EXPORT void markFullscreenWindow(QWindow *, bool fullscreen = true);

    Q_WINEXTRAS_EXPORT void taskbarActivateTab(QWindow *);
    Q_WINEXTRAS_EXPORT void taskbarActivateTabAlt(QWindow *);
    Q_WINEXTRAS_EXPORT void taskbarAddTab(QWindow *);
    Q_WINEXTRAS_EXPORT void taskbarDeleteTab(QWindow *);

#ifdef QT_WIDGETS_LIB
    inline void setWindowExcludedFromPeek(QWidget *window, bool exclude)
    {
        window->createWinId();
        setWindowExcludedFromPeek(window->windowHandle(), exclude);
    }

    inline bool isWindowExcludedFromPeek(QWidget *window)
    {
        auto handle = window->windowHandle();
        return handle && isWindowExcludedFromPeek(handle);
    }

    inline void setWindowDisallowPeek(QWidget *window, bool disallow)
    {
        window->createWinId();
        setWindowDisallowPeek(window->windowHandle(), disallow);
    }

    inline bool isWindowPeekDisallowed(QWidget *window)
    {
        auto handle = window->windowHandle();
        return handle && isWindowPeekDisallowed(handle);
    }

    inline void setWindowFlip3DPolicy(QWidget *window, WindowFlip3DPolicy policy)
    {
        window->createWinId();
        setWindowFlip3DPolicy(window->windowHandle(), policy);
    }

    inline WindowFlip3DPolicy windowFlip3DPolicy(QWidget *window)
    {
        if (!window->windowHandle())
            return FlipDefault;
        else
            return windowFlip3DPolicy(window->windowHandle());
    }

    inline void extendFrameIntoClientArea(QWidget *window, const QMargins &margins)
    {
        window->createWinId();
        extendFrameIntoClientArea(window->windowHandle(), margins);
    }

    inline void extendFrameIntoClientArea(QWidget *window, int left, int top, int right, int bottom)
    {
        window->createWinId();
        extendFrameIntoClientArea(window->windowHandle(), left, top, right, bottom);
    }

    inline void resetExtendedFrame(QWidget *window)
    {
        if (window->windowHandle())
            resetExtendedFrame(window->windowHandle());
    }

    inline void enableBlurBehindWindow(QWidget *window, const QRegion &region)
    {
        window->createWinId();
        enableBlurBehindWindow(window->windowHandle(), region);
    }

    inline void enableBlurBehindWindow(QWidget *window)
    {
        window->createWinId();
        enableBlurBehindWindow(window->windowHandle());
    }

    inline void disableBlurBehindWindow(QWidget *window)
    {
        if (window->windowHandle())
            disableBlurBehindWindow(window->windowHandle());
    }

    inline void markFullscreenWindow(QWidget *window, bool fullscreen = true)
    {
        window->createWinId();
        markFullscreenWindow(window->windowHandle(), fullscreen);
    }

    inline void taskbarActivateTab(QWidget *window)
    {
        window->createWinId();
        taskbarActivateTab(window->windowHandle());
    }

    inline void taskbarActivateTabAlt(QWidget *window)
    {
        window->createWinId();
        taskbarActivateTabAlt(window->windowHandle());
    }

    inline void taskbarAddTab(QWidget *window)
    {
        window->createWinId();
        taskbarAddTab(window->windowHandle());
    }

    inline void taskbarDeleteTab(QWidget *window)
    {
        window->createWinId();
        taskbarDeleteTab(window->windowHandle());
    }
#endif // QT_WIDGETS_LIB
}

QT_END_NAMESPACE

#endif // QWINFUNCTIONS_H
