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

#ifndef QRGBA64_P_H
#define QRGBA64_P_H

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

#include "qrgba64.h"
#include "qdrawhelper_p.h"

#include <QtCore/private/qsimd_p.h>
#include <QtGui/private/qtguiglobal_p.h>

QT_BEGIN_NAMESPACE

inline QRgba64 combineAlpha256(QRgba64 rgba64, uint alpha256)
{
    return QRgba64::fromRgba64(rgba64.red(), rgba64.green(), rgba64.blue(), (rgba64.alpha() * alpha256) >> 8);
}

inline QRgba64 multiplyAlpha65535(QRgba64 rgba64, uint alpha65535)
{
    return QRgba64::fromRgba64(qt_div_65535(rgba64.red()   * alpha65535),
                               qt_div_65535(rgba64.green() * alpha65535),
                               qt_div_65535(rgba64.blue()  * alpha65535),
                               qt_div_65535(rgba64.alpha() * alpha65535));
}

#ifdef __SSE2__
Q_ALWAYS_INLINE __m128i multiplyAlpha65535(__m128i rgba64, __m128i va)
{
    __m128i vs = rgba64;
    vs = _mm_unpacklo_epi16(_mm_mullo_epi16(vs, va), _mm_mulhi_epu16(vs, va));
    vs = _mm_add_epi32(vs, _mm_srli_epi32(vs, 16));
    vs = _mm_add_epi32(vs, _mm_set1_epi32(0x8000));
    vs = _mm_srai_epi32(vs, 16);
    vs = _mm_packs_epi32(vs, _mm_setzero_si128());
    return vs;
}
Q_ALWAYS_INLINE __m128i multiplyAlpha65535(__m128i rgba64, uint alpha65535)
{
    const __m128i va = _mm_shufflelo_epi16(_mm_cvtsi32_si128(alpha65535), _MM_SHUFFLE(0, 0, 0, 0));
    return multiplyAlpha65535(rgba64, va);
}
#endif

#if defined(__ARM_NEON__)
Q_ALWAYS_INLINE uint16x4_t multiplyAlpha65535(uint16x4_t rgba64, uint16x4_t alpha65535)
{
    uint32x4_t vs32 = vmull_u16(rgba64, alpha65535); // vs = vs * alpha
    vs32 = vsraq_n_u32(vs32, vs32, 16); // vs = vs + (vs >> 16)
    return vrshrn_n_u32(vs32, 16); // vs = (vs + 0x8000) >> 16
}
Q_ALWAYS_INLINE uint16x4_t multiplyAlpha65535(uint16x4_t rgba64, uint alpha65535)
{
    uint32x4_t vs32 = vmull_n_u16(rgba64, alpha65535); // vs = vs * alpha
    vs32 = vsraq_n_u32(vs32, vs32, 16); // vs = vs + (vs >> 16)
    return vrshrn_n_u32(vs32, 16); // vs = (vs + 0x8000) >> 16
}
#endif

template<typename T>
inline T multiplyAlpha255(T rgba64, uint alpha255)
{
#if defined(__SSE2__) || defined(__ARM_NEON__)
    return multiplyAlpha65535(rgba64, alpha255 * 257);
#else
    return QRgba64::fromRgba64(qt_div_255(rgba64.red()   * alpha255),
                               qt_div_255(rgba64.green() * alpha255),
                               qt_div_255(rgba64.blue()  * alpha255),
                               qt_div_255(rgba64.alpha() * alpha255));
#endif
}

inline QRgba64 interpolate255(QRgba64 x, uint alpha1, QRgba64 y, uint alpha2)
{
    return QRgba64::fromRgba64(multiplyAlpha255(x, alpha1) + multiplyAlpha255(y, alpha2));
}

#if defined __SSE2__
Q_ALWAYS_INLINE __m128i interpolate255(__m128i x, uint alpha1, __m128i y, uint alpha2)
{
    return _mm_add_epi32(multiplyAlpha255(x, alpha1), multiplyAlpha255(y, alpha2));
}
#endif

