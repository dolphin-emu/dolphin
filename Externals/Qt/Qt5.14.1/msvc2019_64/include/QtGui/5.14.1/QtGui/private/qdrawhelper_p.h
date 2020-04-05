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

#ifndef QDRAWHELPER_P_H
#define QDRAWHELPER_P_H

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

#include <QtGui/private/qtguiglobal_p.h>
#include "QtCore/qmath.h"
#include "QtGui/qcolor.h"
#include "QtGui/qpainter.h"
#include "QtGui/qimage.h"
#include "QtGui/qrgba64.h"
#ifndef QT_FT_BEGIN_HEADER
#define QT_FT_BEGIN_HEADER
#define QT_FT_END_HEADER
#endif
#include "private/qrasterdefs_p.h"
#include <private/qsimd_p.h>

#include <QtCore/qsharedpointer.h>

QT_BEGIN_NAMESPACE

#if defined(Q_CC_GNU)
#  define Q_DECL_RESTRICT __restrict__
#  if defined(Q_PROCESSOR_X86_32) && defined(Q_CC_GNU) && !defined(Q_CC_CLANG) && !defined(Q_CC_INTEL)
#    define Q_DECL_VECTORCALL __attribute__((sseregparm,regparm(3)))
#  else
#    define Q_DECL_VECTORCALL
#  endif
#elif defined(Q_CC_MSVC)
#  define Q_DECL_RESTRICT __restrict
#  define Q_DECL_VECTORCALL __vectorcall
#else
#  define Q_DECL_RESTRICT
#  define Q_DECL_VECTORCALL
#endif

static const uint AMASK = 0xff000000;
static const uint RMASK = 0x00ff0000;
static const uint GMASK = 0x0000ff00;
static const uint BMASK = 0x000000ff;

/*******************************************************************************
 * QSpan
 *
 * duplicate definition of FT_Span
 */
typedef QT_FT_Span QSpan;

struct QSolidData;
struct QTextureData;
struct QGradientData;
struct QLinearGradientData;
struct QRadialGradientData;
struct QConicalGradientData;
struct QSpanData;
class QGradient;
class QRasterBuffer;
class QClipData;
class QRasterPaintEngineState;

typedef QT_FT_SpanFunc ProcessSpans;
typedef void (*BitmapBlitFunc)(QRasterBuffer *rasterBuffer,
                               int x, int y, const QRgba64 &color,
                               const uchar *bitmap,
                               int mapWidth, int mapHeight, int mapStride);

typedef void (*AlphamapBlitFunc)(QRasterBuffer *rasterBuffer,
                                 int x, int y, const QRgba64 &color,
                                 const uchar *bitmap,
                                 int mapWidth, int mapHeight, int mapStride,
                                 const QClipData *clip, bool useGammaCorrection);

typedef void (*AlphaRGBBlitFunc)(QRasterBuffer *rasterBuffer,
                                 int x, int y, const QRgba64 &color,
                                 const uint *rgbmask,
                                 int mapWidth, int mapHeight, int mapStride,
                                 const QClipData *clip, bool useGammaCorrection);

typedef void (*RectFillFunc)(QRasterBuffer *rasterBuffer,
                             int x, int y, int width, int height,
                             const QRgba64 &color);

typedef void (*SrcOverBlendFunc)(uchar *destPixels, int dbpl,
                                 const uchar *src, int spbl,
                                 int w, int h,
                                 int const_alpha);

typedef void (*SrcOverScaleFunc)(uchar *destPixels, int dbpl,
                                 const uchar *src, int spbl, int srch,
                                 const QRectF &targetRect,
                                 const QRectF &sourceRect,
                                 const QRect &clipRect,
                                 int const_alpha);

typedef void (*SrcOverTransformFunc)(uchar *destPixels, int dbpl,
                                     const uchar *src, int spbl,
                                     const QRectF &targetRect,
                                     const QRectF &sourceRect,
                                     const QRect &clipRect,
                                     const QTransform &targetRectTransform,
                                     int const_alpha);

typedef void (*MemRotateFunc)(const uchar *srcPixels, int w, int h, int sbpl, uchar *destPixels, int dbpl);

struct DrawHelper {
    ProcessSpans blendColor;
    BitmapBlitFunc bitmapBlit;
    AlphamapBlitFunc alphamapBlit;
    AlphaRGBBlitFunc alphaRGBBlit;
    RectFillFunc fillRect;
};

extern SrcOverBlendFunc qBlendFunctions[QImage::NImageFormats][QImage::NImageFormats];
extern SrcOverScaleFunc qScaleFunctions[QImage::NImageFormats][QImage::NImageFormats];
extern SrcOverTransformFunc qTransformFunctions[QImage::NImageFormats][QImage::NImageFormats];

extern DrawHelper qDrawHelper[QImage::NImageFormats];

struct quint24 {
    quint24() = default;
    quint24(uint value)
    {
        data[0] = uchar(value >> 16);
        data[1] = uchar(value >> 8);
        data[2] = uchar(value);
    }
    operator uint() const
    {
        return data[2] | (data[1] << 8) | (data[0] << 16);
    }

    uchar data[3];
};

void qBlendGradient(int count, const QSpan *spans, void *userData);
void qBlendTexture(int count, const QSpan *spans, void *userData);
#ifdef __SSE2__
extern void (*qt_memfill64)(quint64 *dest, quint64 value, qsizetype count);
extern void (*qt_memfill32)(quint32 *dest, quint32 value, qsizetype count);
#else
extern void qt_memfill64(quint64 *dest, quint64 value, qsizetype count);
extern void qt_memfill32(quint32 *dest, quint32 value, qsizetype count);
#endif
extern void qt_memfill24(quint24 *dest, quint24 value, qsizetype count);
extern void qt_memfill16(quint16 *dest, quint16 value, qsizetype count);

typedef void (QT_FASTCALL *CompositionFunction)(uint *Q_DECL_RESTRICT dest, const uint *Q_DECL_RESTRICT src, int length, uint const_alpha);
typedef void (QT_FASTCALL *CompositionFunction64)(QRgba64 *Q_DECL_RESTRICT dest, const QRgba64 *Q_DECL_RESTRICT src, int length, uint const_alpha);
typedef void (QT_FASTCALL *CompositionFunctionSolid)(uint *dest, int length, uint color, uint const_alpha);
typedef void (QT_FASTCALL *CompositionFunctionSolid64)(QRgba64 *dest, int length, QRgba64 color, uint const_alpha);

struct LinearGradientValues
{
    qreal dx;
    qreal dy;
    qreal l;
    qreal off;
};

struct RadialGradientValues
{
    qreal dx;
    qreal dy;
    qreal dr;
    qreal sqrfr;
    qreal a;
    qreal inv2a;
    bool extended;
};

