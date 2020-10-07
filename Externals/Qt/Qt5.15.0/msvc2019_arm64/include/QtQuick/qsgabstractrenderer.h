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

#ifndef QSGABSTRACTRENDERER_H
#define QSGABSTRACTRENDERER_H

#include <QtQuick/qsgnode.h>

#ifndef GLuint
#define GLuint uint
#endif

QT_BEGIN_NAMESPACE

class QSGAbstractRendererPrivate;

class Q_QUICK_EXPORT QSGAbstractRenderer : public QObject
{
    Q_OBJECT
public:
    enum ClearModeBit
    {
        ClearColorBuffer    = 0x0001,
        ClearDepthBuffer    = 0x0002,
        ClearStencilBuffer  = 0x0004
    };
    Q_DECLARE_FLAGS(ClearMode, ClearModeBit)
    Q_FLAG(ClearMode)

    enum MatrixTransformFlag
    {
        MatrixTransformFlipY = 0x01
    };
    Q_DECLARE_FLAGS(MatrixTransformFlags, MatrixTransformFlag)
    Q_FLAG(MatrixTransformFlags)

    ~QSGAbstractRenderer() override;

    // just have a warning about becoming private, ifdefing the whole class is not feasible
#if QT_DEPRECATED_SINCE(5, 15)
    QT_DEPRECATED_X("QSGAbstractRenderer is no longer going to be public in Qt 6.0. QSGEngine-based workflows are expected to migrate to QQuickRenderControl instead.")
#endif
    void setRootNode(QSGRootNode *node);
    QSGRootNode *rootNode() const;
    void setDeviceRect(const QRect &rect);
    inline void setDeviceRect(const QSize &size) { setDeviceRect(QRect(QPoint(), size)); }
    QRect deviceRect() const;

    void setViewportRect(const QRect &rect);
    inline void setViewportRect(const QSize &size) { setViewportRect(QRect(QPoint(), size)); }
    QRect viewportRect() const;

    void setProjectionMatrixToRect(const QRectF &rect);
    void setProjectionMatrixToRect(const QRectF &rect, MatrixTransformFlags flags);
    void setProjectionMatrix(const QMatrix4x4 &matrix);
    void setProjectionMatrixWithNativeNDC(const QMatrix4x4 &matrix);
    QMatrix4x4 projectionMatrix() const;
    QMatrix4x4 projectionMatrixWithNativeNDC() const;

    void setClearColor(const QColor &color);
    QColor clearColor() const;

    void setClearMode(ClearMode mode);
    ClearMode clearMode() const;

    virtual void renderScene(GLuint fboId = 0) = 0;

Q_SIGNALS:
    void sceneGraphChanged();

protected:
    explicit QSGAbstractRenderer(QObject *parent = nullptr);
    virtual void nodeChanged(QSGNode *node, QSGNode::DirtyState state) = 0;

private:
    Q_DECLARE_PRIVATE(QSGAbstractRenderer)
    friend class QSGRootNode;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QSGAbstractRenderer::ClearMode)

QT_END_NAMESPACE

#endif