#if defined __ARM_NEON__
Q_ALWAYS_INLINE uint16x4_t interpolate255(uint16x4_t x, uint alpha1, uint16x4_t y, uint alpha2)
{
    return vadd_u16(multiplyAlpha255(x, alpha1), multiplyAlpha255(y, alpha2));
}
#endif

inline QRgba64 interpolate65535(QRgba64 x, uint alpha1, QRgba64 y, uint alpha2)
{
    return QRgba64::fromRgba64(multiplyAlpha65535(x, alpha1) + multiplyAlpha65535(y, alpha2));
}

#if defined __SSE2__
Q_ALWAYS_INLINE __m128i interpolate65535(__m128i x, uint alpha1, __m128i y, uint alpha2)
{
    return _mm_add_epi32(multiplyAlpha65535(x, alpha1), multiplyAlpha65535(y, alpha2));
}
// alpha2 below is const-ref because otherwise MSVC2015 complains that it can't 16-byte align the argument.
Q_ALWAYS_INLINE __m128i interpolate65535(__m128i x, __m128i alpha1, __m128i y, const __m128i &alpha2)
{
    return _mm_add_epi32(multiplyAlpha65535(x, alpha1), multiplyAlpha65535(y, alpha2));
}
#endif

#if defined __ARM_NEON__
Q_ALWAYS_INLINE uint16x4_t interpolate65535(uint16x4_t x, uint alpha1, uint16x4_t y, uint alpha2)
{
    return vadd_u16(multiplyAlpha65535(x, alpha1), multiplyAlpha65535(y, alpha2));
}
Q_ALWAYS_INLINE uint16x4_t interpolate65535(uint16x4_t x, uint16x4_t alpha1, uint16x4_t y, uint16x4_t alpha2)
{
    return vadd_u16(multiplyAlpha65535(x, alpha1), multiplyAlpha65535(y, alpha2));
}
#endif

inline QRgba64 addWithSaturation(QRgba64 a, QRgba64 b)
{
    return QRgba64::fromRgba64(qMin(a.red() + b.red(), 65535),
                               qMin(a.green() + b.green(), 65535),
                               qMin(a.blue() + b.blue(), 65535),
                               qMin(a.alpha() + b.alpha(), 65535));
}

#if QT_COMPILER_SUPPORTS_HERE(SSE2)
QT_FUNCTION_TARGET(SSE2)
Q_ALWAYS_INLINE uint toArgb32(__m128i v)
{
    v = _mm_unpacklo_epi16(v, _mm_setzero_si128());
    v = _mm_add_epi32(v, _mm_set1_epi32(128));
    v = _mm_sub_epi32(v, _mm_srli_epi32(v, 8));
    v = _mm_srli_epi32(v, 8);
    v = _mm_packs_epi32(v, v);
    v = _mm_packus_epi16(v, v);
    return _mm_cvtsi128_si32(v);
}
#elif defined __ARM_NEON__
Q_ALWAYS_INLINE uint toArgb32(uint16x4_t v)
{
    v = vsub_u16(v, vrshr_n_u16(v, 8));
    v = vrshr_n_u16(v, 8);
    uint8x8_t v8 = vmovn_u16(vcombine_u16(v, v));
    return vget_lane_u32(vreinterpret_u32_u8(v8), 0);
}
#endif

inline uint toArgb32(QRgba64 rgba64)
{
#if defined __SSE2__
    __m128i v = _mm_loadl_epi64((const __m128i *)&rgba64);
    v = _mm_shufflelo_epi16(v, _MM_SHUFFLE(3, 0, 1, 2));
    return toArgb32(v);
#elif defined __ARM_NEON__
    uint16x4_t v = vreinterpret_u16_u64(vld1_u64(reinterpret_cast<const uint64_t *>(&rgba64)));
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    const uint8x8_t shuffleMask = { 4, 5, 2, 3, 0, 1, 6, 7 };
    v = vreinterpret_u16_u8(vtbl1_u8(vreinterpret_u8_u16(v), shuffleMask));
#else
    v = vext_u16(v, v, 3);
#endif
    return toArgb32(v);
#else
    return rgba64.toArgb32();
#endif
}

