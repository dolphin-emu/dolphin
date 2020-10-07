/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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
#ifndef QSGRHILAYER_P_H
#define QSGRHILAYER_P_H

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
#include <QtGui/private/qrhi_p.h>

QT_BEGIN_NAMESPACE

class QSGDefaultRenderContext;
class QSGRhiLayerPrivate;

class Q_QUICK_PRIVATE_EXPORT QSGRhiLayer : public QSGLayer
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QSGRhiLayer)
public:
    QSGRhiLayer(QSGRenderContext *context);
    ~QSGRhiLayer();

    bool updateTexture() override;

    bool hasAlphaChannel() const override;
    bool hasMipmaps() const override;
    QSize textureSize() const override { return m_size; }

    void bind() override;
    int textureId() const override;

    void setItem(QSGNode *item) override;
    void setRect(const QRectF &rect) override;
    void setSize(const QSize &size) override;
    void setHasMipmaps(bool mipmap) override;
    void setFormat(uint format) override;
    void setLive(bool live) override;
    void setRecursive(bool recursive) override;
    void setDevicePixelRatio(qreal ratio) override { m_dpr = ratio; }
    void setMirrorHorizontal(bool mirror) override;
    void setMirrorVertical(bool mirror) override;
    QRectF normalizedTextureSubRect() const override;
    void setSamples(int samples) override { m_samples = samples; }

    void scheduleUpdate() override;
    QImage toImage() const override;

public Q_SLOTS:
    void markDirtyTexture() override;
    void invalidated() override;

private:
    void grab();
    void releaseResources();

    QSGNode *m_item = nullptr;
    QRectF m_rect;
    QSize m_size;
    qreal m_dpr = 1;
    QRhiTexture::Format m_format = QRhiTexture::RGBA8;

    QSGRenderer *m_renderer = nullptr;
    QRhiTexture *m_texture = nullptr;
    QRhiRenderBuffer *m_ds = nullptr;
    QRhiRenderBuffer *m_msaaColorBuffer = nullptr;
    QRhiTexture *m_secondaryTexture = nullptr;
    QRhiTextureRenderTarget *m_rt = nullptr;
    QRhiRenderPassDescriptor *m_rtRp = nullptr;

    QSGDefaultRenderContext *m_context = nullptr;
    QRhi *m_rhi = nullptr;
    int m_samples = 0;

    uint m_mipmap : 1;
    uint m_live : 1;
    uint m_recursive : 1;
    uint m_dirtyTexture : 1;
    uint m_multisampling : 1;
    uint m_grab : 1;
    uint m_mirrorHorizontal : 1;
    uint m_mirrorVertical : 1;
};

class QSGRhiLayerPrivate : public QSGTexturePrivate
{
    Q_DECLARE_PUBLIC(QSGRhiLayer)
public:
    int comparisonKey() const override;
    QRhiTexture *rhiTexture() const override;
    void updateRhiTexture(QRhi *rhi, QRhiResourceUpdateBatch *resourceUpdates) override;
};

QT_END_NAMESPACE

#endif // QSGRHILAYER_P_H
