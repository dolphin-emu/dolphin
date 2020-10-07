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

#ifndef QSGRENDERNODE_H
#define QSGRENDERNODE_H

#include <QtQuick/qsgnode.h>

QT_BEGIN_NAMESPACE

class QSGRenderNodePrivate;

class Q_QUICK_EXPORT QSGRenderNode : public QSGNode
{
public:
    enum StateFlag {
        DepthState = 0x01,
        StencilState = 0x02,
        ScissorState = 0x04,
        ColorState = 0x08,
        BlendState = 0x10,
        CullState = 0x20,
        ViewportState = 0x40,
        RenderTargetState = 0x80
    };
    Q_DECLARE_FLAGS(StateFlags, StateFlag)

    enum RenderingFlag {
        BoundedRectRendering = 0x01,
        DepthAwareRendering = 0x02,
        OpaqueRendering = 0x04
    };
    Q_DECLARE_FLAGS(RenderingFlags, RenderingFlag)

    struct Q_QUICK_EXPORT RenderState {
        virtual ~RenderState();
        virtual const QMatrix4x4 *projectionMatrix() const = 0;
        virtual QRect scissorRect() const = 0;
        virtual bool scissorEnabled() const = 0;
        virtual int stencilValue() const = 0;
        virtual bool stencilEnabled() const = 0;
        virtual const QRegion *clipRegion() const = 0;
        virtual void *get(const char *state) const;
    };

    QSGRenderNode();
    ~QSGRenderNode() override;

    virtual StateFlags changedStates() const;
    virtual void render(const RenderState *state) = 0;
    virtual void releaseResources();
    virtual RenderingFlags flags() const;
    virtual QRectF rect() const;

    const QMatrix4x4 *matrix() const;
    const QSGClipNode *clipList() const;
    qreal inheritedOpacity() const;

private:
    QSGRenderNodePrivate *d;
    friend class QSGRenderNodePrivate;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QSGRenderNode::StateFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(QSGRenderNode::RenderingFlags)

QT_END_NAMESPACE

#endif
