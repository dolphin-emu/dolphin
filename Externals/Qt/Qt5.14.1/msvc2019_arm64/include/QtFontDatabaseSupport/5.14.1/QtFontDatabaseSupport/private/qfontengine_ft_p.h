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
#ifndef QFONTENGINE_FT_P_H
#define QFONTENGINE_FT_P_H
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

#include "private/qfontengine_p.h"

#ifndef QT_NO_FREETYPE

#include <ft2build.h>
#include FT_FREETYPE_H


#ifndef Q_OS_WIN
#include <unistd.h>
#endif

#include <qmutex.h>

QT_BEGIN_NAMESPACE

class QFontEngineFTRawFont;
class QFontconfigDatabase;

/*
 * This class represents one font file on disk (like Arial.ttf) and is shared between all the font engines
 * that show this font file (at different pixel sizes).
 */
class QFreetypeFace
{
public:
    void computeSize(const QFontDef &fontDef, int *xsize, int *ysize, bool *outline_drawing, QFixed *scalableBitmapScaleFactor);
    QFontEngine::Properties properties() const;
    bool getSfntTable(uint tag, uchar *buffer, uint *length) const;

    static QFreetypeFace *getFace(const QFontEngine::FaceId &face_id,
                                  const QByteArray &fontData = QByteArray());
    void release(const QFontEngine::FaceId &face_id);

    // locks the struct for usage. Any read/write operations require locking.
    void lock()
    {
        _lock.lock();
    }
    void unlock()
    {
        _lock.unlock();
    }

    FT_Face face;
    int xsize; // 26.6
    int ysize; // 26.6
    FT_Matrix matrix;
    FT_CharMap unicode_map;
    FT_CharMap symbol_map;

    enum { cmapCacheSize = 0x200 };
    glyph_t cmapCache[cmapCacheSize];

    int fsType() const;

    int getPointInOutline(glyph_t glyph, int flags, quint32 point, QFixed *xpos, QFixed *ypos, quint32 *nPoints);

    bool isScalableBitmap() const;

    static void addGlyphToPath(FT_Face face, FT_GlyphSlot g, const QFixedPoint &point, QPainterPath *path, FT_Fixed x_scale, FT_Fixed y_scale);
    static void addBitmapToPath(FT_GlyphSlot slot, const QFixedPoint &point, QPainterPath *path);

private:
    friend class QFontEngineFT;
    friend class QtFreetypeData;
    friend struct QScopedPointerDeleter<QFreetypeFace>;
    QFreetypeFace() = default;
    ~QFreetypeFace() {}
    void cleanup();
    QAtomicInt ref;
    QRecursiveMutex _lock;
    QByteArray fontData;

    QFontEngine::Holder hbFace;
};

class QFontEngineFT : public QFontEngine
{
public:
    struct GlyphInfo {
        int             linearAdvance;
        unsigned short  width;
        unsigned short  height;
        short           x;
        short           y;
        short           xOff;
        short           yOff;
    };

    struct GlyphAndSubPixelPosition
    {
        GlyphAndSubPixelPosition(glyph_t g, QFixed spp) : glyph(g), subPixelPosition(spp) {}

        bool operator==(const GlyphAndSubPixelPosition &other) const
        {
            return glyph == other.glyph && subPixelPosition == other.subPixelPosition;
        }

        glyph_t glyph;
        QFixed subPixelPosition;
    };

    struct QGlyphSet
    {
        QGlyphSet();
        ~QGlyphSet();
        FT_Matrix transformationMatrix;
        bool outline_drawing;

        void removeGlyphFromCache(glyph_t index, QFixed subPixelPosition);
        void clear();
        inline bool useFastGlyphData(glyph_t index, QFixed subPixelPosition) const {
            return (index < 256 && subPixelPosition == 0);
        }
        inline Glyph *getGlyph(glyph_t index, QFixed subPixelPosition = 0) const;
        void setGlyph(glyph_t index, QFixed spp, Glyph *glyph);

