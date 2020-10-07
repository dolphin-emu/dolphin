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

#ifndef QSGRHIATLASTEXTURE_P_H
#define QSGRHIATLASTEXTURE_P_H

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

#include <QtCore/QSize>
#include <QtQuick/private/qsgplaintexture_p.h>
#include <QtQuick/private/qsgareaallocator_p.h>
#include <QtGui/QSurface>
#include <QtGui/private/qrhi_p.h>

QT_BEGIN_NAMESPACE

class QSGDefaultRenderContext;

namespace QSGCompressedAtlasTexture {
    class Atlas;
}
class QSGCompressedTextureFactory;

namespace QSGRhiAtlasTexture
{

class Texture;
class TextureBase;
class TextureBasePrivate;
class Atlas;

class Manager : public QObject
{
    Q_OBJECT

public:
    Manager(QSGDefaultRenderContext *rc, const QSize &surfacePixelSize, QSurface *maybeSurface);
    ~Manager();

    QSGTexture *create(const QImage &image, bool hasAlphaChannel);
    QSGTexture *create(const QSGCompressedTextureFactory *factory);
    void invalidate();

private:
    QSGDefaultRenderContext *m_rc;
    QRhi *m_rhi;
    Atlas *m_atlas = nullptr;
    // set of atlases for different compressed formats
    QHash<unsigned int, QSGCompressedAtlasTexture::Atlas*> m_atlases;

    QSize m_atlas_size;
    int m_atlas_size_limit;
};

class AtlasBase : public QObject
{
    Q_OBJECT
public:
    AtlasBase(QSGDefaultRenderContext *rc, const QSize &size);
    ~AtlasBase();

    void invalidate();
    void updateRhiTexture(QRhiResourceUpdateBatch *resourceUpdates);
    void remove(TextureBase *t);

    QSGDefaultRenderContext *renderContext() const { return m_rc; }
    QRhi *rhi() const { return m_rhi; }
    QRhiTexture *texture() const { return m_texture; }
    QSize size() const { return m_size; }

protected:
    virtual bool generateTexture() = 0;
    virtual void enqueueTextureUpload(TextureBase *t, QRhiResourceUpdateBatch *resourceUpdates) = 0;

protected:
    QSGDefaultRenderContext *m_rc;
    QRhi *m_rhi;
    QSGAreaAllocator m_allocator;
    QRhiTexture *m_texture = nullptr;
    QSize m_size;
    QVector<TextureBase *> m_pending_uploads;
    friend class TextureBase;
    friend class TextureBasePrivate;

private:
    bool m_allocated = false;
};

class Atlas : public AtlasBase
{
public:
    Atlas(QSGDefaultRenderContext *rc, const QSize &size);
    ~Atlas();

    bool generateTexture() override;
    void enqueueTextureUpload(TextureBase *t, QRhiResourceUpdateBatch *resourceUpdates) override;

    Texture *create(const QImage &image);

    QRhiTexture::Format format() const { return m_format; }

private:
    QRhiTexture::Format m_format;
    int m_atlas_transient_image_threshold = 0;

    uint m_debug_overlay : 1;
};

class TextureBase : public QSGTexture
{
    Q_DECLARE_PRIVATE(TextureBase)
    Q_OBJECT
public:
    TextureBase(AtlasBase *atlas, const QRect &textureRect);
    ~TextureBase();

    int textureId() const override { return 0; } // not used
    void bind() override { } // not used

    bool isAtlasTexture() const override { return true; }
    QRect atlasSubRect() const { return m_allocated_rect; }

    QRhiResourceUpdateBatch *workResourceUpdateBatch() const;

protected:
    QRect m_allocated_rect;
    AtlasBase *m_atlas;
};

class TextureBasePrivate : public QSGTexturePrivate
{
    Q_DECLARE_PUBLIC(TextureBase)
public:
    int comparisonKey() const override;
    QRhiTexture *rhiTexture() const override;
    void updateRhiTexture(QRhi *rhi, QRhiResourceUpdateBatch *resourceUpdates) override;
};

class Texture : public TextureBase
{
    Q_OBJECT
public:
    Texture(Atlas *atlas, const QRect &textureRect, const QImage &image);
    ~Texture();

    QSize textureSize() const override { return atlasSubRectWithoutPadding().size(); }
    void setHasAlphaChannel(bool alpha) { m_has_alpha = alpha; }
    bool hasAlphaChannel() const override { return m_has_alpha; }
    bool hasMipmaps() const override { return false; }

    QRectF normalizedTextureSubRect() const override { return m_texture_coords_rect; }

    QRect atlasSubRect() const { return m_allocated_rect; }
    QRect atlasSubRectWithoutPadding() const { return m_allocated_rect.adjusted(1, 1, -1, -1); }

    QSGTexture *removedFromAtlas() const override;

    void releaseImage() { m_image = QImage(); }
    const QImage &image() const { return m_image; }

private:
    QRectF m_texture_coords_rect;
    QImage m_image;
    mutable QSGPlainTexture *m_nonatlas_texture = nullptr;
    bool m_has_alpha;
};

}

QT_END_NAMESPACE

#endif
