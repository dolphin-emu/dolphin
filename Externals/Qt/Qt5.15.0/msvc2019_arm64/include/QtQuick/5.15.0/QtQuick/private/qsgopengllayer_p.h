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
#ifndef QSGOPENGLLAYER_P_H
#define QSGOPENGLLAYER_P_H

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
#include <private/qsgcontext_p.h>
#include <private/qsgtexture_p.h>
#include <qsgsimplerectnode.h>

QT_BEGIN_NAMESPACE

#define QSG_DEBUG_FBO_OVERLAY

class QOpenGLFramebufferObject;
class QSGDepthStencilBuffer;
class QSGDefaultRenderContext;
class QSGOpenGLLayerPrivate;

class Q_QUICK_PRIVATE_EXPORT QSGOpenGLLayer : public QSGLayer
{
    Q_DECLARE_PRIVATE(QSGOpenGLLayer)
    Q_OBJECT
public:
    QSGOpenGLLayer(QSGRenderContext *context);
    ~QSGOpenGLLayer();

    bool updateTexture() override;

    // The item's "paint node", not effect node.
    QSGNode *item() const { return m_item; }
    void setItem(QSGNode *item) override;

    QRectF rect() const { return m_rect; }
    void setRect(const QRectF &rect) override;

    QSize size() const { return m_size; }
    void setSize(const QSize &size) override;

    void setHasMipmaps(bool mipmap) override;

    void bind() override;

    bool hasAlphaChannel() const override;
    bool hasMipmaps() const override;
    int textureId() const override;
    QSize textureSize() const override { return m_size; }

    GLenum format() const { return m_format; }
    void setFormat(GLenum format) override;

    bool live() const { return bool(m_live); }
    void setLive(bool live) override;

    bool recursive() const { return bool(m_recursive); }
    void setRecursive(bool recursive) override;

    void setDevicePixelRatio(qreal ratio) override { m_device_pixel_ratio = ratio; }

    bool mirrorHorizontal() const { return bool(m_mirrorHorizontal); }
    void setMirrorHorizontal(bool mirror) override;

    bool mirrorVertical() const { return bool(m_mirrorVertical); }
    void setMirrorVertical(bool mirror) override;

    void scheduleUpdate() override;

    QImage toImage() const override;

    QRectF normalizedTextureSubRect() const override;

    int samples() const { return m_samples; }
    void setSamples(int samples) override { m_samples = samples; }

public Q_SLOTS:
    void markDirtyTexture() override;
    void invalidated() override;

private:
    void grab();

    QSGNode *m_item;
    QRectF m_rect;
    QSize m_size;
    qreal m_device_pixel_ratio;
    GLenum m_format;

    QSGRenderer *m_renderer;
    QOpenGLFramebufferObject *m_fbo;
    QOpenGLFramebufferObject *m_secondaryFbo;
    QSharedPointer<QSGDepthStencilBuffer> m_depthStencilBuffer;

    GLuint m_transparentTexture;

#ifdef QSG_DEBUG_FBO_OVERLAY
    QSGSimpleRectNode *m_debugOverlay;
#endif

    QSGDefaultRenderContext *m_context;
    int m_samples;

    uint m_mipmap : 1;
    uint m_live : 1;
    uint m_recursive : 1;
    uint m_dirtyTexture : 1;
    uint m_multisamplingChecked : 1;
    uint m_multisampling : 1;
    uint m_grab : 1;
    uint m_mirrorHorizontal : 1;
    uint m_mirrorVertical : 1;
};

class QSGOpenGLLayerPrivate : public QSGTexturePrivate
{
    Q_DECLARE_PUBLIC(QSGOpenGLLayer)
public:
    int comparisonKey() const override;
};

QT_END_NAMESPACE

#endif // QSGOPENGLLAYER_P_H