struct Operator;
typedef uint* (QT_FASTCALL *DestFetchProc)(uint *buffer, QRasterBuffer *rasterBuffer, int x, int y, int length);
typedef QRgba64* (QT_FASTCALL *DestFetchProc64)(QRgba64 *buffer, QRasterBuffer *rasterBuffer, int x, int y, int length);
typedef void (QT_FASTCALL *DestStoreProc)(QRasterBuffer *rasterBuffer, int x, int y, const uint *buffer, int length);
typedef void (QT_FASTCALL *DestStoreProc64)(QRasterBuffer *rasterBuffer, int x, int y, const QRgba64 *buffer, int length);
typedef const uint* (QT_FASTCALL *SourceFetchProc)(uint *buffer, const Operator *o, const QSpanData *data, int y, int x, int length);
typedef const QRgba64* (QT_FASTCALL *SourceFetchProc64)(QRgba64 *buffer, const Operator *o, const QSpanData *data, int y, int x, int length);

struct Operator
{
    QPainter::CompositionMode mode;
    DestFetchProc destFetch;
    DestStoreProc destStore;
    SourceFetchProc srcFetch;
    CompositionFunctionSolid funcSolid;
    CompositionFunction func;

    DestFetchProc64 destFetch64;
    DestStoreProc64 destStore64;
    SourceFetchProc64 srcFetch64;
    CompositionFunctionSolid64 funcSolid64;
    CompositionFunction64 func64;

    union {
        LinearGradientValues linear;
        RadialGradientValues radial;
    };
};

class QRasterPaintEngine;

struct QLinearGradientData
{
    struct {
        qreal x;
        qreal y;
    } origin;
    struct {
        qreal x;
        qreal y;
    } end;
};

struct QRadialGradientData
{
    struct {
        qreal x;
        qreal y;
        qreal radius;
    } center;
    struct {
        qreal x;
        qreal y;
        qreal radius;
    } focal;
};

struct QConicalGradientData
{
    struct {
        qreal x;
        qreal y;
    } center;
    qreal angle;
};

struct QGradientData
{
    QGradient::Spread spread;

    union {
        QLinearGradientData linear;
        QRadialGradientData radial;
        QConicalGradientData conical;
    };

#define GRADIENT_STOPTABLE_SIZE 1024
#define GRADIENT_STOPTABLE_SIZE_SHIFT 10

#if QT_CONFIG(raster_64bit)
    const QRgba64 *colorTable64; //[GRADIENT_STOPTABLE_SIZE];
#endif
    const QRgb *colorTable32; //[GRADIENT_STOPTABLE_SIZE];

    uint alphaColor : 1;
};

struct QTextureData
{
    const uchar *imageData;
    const uchar *scanLine(int y) const { return imageData + y*bytesPerLine; }

    int width;
    int height;
    // clip rect
    int x1;
    int y1;
    int x2;
    int y2;
    qsizetype bytesPerLine;
    QImage::Format format;
    const QVector<QRgb> *colorTable;
    bool hasAlpha;
    enum Type {
        Plain,
        Tiled
    };
    Type type;
    int const_alpha;
};

struct QSpanData
{
    QSpanData() : tempImage(nullptr) {}
    ~QSpanData() { delete tempImage; }

    QRasterBuffer *rasterBuffer;
    ProcessSpans blend;
    ProcessSpans unclipped_blend;
    BitmapBlitFunc bitmapBlit;
    AlphamapBlitFunc alphamapBlit;
    AlphaRGBBlitFunc alphaRGBBlit;
    RectFillFunc fillRect;
    qreal m11, m12, m13, m21, m22, m23, m33, dx, dy;   // inverse xform matrix
    const QClipData *clip;
    enum Type {
        None,
        Solid,
        LinearGradient,
        RadialGradient,
        ConicalGradient,
        Texture
    } type : 8;
    int txop : 8;
    int fast_matrix : 1;
    bool bilinear;
    QImage *tempImage;
    QRgba64 solidColor;
    union {
        QGradientData gradient;
        QTextureData texture;
    };
    class Pinnable {
    protected:
        ~Pinnable() {}
    }; // QSharedPointer<const void> is not supported
    QSharedPointer<const Pinnable> cachedGradient;


    void init(QRasterBuffer *rb, const QRasterPaintEngine *pe);
    void setup(const QBrush &brush, int alpha, QPainter::CompositionMode compositionMode);
    void setupMatrix(const QTransform &matrix, int bilinear);
    void initTexture(const QImage *image, int alpha, QTextureData::Type = QTextureData::Plain, const QRect &sourceRect = QRect());
    void adjustSpanMethods();
};

static inline uint qt_gradient_clamp(const QGradientData *data, int ipos)
{
    if (ipos < 0 || ipos >= GRADIENT_STOPTABLE_SIZE) {
        if (data->spread == QGradient::RepeatSpread) {
            ipos = ipos % GRADIENT_STOPTABLE_SIZE;
            ipos = ipos < 0 ? GRADIENT_STOPTABLE_SIZE + ipos : ipos;
        } else if (data->spread == QGradient::ReflectSpread) {
            const int limit = GRADIENT_STOPTABLE_SIZE * 2;
            ipos = ipos % limit;
            ipos = ipos < 0 ? limit + ipos : ipos;
            ipos = ipos >= GRADIENT_STOPTABLE_SIZE ? limit - 1 - ipos : ipos;
        } else {
            if (ipos < 0)
                ipos = 0;
            else if (ipos >= GRADIENT_STOPTABLE_SIZE)
                ipos = GRADIENT_STOPTABLE_SIZE-1;
        }
    }

    Q_ASSERT(ipos >= 0);
    Q_ASSERT(ipos < GRADIENT_STOPTABLE_SIZE);

    return ipos;
}

static inline uint qt_gradient_pixel(const QGradientData *data, qreal pos)
{
    int ipos = int(pos * (GRADIENT_STOPTABLE_SIZE - 1) + qreal(0.5));
    return data->colorTable32[qt_gradient_clamp(data, ipos)];
}

#if QT_CONFIG(raster_64bit)
static inline const QRgba64& qt_gradient_pixel64(const QGradientData *data, qreal pos)
{
    int ipos = int(pos * (GRADIENT_STOPTABLE_SIZE - 1) + qreal(0.5));
    return data->colorTable64[qt_gradient_clamp(data, ipos)];
}
#endif

static inline qreal qRadialDeterminant(qreal a, qreal b, qreal c)
{
    return (b * b) - (4 * a * c);
}

