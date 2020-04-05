/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#ifndef QCOLORMATRIX_H
#define QCOLORMATRIX_H

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

#include <QtGui/qtguiglobal.h>
#include <QtCore/qpoint.h>
#include <cmath>

QT_BEGIN_NAMESPACE

// An abstract 3 value color
class QColorVector
{
public:
    QColorVector() = default;
    Q_DECL_CONSTEXPR QColorVector(float x, float y, float z) : x(x), y(y), z(z) { }
    explicit Q_DECL_CONSTEXPR QColorVector(const QPointF &chr) // from XY chromaticity
            : x(chr.x() / chr.y())
            , y(1.0f)
            , z((1.0 - chr.x() - chr.y()) / chr.y())
    { }
    float x = 0.0f; // X, x or red
    float y = 0.0f; // Y, y or green
    float z = 0.0f; // Z, Y or blue
    float _unused = 0.0f;

    friend inline bool operator==(const QColorVector &v1, const QColorVector &v2);
    friend inline bool operator!=(const QColorVector &v1, const QColorVector &v2);
    bool isNull() const
    {
        return !x && !y && !z;
    }

    static bool isValidChromaticity(const QPointF &chr)
    {
        if (chr.x() < qreal(0.0) || chr.x() > qreal(1.0))
            return false;
        if (chr.y() <= qreal(0.0) || chr.y() > qreal(1.0))
            return false;
        if (chr.x() + chr.y() > qreal(1.0))
            return false;
        return true;
    }

    // Common whitepoints:
    static Q_DECL_CONSTEXPR QPointF D50Chromaticity() { return QPointF(0.34567, 0.35850); }
    static Q_DECL_CONSTEXPR QPointF D65Chromaticity() { return QPointF(0.31271, 0.32902); }
    static Q_DECL_CONSTEXPR QColorVector D50() { return QColorVector(D50Chromaticity()); }
    static Q_DECL_CONSTEXPR QColorVector D65() { return QColorVector(D65Chromaticity()); }
};

inline bool operator==(const QColorVector &v1, const QColorVector &v2)
{
    return (std::abs(v1.x - v2.x) < (1.0f / 2048.0f))
        && (std::abs(v1.y - v2.y) < (1.0f / 2048.0f))
        && (std::abs(v1.z - v2.z) < (1.0f / 2048.0f));
}

inline bool operator!=(const QColorVector &v1, const QColorVector &v2)
{
    return !(v1 == v2);
}


// A matrix mapping 3 value colors.
// Not using QMatrix because only floats are needed and performance is critical.
class QColorMatrix
{
public:
    // We are storing the matrix transposed as that is more convenient:
    QColorVector r;
    QColorVector g;
    QColorVector b;

    friend inline bool operator==(const QColorMatrix &m1, const QColorMatrix &m2);
    friend inline bool operator!=(const QColorMatrix &m1, const QColorMatrix &m2);

    bool isNull() const
    {
        return r.isNull() && g.isNull() && b.isNull();
    }
    bool isValid() const
    {
        // A color matrix must be invertible
        float det = r.x * (b.z * g.y - g.z * b.y) -
                    r.y * (b.z * g.x - g.z * b.x) +
                    r.z * (b.y * g.x - g.y * b.x);
        return !qFuzzyIsNull(det);
    }

