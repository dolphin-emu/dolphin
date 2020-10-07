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

#ifndef QQUICKFRAMEBUFFEROBJECT_H
#define QQUICKFRAMEBUFFEROBJECT_H

#include <QtQuick/QQuickItem>

QT_BEGIN_NAMESPACE

class QOpenGLFramebufferObject;
class QQuickFramebufferObjectPrivate;
class QSGFramebufferObjectNode;

// ### Qt 6: Consider what to do here. QQuickFbo supports both direct OpenGL and
// OpenGL via QRhi, but it cannot function when running with another rhi backend.

class Q_QUICK_EXPORT QQuickFramebufferObject : public QQuickItem
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQuickFramebufferObject)

    Q_PROPERTY(bool textureFollowsItemSize READ textureFollowsItemSize WRITE setTextureFollowsItemSize NOTIFY textureFollowsItemSizeChanged)
    Q_PROPERTY(bool mirrorVertically READ mirrorVertically WRITE setMirrorVertically NOTIFY mirrorVerticallyChanged)

public:

    class Q_QUICK_EXPORT Renderer {
    protected:
        Renderer();
        virtual ~Renderer();
        virtual void render() = 0;
        virtual QOpenGLFramebufferObject *createFramebufferObject(const QSize &size);
        virtual void synchronize(QQuickFramebufferObject *);
        QOpenGLFramebufferObject *framebufferObject() const;
        void update();
        void invalidateFramebufferObject();
    private:
        friend class QSGFramebufferObjectNode;
        friend class QQuickFramebufferObject;
        void *data;
    };

    QQuickFramebufferObject(QQuickItem *parent = nullptr);

    bool textureFollowsItemSize() const;
    void setTextureFollowsItemSize(bool follows);

    bool mirrorVertically() const;
    void setMirrorVertically(bool enable);

    virtual Renderer *createRenderer() const = 0;

    bool isTextureProvider() const override;
    QSGTextureProvider *textureProvider() const override;
    void releaseResources() override;

protected:
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;

protected:
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;

Q_SIGNALS:
    void textureFollowsItemSizeChanged(bool);
    void mirrorVerticallyChanged(bool);

private Q_SLOTS:
    void invalidateSceneGraph();
};

QT_END_NAMESPACE

#endif // QQUICKFRAMEBUFFEROBJECT_H
