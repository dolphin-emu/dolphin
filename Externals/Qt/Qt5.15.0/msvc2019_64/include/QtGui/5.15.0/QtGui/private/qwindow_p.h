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

#ifndef QWINDOW_P_H
#define QWINDOW_P_H

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

#include <QtGui/private/qtguiglobal_p.h>
#include <QtGui/qscreen.h>
#include <QtGui/qwindow.h>
#include <qpa/qplatformwindow.h>

#include <QtCore/private/qobject_p.h>
#include <QtCore/qelapsedtimer.h>
#include <QtGui/QIcon>

QT_BEGIN_NAMESPACE

#define QWINDOWSIZE_MAX ((1<<24)-1)

class Q_GUI_EXPORT QWindowPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QWindow)

public:
    enum PositionPolicy
    {
        WindowFrameInclusive,
        WindowFrameExclusive
    };

    QWindowPrivate()
        : QObjectPrivate()
    {
        isWindow = true;
    }

    void init(QScreen *targetScreen = nullptr);

    void maybeQuitOnLastWindowClosed();
#ifndef QT_NO_CURSOR
    void setCursor(const QCursor *c = nullptr);
    bool applyCursor();
#endif

    QPoint globalPosition() const;

    QWindow *topLevelWindow(QWindow::AncestorMode mode = QWindow::IncludeTransients) const;

#if QT_CONFIG(opengl)
    virtual QOpenGLContext *shareContext() const;
#endif

    virtual QWindow *eventReceiver() { Q_Q(QWindow); return q; }

    virtual void setVisible(bool visible);
    void updateVisibility();
    void _q_clearAlert();

    enum SiblingPosition { PositionTop, PositionBottom };
    void updateSiblingPosition(SiblingPosition);

    bool windowRecreationRequired(QScreen *newScreen) const;
    void create(bool recursive, WId nativeHandle = 0);
    void destroy();
    void setTopLevelScreen(QScreen *newScreen, bool recreate);
    void connectToScreen(QScreen *topLevelScreen);
    void disconnectFromScreen();
    void emitScreenChangedRecursion(QScreen *newScreen);
    QScreen *screenForGeometry(const QRect &rect) const;
    void setTransientParent(QWindow *parent);

    virtual void clearFocusObject();
    virtual QRectF closestAcceptableGeometry(const QRectF &rect) const;

    virtual void processSafeAreaMarginsChanged() {};

    bool isPopup() const { return (windowFlags & Qt::WindowType_Mask) == Qt::Popup; }
    void setAutomaticPositionAndResizeEnabled(bool a)
    { positionAutomatic = resizeAutomatic = a; }

    static QWindowPrivate *get(QWindow *window) { return window->d_func(); }

    static Qt::WindowState effectiveState(Qt::WindowStates);

    // ### Qt6: unused
    virtual bool allowClickThrough(const QPoint &) const { return true; }

    QWindow::SurfaceType surfaceType = QWindow::RasterSurface;
    Qt::WindowFlags windowFlags = Qt::Window;
    QWindow *parentWindow = nullptr;
    QPlatformWindow *platformWindow = nullptr;
    bool visible= false;
    bool visibilityOnDestroy = false;
    bool exposed = false;
    QSurfaceFormat requestedFormat;
    QString windowTitle;
    QString windowFilePath;
    QIcon windowIcon;
    QRect geometry;
    Qt::WindowStates windowState = Qt::WindowNoState;
    QWindow::Visibility visibility = QWindow::Hidden;
    bool resizeEventPending = true;
    bool receivedExpose = false;
    PositionPolicy positionPolicy = WindowFrameExclusive;
    bool positionAutomatic = true;
    // resizeAutomatic suppresses resizing by QPlatformWindow::initialGeometry().
    // It also indicates that width/height=0 is acceptable (for example, for
    // the QRollEffect widget) and is thus not cleared in setGeometry().
    // An alternative approach might be using -1,-1 as a default size.
    bool resizeAutomatic = true;
    Qt::ScreenOrientation contentOrientation = Qt::PrimaryOrientation;
    qreal opacity= 1;
    QRegion mask;

    QSize minimumSize = {0, 0};
    QSize maximumSize = {QWINDOWSIZE_MAX, QWINDOWSIZE_MAX};
    QSize baseSize;
    QSize sizeIncrement;

    Qt::WindowModality modality = Qt::NonModal;
    bool blockedByModalWindow = false;

    bool updateRequestPending = false;
    bool transientParentPropertySet = false;

    QPointer<QWindow> transientParent;
    QPointer<QScreen> topLevelScreen;

#ifndef QT_NO_CURSOR
    QCursor cursor = {Qt::ArrowCursor};
    bool hasCursor = false;
#endif

    bool compositing = false;
    QElapsedTimer lastComposeTime;

#if QT_CONFIG(vulkan)
    QVulkanInstance *vulkanInstance = nullptr;
#endif
};


QT_END_NAMESPACE

#endif // QWINDOW_P_H