template <class RadialFetchFunc, typename BlendType> static
const BlendType * QT_FASTCALL qt_fetch_radial_gradient_template(BlendType *buffer, const Operator *op,
                                                                const QSpanData *data, int y, int x, int length)
{
    // avoid division by zero
    if (qFuzzyIsNull(op->radial.a)) {
        RadialFetchFunc::memfill(buffer, RadialFetchFunc::null(), length);
        return buffer;
    }

    const BlendType *b = buffer;
    qreal rx = data->m21 * (y + qreal(0.5))
               + data->dx + data->m11 * (x + qreal(0.5));
    qreal ry = data->m22 * (y + qreal(0.5))
               + data->dy + data->m12 * (x + qreal(0.5));
    bool affine = !data->m13 && !data->m23;

    BlendType *end = buffer + length;
    if (affine) {
        rx -= data->gradient.radial.focal.x;
        ry -= data->gradient.radial.focal.y;

        qreal inv_a = 1 / qreal(2 * op->radial.a);

        const qreal delta_rx = data->m11;
        const qreal delta_ry = data->m12;

        qreal b = 2*(op->radial.dr*data->gradient.radial.focal.radius + rx * op->radial.dx + ry * op->radial.dy);
        qreal delta_b = 2*(delta_rx * op->radial.dx + delta_ry * op->radial.dy);
        const qreal b_delta_b = 2 * b * delta_b;
        const qreal delta_b_delta_b = 2 * delta_b * delta_b;

        const qreal bb = b * b;
        const qreal delta_bb = delta_b * delta_b;

        b *= inv_a;
        delta_b *= inv_a;

        const qreal rxrxryry = rx * rx + ry * ry;
        const qreal delta_rxrxryry = delta_rx * delta_rx + delta_ry * delta_ry;
        const qreal rx_plus_ry = 2*(rx * delta_rx + ry * delta_ry);
        const qreal delta_rx_plus_ry = 2 * delta_rxrxryry;

        inv_a *= inv_a;

        qreal det = (bb - 4 * op->radial.a * (op->radial.sqrfr - rxrxryry)) * inv_a;
        qreal delta_det = (b_delta_b + delta_bb + 4 * op->radial.a * (rx_plus_ry + delta_rxrxryry)) * inv_a;
        const qreal delta_delta_det = (delta_b_delta_b + 4 * op->radial.a * delta_rx_plus_ry) * inv_a;

        RadialFetchFunc::fetch(buffer, end, op, data, det, delta_det, delta_delta_det, b, delta_b);
    } else {
        qreal rw = data->m23 * (y + qreal(0.5))
                   + data->m33 + data->m13 * (x + qreal(0.5));

        while (buffer < end) {
            if (rw == 0) {
                *buffer = 0;
            } else {
                qreal invRw = 1 / rw;
                qreal gx = rx * invRw - data->gradient.radial.focal.x;
                qreal gy = ry * invRw - data->gradient.radial.focal.y;
                qreal b  = 2*(op->radial.dr*data->gradient.radial.focal.radius + gx*op->radial.dx + gy*op->radial.dy);
                qreal det = qRadialDeterminant(op->radial.a, b, op->radial.sqrfr - (gx*gx + gy*gy));

                BlendType result = RadialFetchFunc::null();
                if (det >= 0) {
                    qreal detSqrt = qSqrt(det);

                    qreal s0 = (-b - detSqrt) * op->radial.inv2a;
                    qreal s1 = (-b + detSqrt) * op->radial.inv2a;

                    qreal s = qMax(s0, s1);

                    if (data->gradient.radial.focal.radius + op->radial.dr * s >= 0)
                        result = RadialFetchFunc::fetchSingle(data->gradient, s);
                }

                *buffer = result;
            }

            rx += data->m11;
            ry += data->m12;
            rw += data->m13;

            ++buffer;
        }
    }

    return b;
}

template <class Simd>
class QRadialFetchSimd
{
public:
    static uint null() { return 0; }
    static uint fetchSingle(const QGradientData& gradient, qreal v)
    {
        return qt_gradient_pixel(&gradient, v);
    }
    static void memfill(uint *buffer, uint fill, int length)
    {
        qt_memfill32(buffer, fill, length);
    }
    static void fetch(uint *buffer, uint *end, const Operator *op, const QSpanData *data, qreal det,
                      qreal delta_det, qreal delta_delta_det, qreal b, qreal delta_b)
    {
        typename Simd::Vect_buffer_f det_vec;
        typename Simd::Vect_buffer_f delta_det4_vec;
        typename Simd::Vect_buffer_f b_vec;

        for (int i = 0; i < 4; ++i) {
            det_vec.f[i] = det;
            delta_det4_vec.f[i] = 4 * delta_det;
            b_vec.f[i] = b;

            det += delta_det;
            delta_det += delta_delta_det;
            b += delta_b;
        }

        const typename Simd::Float32x4 v_delta_delta_det16 = Simd::v_dup(16 * delta_delta_det);
        const typename Simd::Float32x4 v_delta_delta_det6 = Simd::v_dup(6 * delta_delta_det);
        const typename Simd::Float32x4 v_delta_b4 = Simd::v_dup(4 * delta_b);

        const typename Simd::Float32x4 v_r0 = Simd::v_dup(data->gradient.radial.focal.radius);
        const typename Simd::Float32x4 v_dr = Simd::v_dup(op->radial.dr);

#if defined(__ARM_NEON__)
        // NEON doesn't have SIMD sqrt, but uses rsqrt instead that can't be taken of 0.
        const typename Simd::Float32x4 v_min = Simd::v_dup(std::numeric_limits<float>::epsilon());
#else
        const typename Simd::Float32x4 v_min = Simd::v_dup(0.0f);
#endif
        const typename Simd::Float32x4 v_max = Simd::v_dup(float(GRADIENT_STOPTABLE_SIZE-1));
        const typename Simd::Float32x4 v_half = Simd::v_dup(0.5f);

        const typename Simd::Int32x4 v_repeat_mask = Simd::v_dup(~(uint(0xffffff) << GRADIENT_STOPTABLE_SIZE_SHIFT));
        const typename Simd::Int32x4 v_reflect_mask = Simd::v_dup(~(uint(0xffffff) << (GRADIENT_STOPTABLE_SIZE_SHIFT+1)));

        const typename Simd::Int32x4 v_reflect_limit = Simd::v_dup(2 * GRADIENT_STOPTABLE_SIZE - 1);

        const int extended_mask = op->radial.extended ? 0x0 : ~0x0;

#define FETCH_RADIAL_LOOP_PROLOGUE \
        while (buffer < end) { \
            typename Simd::Vect_buffer_i v_buffer_mask; \
            v_buffer_mask.v = Simd::v_greaterOrEqual(det_vec.v, v_min); \
            const typename Simd::Float32x4 v_index_local = Simd::v_sub(Simd::v_sqrt(Simd::v_max(v_min, det_vec.v)), b_vec.v); \
            const typename Simd::Float32x4 v_index = Simd::v_add(Simd::v_mul(v_index_local, v_max), v_half); \
            v_buffer_mask.v = Simd::v_and(v_buffer_mask.v, Simd::v_greaterOrEqual(Simd::v_add(v_r0, Simd::v_mul(v_dr, v_index_local)), v_min)); \
            typename Simd::Vect_buffer_i index_vec;
#define FETCH_RADIAL_LOOP_CLAMP_REPEAT \
            index_vec.v = Simd::v_and(v_repeat_mask, Simd::v_toInt(v_index));
#define FETCH_RADIAL_LOOP_CLAMP_REFLECT \
            const typename Simd::Int32x4 v_index_i = Simd::v_and(v_reflect_mask, Simd::v_toInt(v_index)); \
            const typename Simd::Int32x4 v_index_i_inv = Simd::v_sub(v_reflect_limit, v_index_i); \
            index_vec.v = Simd::v_min_16(v_index_i, v_index_i_inv);
#define FETCH_RADIAL_LOOP_CLAMP_PAD \
            index_vec.v = Simd::v_toInt(Simd::v_min(v_max, Simd::v_max(v_min, v_index)));
#define FETCH_RADIAL_LOOP_EPILOGUE \
            det_vec.v = Simd::v_add(Simd::v_add(det_vec.v, delta_det4_vec.v), v_delta_delta_det6); \
            delta_det4_vec.v = Simd::v_add(delta_det4_vec.v, v_delta_delta_det16); \
            b_vec.v = Simd::v_add(b_vec.v, v_delta_b4); \
            for (int i = 0; i < 4; ++i) \
                *buffer++ = (extended_mask | v_buffer_mask.i[i]) & data->gradient.colorTable32[index_vec.i[i]]; \
        }

#define FETCH_RADIAL_LOOP(FETCH_RADIAL_LOOP_CLAMP) \
        FETCH_RADIAL_LOOP_PROLOGUE \
        FETCH_RADIAL_LOOP_CLAMP \
        FETCH_RADIAL_LOOP_EPILOGUE

        switch (data->gradient.spread) {
        case QGradient::RepeatSpread:
            FETCH_RADIAL_LOOP(FETCH_RADIAL_LOOP_CLAMP_REPEAT)
            break;
        case QGradient::ReflectSpread:
            FETCH_RADIAL_LOOP(FETCH_RADIAL_LOOP_CLAMP_REFLECT)
            break;
        case QGradient::PadSpread:
            FETCH_RADIAL_LOOP(FETCH_RADIAL_LOOP_CLAMP_PAD)
            break;
        default:
            Q_ASSERT(false);
        }
    }
};

