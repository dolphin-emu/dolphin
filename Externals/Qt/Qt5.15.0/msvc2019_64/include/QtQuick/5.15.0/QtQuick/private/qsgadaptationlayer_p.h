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

#ifndef QSGADAPTATIONLAYER_P_H
#define QSGADAPTATIONLAYER_P_H

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

#include <QtQuick/qsgnode.h>
#include <QtQuick/qsgtexture.h>
#include <QtCore/qobject.h>
#include <QtCore/qrect.h>
#include <QtGui/qbrush.h>
#include <QtGui/qcolor.h>
#include <QtGui/qpainterpath.h>
#include <QtCore/qsharedpointer.h>
#include <QtGui/qglyphrun.h>
#include <QtGui/qpainterpath.h>
#include <QtCore/qurl.h>
#include <private/qfontengine_p.h>
#include <QtGui/private/qdatabuffer_p.h>
#include <private/qdistancefield_p.h>
#include <private/qintrusivelist_p.h>
#include <QtGui/private/qshader_p.h>

// ### remove
#include <QtQuick/private/qquicktext_p.h>

QT_BEGIN_NAMESPACE

class QSGNode;
class QImage;
class TextureReference;
class QSGDistanceFieldGlyphNode;
class QSGInternalImageNode;
class QSGPainterNode;
class QSGInternalRectangleNode;
class QSGGlyphNode;
class QSGRootNode;
class QSGSpriteNode;
class QSGRenderNode;
class QSGRenderContext;
class QRhiTexture;

class Q_QUICK_PRIVATE_EXPORT QSGNodeVisitorEx
{
public:
    virtual ~QSGNodeVisitorEx() {}

    // visit(...) returns true if the children are supposed to be
    // visisted and false if they're supposed to be skipped by the visitor.

    virtual bool visit(QSGTransformNode *) = 0;
    virtual void endVisit(QSGTransformNode *) = 0;
    virtual bool visit(QSGClipNode *) = 0;
    virtual void endVisit(QSGClipNode *) = 0;
    virtual bool visit(QSGGeometryNode *) = 0;
    virtual void endVisit(QSGGeometryNode *) = 0;
    virtual bool visit(QSGOpacityNode *) = 0;
    virtual void endVisit(QSGOpacityNode *) = 0;
    virtual bool visit(QSGInternalImageNode *) = 0;
    virtual void endVisit(QSGInternalImageNode *) = 0;
    virtual bool visit(QSGPainterNode *) = 0;
    virtual void endVisit(QSGPainterNode *) = 0;
    virtual bool visit(QSGInternalRectangleNode *) = 0;
    virtual void endVisit(QSGInternalRectangleNode *) = 0;
    virtual bool visit(QSGGlyphNode *) = 0;
    virtual void endVisit(QSGGlyphNode *) = 0;
    virtual bool visit(QSGRootNode *) = 0;
    virtual void endVisit(QSGRootNode *) = 0;
#if QT_CONFIG(quick_sprite)
    virtual bool visit(QSGSpriteNode *) = 0;
    virtual void endVisit(QSGSpriteNode *) = 0;
#endif
    virtual bool visit(QSGRenderNode *) = 0;
    virtual void endVisit(QSGRenderNode *) = 0;

    void visitChildren(QSGNode *node);
};


class Q_QUICK_PRIVATE_EXPORT QSGVisitableNode : public QSGGeometryNode
{
public:
    QSGVisitableNode() { setFlag(IsVisitableNode); }

    virtual void accept(QSGNodeVisitorEx *) = 0;
};

class Q_QUICK_PRIVATE_EXPORT QSGInternalRectangleNode : public QSGVisitableNode
{
public:
    virtual void setRect(const QRectF &rect) = 0;
    virtual void setColor(const QColor &color) = 0;
    virtual void setPenColor(const QColor &color) = 0;
    virtual void setPenWidth(qreal width) = 0;
    virtual void setGradientStops(const QGradientStops &stops) = 0;
    virtual void setGradientVertical(bool vertical) = 0;
    virtual void setRadius(qreal radius) = 0;
    virtual void setAntialiasing(bool antialiasing) { Q_UNUSED(antialiasing) }
    virtual void setAligned(bool aligned) = 0;