        inline bool isGlyphMissing(glyph_t index) const { return missing_glyphs.contains(index); }
        inline void setGlyphMissing(glyph_t index) const { missing_glyphs.insert(index); }
private:
        mutable QHash<GlyphAndSubPixelPosition, Glyph *> glyph_data; // maps from glyph index to glyph data
        mutable QSet<glyph_t> missing_glyphs;
        mutable Glyph *fast_glyph_data[256]; // for fast lookup of glyphs < 256
        mutable int fast_glyph_count;
    };

    QFontEngine::FaceId faceId() const override;
    QFontEngine::Properties properties() const override;
    QFixed emSquareSize() const override;
    bool supportsSubPixelPositions() const override
    {
        return default_hint_style == HintLight ||
               default_hint_style == HintNone;
    }

    bool getSfntTableData(uint tag, uchar *buffer, uint *length) const override;
    int synthesized() const override;

    QFixed ascent() const override;
    QFixed capHeight() const override;
    QFixed descent() const override;
    QFixed leading() const override;
    QFixed xHeight() const override;
    QFixed averageCharWidth() const override;

    qreal maxCharWidth() const override;
    QFixed lineThickness() const override;
    QFixed underlinePosition() const override;

    glyph_t glyphIndex(uint ucs4) const override;
    void doKerning(QGlyphLayout *, ShaperFlags) const override;

    void getUnscaledGlyph(glyph_t glyph, QPainterPath *path, glyph_metrics_t *metrics) override;

    bool supportsTransformation(const QTransform &transform) const override;

    void addGlyphsToPath(glyph_t *glyphs, QFixedPoint *positions, int nglyphs,
                 QPainterPath *path, QTextItem::RenderFlags flags) override;
    void addOutlineToPath(qreal x, qreal y, const QGlyphLayout &glyphs,
                  QPainterPath *path, QTextItem::RenderFlags flags) override;

    bool stringToCMap(const QChar *str, int len, QGlyphLayout *glyphs, int *nglyphs, ShaperFlags flags) const override;

    glyph_metrics_t boundingBox(const QGlyphLayout &glyphs) override;
    glyph_metrics_t boundingBox(glyph_t glyph) override;
    glyph_metrics_t boundingBox(glyph_t glyph, const QTransform &matrix) override;

    void recalcAdvances(QGlyphLayout *glyphs, ShaperFlags flags) const override;
    QImage alphaMapForGlyph(glyph_t g) override { return alphaMapForGlyph(g, 0); }
    QImage alphaMapForGlyph(glyph_t, QFixed) override;
    QImage alphaMapForGlyph(glyph_t glyph, QFixed subPixelPosition, const QTransform &t) override;
    QImage alphaRGBMapForGlyph(glyph_t, QFixed subPixelPosition, const QTransform &t) override;
    QImage bitmapForGlyph(glyph_t, QFixed subPixelPosition, const QTransform &t, const QColor &color) override;
    glyph_metrics_t alphaMapBoundingBox(glyph_t glyph,
                                        QFixed subPixelPosition,
                                        const QTransform &matrix,
                                        QFontEngine::GlyphFormat format) override;
    Glyph *glyphData(glyph_t glyph, QFixed subPixelPosition,
                     GlyphFormat neededFormat, const QTransform &t) override;
    bool hasInternalCaching() const override { return cacheEnabled; }
    bool expectsGammaCorrectedBlending() const override;

    void removeGlyphFromCache(glyph_t glyph) override;
    int glyphMargin(QFontEngine::GlyphFormat /* format */) override { return 0; }

    int glyphCount() const override;

    enum Scaling {
        Scaled,
        Unscaled
    };
    FT_Face lockFace(Scaling scale = Scaled) const;
    void unlockFace() const;

    FT_Face non_locked_face() const;

    inline bool drawAntialiased() const { return antialias; }
    inline bool invalid() const { return xsize == 0 && ysize == 0; }
    inline bool isBitmapFont() const { return defaultFormat == Format_Mono; }
    inline bool isScalableBitmap() const { return freetype->isScalableBitmap(); }

