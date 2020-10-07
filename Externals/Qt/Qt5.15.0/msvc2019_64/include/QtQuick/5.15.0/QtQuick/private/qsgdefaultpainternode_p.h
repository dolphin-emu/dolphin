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

#ifndef QSGDEFAULTPAINTERNODE_P_H
#define QSGDEFAULTPAINTERNODE_P_H

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

#include <private/qsgadaptationlayer_p.h>
#include "qsgtexturematerial.h"
#include "qsgplaintexture_p.h"

#include <QtQuick/qquickpainteditem.h>

#include <QtGui/qcolor.h>

QT_BEGIN_NAMESPACE

class QOpenGLFramebufferObject;
class QOpenGLPaintDevice;
class QSGDefaultRenderContext;
class QSGPainterTexturePrivate;

class Q_QUICK_PRIVATE_EXPORT QSGPainterTexture : public QSGPlainTexture
{
    Q_DECLARE_PRIVATE(QSGPainterTexture)
public:
    QSGPainterTexture();

    void setDirtyRect(const QRect &rect) { m_dirty_rect = rect; }

    void bind() override;

private:
    QRect m_dirty_rect;
};

class QSGPainterTexturePrivate : public QSGPlainTexturePrivate
{
    Q_DECLARE_PUBLIC(QSGPainterTexture)
public:
    void updateRhiTexture(QRhi *rhi, QRhiResourceUpdateBatch *resourceUpdates) override;
};

class Q_QUICK_PRIVATE_EXPORT QSGDefaultPainterNode : public QSGPainterNode
{
public:
    QSGDefaultPainterNode(QQuickPaintedItem *item);
    virtual ~QSGDefaultPainterNode();

    void setPreferredRenderTarget(QQuickPaintedItem::RenderTarget target) override;

    void setSize(const QSize &size) override;
    QSize size() const { return m_size; }

    void setDirty(const QRect &dirtyRect = QRect()) override;

    void setOpaquePainting(bool opaque) override;
    bool opaquePainting() const { return m_opaquePainting; }

    void setLinearFiltering(bool linearFiltering) override;
    bool linearFiltering() const { return m_linear_filtering; }

    void setMipmapping(bool mipmapping) override;
    bool mipmapping() const { return m_mipmapping; }

    void setSmoothPainting(bool s) override;
    bool smoothPainting() const { return m_smoothPainting; }

    void setFillColor(const QColor &c) override;
    QColor fillColor() const { return m_fillColor; }

    void setContentsScale(qreal s) override;
    qreal contentsScale() const { return m_contentsScale; }

    void setFastFBOResizing(bool fastResizing) override;
    bool fastFBOResizing() const { return m_fastFBOResizing; }

    void setTextureSize(const QSize &textureSize) override;
    QSize textureSize() const { return m_textureSize; }

    QImage toImage() const override;
    void update() override;

    void paint();

    QSGTexture *texture() const override { return m_texture; }

private:
    void updateTexture();
    void updateGeometry();
    void updateRenderTarget();
    void updateFBOSize();

    QSGDefaultRenderContext *m_context;

    QQuickPaintedItem::RenderTarget m_preferredRenderTarget;
    QQuickPaintedItem::RenderTarget m_actualRenderTarget;

    QQuickPaintedItem *m_item;

    QOpenGLFramebufferObject *m_fbo;
    QOpenGLFramebufferObject *m_multisampledFbo;
    QImage m_image;

    QSGOpaqueTextureMaterial m_material;
    QSGTextureMaterial m_materialO;
    QSGGeometry m_geometry;
    QSGPainterTexture *m_texture;
    QOpenGLPaintDevice *m_gl_device;

    QSize m_size;
    QSize m_fboSize;
    QSize m_textureSize;
    QRect m_dirtyRect;
    QColor m_fillColor;
#if QT_VERSION >= 0x060000
#warning "Remove m_contentsScale and assume 1 everywhere"
#endif
    qreal m_contentsScale;

    bool m_dirtyContents : 1;
    bool m_opaquePainting : 1;
    bool m_linear_filtering : 1;
    bool m_mipmapping : 1;
    bool m_smoothPainting : 1;
    bool m_extensionsChecked : 1;
    bool m_multisamplingSupported : 1;
    bool m_fastFBOResizing : 1;
    bool m_dirtyGeometry : 1;
    bool m_dirtyRenderTarget : 1;
    bool m_dirtyTexture : 1;
};

QT_END_NAMESPACE

#endif // QSGDEFAULTPAINTERNODE_P_H
