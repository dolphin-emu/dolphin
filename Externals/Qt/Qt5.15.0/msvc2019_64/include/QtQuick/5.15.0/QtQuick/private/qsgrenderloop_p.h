/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#ifndef QSGRENDERLOOP_P_H
#define QSGRENDERLOOP_P_H

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

#include <QtGui/QImage>
#include <QtGui/QSurface>
#include <private/qtquickglobal_p.h>
#include <QtCore/QSet>

QT_BEGIN_NAMESPACE

class QQuickWindow;
class QSGContext;
class QSGRenderContext;
class QAnimationDriver;
class QRunnable;

class Q_QUICK_PRIVATE_EXPORT QSGRenderLoop : public QObject
{
    Q_OBJECT

public:
    enum RenderLoopFlags {
        SupportsGrabWithoutExpose = 0x01
    };

    virtual ~QSGRenderLoop();

    virtual void show(QQuickWindow *window) = 0;
    virtual void hide(QQuickWindow *window) = 0;
    virtual void resize(QQuickWindow *) {};

    virtual void windowDestroyed(QQuickWindow *window) = 0;

    virtual void exposureChanged(QQuickWindow *window) = 0;
    virtual QImage grab(QQuickWindow *window) = 0;

    virtual void update(QQuickWindow *window) = 0;
    virtual void maybeUpdate(QQuickWindow *window) = 0;
    virtual void handleUpdateRequest(QQuickWindow *) { }

    virtual QAnimationDriver *animationDriver() const = 0;

    virtual QSGContext *sceneGraphContext() const = 0;
    virtual QSGRenderContext *createRenderContext(QSGContext *) const = 0;

    virtual void releaseResources(QQuickWindow *window) = 0;
    virtual void postJob(QQuickWindow *window, QRunnable *job);

    void addWindow(QQuickWindow *win) { m_windows.insert(win); }
    void removeWindow(QQuickWindow *win) { m_windows.remove(win); }
    QSet<QQuickWindow *> windows() const { return m_windows; }

    virtual QSurface::SurfaceType windowSurfaceType() const;

    // ### make this less of a singleton
    static QSGRenderLoop *instance();
    static void setInstance(QSGRenderLoop *instance);

    virtual bool interleaveIncubation() const { return false; }

    virtual int flags() const { return 0; }

    static void cleanup();

    void handleContextCreationFailure(QQuickWindow *window);

Q_SIGNALS:
    void timeToIncubate();

private:
    static QSGRenderLoop *s_instance;

    QSet<QQuickWindow *> m_windows;
};

enum QSGRenderLoopType
{
    BasicRenderLoop,
    ThreadedRenderLoop,
    WindowsRenderLoop
};

QT_END_NAMESPACE

#endif // QSGRENDERLOOP_P_H