    QColorMatrix inverted() const
    {
        float det = r.x * (b.z * g.y - g.z * b.y) -
                    r.y * (b.z * g.x - g.z * b.x) +
                    r.z * (b.y * g.x - g.y * b.x);
        det = 1.0f / det;
        QColorMatrix inv;
        inv.r.x = (g.y * b.z - b.y * g.z) * det;
        inv.r.y = (b.y * r.z - r.y * b.z) * det;
        inv.r.z = (r.y * g.z - g.y * r.z) * det;
        inv.g.x = (b.x * g.z - g.x * b.z) * det;
        inv.g.y = (r.x * b.z - b.x * r.z) * det;
        inv.g.z = (g.x * r.z - r.x * g.z) * det;
        inv.b.x = (g.x * b.y - b.x * g.y) * det;
        inv.b.y = (b.x * r.y - r.x * b.y) * det;
        inv.b.z = (r.x * g.y - g.x * r.y) * det;
        return inv;
    }
    QColorMatrix operator*(const QColorMatrix &o) const
    {
        QColorMatrix comb;
        comb.r.x = r.x * o.r.x + g.x * o.r.y + b.x * o.r.z;
        comb.g.x = r.x * o.g.x + g.x * o.g.y + b.x * o.g.z;
        comb.b.x = r.x * o.b.x + g.x * o.b.y + b.x * o.b.z;

        comb.r.y = r.y * o.r.x + g.y * o.r.y + b.y * o.r.z;
        comb.g.y = r.y * o.g.x + g.y * o.g.y + b.y * o.g.z;
        comb.b.y = r.y * o.b.x + g.y * o.b.y + b.y * o.b.z;

        comb.r.z = r.z * o.r.x + g.z * o.r.y + b.z * o.r.z;
        comb.g.z = r.z * o.g.x + g.z * o.g.y + b.z * o.g.z;
        comb.b.z = r.z * o.b.x + g.z * o.b.y + b.z * o.b.z;
        return comb;

    }
    QColorVector map(const QColorVector &c) const
    {
        return QColorVector { c.x * r.x + c.y * g.x + c.z * b.x,
                              c.x * r.y + c.y * g.y + c.z * b.y,
                              c.x * r.z + c.y * g.z + c.z * b.z };
    }
    QColorMatrix transposed() const
    {
        return QColorMatrix { { r.x, g.x, b.x },
                              { r.y, g.y, b.y },
                              { r.z, g.z, b.z } };
    }

    static QColorMatrix identity()
    {
        return { { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };
    }
    static QColorMatrix fromScale(QColorVector v)
    {
        return QColorMatrix { { v.x,  0.0f, 0.0f },
                              { 0.0f, v.y,  0.0f },
                              { 0.0f, 0.0f, v.z  } };
    }
    // These are used to recognize matrices from ICC profiles:
    static QColorMatrix toXyzFromSRgb()
    {
        return QColorMatrix { { 0.4360217452f, 0.2224751115f, 0.0139281144f },
                              { 0.3851087987f, 0.7169067264f, 0.0971015394f },
                              { 0.1430812478f, 0.0606181994f, 0.7141585946f } };
    }
    static QColorMatrix toXyzFromAdobeRgb()
    {
        return QColorMatrix { { 0.6097189188f, 0.3111021519f, 0.0194766335f },
                              { 0.2052682191f, 0.6256770492f, 0.0608891509f },
                              { 0.1492247432f, 0.0632209629f, 0.7448224425f } };
    }
    static QColorMatrix toXyzFromDciP3D65()
    {
        return QColorMatrix { { 0.5150973201f, 0.2411795557f, -0.0010491034f },
                              { 0.2919696569f, 0.6922441125f,  0.0418830328f },
                              { 0.1571449190f, 0.0665764511f,  0.7843542695f } };
    }
    static QColorMatrix toXyzFromProPhotoRgb()
    {
        return QColorMatrix { { 0.7976672649f, 0.2880374491f, 0.0000000000f },
                              { 0.1351922452f, 0.7118769884f, 0.0000000000f },
                              { 0.0313525312f, 0.0000856627f, 0.8251883388f } };
    }
};

inline bool operator==(const QColorMatrix &m1, const QColorMatrix &m2)
{
    return (m1.r == m2.r) && (m1.g == m2.g) && (m1.b == m2.b);
}

inline bool operator!=(const QColorMatrix &m1, const QColorMatrix &m2)
{
    return !(m1 == m2);
}

QT_END_NAMESPACE

#endif // QCOLORMATRIX_P_H
