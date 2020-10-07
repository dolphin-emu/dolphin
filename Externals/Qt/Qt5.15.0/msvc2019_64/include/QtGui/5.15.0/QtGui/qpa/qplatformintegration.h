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

#ifndef QPLATFORMINTEGRATION_H
#define QPLATFORMINTEGRATION_H

//
//  W A R N I N G
//  -------------
//
// This file is part of the QPA API and is not meant to be used
// in applications. Usage of this API may make your code
// source and binary incompatible with future versions of Qt.
//

#include <QtGui/qtguiglobal.h>
#include <QtGui/qwindowdefs.h>
#include <qpa/qplatformscreen.h>
#include <QtGui/qsurfaceformat.h>
#include <QtGui/qopenglcontext.h>

QT_BEGIN_NAMESPACE


class QPlatformWindow;
class QWindow;
class QPlatformBackingStore;
class QPlatformFontDatabase;
class QPlatformClipboard;
class QPlatformNativeInterface;
class QPlatformDrag;
class QPlatformOpenGLContext;
class QGuiGLFormat;
class QAbstractEventDispatcher;
class QPlatformInputContext;
class QPlatformAccessibility;
class QPlatformTheme;
class QPlatformDialogHelper;
class QPlatformSharedGraphicsCache;
class QPlatformServices;
class QPlatformSessionManager;
class QKeyEvent;
class QPlatformOffscreenSurface;
class QOffscreenSurface;
class QPlatformVulkanInstance;
class QVulkanInstance;

class Q_GUI_EXPORT QPlatformIntegration
{
public:
    Q_DISABLE_COPY_MOVE(QPlatformIntegration)

    enum Capability {
        ThreadedPixmaps = 1,
        OpenGL,
        ThreadedOpenGL,
        SharedGraphicsCache,
        BufferQueueingOpenGL,
        WindowMasks,
        MultipleWindows,
        ApplicationState,
        ForeignWindows,
        NonFullScreenWindows,
        NativeWidgets,
        WindowManagement,
        WindowActivation, // whether requestActivate is supported
        SyncState,
        RasterGLSurface,
        AllGLFunctionsQueryable,
        ApplicationIcon,
        SwitchableWidgetComposition,
        TopStackedNativeChildWindows,
        OpenGLOnRasterSurface,
        MaximizeUsingFullscreenGeometry
    };

    virtual ~QPlatformIntegration() { }

    virtual bool hasCapability(Capability cap) const;

    virtual QPlatformPixmap *createPlatformPixmap(QPlatformPixmap::PixelType type) const;
    virtual QPlatformWindow *createPlatformWindow(QWindow *window) const = 0;
    virtual QPlatformWindow *createForeignWindow(QWindow *, WId) const { return nullptr; }
    virtual QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const = 0;
#ifndef QT_NO_OPENGL
    virtual QPlatformOpenGLContext *createPlatformOpenGLContext(QOpenGLContext *context) const;
#endif
    virtual QPlatformSharedGraphicsCache *createPlatformSharedGraphicsCache(const char *cacheId) const;
    virtual QPaintEngine *createImagePaintEngine(QPaintDevice *paintDevice) const;

// Event dispatcher:
    virtual QAbstractEventDispatcher *createEventDispatcher() const = 0;
    virtual void initialize();
    virtual void destroy();

//Deeper window system integrations
    virtual QPlatformFontDatabase *fontDatabase() const;
#ifndef QT_NO_CLIPBOARD
    virtual QPlatformClipboard *clipboard() const;
#endif
#if QT_CONFIG(draganddrop)
    virtual QPlatformDrag *drag() const;
#endif
    virtual QPlatformInputContext *inputContext() const;
#ifndef QT_NO_ACCESSIBILITY
    virtual QPlatformAccessibility *accessibility() const;
#endif

    // Access native handles. The window handle is already available from Wid;
    virtual QPlatformNativeInterface *nativeInterface() const;

    virtual QPlatformServices *services() const;

    enum StyleHint {
        CursorFlashTime,
        KeyboardInputInterval,
        MouseDoubleClickInterval,
        StartDragDistance,
        StartDragTime,
        KeyboardAutoRepeatRate,
        ShowIsFullScreen,
        PasswordMaskDelay,
        FontSmoothingGamma,
        StartDragVelocity,
        UseRtlExtensions,
        PasswordMaskCharacter,
        SetFocusOnTouchRelease,
        ShowIsMaximized,
        MousePressAndHoldInterval,
        TabFocusBehavior,
        ReplayMousePressOutsidePopup,
        ItemViewActivateItemOnSingleClick,
        UiEffects,
        WheelScrollLines,
        ShowShortcutsInContextMenus,
        MouseQuickSelectionThreshold
    };

    virtual QVariant styleHint(StyleHint hint) const;
    virtual Qt::WindowState defaultWindowState(Qt::WindowFlags) const;

    virtual Qt::KeyboardModifiers queryKeyboardModifiers() const;
    virtual QList<int> possibleKeys(const QKeyEvent *) const;

    virtual QStringList themeNames() const;
    virtual QPlatformTheme *createPlatformTheme(const QString &name) const;

    virtual QPlatformOffscreenSurface *createPlatformOffscreenSurface(QOffscreenSurface *surface) const;

#ifndef QT_NO_SESSIONMANAGER
    virtual QPlatformSessionManager *createPlatformSessionManager(const QString &id, const QString &key) const;
#endif

    virtual void sync();

#ifndef QT_NO_OPENGL
    virtual QOpenGLContext::OpenGLModuleType openGLModuleType();
#endif
    virtual void setApplicationIcon(const QIcon &icon) const;

    virtual void beep() const;

#if QT_CONFIG(vulkan) || defined(Q_CLANG_QDOC)
    virtual QPlatformVulkanInstance *createPlatformVulkanInstance(QVulkanInstance *instance) const;
#endif

protected:
    QPlatformIntegration() = default;
};

QT_END_NAMESPACE

#endif // QPLATFORMINTEGRATION_H
