/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QOPENGLTEXTUREGLYPHCACHE_P_H
#define QOPENGLTEXTUREGLYPHCACHE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtGui/private/qtguiglobal_p.h>
#include <private/qtextureglyphcache_p.h>
#include <private/qopenglcontext_p.h>
#include <qopenglshaderprogram.h>
#include <qopenglfunctions.h>
#include <qopenglbuffer.h>
#include <qopenglvertexarrayobject.h>

// #define QT_GL_TEXTURE_GLYPH_CACHE_DEBUG

QT_BEGIN_NAMESPACE

class QOpenGL2PaintEngineExPrivate;

class QOpenGLGlyphTexture : public QOpenGLSharedResource
{
public:
    explicit QOpenGLGlyphTexture(QOpenGLContext *ctx)
        : QOpenGLSharedResource(ctx->shareGroup())
        , m_width(0)
        , m_height(0)
    {
        if (!ctx->d_func()->workaround_brokenFBOReadBack)
            QOpenGLFunctions(ctx).glGenFramebuffers(1, &m_fbo);

#ifdef QT_GL_TEXTURE_GLYPH_CACHE_DEBUG
        qDebug(" -> QOpenGLGlyphTexture() %p for context %p.", this, ctx);
#endif
    }

    void freeResource(QOpenGLContext *context) override
    {
        QOpenGLContext *ctx = context;
#ifdef QT_GL_TEXTURE_GLYPH_CACHE_DEBUG
        qDebug("~QOpenGLGlyphTexture() %p for context %p.", this, ctx);
#endif
        if (!ctx->d_func()->workaround_brokenFBOReadBack)
            ctx->functions()->glDeleteFramebuffers(1, &m_fbo);
        if (m_width || m_height)
            ctx->functions()->glDeleteTextures(1, &m_texture);
    }

    void invalidateResource() override
    {
        m_texture = 0;
        m_fbo = 0;
        m_width = 0;
        m_height = 0;
    }

    GLuint m_texture;
    GLuint m_fbo;
    int m_width;
    int m_height;
};

class Q_GUI_EXPORT QOpenGLTextureGlyphCache : public QImageTextureGlyphCache
{
public:
    QOpenGLTextureGlyphCache(QFontEngine::GlyphFormat glyphFormat, const QTransform &matrix, const QColor &color = QColor());
    ~QOpenGLTextureGlyphCache();

    virtual void createTextureData(int width, int height) override;
    virtual void resizeTextureData(int width, int height) override;
    virtual void fillTexture(const Coord &c, glyph_t glyph, QFixed subPixelPosition) override;
    virtual int glyphPadding() const override;
    virtual int maxTextureWidth() const override;
    virtual int maxTextureHeight() const override;

    inline GLuint texture() const {
        QOpenGLTextureGlyphCache *that = const_cast<QOpenGLTextureGlyphCache *>(this);
        QOpenGLGlyphTexture *glyphTexture = that->m_textureResource;
        return glyphTexture ? glyphTexture->m_texture : 0;
    }

    inline int width() const {
        QOpenGLTextureGlyphCache *that = const_cast<QOpenGLTextureGlyphCache *>(this);
        QOpenGLGlyphTexture *glyphTexture = that->m_textureResource;
        return glyphTexture ? glyphTexture->m_width : 0;
    }
    inline int height() const {
        QOpenGLTextureGlyphCache *that = const_cast<QOpenGLTextureGlyphCache *>(this);
        QOpenGLGlyphTexture *glyphTexture = that->m_textureResource;
        return glyphTexture ? glyphTexture->m_height : 0;
    }

    inline void setPaintEnginePrivate(QOpenGL2PaintEngineExPrivate *p) { pex = p; }

    inline const QOpenGLContextGroup *contextGroup() const { return m_textureResource ? m_textureResource->group() : nullptr; }

    inline int serialNumber() const { return m_serialNumber; }

    enum FilterMode {
        Nearest,
        Linear
    };
    FilterMode filterMode() const { return m_filterMode; }
    void setFilterMode(FilterMode m) { m_filterMode = m; }

    void clear();

    QOpenGL2PaintEngineExPrivate *paintEnginePrivate() const
    {
        return pex;
    }

private:
    void setupVertexAttribs();

    QOpenGLGlyphTexture *m_textureResource;

    QOpenGL2PaintEngineExPrivate *pex;
    QOpenGLShaderProgram *m_blitProgram;
    FilterMode m_filterMode;

    GLfloat m_vertexCoordinateArray[8];
    GLfloat m_textureCoordinateArray[8];

    int m_serialNumber;

    QOpenGLBuffer m_buffer;
    QOpenGLVertexArrayObject m_vao;
};

QT_END_NAMESPACE

#endif // QOPENGLTEXTUREGLYPHCACHE_P_H