static Q_ALWAYS_INLINE uint INTERPOLATE_PIXEL_255(uint x, uint a, uint y, uint b) {
    uint t = (x & 0xff00ff) * a + (y & 0xff00ff) * b;
    t = (t + ((t >> 8) & 0xff00ff) + 0x800080) >> 8;
    t &= 0xff00ff;

    x = ((x >> 8) & 0xff00ff) * a + ((y >> 8) & 0xff00ff) * b;
    x = (x + ((x >> 8) & 0xff00ff) + 0x800080);
    x &= 0xff00ff00;
    x |= t;
    return x;
}

#if Q_PROCESSOR_WORDSIZE == 8 // 64-bit versions

static Q_ALWAYS_INLINE uint INTERPOLATE_PIXEL_256(uint x, uint a, uint y, uint b) {
    quint64 t = (((quint64(x)) | ((quint64(x)) << 24)) & 0x00ff00ff00ff00ff) * a;
    t += (((quint64(y)) | ((quint64(y)) << 24)) & 0x00ff00ff00ff00ff) * b;
    t >>= 8;
    t &= 0x00ff00ff00ff00ff;
    return (uint(t)) | (uint(t >> 24));
}

static Q_ALWAYS_INLINE uint BYTE_MUL(uint x, uint a) {
    quint64 t = (((quint64(x)) | ((quint64(x)) << 24)) & 0x00ff00ff00ff00ff) * a;
    t = (t + ((t >> 8) & 0xff00ff00ff00ff) + 0x80008000800080) >> 8;
    t &= 0x00ff00ff00ff00ff;
    return (uint(t)) | (uint(t >> 24));
}

#else // 32-bit versions

static Q_ALWAYS_INLINE uint INTERPOLATE_PIXEL_256(uint x, uint a, uint y, uint b) {
    uint t = (x & 0xff00ff) * a + (y & 0xff00ff) * b;
    t >>= 8;
    t &= 0xff00ff;

    x = ((x >> 8) & 0xff00ff) * a + ((y >> 8) & 0xff00ff) * b;
    x &= 0xff00ff00;
    x |= t;
    return x;
}

static Q_ALWAYS_INLINE uint BYTE_MUL(uint x, uint a) {
    uint t = (x & 0xff00ff) * a;
    t = (t + ((t >> 8) & 0xff00ff) + 0x800080) >> 8;
    t &= 0xff00ff;

    x = ((x >> 8) & 0xff00ff) * a;
    x = (x + ((x >> 8) & 0xff00ff) + 0x800080);
    x &= 0xff00ff00;
    x |= t;
    return x;
}
#endif

static Q_ALWAYS_INLINE void blend_pixel(quint32 &dst, const quint32 src)
{
    if (src >= 0xff000000)
        dst = src;
    else if (src != 0)
        dst = src + BYTE_MUL(dst, qAlpha(~src));
}

static Q_ALWAYS_INLINE void blend_pixel(quint32 &dst, const quint32 src, const int const_alpha)
{
    if (const_alpha == 255)
        return blend_pixel(dst, src);
    if (src != 0) {
        const quint32 s = BYTE_MUL(src, const_alpha);
        dst = s + BYTE_MUL(dst, qAlpha(~s));
    }
}

#if defined(__SSE2__)
static Q_ALWAYS_INLINE uint interpolate_4_pixels_sse2(__m128i vt, __m128i vb, uint distx, uint disty)
{
    // First interpolate top and bottom pixels in parallel.
    vt = _mm_unpacklo_epi8(vt, _mm_setzero_si128());
    vb = _mm_unpacklo_epi8(vb, _mm_setzero_si128());
    vt = _mm_mullo_epi16(vt, _mm_set1_epi16(256 - disty));
    vb = _mm_mullo_epi16(vb, _mm_set1_epi16(disty));
    __m128i vlr = _mm_add_epi16(vt, vb);
    vlr = _mm_srli_epi16(vlr, 8);
    // vlr now contains the result of the first two interpolate calls vlr = unpacked((xright << 64) | xleft)

    // Now the last interpolate between left and right..
    const __m128i vidistx = _mm_shufflelo_epi16(_mm_cvtsi32_si128(256 - distx), _MM_SHUFFLE(0, 0, 0, 0));
    const __m128i vdistx = _mm_shufflelo_epi16(_mm_cvtsi32_si128(distx), _MM_SHUFFLE(0, 0, 0, 0));
    const __m128i vmulx = _mm_unpacklo_epi16(vidistx, vdistx);
    vlr = _mm_unpacklo_epi16(vlr, _mm_srli_si128(vlr, 8));
    // vlr now contains the colors of left and right interleaved { la, ra, lr, rr, lg, rg, lb, rb }
    vlr = _mm_madd_epi16(vlr, vmulx); // Multiply and horizontal add.
    vlr = _mm_srli_epi32(vlr, 8);
    vlr = _mm_packs_epi32(vlr, vlr);
    vlr = _mm_packus_epi16(vlr, vlr);
    return _mm_cvtsi128_si32(vlr);
}

static inline uint interpolate_4_pixels(uint tl, uint tr, uint bl, uint br, uint distx, uint disty)
{
    __m128i vt = _mm_unpacklo_epi32(_mm_cvtsi32_si128(tl), _mm_cvtsi32_si128(tr));
    __m128i vb = _mm_unpacklo_epi32(_mm_cvtsi32_si128(bl), _mm_cvtsi32_si128(br));
    return interpolate_4_pixels_sse2(vt, vb, distx, disty);
}

static inline uint interpolate_4_pixels(const uint t[], const uint b[], uint distx, uint disty)
{
    __m128i vt = _mm_loadl_epi64((const __m128i*)t);
    __m128i vb = _mm_loadl_epi64((const __m128i*)b);
    return interpolate_4_pixels_sse2(vt, vb, distx, disty);
}

static constexpr inline bool hasFastInterpolate4() { return true; }

