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

#ifndef QSGOPENGLDISTANCEFIELDGLYPHCACHE_H
#define QSGOPENGLDISTANCEFIELDGLYPHCACHE_H

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
#include <QtGui/qopenglfunctions.h>
#include <qopenglshaderprogram.h>
#include <qopenglbuffer.h>
#include <qopenglvertexarrayobject.h>
#include <QtGui/private/qopenglengineshadersource_p.h>
#include <private/qsgareaallocator_p.h>

QT_BEGIN_NAMESPACE

class QOpenGLSharedResourceGuard;
#if !defined(QT_OPENGL_ES_2)
class QOpenGLFunctions_3_2_Core;
#endif

class Q_QUICK_PRIVATE_EXPORT QSGOpenGLDistanceFieldGlyphCache : public QSGDistanceFieldGlyphCache
{
public:
    QSGOpenGLDistanceFieldGlyphCache(QOpenGLContext *c, const QRawFont &font);
    virtual ~QSGOpenGLDistanceFieldGlyphCache();

    void requestGlyphs(const QSet<glyph_t> &glyphs) override;
    void storeGlyphs(const QList<QDistanceField> &glyphs) override;
    void referenceGlyphs(const QSet<glyph_t> &glyphs) override;
    void releaseGlyphs(const QSet<glyph_t> &glyphs) override;

    bool useTextureResizeWorkaround() const;
    bool useTextureUploadWorkaround() const;
    bool createFullSizeTextures() const;

    void setMaxTextureCount(int max) { m_maxTextureCount = max; }
    int maxTextureCount() const { return m_maxTextureCount; }

    bool eightBitFormatIsAlphaSwizzled() const override { return !m_coreProfile; }

private:
    bool loadPregeneratedCache(const QRawFont &font);
    inline bool isCoreProfile() const { return m_coreProfile; }

    struct TextureInfo {
        GLuint texture;
        QSize size;
        QRect allocatedArea;
        QDistanceField image;
        int padding = -1;

        TextureInfo(const QRect &preallocRect = QRect(0, 0, 1, 1)) : texture(0), allocatedArea(preallocRect) { }
    };

    void createTexture(TextureInfo * texInfo, int width, int height, const void *pixels);
    void createTexture(TextureInfo * texInfo, int width, int height);
    void resizeTexture(TextureInfo * texInfo, int width, int height);

    TextureInfo *textureInfo(int index)
    {
        Q_ASSERT(m_maxTextureWidth > 0 && m_maxTextureHeight > 0);
        for (int i = m_textures.count(); i <= index; ++i) {
            if (createFullSizeTextures())
                m_textures.append(QRect(0, 0, m_maxTextureWidth, m_maxTextureWidth));
            else
                m_textures.append(TextureInfo());
        }

        return &m_textures[index];
    }

    void createBlitProgram()
    {
        m_blitProgram = new QOpenGLShaderProgram;
        {
            const QString source = QLatin1String(qopenglslMainWithTexCoordsVertexShader)
                                 + QLatin1String(qopenglslUntransformedPositionVertexShader);

            m_blitProgram->addCacheableShaderFromSourceCode(QOpenGLShader::Vertex, source);
        }
        {
            const QString source = QLatin1String(qopenglslMainFragmentShader)
                                 + QLatin1String(qopenglslImageSrcFragmentShader);

            m_blitProgram->addCacheableShaderFromSourceCode(QOpenGLShader::Fragment, source);
        }
        m_blitProgram->bindAttributeLocation("vertexCoordsArray", QT_VERTEX_COORDS_ATTR);
        m_blitProgram->bindAttributeLocation("textureCoordArray", QT_TEXTURE_COORDS_ATTR);
        m_blitProgram->link();
    }

    int m_maxTextureWidth;
    int m_maxTextureHeight;
    int m_maxTextureCount;
    bool m_coreProfile;

    QList<TextureInfo> m_textures;
    QHash<glyph_t, TextureInfo *> m_glyphsTexture;
    QSet<glyph_t> m_unusedGlyphs;

    QSGAreaAllocator *m_areaAllocator;

    QOpenGLShaderProgram *m_blitProgram;
    QOpenGLBuffer m_blitBuffer;
    QOpenGLVertexArrayObject m_vao;

    QOpenGLSharedResourceGuard *m_fboGuard;
    QOpenGLFunctions *m_funcs;
#if !defined(QT_OPENGL_ES_2)
    QOpenGLFunctions_3_2_Core *m_coreFuncs;
#endif
};

QT_END_NAMESPACE

#endif // QSGOPENGLDISTANCEFIELDGLYPHCACHE_H
