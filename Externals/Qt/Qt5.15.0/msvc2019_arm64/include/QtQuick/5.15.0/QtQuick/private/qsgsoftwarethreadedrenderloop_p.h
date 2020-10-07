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

#ifndef QSGSOFTWARETHREADEDRENDERLOOP_H
#define QSGSOFTWARETHREADEDRENDERLOOP_H

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

#include <private/qsgrenderloop_p.h>

QT_BEGIN_NAMESPACE

class QSGSoftwareRenderThread;
class QSGSoftwareContext;

class QSGSoftwareThreadedRenderLoop : public QSGRenderLoop
{
    Q_OBJECT
public:
    QSGSoftwareThreadedRenderLoop();
    ~QSGSoftwareThreadedRenderLoop();

    void show(QQuickWindow *window) override;
    void hide(QQuickWindow *window) override;
    void resize(QQuickWindow *window) override;
    void windowDestroyed(QQuickWindow *window) override;
    void exposureChanged(QQuickWindow *window) override;
    QImage grab(QQuickWindow *window) override;
    void update(QQuickWindow *window) override;
    void maybeUpdate(QQuickWindow *window) override;
    void handleUpdateRequest(QQuickWindow *window) override;
    QAnimationDriver *animationDriver() const override;
    QSGContext *sceneGraphContext() const override;
    QSGRenderContext *createRenderContext(QSGContext *) const override;
    void releaseResources(QQuickWindow *window) override;
    void postJob(QQuickWindow *window, QRunnable *job) override;
    QSurface::SurfaceType windowSurfaceType() const override;
    bool interleaveIncubation() const override;
    int flags() const override;

    bool event(QEvent *e) override;

public Q_SLOTS:
    void onAnimationStarted();
    void onAnimationStopped();

private:
    struct WindowData {
        QQuickWindow *window;
        QSGSoftwareRenderThread *thread;
        uint updateDuringSync : 1;
        uint forceRenderPass : 1;
    };

    void startOrStopAnimationTimer();
    void handleExposure(QQuickWindow *window);
    void handleObscurity(WindowData *w);
    void scheduleUpdate(WindowData *w);
    void handleResourceRelease(WindowData *w, bool destroying);
    void polishAndSync(WindowData *w, bool inExpose);

    QSGSoftwareContext *m_sg;
    QAnimationDriver *m_anim;
    int animationTimer = 0;
    bool lockedForSync = false;
    QVector<WindowData> m_windows;

    friend class QSGSoftwareRenderThread;
};

QT_END_NAMESPACE

#endif // QSGSOFTWARETHREADEDRENDERLOOP_H