#elif defined(__ARM_NEON__)
static Q_ALWAYS_INLINE uint interpolate_4_pixels_neon(uint32x2_t vt32, uint32x2_t vb32, uint distx, uint disty)
{
    uint16x8_t vt16 = vmovl_u8(vreinterpret_u8_u32(vt32));
    uint16x8_t vb16 = vmovl_u8(vreinterpret_u8_u32(vb32));
    vt16 = vmulq_n_u16(vt16, 256 - disty);
    vt16 = vmlaq_n_u16(vt16, vb16, disty);
    vt16 = vshrq_n_u16(vt16, 8);
    uint16x4_t vl16 = vget_low_u16(vt16);
    uint16x4_t vr16 = vget_high_u16(vt16);
    vl16 = vmul_n_u16(vl16, 256 - distx);
    vl16 = vmla_n_u16(vl16, vr16, distx);
    vl16 = vshr_n_u16(vl16, 8);
    uint8x8_t vr = vmovn_u16(vcombine_u16(vl16, vl16));
    return vget_lane_u32(vreinterpret_u32_u8(vr), 0);
}

static inline uint interpolate_4_pixels(uint tl, uint tr, uint bl, uint br, uint distx, uint disty)
{
    uint32x2_t vt32 = vmov_n_u32(tl);
    uint32x2_t vb32 = vmov_n_u32(bl);
    vt32 = vset_lane_u32(tr, vt32, 1);
    vb32 = vset_lane_u32(br, vb32, 1);
    return interpolate_4_pixels_neon(vt32, vb32, distx, disty);
}

static inline uint interpolate_4_pixels(const uint t[], const uint b[], uint distx, uint disty)
{
    uint32x2_t vt32 = vld1_u32(t);
    uint32x2_t vb32 = vld1_u32(b);
    return interpolate_4_pixels_neon(vt32, vb32, distx, disty);
}

static constexpr inline bool hasFastInterpolate4() { return true; }

#else
static inline uint interpolate_4_pixels(uint tl, uint tr, uint bl, uint br, uint distx, uint disty)
{
    uint idistx = 256 - distx;
    uint idisty = 256 - disty;
    uint xtop = INTERPOLATE_PIXEL_256(tl, idistx, tr, distx);
    uint xbot = INTERPOLATE_PIXEL_256(bl, idistx, br, distx);
    return INTERPOLATE_PIXEL_256(xtop, idisty, xbot, disty);
}

static inline uint interpolate_4_pixels(const uint t[], const uint b[], uint distx, uint disty)
{
    return interpolate_4_pixels(t[0], t[1], b[0], b[1], distx, disty);
}

static constexpr inline bool hasFastInterpolate4() { return false; }

#endif

static inline QRgba64 multiplyAlpha256(QRgba64 rgba64, uint alpha256)
{
    return QRgba64::fromRgba64((rgba64.red()   * alpha256) >> 8,
                               (rgba64.green() * alpha256) >> 8,
                               (rgba64.blue()  * alpha256) >> 8,
                               (rgba64.alpha() * alpha256) >> 8);
}
static inline QRgba64 interpolate256(QRgba64 x, uint alpha1, QRgba64 y, uint alpha2)
{
    return QRgba64::fromRgba64(multiplyAlpha256(x, alpha1) + multiplyAlpha256(y, alpha2));
}

#ifdef __SSE2__
static inline QRgba64 interpolate_4_pixels_rgb64(const QRgba64 t[], const QRgba64 b[], uint distx, uint disty)
{
    __m128i vt = _mm_loadu_si128((const __m128i*)t);
    if (disty) {
       __m128i vb = _mm_loadu_si128((const __m128i*)b);
        vt = _mm_mulhi_epu16(vt, _mm_set1_epi16(0x10000 - disty));
        vb = _mm_mulhi_epu16(vb, _mm_set1_epi16(disty));
        vt = _mm_add_epi16(vt, vb);
    }
    if (distx) {
        const __m128i vdistx = _mm_shufflelo_epi16(_mm_cvtsi32_si128(distx), _MM_SHUFFLE(0, 0, 0, 0));
        const __m128i vidistx = _mm_shufflelo_epi16(_mm_cvtsi32_si128(0x10000 - distx), _MM_SHUFFLE(0, 0, 0, 0));
        vt = _mm_mulhi_epu16(vt, _mm_unpacklo_epi64(vidistx, vdistx));
        vt = _mm_add_epi16(vt, _mm_srli_si128(vt, 8));
    }
#ifdef Q_PROCESSOR_X86_64
    return QRgba64::fromRgba64(_mm_cvtsi128_si64(vt));
#else
    QRgba64 out;
    _mm_storel_epi64((__m128i*)&out, vt);
    return out;
#endif // Q_PROCESSOR_X86_64
}
#elif defined(__ARM_NEON__)
static inline QRgba64 interpolate_4_pixels_rgb64(const QRgba64 t[], const QRgba64 b[], uint distx, uint disty)
{
    uint64x1x2_t vt = vld2_u64(reinterpret_cast<const uint64_t *>(t));
    if (disty) {
        uint64x1x2_t vb = vld2_u64(reinterpret_cast<const uint64_t *>(b));
        uint32x4_t vt0 = vmull_n_u16(vreinterpret_u16_u64(vt.val[0]), 0x10000 - disty);
        uint32x4_t vt1 = vmull_n_u16(vreinterpret_u16_u64(vt.val[1]), 0x10000 - disty);
        vt0 = vmlal_n_u16(vt0, vreinterpret_u16_u64(vb.val[0]), disty);
        vt1 = vmlal_n_u16(vt1, vreinterpret_u16_u64(vb.val[1]), disty);
        vt.val[0] = vreinterpret_u64_u16(vshrn_n_u32(vt0, 16));
        vt.val[1] = vreinterpret_u64_u16(vshrn_n_u32(vt1, 16));
    }
    if (distx) {
        uint32x4_t vt0 = vmull_n_u16(vreinterpret_u16_u64(vt.val[0]), 0x10000 - distx);
        vt0 = vmlal_n_u16(vt0, vreinterpret_u16_u64(vt.val[1]), distx);
        vt.val[0] = vreinterpret_u64_u16(vshrn_n_u32(vt0, 16));
    }
    QRgba64 out;
    vst1_u64(reinterpret_cast<uint64_t *>(&out), vt.val[0]);
    return out;
}
#else
static inline QRgba64 interpolate_4_pixels_rgb64(const QRgba64 t[], const QRgba64 b[], uint distx, uint disty)
{
    const uint dx = distx>>8;
    const uint dy = disty>>8;
    const uint idx = 256 - dx;
    const uint idy = 256 - dy;
    QRgba64 xtop = interpolate256(t[0], idx, t[1], dx);
    QRgba64 xbot = interpolate256(b[0], idx, b[1], dx);
    return interpolate256(xtop, idy, xbot, dy);
}
#endif // __SSE2__

#if Q_BYTE_ORDER == Q_BIG_ENDIAN
static Q_ALWAYS_INLINE quint32 RGBA2ARGB(quint32 x) {
    quint32 rgb = x >> 8;
    quint32 a = x << 24;
    return a | rgb;
}