    inline Glyph *loadGlyph(uint glyph, QFixed subPixelPosition, GlyphFormat format = Format_None, bool fetchMetricsOnly = false, bool disableOutlineDrawing = false) const
    { return loadGlyph(cacheEnabled ? &defaultGlyphSet : nullptr, glyph, subPixelPosition, format, fetchMetricsOnly, disableOutlineDrawing); }
    Glyph *loadGlyph(QGlyphSet *set, uint glyph, QFixed subPixelPosition, GlyphFormat = Format_None, bool fetchMetricsOnly = false, bool disableOutlineDrawing = false) const;
    Glyph *loadGlyphFor(glyph_t g, QFixed subPixelPosition, GlyphFormat format, const QTransform &t, bool fetchBoundingBox = false, bool disableOutlineDrawing = false);

    QGlyphSet *loadGlyphSet(const QTransform &matrix);

    QFontEngineFT(const QFontDef &fd);
    virtual ~QFontEngineFT();

    bool init(FaceId faceId, bool antiaalias, GlyphFormat defaultFormat = Format_None,
              const QByteArray &fontData = QByteArray());
    bool init(FaceId faceId, bool antialias, GlyphFormat format,
              QFreetypeFace *freetypeFace);

    int getPointInOutline(glyph_t glyph, int flags, quint32 point, QFixed *xpos, QFixed *ypos, quint32 *nPoints) override;

    void setQtDefaultHintStyle(QFont::HintingPreference hintingPreference);
    void setDefaultHintStyle(HintStyle style) override;

    QFontEngine *cloneWithSize(qreal pixelSize) const override;
    Qt::HANDLE handle() const override;
    bool initFromFontEngine(const QFontEngineFT *fontEngine);

    HintStyle defaultHintStyle() const { return default_hint_style; }

    static QFontEngineFT *create(const QFontDef &fontDef, FaceId faceId, const QByteArray &fontData = QByteArray());
    static QFontEngineFT *create(const QByteArray &fontData, qreal pixelSize, QFont::HintingPreference hintingPreference);

protected:

    QFreetypeFace *freetype;
    mutable int default_load_flags;
    HintStyle default_hint_style;
    bool antialias;
    bool transform;
    bool embolden;
    bool obliquen;
    SubpixelAntialiasingType subpixelType;
    int lcdFilterType;
    bool embeddedbitmap;
    bool cacheEnabled;
    bool forceAutoHint;
    bool stemDarkeningDriver;

private:
    friend class QFontEngineFTRawFont;
    friend class QFontconfigDatabase;
    friend class QFreeTypeFontDatabase;
    friend class QFontEngineMultiFontConfig;

    int loadFlags(QGlyphSet *set, GlyphFormat format, int flags, bool &hsubpixel, int &vfactor) const;
    bool shouldUseDesignMetrics(ShaperFlags flags) const;
    QFixed scaledBitmapMetrics(QFixed m) const;
    glyph_metrics_t scaledBitmapMetrics(const glyph_metrics_t &m, const QTransform &matrix) const;

    GlyphFormat defaultFormat;
    FT_Matrix matrix;

    QList<QGlyphSet> transformedGlyphSets;
    mutable QGlyphSet defaultGlyphSet;

    QFontEngine::FaceId face_id;

    int xsize;
    int ysize;

    QFixed line_thickness;
    QFixed underline_position;

    FT_Size_Metrics metrics;
    mutable bool kerning_pairs_loaded;
    QFixed scalableBitmapScaleFactor;
};

inline uint qHash(const QFontEngineFT::GlyphAndSubPixelPosition &g)
{
    return (g.glyph << 8)  | (g.subPixelPosition * 10).round().toInt();
}

inline QFontEngineFT::Glyph *QFontEngineFT::QGlyphSet::getGlyph(glyph_t index, QFixed subPixelPosition) const
{
    if (useFastGlyphData(index, subPixelPosition))
        return fast_glyph_data[index];
    return glyph_data.value(GlyphAndSubPixelPosition(index, subPixelPosition));
}

extern FT_Library qt_getFreetype();

QT_END_NAMESPACE

#endif // QT_NO_FREETYPE

#endif // QFONTENGINE_FT_P_H
