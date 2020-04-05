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

#ifndef QWINDOWSFONTENGINE_H
#define QWINDOWSFONTENGINE_H

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

#include <QtGui/private/qfontengine_p.h>

#include <QtGui/QImage>
#include <QtCore/QSharedPointer>
#include <QtCore/QMetaType>

#include <QtCore/qt_windows.h>

QT_BEGIN_NAMESPACE

class QWindowsNativeImage;
class QWindowsFontEngineData;

class QWindowsFontEngine : public QFontEngine
{
    Q_DISABLE_COPY_MOVE(QWindowsFontEngine)
public:
    QWindowsFontEngine(const QString &name, LOGFONT lf,
                       const QSharedPointer<QWindowsFontEngineData> &fontEngineData);

    ~QWindowsFontEngine() override;
    void initFontInfo(const QFontDef &request,
                      int dpi);

    QFixed lineThickness() const override;
    Properties properties() const override;
    void getUnscaledGlyph(glyph_t glyph, QPainterPath *path, glyph_metrics_t *metrics) override;
    FaceId faceId() const override;
    bool getSfntTableData(uint tag, uchar *buffer, uint *length) const override;
    int synthesized() const override;
    QFixed emSquareSize() const override;

    glyph_t glyphIndex(uint ucs4) const override;
    bool stringToCMap(const QChar *str, int len, QGlyphLayout *glyphs, int *nglyphs, ShaperFlags flags) const override;
    void recalcAdvances(QGlyphLayout *glyphs, ShaperFlags) const override;

    void addOutlineToPath(qreal x, qreal y, const QGlyphLayout &glyphs, QPainterPath *path, QTextItem::RenderFlags flags) override;
    void addGlyphsToPath(glyph_t *glyphs, QFixedPoint *positions, int nglyphs,
                         QPainterPath *path, QTextItem::RenderFlags flags) override;

    HGDIOBJ selectDesignFont() const;

    glyph_metrics_t boundingBox(const QGlyphLayout &glyphs) override;
    glyph_metrics_t boundingBox(glyph_t g) override { return boundingBox(g, QTransform()); }
    glyph_metrics_t boundingBox(glyph_t g, const QTransform &t) override;


    QFixed ascent() const override;
    QFixed descent() const override;
    QFixed leading() const override;
    QFixed xHeight() const override;
    QFixed capHeight() const override;
    QFixed averageCharWidth() const override;
    qreal maxCharWidth() const override;
    qreal minLeftBearing() const override;
    qreal minRightBearing() const override;

    QImage alphaMapForGlyph(glyph_t t) override { return alphaMapForGlyph(t, QTransform()); }
    QImage alphaMapForGlyph(glyph_t, const QTransform &xform) override;
    QImage alphaRGBMapForGlyph(glyph_t t, QFixed subPixelPosition, const QTransform &xform) override;
    glyph_metrics_t alphaMapBoundingBox(glyph_t glyph, QFixed, const QTransform &matrix, GlyphFormat) override;

    QFontEngine *cloneWithSize(qreal pixelSize) const override;
    Qt::HANDLE handle() const override;
    bool supportsTransformation(const QTransform &transform) const override;

#ifndef Q_CC_MINGW
    void getGlyphBearings(glyph_t glyph, qreal *leftBearing = 0, qreal *rightBearing = 0) override;
#endif

    bool hasUnreliableGlyphOutline() const override;

    int getGlyphIndexes(const QChar *ch, int numChars, QGlyphLayout *glyphs) const;
    void getCMap();

    bool getOutlineMetrics(glyph_t glyph, const QTransform &t, glyph_metrics_t *metrics) const;

    const QSharedPointer<QWindowsFontEngineData> &fontEngineData() const { return m_fontEngineData; }

    void setUniqueFamilyName(const QString &newName) { uniqueFamilyName = newName; }

private:
    QWindowsNativeImage *drawGDIGlyph(HFONT font, glyph_t, int margin, const QTransform &xform,
                                      QImage::Format mask_format);
    bool hasCFFTable() const;
    bool hasCMapTable() const;

    const QSharedPointer<QWindowsFontEngineData> m_fontEngineData;

    const QString     _name;
    QString     uniqueFamilyName;
    HFONT       hfont = 0;
    const LOGFONT     m_logfont;
    uint        ttf        : 1;
    uint        hasOutline : 1;
    uint        hasUnreliableOutline : 1;
    uint        cffTable   : 1;
    TEXTMETRIC  tm;
    const unsigned char *cmap = nullptr;
    int cmapSize = 0;
    QByteArray cmapTable;
    mutable qreal lbearing = SHRT_MIN;
    mutable qreal rbearing = SHRT_MIN;
    QFixed designToDevice;
    int unitsPerEm = 0;
    QFixed x_height = -1;
    FaceId _faceId;

    mutable int synthesized_flags = -1;
    mutable QFixed lineWidth = -1;
    mutable unsigned char *widthCache = nullptr;
    mutable uint widthCacheSize = 0;
    mutable QFixed *designAdvances = nullptr;
    mutable int designAdvancesSize = 0;
};

QT_END_NAMESPACE

Q_DECLARE_METATYPE(HFONT)
Q_DECLARE_METATYPE(LOGFONT)

#endif // QWINDOWSFONTENGINE_H
