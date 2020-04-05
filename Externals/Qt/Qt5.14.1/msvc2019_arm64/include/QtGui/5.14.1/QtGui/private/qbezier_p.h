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

#ifndef QBEZIER_P_H
#define QBEZIER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtGui/private/qtguiglobal_p.h>
#include "QtCore/qpoint.h"
#include "QtCore/qline.h"
#include "QtCore/qrect.h"
#include "QtCore/qvector.h"
#include "QtCore/qlist.h"
#include "QtCore/qpair.h"
#include "QtGui/qtransform.h"
#include <private/qdatabuffer_p.h>

QT_BEGIN_NAMESPACE

class QPolygonF;

class Q_GUI_EXPORT QBezier
{
public:
    static QBezier fromPoints(const QPointF &p1, const QPointF &p2,
                              const QPointF &p3, const QPointF &p4)
    { return {p1.x(), p1.y(), p2.x(), p2.y(), p3.x(), p3.y(), p4.x(), p4.y()}; }

    static void coefficients(qreal t, qreal &a, qreal &b, qreal &c, qreal &d);

    inline QPointF pointAt(qreal t) const;
    inline QPointF normalVector(qreal t) const;

    inline QPointF derivedAt(qreal t) const;
    inline QPointF secondDerivedAt(qreal t) const;

    QPolygonF toPolygon(qreal bezier_flattening_threshold = 0.5) const;
    void addToPolygon(QPolygonF *p, qreal bezier_flattening_threshold = 0.5) const;
    void addToPolygon(QDataBuffer<QPointF> &polygon, qreal bezier_flattening_threshold) const;

    QRectF bounds() const;
    qreal length(qreal error = 0.01) const;
    void addIfClose(qreal *length, qreal error) const;

    qreal tAtLength(qreal len) const;

    int stationaryYPoints(qreal &t0, qreal &t1) const;
    qreal tForY(qreal t0, qreal t1, qreal y) const;

    QPointF pt1() const { return QPointF(x1, y1); }
    QPointF pt2() const { return QPointF(x2, y2); }
    QPointF pt3() const { return QPointF(x3, y3); }
    QPointF pt4() const { return QPointF(x4, y4); }

    QBezier mapBy(const QTransform &transform) const;

    inline QPointF midPoint() const;
    inline QLineF midTangent() const;

    inline QLineF startTangent() const;
    inline QLineF endTangent() const;

    inline void parameterSplitLeft(qreal t, QBezier *left);
    inline std::pair<QBezier, QBezier> split() const;

    int shifted(QBezier *curveSegments, int maxSegmets,
                qreal offset, float threshold) const;

    QBezier bezierOnInterval(qreal t0, qreal t1) const;
    QBezier getSubRange(qreal t0, qreal t1) const;

    qreal x1, y1, x2, y2, x3, y3, x4, y4;
};

inline QPointF QBezier::midPoint() const
{
    return QPointF((x1 + x4 + 3*(x2 + x3))/8., (y1 + y4 + 3*(y2 + y3))/8.);
}

inline QLineF QBezier::midTangent() const
{
    QPointF mid = midPoint();
    QLineF dir(QLineF(x1, y1, x2, y2).pointAt(0.5), QLineF(x3, y3, x4, y4).pointAt(0.5));
    return QLineF(mid.x() - dir.dx(), mid.y() - dir.dy(),
                  mid.x() + dir.dx(), mid.y() + dir.dy());
}

inline QLineF QBezier::startTangent() const
{
    QLineF tangent(pt1(), pt2());
    if (tangent.isNull())
        tangent = QLineF(pt1(), pt3());
    if (tangent.isNull())
        tangent = QLineF(pt1(), pt4());
    return tangent;
}

inline QLineF QBezier::endTangent() const
{
    QLineF tangent(pt4(), pt3());
    if (tangent.isNull())
        tangent = QLineF(pt4(), pt2());
    if (tangent.isNull())
        tangent = QLineF(pt4(), pt1());
    return tangent;
}

