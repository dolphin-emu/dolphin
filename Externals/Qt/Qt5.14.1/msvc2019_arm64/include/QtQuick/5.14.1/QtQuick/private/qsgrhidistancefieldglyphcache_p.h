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

#ifndef QSGRHIDISTANCEFIELDGLYPHCACHE_H
#define QSGRHIDISTANCEFIELDGLYPHCACHE_H

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

#include "qsgadaptationlayer_p.h"
#include <private/qsgareaallocator_p.h>
#include <QtGui/private/qrhi_p.h>

QT_BEGIN_NAMESPACE

class Q_QUICK_PRIVATE_EXPORT QSGRhiDistanceFieldGlyphCache : public QSGDistanceFieldGlyphCache
{
public:
    QSGRhiDistanceFieldGlyphCache(QRhi *rhi, const QRawFont &font);
    virtual ~QSGRhiDistanceFieldGlyphCache();

    void requestGlyphs(const QSet<glyph_t> &glyphs) override;
    void storeGlyphs(const QList<QDistanceField> &glyphs) override;
    void referenceGlyphs(const QSet<glyph_t> &glyphs) override;
    void releaseGlyphs(const QSet<glyph_t> &glyphs) override;

    bool useTextureResizeWorkaround() const;
    bool createFullSizeTextures() const;
    int maxTextureSize() const;

    void setMaxTextureCount(int max) { m_maxTextureCount = max; }
    int maxTextureCount() const { return m_maxTextureCount; }

    void commitResourceUpdates(QRhiResourceUpdateBatch *mergeInto);

    bool eightBitFormatIsAlphaSwizzled() const override;

private:
    bool loadPregeneratedCache(const QRawFont &font);

    struct TextureInfo {
        QRhiTexture *texture;
        QSize size;
        QRect allocatedArea;
        QDistanceField image;
        int padding = -1;
        QVarLengthArray<QRhiTextureUploadEntry, 16> uploads;

        TextureInfo(const QRect &preallocRect = QRect()) : texture(nullptr), allocatedArea(preallocRect) { }
    };

    void createTexture(TextureInfo *texInfo, int width, int height, const void *pixels);
    void createTexture(TextureInfo *texInfo, int width, int height);
    void resizeTexture(TextureInfo *texInfo, int width, int height);

    TextureInfo *textureInfo(int index)
    {
        for (int i = m_textures.count(); i <= index; ++i) {
            if (createFullSizeTextures())
                m_textures.append(QRect(0, 0, maxTextureSize(), maxTextureSize()));
            else
                m_textures.append(TextureInfo());
        }

        return &m_textures[index];
    }

    QRhi *m_rhi;
    mutable int m_maxTextureSize = 0;
    int m_maxTextureCount = 3;
    QSGAreaAllocator *m_areaAllocator = nullptr;
    QRhiResourceUpdateBatch *m_resourceUpdates = nullptr;
    QList<TextureInfo> m_textures;
    QHash<glyph_t, TextureInfo *> m_glyphsTexture;
    QSet<glyph_t> m_unusedGlyphs;
    QSet<QRhiTexture *> m_pendingDispose;
};

QT_END_NAMESPACE

#endif // QSGRHIDISTANCEFIELDGLYPHCACHE_H