    virtual void update() = 0;

    void accept(QSGNodeVisitorEx *visitor) override { if (visitor->visit(this)) visitor->visitChildren(this); visitor->endVisit(this); }
};


class Q_QUICK_PRIVATE_EXPORT QSGInternalImageNode : public QSGVisitableNode
{
public:
    virtual void setTargetRect(const QRectF &rect) = 0;
    virtual void setInnerTargetRect(const QRectF &rect) = 0;
    virtual void setInnerSourceRect(const QRectF &rect) = 0;
    // The sub-source rect's width and height specify the number of times the inner source rect
    // is repeated inside the inner target rect. The x and y specify which (normalized) location
    // in the inner source rect maps to the upper-left corner of the inner target rect.
    virtual void setSubSourceRect(const QRectF &rect) = 0;
    virtual void setTexture(QSGTexture *texture) = 0;
    virtual void setAntialiasing(bool antialiasing) { Q_UNUSED(antialiasing) }
    virtual void setMirror(bool mirror) = 0;
    virtual void setMipmapFiltering(QSGTexture::Filtering filtering) = 0;
    virtual void setFiltering(QSGTexture::Filtering filtering) = 0;
    virtual void setHorizontalWrapMode(QSGTexture::WrapMode wrapMode) = 0;
    virtual void setVerticalWrapMode(QSGTexture::WrapMode wrapMode) = 0;

    virtual void update() = 0;

    void accept(QSGNodeVisitorEx *visitor) override { if (visitor->visit(this)) visitor->visitChildren(this); visitor->endVisit(this); }
};

class Q_QUICK_PRIVATE_EXPORT QSGPainterNode : public QSGVisitableNode
{
public:

    virtual void setPreferredRenderTarget(QQuickPaintedItem::RenderTarget target) = 0;
    virtual void setSize(const QSize &size) = 0;
    virtual void setDirty(const QRect &dirtyRect = QRect()) = 0;
    virtual void setOpaquePainting(bool opaque) = 0;
    virtual void setLinearFiltering(bool linearFiltering) = 0;
    virtual void setMipmapping(bool mipmapping) = 0;
    virtual void setSmoothPainting(bool s) = 0;
    virtual void setFillColor(const QColor &c) = 0;
    virtual void setContentsScale(qreal s) = 0;
    virtual void setFastFBOResizing(bool dynamic) = 0;
    virtual void setTextureSize(const QSize &size) = 0;

    virtual QImage toImage() const = 0;
    virtual void update() = 0;
    virtual QSGTexture *texture() const = 0;

    void accept(QSGNodeVisitorEx *visitor) override { if (visitor->visit(this)) visitor->visitChildren(this); visitor->endVisit(this); }
};

class Q_QUICK_EXPORT QSGLayer : public QSGDynamicTexture
{
    Q_OBJECT
public:
    virtual void setItem(QSGNode *item) = 0;
    virtual void setRect(const QRectF &rect) = 0;
    virtual void setSize(const QSize &size) = 0;
    virtual void scheduleUpdate() = 0;
    virtual QImage toImage() const = 0;
    virtual void setLive(bool live) = 0;
    virtual void setRecursive(bool recursive) = 0;
    virtual void setFormat(uint format) = 0;
    virtual void setHasMipmaps(bool mipmap) = 0;
    virtual void setDevicePixelRatio(qreal ratio) = 0;
    virtual void setMirrorHorizontal(bool mirror) = 0;
    virtual void setMirrorVertical(bool mirror) = 0;
    virtual void setSamples(int samples) = 0;
    Q_SLOT virtual void markDirtyTexture() = 0;
    Q_SLOT virtual void invalidated() = 0;

Q_SIGNALS:
    void updateRequested();
    void scheduledUpdateCompleted();

protected:
    QSGLayer(QSGTexturePrivate &dd);
};

#if QT_CONFIG(quick_sprite)

