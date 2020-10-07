/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#ifndef QSGCOMPRESSEDATLASTEXTURE_P_H
#define QSGCOMPRESSEDATLASTEXTURE_P_H

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
#include <QtQuick/private/qsgareaallocator_p.h>
#include <QtQuick/private/qsgopenglatlastexture_p.h>

QT_BEGIN_NAMESPACE

class QSGCompressedTextureFactory;

namespace QSGCompressedAtlasTexture {

class Texture;

class Atlas : public QSGOpenGLAtlasTexture::AtlasBase
{
public:
    Atlas(const QSize &size, uint format);
    ~Atlas();

    void generateTexture() override;
    void uploadPendingTexture(int i) override;

    Texture *create(const QByteArray &data, int dataLength, int dataOffset, const QSize &size, const QSize &paddedSize);

    uint format() const { return m_format; }

private:
    uint m_format;
};

class Texture : public QSGOpenGLAtlasTexture::TextureBase
{
    Q_OBJECT
public:
    Texture(Atlas *atlas, const QRect &textureRect, const QByteArray &data, int dataLength, int dataOffset, const QSize &size);
    ~Texture();

    QSize textureSize() const override { return m_size; }
    bool hasAlphaChannel() const override;
    bool hasMipmaps() const override { return false; }

    QRectF normalizedTextureSubRect() const override { return m_texture_coords_rect; }

    QSGTexture *removedFromAtlas() const override;

    const QByteArray &data() const { return m_data; }
    int sizeInBytes() const { return m_dataLength; }
    int dataOffset() const { return m_dataOffset; }

private:
    QRectF m_texture_coords_rect;
    mutable QSGTexture *m_nonatlas_texture;
    QByteArray m_data;
    QSize m_size;
    int m_dataLength;
    int m_dataOffset;
};

}

QT_END_NAMESPACE

#endif