static Q_ALWAYS_INLINE quint32 ARGB2RGBA(quint32 x) {
    quint32 rgb = x << 8;
    quint32 a = x >> 24;
    return a | rgb;
}
#else
static Q_ALWAYS_INLINE quint32 RGBA2ARGB(quint32 x) {
    // RGBA8888 is ABGR32 on little endian.
    quint32 ag = x & 0xff00ff00;
    quint32 rg = x & 0x00ff00ff;
    return ag | (rg  << 16) | (rg >> 16);
}

static Q_ALWAYS_INLINE quint32 ARGB2RGBA(quint32 x) {
    return RGBA2ARGB(x);
}
#endif

static Q_ALWAYS_INLINE uint BYTE_MUL_RGB16(uint x, uint a) {
    a += 1;
    uint t = (((x & 0x07e0)*a) >> 8) & 0x07e0;
    t |= (((x & 0xf81f)*(a>>2)) >> 6) & 0xf81f;
    return t;
}

static Q_ALWAYS_INLINE uint BYTE_MUL_RGB16_32(uint x, uint a) {
    uint t = (((x & 0xf81f07e0) >> 5)*a) & 0xf81f07e0;
    t |= (((x & 0x07e0f81f)*a) >> 5) & 0x07e0f81f;
    return t;
}

// qt_div_255 is a fast rounded division by 255 using an approximation that is accurate for all positive 16-bit integers
static Q_DECL_CONSTEXPR Q_ALWAYS_INLINE int qt_div_255(int x) { return (x + (x>>8) + 0x80) >> 8; }
static Q_DECL_CONSTEXPR Q_ALWAYS_INLINE uint qt_div_257_floor(uint x) { return  (x - (x >> 8)) >> 8; }
static Q_DECL_CONSTEXPR Q_ALWAYS_INLINE uint qt_div_257(uint x) { return qt_div_257_floor(x + 128); }
static Q_DECL_CONSTEXPR Q_ALWAYS_INLINE uint qt_div_65535(uint x) { return (x + (x>>16) + 0x8000U) >> 16; }

static Q_ALWAYS_INLINE uint qAlphaRgb30(uint c)
{
    uint a = c >> 30;
    a |= a << 2;
    a |= a << 4;
    return a;
}

template <class T> inline void qt_memfill_template(T *dest, T color, qsizetype count)
{
    if (!count)
        return;

    qsizetype n = (count + 7) / 8;
    switch (count & 0x07)
    {
    case 0: do { *dest++ = color; Q_FALLTHROUGH();
    case 7:      *dest++ = color; Q_FALLTHROUGH();
    case 6:      *dest++ = color; Q_FALLTHROUGH();
    case 5:      *dest++ = color; Q_FALLTHROUGH();
    case 4:      *dest++ = color; Q_FALLTHROUGH();
    case 3:      *dest++ = color; Q_FALLTHROUGH();
    case 2:      *dest++ = color; Q_FALLTHROUGH();
    case 1:      *dest++ = color;
    } while (--n > 0);
    }
}

template <class T> inline void qt_memfill(T *dest, T value, qsizetype count)
{
    qt_memfill_template(dest, value, count);
}

template<> inline void qt_memfill(quint64 *dest, quint64 color, qsizetype count)
{
    qt_memfill64(dest, color, count);
}

template<> inline void qt_memfill(quint32 *dest, quint32 color, qsizetype count)
{
    qt_memfill32(dest, color, count);
}

template<> inline void qt_memfill(quint24 *dest, quint24 color, qsizetype count)
{
    qt_memfill24(dest, color, count);
}

template<> inline void qt_memfill(quint16 *dest, quint16 color, qsizetype count)
{
    qt_memfill16(dest, color, count);
}

template<> inline void qt_memfill(quint8 *dest, quint8 color, qsizetype count)
{
    memset(dest, color, count);
}

template <class T> static
inline void qt_rectfill(T *dest, T value,
                        int x, int y, int width, int height, qsizetype stride)
{
    char *d = reinterpret_cast<char*>(dest + x) + y * stride;
    if (uint(stride) == (width * sizeof(T))) {
        qt_memfill(reinterpret_cast<T*>(d), value, qsizetype(width) * height);
    } else {
        for (int j = 0; j < height; ++j) {
            dest = reinterpret_cast<T*>(d);
            qt_memfill(dest, value, width);
            d += stride;
        }
    }
}

inline ushort qConvertRgb32To16(uint c)
{
   return (((c) >> 3) & 0x001f)
       | (((c) >> 5) & 0x07e0)
       | (((c) >> 8) & 0xf800);
}

inline QRgb qConvertRgb16To32(uint c)
{
    return 0xff000000
        | ((((c) << 3) & 0xf8) | (((c) >> 2) & 0x7))
        | ((((c) << 5) & 0xfc00) | (((c) >> 1) & 0x300))
        | ((((c) << 8) & 0xf80000) | (((c) << 3) & 0x70000));
}

enum QtPixelOrder {
    PixelOrderRGB,
    PixelOrderBGR
};

template<enum QtPixelOrder> inline uint qConvertArgb32ToA2rgb30(QRgb);

template<enum QtPixelOrder> inline uint qConvertRgb32ToRgb30(QRgb);

template<enum QtPixelOrder> inline QRgb qConvertA2rgb30ToArgb32(uint c);

// A combined unpremultiply and premultiply with new simplified alpha.
// Needed when alpha loses precision relative to other colors during conversion (ARGB32 -> A2RGB30).
template<unsigned int Shift>
inline QRgb qRepremultiply(QRgb p)
{
    const uint alpha = qAlpha(p);
    if (alpha == 255 || alpha == 0)
        return p;
    p = qUnpremultiply(p);
    Q_CONSTEXPR  uint mult = 255 / (255 >> Shift);
    const uint newAlpha = mult * (alpha >> Shift);
    p = (p & ~0xff000000) | (newAlpha<<24);
    return qPremultiply(p);
}

template<unsigned int Shift>
inline QRgba64 qRepremultiply(QRgba64 p)
{
    const uint alpha = p.alpha();
    if (alpha == 65535 || alpha == 0)
        return p;
    p = p.unpremultiplied();
    Q_CONSTEXPR  uint mult = 65535 / (65535 >> Shift);
    p.setAlpha(mult * (alpha >> Shift));
    return p.premultiplied();
}

template<>
inline uint qConvertArgb32ToA2rgb30<PixelOrderBGR>(QRgb c)
{
    c = qRepremultiply<6>(c);
    return (c & 0xc0000000)
        | (((c << 22) & 0x3fc00000) | ((c << 14) & 0x00300000))
        | (((c << 4) & 0x000ff000) | ((c >> 4) & 0x00000c00))
        | (((c >> 14) & 0x000003fc) | ((c >> 22) & 0x00000003));
}

template<>
inline uint qConvertArgb32ToA2rgb30<PixelOrderRGB>(QRgb c)
{
    c = qRepremultiply<6>(c);
    return (c & 0xc0000000)
        | (((c << 6) & 0x3fc00000) | ((c >> 2) & 0x00300000))
        | (((c << 4) & 0x000ff000) | ((c >> 4) & 0x00000c00))
        | (((c << 2) & 0x000003fc) | ((c >> 6) & 0x00000003));
}

