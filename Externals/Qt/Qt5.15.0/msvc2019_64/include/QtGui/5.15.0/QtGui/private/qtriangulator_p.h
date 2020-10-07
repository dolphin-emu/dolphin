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

#ifndef QTRIANGULATOR_P_H
#define QTRIANGULATOR_P_H

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
#include <QtCore/qvector.h>
#include <QtGui/private/qvectorpath_p.h>

QT_BEGIN_NAMESPACE

class QVertexIndexVector
{
public:
    enum Type {
        UnsignedInt,
        UnsignedShort
    };

    inline Type type() const { return t; }

    inline void setDataUint(const QVector<quint32> &data)
    {
        t = UnsignedInt;
        indices32 = data;
    }

    inline void setDataUshort(const QVector<quint16> &data)
    {
        t = UnsignedShort;
        indices16 = data;
    }

    inline const void* data() const
    {
        if (t == UnsignedInt)
            return indices32.data();
        return indices16.data();
    }

    inline int size() const
    {
        if (t == UnsignedInt)
            return indices32.size();
        return indices16.size();
    }

private:

    Type t;
    QVector<quint32> indices32;
    QVector<quint16> indices16;
};

struct QTriangleSet
{
    // The vertices of a triangle are given by: (x[i[n]], y[i[n]]), (x[j[n]], y[j[n]]), (x[k[n]], y[k[n]]), n = 0, 1, ...
    QVector<qreal> vertices; // [x[0], y[0], x[1], y[1], x[2], ...]
    QVertexIndexVector indices; // [i[0], j[0], k[0], i[1], j[1], k[1], i[2], ...]
};

struct QPolylineSet
{
    QVector<qreal> vertices; // [x[0], y[0], x[1], y[1], x[2], ...]
    QVertexIndexVector indices; // End of polyline is marked with -1.
};

// The vertex coordinates of the returned triangle set will be rounded to a grid with a mesh size
// of 1/32. The polygon is first transformed, then scaled by 32, the coordinates are rounded to
// integers, the polygon is triangulated, and then scaled back by 1/32.
// 'hint' should be a combination of QVectorPath::Hints.
// 'lod' is the level of detail. Default is 1. Curves are split into more lines when 'lod' is higher.
QTriangleSet Q_GUI_EXPORT qTriangulate(const qreal *polygon, int count,
                                       uint hint = QVectorPath::PolygonHint | QVectorPath::OddEvenFill,
                                       const QTransform &matrix = QTransform(),
                                       bool allowUintIndices = true);
QTriangleSet Q_GUI_EXPORT qTriangulate(const QVectorPath &path, const QTransform &matrix = QTransform(),
                                       qreal lod = 1, bool allowUintIndices = true);
QTriangleSet Q_GUI_EXPORT qTriangulate(const QPainterPath &path, const QTransform &matrix = QTransform(),
                                       qreal lod = 1, bool allowUintIndices = true);
QPolylineSet qPolyline(const QVectorPath &path, const QTransform &matrix = QTransform(),
                       qreal lod = 1, bool allowUintIndices = true);
QPolylineSet Q_GUI_EXPORT qPolyline(const QPainterPath &path, const QTransform &matrix = QTransform(),
                                    qreal lod = 1, bool allowUintIndices = true);

QT_END_NAMESPACE

#endif
