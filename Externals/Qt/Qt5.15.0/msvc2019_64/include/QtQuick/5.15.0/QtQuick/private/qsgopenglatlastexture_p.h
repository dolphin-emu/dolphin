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

#ifndef QSGOPENGLATLASTEXTURE_P_H
#define QSGOPENGLATLASTEXTURE_P_H

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

#include <QtGui/qopengl.h>

#include <QtQuick/QSGTexture>
#include <QtQuick/private/qsgplaintexture_p.h>
#include <QtQuick/private/qsgareaallocator_p.h>

QT_BEGIN_NAMESPACE

namespace QSGCompressedAtlasTexture {
    class Atlas;
}
class QSGCompressedTextureFactory;

namespace QSGOpenGLAtlasTexture
{

class Texture;
class TextureBase;
class Atlas;

class Manager : public QObject
{
    Q_OBJECT

public:
    Manager(const QSize &surfacePixelSize);
    ~Manager();

    QSGTexture *create(const QImage &image, bool hasAlphaChannel);
    QSGTexture *create(const QSGCompressedTextureFactory *factory);
    void invalidate();

private:
    Atlas *m_atlas;
    // set of atlases for different compressed formats
    QHash<unsigned int, QSGCompressedAtlasTexture::Atlas*> m_atlases;

    QSize m_atlas_size;
    int m_atlas_size_limit;
};

class AtlasBase : public QObject
{
    Q_OBJECT
public:
    AtlasBase(const QSize &size);
    ~AtlasBase();

    void invalidate();

    int textureId() const;
    void bind(QSGTexture::Filtering filtering);

    void remove(TextureBase *t);

    QSize size() const { return m_size; }

protected:
    virtual void generateTexture() = 0;
    virtual void uploadPendingTexture(int i) = 0;

protected:
    QSGAreaAllocator m_allocator;
    unsigned int m_texture_id;
    QSize m_size;
    QList<TextureBase *> m_pending_uploads;

private:
    bool m_allocated;
};

class Atlas : public AtlasBase
{
public:
    Atlas(const QSize &size);
    ~Atlas();

    void generateTexture() override;
    void uploadPendingTexture(int i) override;

    void upload(Texture *texture);
    void uploadBgra(Texture *texture);

    Texture *create(const QImage &image);

    uint internalFormat() const { return m_internalFormat; }
    uint externalFormat() const { return m_externalFormat; }

private:
    uint m_internalFormat;
    uint m_externalFormat;

    int m_atlas_transient_image_threshold;

    uint m_use_bgra_fallback: 1;
    uint m_debug_overlay : 1;
};

class TextureBase : public QSGTexture
{
    Q_OBJECT
public:
    TextureBase(AtlasBase *atlas, const QRect &textureRect);
    ~TextureBase();

    int textureId() const override { return m_atlas->textureId(); }
    bool isAtlasTexture() const override { return true; }

    QRect atlasSubRect() const { return m_allocated_rect; }

    void bind() override;

protected:
    QRect m_allocated_rect;
    AtlasBase *m_atlas;
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
    mutable QSGPlainTexture *m_nonatlas_texture;
    bool m_has_alpha;
};

}

QT_END_NAMESPACE

#endif