class Q_QUICK_PRIVATE_EXPORT QSGSpriteNode : public QSGVisitableNode
{
public:
    virtual void setTexture(QSGTexture *texture) = 0;
    virtual void setTime(float time) = 0;
    virtual void setSourceA(const QPoint &source) = 0;
    virtual void setSourceB(const QPoint &source) = 0;
    virtual void setSpriteSize(const QSize &size) = 0;
    virtual void setSheetSize(const QSize &size) = 0;
    virtual void setSize(const QSizeF &size) = 0;
    virtual void setFiltering(QSGTexture::Filtering filtering) = 0;

    virtual void update() = 0;

    void accept(QSGNodeVisitorEx *visitor) override { if (visitor->visit(this)) visitor->visitChildren(this); visitor->endVisit(this); }
};

#endif

class Q_QUICK_PRIVATE_EXPORT QSGGuiThreadShaderEffectManager : public QObject
{
    Q_OBJECT

public:
    enum Status {
        Compiled,
        Uncompiled,
        Error
    };

    virtual bool hasSeparateSamplerAndTextureObjects() const = 0;

    virtual QString log() const = 0;
    virtual Status status() const = 0;

    struct ShaderInfo {
        enum Type {
            TypeVertex,
            TypeFragment,
            TypeOther
        };
        enum VariableType {
            Constant, // cbuffer members or uniforms
            Sampler,
            Texture // for APIs with separate texture and sampler objects
        };
        struct Variable {
            Variable() {}
            VariableType type = Constant;
            QByteArray name;
            uint offset = 0; // for cbuffer members
            uint size = 0; // for cbuffer members
            int bindPoint = 0; // for textures/samplers, where applicable
        };

        QString name; // optional, f.ex. the filename, used for debugging purposes only
        QByteArray blob; // source or bytecode (when not using QRhi)
        QShader rhiShader;
        Type type;
        QVector<Variable> variables;
        uint constantDataSize;

        // Vertex inputs are not tracked here as QSGGeometry::AttributeSet
        // hardwires that anyways so it is up to the shader to provide
        // compatible inputs (e.g. compatible with
        // QSGGeometry::defaultAttributes_TexturedPoint2D()).
    };

    virtual void prepareShaderCode(ShaderInfo::Type typeHint, const QByteArray &src, ShaderInfo *result) = 0;

Q_SIGNALS:
    void shaderCodePrepared(bool ok, ShaderInfo::Type typeHint, const QByteArray &src, ShaderInfo *result);
    void textureChanged();
    void logAndStatusChanged();
};

#ifndef QT_NO_DEBUG_STREAM
Q_QUICK_PRIVATE_EXPORT QDebug operator<<(QDebug debug, const QSGGuiThreadShaderEffectManager::ShaderInfo::Variable &v);
#endif

class Q_QUICK_PRIVATE_EXPORT QSGShaderEffectNode : public QSGVisitableNode
{
public:
    enum DirtyShaderFlag {
        DirtyShaders = 0x01,
        DirtyShaderConstant = 0x02,
        DirtyShaderTexture = 0x04,
        DirtyShaderGeometry = 0x08,
        DirtyShaderMesh = 0x10,

        DirtyShaderAll = 0xFF
    };
    Q_DECLARE_FLAGS(DirtyShaderFlags, DirtyShaderFlag)

    enum CullMode { // must match ShaderEffect
        NoCulling,
        BackFaceCulling,
        FrontFaceCulling
    };

    struct VariableData {
        enum SpecialType { None, Unused, Source, SubRect, Opacity, Matrix };

        QVariant value;
        SpecialType specialType;
    };

    struct ShaderData {
        ShaderData() {}
        bool hasShaderCode = false;
        QSGGuiThreadShaderEffectManager::ShaderInfo shaderInfo;
        QVector<VariableData> varData;
    };

    struct SyncData {
        DirtyShaderFlags dirty;
        CullMode cullMode;
        bool blending;
        struct ShaderSyncData {
            const ShaderData *shader;
            const QSet<int> *dirtyConstants;
            const QSet<int> *dirtyTextures;
        };
        ShaderSyncData vertex;
        ShaderSyncData fragment;
    };

