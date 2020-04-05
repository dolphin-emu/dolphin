/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QOPENGLCOMPOSITOR_H
#define QOPENGLCOMPOSITOR_H

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

#include <QtCore/QTimer>
#include <QtGui/QOpenGLTextureBlitter>
#include <QtGui/QMatrix4x4>

QT_BEGIN_NAMESPACE

class QOpenGLContext;
class QOpenGLFramebufferObject;
class QWindow;
class QPlatformTextureList;

class QOpenGLCompositorWindow
{
public:
    virtual ~QOpenGLCompositorWindow() { }
    virtual QWindow *sourceWindow() const = 0;
    virtual const QPlatformTextureList *textures() const = 0;
    virtual void beginCompositing() { }
    virtual void endCompositing() { }
};

class QOpenGLCompositor : public QObject
{
    Q_OBJECT

public:
    static QOpenGLCompositor *instance();
    static void destroy();

    void setTarget(QOpenGLContext *context, QWindow *window, const QRect &nativeTargetGeometry);
    void setRotation(int degrees);
    QOpenGLContext *context() const { return m_context; }
    QWindow *targetWindow() const { return m_targetWindow; }

    void update();
    QImage grab();

    QList<QOpenGLCompositorWindow *> windows() const { return m_windows; }
    void addWindow(QOpenGLCompositorWindow *window);
    void removeWindow(QOpenGLCompositorWindow *window);
    void moveToTop(QOpenGLCompositorWindow *window);
    void changeWindowIndex(QOpenGLCompositorWindow *window, int newIdx);

signals:
    void topWindowChanged(QOpenGLCompositorWindow *window);

private slots:
    void handleRenderAllRequest();

private:
    QOpenGLCompositor();
    ~QOpenGLCompositor();

    void renderAll(QOpenGLFramebufferObject *fbo);
    void render(QOpenGLCompositorWindow *window);

    QOpenGLContext *m_context;
    QWindow *m_targetWindow;
    QRect m_nativeTargetGeometry;
    int m_rotation;
    QMatrix4x4 m_rotationMatrix;
    QTimer m_updateTimer;
    QOpenGLTextureBlitter m_blitter;
    QList<QOpenGLCompositorWindow *> m_windows;
};

QT_END_NAMESPACE

#endif // QOPENGLCOMPOSITOR_H