inline void QBezier::coefficients(qreal t, qreal &a, qreal &b, qreal &c, qreal &d)
{
    qreal m_t = 1. - t;
    b = m_t * m_t;
    c = t * t;
    d = c * t;
    a = b * m_t;
    b *= 3. * t;
    c *= 3. * m_t;
}

inline QPointF QBezier::pointAt(qreal t) const
{
    // numerically more stable:
    qreal x, y;

    qreal m_t = 1. - t;
    {
        qreal a = x1*m_t + x2*t;
        qreal b = x2*m_t + x3*t;
        qreal c = x3*m_t + x4*t;
        a = a*m_t + b*t;
        b = b*m_t + c*t;
        x = a*m_t + b*t;
    }
    {
        qreal a = y1*m_t + y2*t;
        qreal b = y2*m_t + y3*t;
        qreal c = y3*m_t + y4*t;
        a = a*m_t + b*t;
        b = b*m_t + c*t;
        y = a*m_t + b*t;
    }
    return QPointF(x, y);
}

inline QPointF QBezier::normalVector(qreal t) const
{
    qreal m_t = 1. - t;
    qreal a = m_t * m_t;
    qreal b = t * m_t;
    qreal c = t * t;

    return QPointF((y2-y1) * a + (y3-y2) * b + (y4-y3) * c,  -(x2-x1) * a - (x3-x2) * b - (x4-x3) * c);
}

inline QPointF QBezier::derivedAt(qreal t) const
{
    // p'(t) = 3 * (-(1-2t+t^2) * p0 + (1 - 4 * t + 3 * t^2) * p1 + (2 * t - 3 * t^2) * p2 + t^2 * p3)

    qreal m_t = 1. - t;

    qreal d = t * t;
    qreal a = -m_t * m_t;
    qreal b = 1 - 4 * t + 3 * d;
    qreal c = 2 * t - 3 * d;

    return 3 * QPointF(a * x1 + b * x2 + c * x3 + d * x4,
                       a * y1 + b * y2 + c * y3 + d * y4);
}

inline QPointF QBezier::secondDerivedAt(qreal t) const
{
    qreal a = 2. - 2. * t;
    qreal b = -4 + 6 * t;
    qreal c = 2 - 6 * t;
    qreal d = 2 * t;

    return 3 * QPointF(a * x1 + b * x2 + c * x3 + d * x4,
                       a * y1 + b * y2 + c * y3 + d * y4);
}

std::pair<QBezier, QBezier> QBezier::split() const
{
    const auto mid = [](QPointF lhs, QPointF rhs) { return (lhs + rhs) * .5; };

    const QPointF mid_12 = mid(pt1(), pt2());
    const QPointF mid_23 = mid(pt2(), pt3());
    const QPointF mid_34 = mid(pt3(), pt4());
    const QPointF mid_12_23 = mid(mid_12, mid_23);
    const QPointF mid_23_34 = mid(mid_23, mid_34);
    const QPointF mid_12_23__23_34 = mid(mid_12_23, mid_23_34);

    return {
        fromPoints(pt1(), mid_12, mid_12_23, mid_12_23__23_34),
        fromPoints(mid_12_23__23_34, mid_23_34, mid_34, pt4()),
    };
}

inline void QBezier::parameterSplitLeft(qreal t, QBezier *left)
{
    left->x1 = x1;
    left->y1 = y1;

    left->x2 = x1 + t * ( x2 - x1 );
    left->y2 = y1 + t * ( y2 - y1 );

    left->x3 = x2 + t * ( x3 - x2 ); // temporary holding spot
    left->y3 = y2 + t * ( y3 - y2 ); // temporary holding spot

    x3 = x3 + t * ( x4 - x3 );
    y3 = y3 + t * ( y4 - y3 );

    x2 = left->x3 + t * ( x3 - left->x3);
    y2 = left->y3 + t * ( y3 - left->y3);

    left->x3 = left->x2 + t * ( left->x3 - left->x2 );
    left->y3 = left->y2 + t * ( left->y3 - left->y2 );

    left->x4 = x1 = left->x3 + t * (x2 - left->x3);
    left->y4 = y1 = left->y3 + t * (y2 - left->y3);
}

QT_END_NAMESPACE

#endif // QBEZIER_P_H