    // Each ShaderEffect item has one node (render thread) and one manager (gui thread).
    QSGShaderEffectNode(QSGGuiThreadShaderEffectManager *) { }

    virtual QRectF updateNormalizedTextureSubRect(bool supportsAtlasTextures) = 0;
    virtual void syncMaterial(SyncData *syncData) = 0;

    void accept(QSGNodeVisitorEx *visitor) override { if (visitor->visit(this)) visitor->visitChildren(this); visitor->endVisit(this); }
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QSGShaderEffectNode::DirtyShaderFlags)

#ifndef QT_NO_DEBUG_STREAM
Q_QUICK_PRIVATE_EXPORT QDebug operator<<(QDebug debug, const QSGShaderEffectNode::VariableData &vd);
#endif

class Q_QUICK_PRIVATE_EXPORT QSGGlyphNode : public QSGVisitableNode
{
public:
    enum AntialiasingMode
    {
        GrayAntialiasing,
        LowQualitySubPixelAntialiasing,
        HighQualitySubPixelAntialiasing
    };

    QSGGlyphNode() {}

    virtual void setGlyphs(const QPointF &position, const QGlyphRun &glyphs) = 0;
    virtual void setColor(const QColor &color) = 0;
    virtual void setStyle(QQuickText::TextStyle style) = 0;
    virtual void setStyleColor(const QColor &color) = 0;
    virtual QPointF baseLine() const = 0;

    virtual QRectF boundingRect() const { return m_bounding_rect; }
    virtual void setBoundingRect(const QRectF &bounds) { m_bounding_rect = bounds; }

    virtual void setPreferredAntialiasingMode(AntialiasingMode) = 0;

    virtual void update() = 0;

    void setOwnerElement(QQuickItem *ownerElement) { m_ownerElement = ownerElement; }
    QQuickItem *ownerElement() const { return m_ownerElement; }

    void accept(QSGNodeVisitorEx *visitor) override { if (visitor->visit(this)) visitor->visitChildren(this); visitor->endVisit(this); }
protected:
    QRectF m_bounding_rect;
    QQuickItem *m_ownerElement = nullptr;
};

class Q_QUICK_PRIVATE_EXPORT QSGDistanceFieldGlyphConsumer
{
public:
    virtual ~QSGDistanceFieldGlyphConsumer() {}

    virtual void invalidateGlyphs(const QVector<quint32> &glyphs) = 0;
    QIntrusiveListNode node;
};
typedef QIntrusiveList<QSGDistanceFieldGlyphConsumer, &QSGDistanceFieldGlyphConsumer::node> QSGDistanceFieldGlyphConsumerList;

class Q_QUICK_PRIVATE_EXPORT QSGDistanceFieldGlyphCache
{
public:
    QSGDistanceFieldGlyphCache(const QRawFont &font);
    virtual ~QSGDistanceFieldGlyphCache();

    struct Metrics {
        qreal width;
        qreal height;
        qreal baselineX;
        qreal baselineY;

        bool isNull() const { return width == 0 || height == 0; }
    };

    struct TexCoord {
        qreal x = 0;
        qreal y = 0;
        qreal width = -1;
        qreal height = -1;
        qreal xMargin = 0;
        qreal yMargin = 0;

        TexCoord() {}

        bool isNull() const { return width <= 0 || height <= 0; }
        bool isValid() const { return width >= 0 && height >= 0; }
    };

    struct Texture {
        uint textureId = 0;
        QRhiTexture *texture = nullptr;
        QSize size;
        bool rhiBased = false;

        bool operator == (const Texture &other) const {
            if (rhiBased != other.rhiBased)
                return false;
            if (rhiBased)
                return texture == other.texture;
            else
                return textureId == other.textureId;
        }
    };

    const QRawFont &referenceFont() const { return m_referenceFont; }