template<>
inline uint qConvertRgb32ToRgb30<PixelOrderBGR>(QRgb c)
{
    return 0xc0000000
        | (((c << 22) & 0x3fc00000) | ((c << 14) & 0x00300000))
        | (((c << 4) & 0x000ff000) | ((c >> 4) & 0x00000c00))
        | (((c >> 14) & 0x000003fc) | ((c >> 22) & 0x00000003));
}

template<>
inline uint qConvertRgb32ToRgb30<PixelOrderRGB>(QRgb c)
{
    return 0xc0000000
        | (((c << 6) & 0x3fc00000) | ((c >> 2) & 0x00300000))
        | (((c << 4) & 0x000ff000) | ((c >> 4) & 0x00000c00))
        | (((c << 2) & 0x000003fc) | ((c >> 6) & 0x00000003));
}

template<>
inline QRgb qConvertA2rgb30ToArgb32<PixelOrderBGR>(uint c)
{
    uint a = c >> 30;
    a |= a << 2;
    a |= a << 4;
    return (a << 24)
        | ((c << 14) & 0x00ff0000)
        | ((c >> 4) & 0x0000ff00)
        | ((c >> 22) & 0x000000ff);
}

template<>
inline QRgb qConvertA2rgb30ToArgb32<PixelOrderRGB>(uint c)
{
    uint a = c >> 30;
    a |= a << 2;
    a |= a << 4;
    return (a << 24)
        | ((c >> 6) & 0x00ff0000)
        | ((c >> 4) & 0x0000ff00)
        | ((c >> 2) & 0x000000ff);
}

template<enum QtPixelOrder> inline QRgba64 qConvertA2rgb30ToRgb64(uint rgb);

template<>
inline QRgba64 qConvertA2rgb30ToRgb64<PixelOrderBGR>(uint rgb)
{
    quint16 alpha = rgb >> 30;
    quint16 blue  = (rgb >> 20) & 0x3ff;
    quint16 green = (rgb >> 10) & 0x3ff;
    quint16 red   = rgb & 0x3ff;
    // Expand the range.
    alpha |= (alpha << 2);
    alpha |= (alpha << 4);
    alpha |= (alpha << 8);
    red   = (red   << 6) | (red   >> 4);
    green = (green << 6) | (green >> 4);
    blue  = (blue  << 6) | (blue  >> 4);
    return qRgba64(red, green, blue, alpha);
}

template<>
inline QRgba64 qConvertA2rgb30ToRgb64<PixelOrderRGB>(uint rgb)
{
    quint16 alpha = rgb >> 30;
    quint16 red   = (rgb >> 20) & 0x3ff;
    quint16 green = (rgb >> 10) & 0x3ff;
    quint16 blue  = rgb & 0x3ff;
    // Expand the range.
    alpha |= (alpha << 2);
    alpha |= (alpha << 4);
    alpha |= (alpha << 8);
    red   = (red   << 6) | (red   >> 4);
    green = (green << 6) | (green >> 4);
    blue  = (blue  << 6) | (blue  >> 4);
    return qRgba64(red, green, blue, alpha);
}

template<enum QtPixelOrder> inline unsigned int qConvertRgb64ToRgb30(QRgba64);

template<>
inline unsigned int qConvertRgb64ToRgb30<PixelOrderBGR>(QRgba64 c)
{
    c = qRepremultiply<14>(c);
    const uint a = c.alpha() >> 14;
    const uint r = c.red() >> 6;
    const uint g = c.green() >> 6;
    const uint b = c.blue() >> 6;
    return (a << 30) | (b << 20) | (g << 10) | r;
}

template<>
inline unsigned int qConvertRgb64ToRgb30<PixelOrderRGB>(QRgba64 c)
{
    c = qRepremultiply<14>(c);
    const uint a = c.alpha() >> 14;
    const uint r = c.red() >> 6;
    const uint g = c.green() >> 6;
    const uint b = c.blue() >> 6;
    return (a << 30) | (r << 20) | (g << 10) | b;
}

inline uint qRgbSwapRgb30(uint c)
{
    const uint ag = c & 0xc00ffc00;
    const uint rb = c & 0x3ff003ff;
    return ag | (rb << 20) | (rb >> 20);
}

inline int qRed565(quint16 rgb) {
    const int r = (rgb & 0xf800);
    return (r >> 8) | (r >> 13);
}

inline int qGreen565(quint16 rgb) {
    const int g = (rgb & 0x07e0);
    return (g >> 3) | (g >> 9);
}

inline int qBlue565(quint16 rgb) {
    const int b = (rgb & 0x001f);
    return (b << 3) | (b >> 2);
}

// We manually unalias the variables to make sure the compiler
// fully optimizes both aliased and unaliased cases.
#define UNALIASED_CONVERSION_LOOP(buffer, src, count, conversion) \
    if (src == buffer) { \
        for (int i = 0; i < count; ++i) \
            buffer[i] = conversion(buffer[i]); \
    } else { \
        for (int i = 0; i < count; ++i) \
            buffer[i] = conversion(src[i]); \
    }


static Q_ALWAYS_INLINE const uint *qt_convertARGB32ToARGB32PM(uint *buffer, const uint *src, int count)
{
    UNALIASED_CONVERSION_LOOP(buffer, src, count, qPremultiply);
    return buffer;
}

static Q_ALWAYS_INLINE const uint *qt_convertRGBA8888ToARGB32PM(uint *buffer, const uint *src, int count)
{
    UNALIASED_CONVERSION_LOOP(buffer, src, count, [](uint s) { return qPremultiply(RGBA2ARGB(s));});
    return buffer;
}

template<bool RGBA> void qt_convertRGBA64ToARGB32(uint *dst, const QRgba64 *src, int count);

