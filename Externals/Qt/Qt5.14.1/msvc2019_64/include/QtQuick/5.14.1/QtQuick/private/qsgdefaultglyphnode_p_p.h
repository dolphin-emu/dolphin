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

#ifndef QSGDEFAULTGLYPHNODE_P_P_H
#define QSGDEFAULTGLYPHNODE_P_P_H

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

#include <qcolor.h>
#include <QtGui/private/qopengltextureglyphcache_p.h>
#include <QtQuick/qsgmaterial.h>
#include <QtQuick/qsgtexture.h>
#include <QtQuick/qsggeometry.h>
#include <qshareddata.h>
#include <QtQuick/private/qsgplaintexture_p.h>
#include <QtQuick/private/qsgrhitextureglyphcache_p.h>
#include <qrawfont.h>
#include <qmargins.h>

QT_BEGIN_NAMESPACE

class QFontEngine;
class Geometry;
class QSGRenderContext;
class QSGDefaultRenderContext;

class QSGTextMaskMaterial: public QSGMaterial
{
public:
    QSGTextMaskMaterial(QSGRenderContext *rc, const QVector4D &color, const QRawFont &font, QFontEngine::GlyphFormat glyphFormat = QFontEngine::Format_None);
    virtual ~QSGTextMaskMaterial();

    QSGMaterialType *type() const override;
    QSGMaterialShader *createShader() const override;
    int compare(const QSGMaterial *other) const override;

    void setColor(const QColor &c) { setColor(QVector4D(c.redF(), c.greenF(), c.blueF(), c.alphaF())); }
    void setColor(const QVector4D &color);
    const QVector4D &color() const { return m_color; }

    QSGTexture *texture() const { return m_texture; }

    bool ensureUpToDate();

    QTextureGlyphCache *glyphCache() const;
    QOpenGLTextureGlyphCache *openglGlyphCache() const;
    QSGRhiTextureGlyphCache *rhiGlyphCache() const;

    void populate(const QPointF &position,
                  const QVector<quint32> &glyphIndexes, const QVector<QPointF> &glyphPositions,
                  QSGGeometry *geometry, QRectF *boundingRect, QPointF *baseLine,
                  const QMargins &margins = QMargins(0, 0, 0, 0));

private:
    void init(QFontEngine::GlyphFormat glyphFormat);
    void updateCache(QFontEngine::GlyphFormat glyphFormat);

    QSGDefaultRenderContext *m_rc;
    QSGPlainTexture *m_texture;
    QExplicitlySharedDataPointer<QFontEngineGlyphCache> m_glyphCache;
    QRawFont m_font;
    QRhi *m_rhi;
    QVector4D m_color;
    QSize m_size;
};

class QSGStyledTextMaterial : public QSGTextMaskMaterial
{
public:
    QSGStyledTextMaterial(QSGRenderContext *rc, const QRawFont &font);
    virtual ~QSGStyledTextMaterial() { }

    void setStyleShift(const QVector2D &shift) { m_styleShift = shift; }
    const QVector2D &styleShift() const { return m_styleShift; }

    void setStyleColor(const QColor &c) { m_styleColor = QVector4D(c.redF(), c.greenF(), c.blueF(), c.alphaF()); }
    void setStyleColor(const QVector4D &color) { m_styleColor = color; }
    const QVector4D &styleColor() const { return m_styleColor; }

    QSGMaterialType *type() const override;
    QSGMaterialShader *createShader() const override;
    int compare(const QSGMaterial *other) const override;

private:
    QVector2D m_styleShift;
    QVector4D m_styleColor;
};

class QSGOutlinedTextMaterial : public QSGStyledTextMaterial
{
public:
    QSGOutlinedTextMaterial(QSGRenderContext *rc, const QRawFont &font);
    ~QSGOutlinedTextMaterial() { }

    QSGMaterialType *type() const override;
    QSGMaterialShader *createShader() const override;
};

QT_END_NAMESPACE

#endif