    qreal fontScale(qreal pixelSize) const
    {
        return pixelSize / QT_DISTANCEFIELD_BASEFONTSIZE(m_doubleGlyphResolution);
    }
    int distanceFieldRadius() const
    {
        return QT_DISTANCEFIELD_RADIUS(m_doubleGlyphResolution) / QT_DISTANCEFIELD_SCALE(m_doubleGlyphResolution);
    }
    int glyphCount() const { return m_glyphCount; }
    bool doubleGlyphResolution() const { return m_doubleGlyphResolution; }

    Metrics glyphMetrics(glyph_t glyph, qreal pixelSize);
    inline TexCoord glyphTexCoord(glyph_t glyph);
    inline const Texture *glyphTexture(glyph_t glyph);

    void populate(const QVector<glyph_t> &glyphs);
    void release(const QVector<glyph_t> &glyphs);

    void update();

    void registerGlyphNode(QSGDistanceFieldGlyphConsumer *node) { m_registeredNodes.insert(node); }
    void unregisterGlyphNode(QSGDistanceFieldGlyphConsumer *node) { m_registeredNodes.remove(node); }

    virtual void registerOwnerElement(QQuickItem *ownerElement);
    virtual void unregisterOwnerElement(QQuickItem *ownerElement);
    virtual void processPendingGlyphs();

    virtual bool eightBitFormatIsAlphaSwizzled() const = 0;

protected:
    struct GlyphPosition {
        glyph_t glyph;
        QPointF position;
    };

    struct GlyphData {
        Texture *texture = nullptr;
        TexCoord texCoord;
        QRectF boundingRect;
        QPainterPath path;
        quint32 ref = 0;

        GlyphData() {}
    };

    virtual void requestGlyphs(const QSet<glyph_t> &glyphs) = 0;
    virtual void storeGlyphs(const QList<QDistanceField> &glyphs) = 0;
    virtual void referenceGlyphs(const QSet<glyph_t> &glyphs) = 0;
    virtual void releaseGlyphs(const QSet<glyph_t> &glyphs) = 0;

    void setGlyphsPosition(const QList<GlyphPosition> &glyphs);
    void setGlyphsTexture(const QVector<glyph_t> &glyphs, const Texture &tex);
    void markGlyphsToRender(const QVector<glyph_t> &glyphs);
    inline void removeGlyph(glyph_t glyph);

    void updateTexture(uint oldTex, uint newTex, const QSize &newTexSize);
    void updateRhiTexture(QRhiTexture *oldTex, QRhiTexture *newTex, const QSize &newTexSize);

    inline bool containsGlyph(glyph_t glyph);
    uint textureIdForGlyph(glyph_t glyph) const;

    GlyphData &glyphData(glyph_t glyph);
    GlyphData &emptyData(glyph_t glyph);

#if defined(QSG_DISTANCEFIELD_CACHE_DEBUG)
    void saveTexture(GLuint textureId, int width, int height) const;
#endif

    bool m_doubleGlyphResolution;

protected:
    QRawFont m_referenceFont;

private:
    int m_glyphCount;
    QList<Texture> m_textures;
    QHash<glyph_t, GlyphData> m_glyphsData;
    QDataBuffer<glyph_t> m_pendingGlyphs;
    QSet<glyph_t> m_populatingGlyphs;
    QSGDistanceFieldGlyphConsumerList m_registeredNodes;

    static Texture s_emptyTexture;
};

inline QSGDistanceFieldGlyphCache::TexCoord QSGDistanceFieldGlyphCache::glyphTexCoord(glyph_t glyph)
{
    return glyphData(glyph).texCoord;
}

inline const QSGDistanceFieldGlyphCache::Texture *QSGDistanceFieldGlyphCache::glyphTexture(glyph_t glyph)
{
    return glyphData(glyph).texture;
}

inline void QSGDistanceFieldGlyphCache::removeGlyph(glyph_t glyph)
{
    GlyphData &gd = glyphData(glyph);
    gd.texCoord = TexCoord();
    gd.texture = &s_emptyTexture;
}

inline bool QSGDistanceFieldGlyphCache::containsGlyph(glyph_t glyph)
{
    return glyphData(glyph).texCoord.isValid();
}

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QSGGuiThreadShaderEffectManager::ShaderInfo::Type)

#endif