const uint qt_bayer_matrix[16][16] = {
    { 0x1, 0xc0, 0x30, 0xf0, 0xc, 0xcc, 0x3c, 0xfc,
      0x3, 0xc3, 0x33, 0xf3, 0xf, 0xcf, 0x3f, 0xff},
    { 0x80, 0x40, 0xb0, 0x70, 0x8c, 0x4c, 0xbc, 0x7c,
      0x83, 0x43, 0xb3, 0x73, 0x8f, 0x4f, 0xbf, 0x7f},
    { 0x20, 0xe0, 0x10, 0xd0, 0x2c, 0xec, 0x1c, 0xdc,
      0x23, 0xe3, 0x13, 0xd3, 0x2f, 0xef, 0x1f, 0xdf},
    { 0xa0, 0x60, 0x90, 0x50, 0xac, 0x6c, 0x9c, 0x5c,
      0xa3, 0x63, 0x93, 0x53, 0xaf, 0x6f, 0x9f, 0x5f},
    { 0x8, 0xc8, 0x38, 0xf8, 0x4, 0xc4, 0x34, 0xf4,
      0xb, 0xcb, 0x3b, 0xfb, 0x7, 0xc7, 0x37, 0xf7},
    { 0x88, 0x48, 0xb8, 0x78, 0x84, 0x44, 0xb4, 0x74,
      0x8b, 0x4b, 0xbb, 0x7b, 0x87, 0x47, 0xb7, 0x77},
    { 0x28, 0xe8, 0x18, 0xd8, 0x24, 0xe4, 0x14, 0xd4,
      0x2b, 0xeb, 0x1b, 0xdb, 0x27, 0xe7, 0x17, 0xd7},
    { 0xa8, 0x68, 0x98, 0x58, 0xa4, 0x64, 0x94, 0x54,
      0xab, 0x6b, 0x9b, 0x5b, 0xa7, 0x67, 0x97, 0x57},
    { 0x2, 0xc2, 0x32, 0xf2, 0xe, 0xce, 0x3e, 0xfe,
      0x1, 0xc1, 0x31, 0xf1, 0xd, 0xcd, 0x3d, 0xfd},
    { 0x82, 0x42, 0xb2, 0x72, 0x8e, 0x4e, 0xbe, 0x7e,
      0x81, 0x41, 0xb1, 0x71, 0x8d, 0x4d, 0xbd, 0x7d},
    { 0x22, 0xe2, 0x12, 0xd2, 0x2e, 0xee, 0x1e, 0xde,
      0x21, 0xe1, 0x11, 0xd1, 0x2d, 0xed, 0x1d, 0xdd},
    { 0xa2, 0x62, 0x92, 0x52, 0xae, 0x6e, 0x9e, 0x5e,
      0xa1, 0x61, 0x91, 0x51, 0xad, 0x6d, 0x9d, 0x5d},
    { 0xa, 0xca, 0x3a, 0xfa, 0x6, 0xc6, 0x36, 0xf6,
      0x9, 0xc9, 0x39, 0xf9, 0x5, 0xc5, 0x35, 0xf5},
    { 0x8a, 0x4a, 0xba, 0x7a, 0x86, 0x46, 0xb6, 0x76,
      0x89, 0x49, 0xb9, 0x79, 0x85, 0x45, 0xb5, 0x75},
    { 0x2a, 0xea, 0x1a, 0xda, 0x26, 0xe6, 0x16, 0xd6,
      0x29, 0xe9, 0x19, 0xd9, 0x25, 0xe5, 0x15, 0xd5},
    { 0xaa, 0x6a, 0x9a, 0x5a, 0xa6, 0x66, 0x96, 0x56,
      0xa9, 0x69, 0x99, 0x59, 0xa5, 0x65, 0x95, 0x55}
};

#define ARGB_COMBINE_ALPHA(argb, alpha) \
    ((((argb >> 24) * alpha) >> 8) << 24) | (argb & 0x00ffffff)


#if Q_PROCESSOR_WORDSIZE == 8 // 64-bit versions
#define AMIX(mask) (qMin(((quint64(s)&mask) + (quint64(d)&mask)), quint64(mask)))
#define MIX(mask) (qMin(((quint64(s)&mask) + (quint64(d)&mask)), quint64(mask)))
#else // 32 bits
// The mask for alpha can overflow over 32 bits
#define AMIX(mask) quint32(qMin(((quint64(s)&mask) + (quint64(d)&mask)), quint64(mask)))
#define MIX(mask) (qMin(((quint32(s)&mask) + (quint32(d)&mask)), quint32(mask)))
#endif

inline uint comp_func_Plus_one_pixel_const_alpha(uint d, const uint s, const uint const_alpha, const uint one_minus_const_alpha)
{
    const uint result = uint(AMIX(AMASK) | MIX(RMASK) | MIX(GMASK) | MIX(BMASK));
    return INTERPOLATE_PIXEL_255(result, const_alpha, d, one_minus_const_alpha);
}

inline uint comp_func_Plus_one_pixel(uint d, const uint s)
{
    const uint result = uint(AMIX(AMASK) | MIX(RMASK) | MIX(GMASK) | MIX(BMASK));
    return result;
}

#undef MIX
#undef AMIX

// must be multiple of 4 for easier SIMD implementations
static Q_CONSTEXPR int BufferSize = 2048;

// A buffer of intermediate results used by simple bilinear scaling.
struct IntermediateBuffer
{
    // The idea is first to do the interpolation between the row s1 and the row s2
    // into this intermediate buffer, then later interpolate between two pixel of this buffer.
    //
    // buffer_rb is a buffer of red-blue component of the pixel, in the form 0x00RR00BB
    // buffer_ag is the alpha-green component of the pixel, in the form 0x00AA00GG
    // +1 for the last pixel to interpolate with, and +1 for rounding errors.
    quint32 buffer_rb[BufferSize+2];
    quint32 buffer_ag[BufferSize+2];
};

struct QDitherInfo {
    int x;
    int y;
};

typedef const uint *(QT_FASTCALL *FetchAndConvertPixelsFunc)(uint *buffer, const uchar *src, int index, int count,
                                                             const QVector<QRgb> *clut, QDitherInfo *dither);
typedef void (QT_FASTCALL *ConvertAndStorePixelsFunc)(uchar *dest, const uint *src, int index, int count,
                                                      const QVector<QRgb> *clut, QDitherInfo *dither);

typedef const QRgba64 *(QT_FASTCALL *FetchAndConvertPixelsFunc64)(QRgba64 *buffer, const uchar *src, int index, int count,
                                                                 const QVector<QRgb> *clut, QDitherInfo *dither);
typedef void (QT_FASTCALL *ConvertAndStorePixelsFunc64)(uchar *dest, const QRgba64 *src, int index, int count,
                                                        const QVector<QRgb> *clut, QDitherInfo *dither);

typedef void (QT_FASTCALL *ConvertFunc)(uint *buffer, int count, const QVector<QRgb> *clut);
typedef void (QT_FASTCALL *Convert64Func)(quint64 *buffer, int count, const QVector<QRgb> *clut);
typedef const QRgba64 *(QT_FASTCALL *ConvertTo64Func)(QRgba64 *buffer, const uint *src, int count,
                                                      const QVector<QRgb> *clut, QDitherInfo *dither);
typedef void (QT_FASTCALL *RbSwapFunc)(uchar *dst, const uchar *src, int count);


struct QPixelLayout
{
    // Bits per pixel
    enum BPP {
        BPPNone,
        BPP1MSB,
        BPP1LSB,
        BPP8,
        BPP16,
        BPP24,
        BPP32,
        BPP64,
        BPPCount
    };

    bool hasAlphaChannel;
    bool premultiplied;
    BPP bpp;
    RbSwapFunc rbSwap;
    ConvertFunc convertToARGB32PM;
    ConvertTo64Func convertToRGBA64PM;
    FetchAndConvertPixelsFunc fetchToARGB32PM;
    FetchAndConvertPixelsFunc64 fetchToRGBA64PM;
    ConvertAndStorePixelsFunc storeFromARGB32PM;
    ConvertAndStorePixelsFunc storeFromRGB32;
};

extern ConvertAndStorePixelsFunc64 qStoreFromRGBA64PM[QImage::NImageFormats];

extern QPixelLayout qPixelLayouts[QImage::NImageFormats];

extern MemRotateFunc qMemRotateFunctions[QPixelLayout::BPPCount][3];

QT_END_NAMESPACE

#endif // QDRAWHELPER_P_H