inline uint toRgba8888(QRgba64 rgba64)
{
#if defined __SSE2__
    __m128i v = _mm_loadl_epi64((const __m128i *)&rgba64);
    return toArgb32(v);
#elif defined __ARM_NEON__
    uint16x4_t v = vreinterpret_u16_u64(vld1_u64(reinterpret_cast<const uint64_t *>(&rgba64)));
    return toArgb32(v);
#else
    return ARGB2RGBA(toArgb32(rgba64));
#endif
}

inline QRgba64 rgbBlend(QRgba64 d, QRgba64 s, uint rgbAlpha)
{
    QRgba64 blend;
#if defined(__SSE2__)
    __m128i vd = _mm_loadl_epi64((const __m128i *)&d);
    __m128i vs = _mm_loadl_epi64((const __m128i *)&s);
    __m128i va =  _mm_cvtsi32_si128(rgbAlpha);
    va = _mm_unpacklo_epi8(va, va);
    va = _mm_shufflelo_epi16(va, _MM_SHUFFLE(3, 0, 1, 2));
    __m128i vb = _mm_xor_si128(_mm_set1_epi16(-1), va);

    vs = _mm_unpacklo_epi16(_mm_mullo_epi16(vs, va), _mm_mulhi_epu16(vs, va));
    vd = _mm_unpacklo_epi16(_mm_mullo_epi16(vd, vb), _mm_mulhi_epu16(vd, vb));
    vd = _mm_add_epi32(vd, vs);
    vd = _mm_add_epi32(vd, _mm_srli_epi32(vd, 16));
    vd = _mm_add_epi32(vd, _mm_set1_epi32(0x8000));
    vd = _mm_srai_epi32(vd, 16);
    vd = _mm_packs_epi32(vd, _mm_setzero_si128());

    _mm_storel_epi64((__m128i *)&blend, vd);
#elif defined(__ARM_NEON__)
    uint16x4_t vd = vreinterpret_u16_u64(vmov_n_u64(d));
    uint16x4_t vs = vreinterpret_u16_u64(vmov_n_u64(s));
    uint8x8_t va8 = vreinterpret_u8_u32(vmov_n_u32(ARGB2RGBA(rgbAlpha)));
    uint16x4_t va = vreinterpret_u16_u8(vzip_u8(va8, va8).val[0]);
    uint16x4_t vb = vdup_n_u16(0xffff);
    vb = vsub_u16(vb, va);

    uint32x4_t vs32 = vmull_u16(vs, va);
    uint32x4_t vd32 = vmull_u16(vd, vb);
    vd32 = vaddq_u32(vd32, vs32);
    vd32 = vsraq_n_u32(vd32, vd32, 16);
    vd = vrshrn_n_u32(vd32, 16);
    vst1_u64(reinterpret_cast<uint64_t *>(&blend), vreinterpret_u64_u16(vd));
#else
    const int mr = qRed(rgbAlpha);
    const int mg = qGreen(rgbAlpha);
    const int mb = qBlue(rgbAlpha);
    blend = qRgba64(qt_div_255(s.red()   * mr + d.red()   * (255 - mr)),
                    qt_div_255(s.green() * mg + d.green() * (255 - mg)),
                    qt_div_255(s.blue()  * mb + d.blue()  * (255 - mb)),
                    s.alpha());
#endif
    return blend;
}

static Q_ALWAYS_INLINE void blend_pixel(QRgba64 &dst, QRgba64 src)
{
    if (src.isOpaque())
        dst = src;
    else if (!src.isTransparent())
        dst = src + multiplyAlpha65535(dst, 65535 - src.alpha());
}

static Q_ALWAYS_INLINE void blend_pixel(QRgba64 &dst, QRgba64 src, const int const_alpha)
{
    if (const_alpha == 255)
        return blend_pixel(dst, src);
    if (!src.isTransparent()) {
        src = multiplyAlpha255(src, const_alpha);
        dst = src + multiplyAlpha65535(dst, 65535 - src.alpha());
    }
}

QT_END_NAMESPACE

#endif // QRGBA64_P_H
