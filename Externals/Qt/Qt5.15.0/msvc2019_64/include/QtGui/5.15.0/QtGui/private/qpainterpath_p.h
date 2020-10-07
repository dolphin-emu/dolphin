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

#ifndef QPAINTERPATH_P_H
#define QPAINTERPATH_P_H

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
#include "QtGui/qpainterpath.h"
#include "QtGui/qregion.h"
#include "QtCore/qlist.h"
#include "QtCore/qvarlengtharray.h"

#include <qdebug.h>

#include <private/qvectorpath_p.h>
#include <private/qstroker_p.h>

#include <memory>

QT_BEGIN_NAMESPACE

// ### Qt 6: merge with QPainterPathData
class QPainterPathPrivate
{
public:
    friend class QPainterPath;
    friend class QPainterPathData;
    friend class QPainterPathStroker;
    friend class QPainterPathStrokerPrivate;
    friend class QMatrix;
    friend class QTransform;
    friend class QVectorPath;
    friend struct QPainterPathPrivateDeleter;
#ifndef QT_NO_DATASTREAM
    friend Q_GUI_EXPORT QDataStream &operator<<(QDataStream &, const QPainterPath &);
    friend Q_GUI_EXPORT QDataStream &operator>>(QDataStream &, QPainterPath &);
#endif

    QPainterPathPrivate() noexcept
        : ref(1)
    {
    }

    QPainterPathPrivate(const QPainterPathPrivate &other) noexcept
        : ref(1),
          elements(other.elements)
    {
    }

    QPainterPathPrivate &operator=(const QPainterPathPrivate &) = delete;
    ~QPainterPathPrivate() = default;

private:
    QAtomicInt ref;
    QVector<QPainterPath::Element> elements;
};

class QPainterPathStrokerPrivate
{
public:
    QPainterPathStrokerPrivate();

    QStroker stroker;
    QVector<qfixed> dashPattern;
    qreal dashOffset;
};

class QPolygonF;
class QVectorPathConverter;

class QVectorPathConverter
{
public:
    QVectorPathConverter(const QVector<QPainterPath::Element> &path, uint fillRule, bool convex)
        : pathData(path, fillRule, convex),
          path(pathData.points.data(), path.size(),
               pathData.elements.data(), pathData.flags) {}

    const QVectorPath &vectorPath() {
        return path;
    }

    struct QVectorPathData {
        QVectorPathData(const QVector<QPainterPath::Element> &path, uint fillRule, bool convex)
            : elements(path.size()),
              points(path.size() * 2),
              flags(0)
        {
            int ptsPos = 0;
            bool isLines = true;
            for (int i=0; i<path.size(); ++i) {
                const QPainterPath::Element &e = path.at(i);
                elements[i] = e.type;
                points[ptsPos++] = e.x;
                points[ptsPos++] = e.y;
                if (e.type == QPainterPath::CurveToElement)
                    flags |= QVectorPath::CurvedShapeMask;

                // This is to check if the path contains only alternating lineTo/moveTo,
                // in which case we can set the LinesHint in the path. MoveTo is 0 and
                // LineTo is 1 so the i%2 gets us what we want cheaply.
                isLines = isLines && e.type == (QPainterPath::ElementType) (i%2);
            }

            if (fillRule == Qt::WindingFill)
                flags |= QVectorPath::WindingFill;
            else
                flags |= QVectorPath::OddEvenFill;

            if (isLines)
                flags |= QVectorPath::LinesShapeMask;
            else {
                flags |= QVectorPath::AreaShapeMask;
                if (!convex)
                    flags |= QVectorPath::NonConvexShapeMask;
            }

        }
        QVarLengthArray<QPainterPath::ElementType> elements;
        QVarLengthArray<qreal> points;
        uint flags;
    };

    QVectorPathData pathData;
    QVectorPath path;

private:
    Q_DISABLE_COPY_MOVE(QVectorPathConverter)
};

class QPainterPathData : public QPainterPathPrivate
{
public:
    QPainterPathData() :
        cStart(0),
        fillRule(Qt::OddEvenFill),
        require_moveTo(false),
        dirtyBounds(false),
        dirtyControlBounds(false),
        convex(false),
        pathConverter(nullptr)
    {
    }

    QPainterPathData(const QPainterPathData &other) :
        QPainterPathPrivate(other),
        cStart(other.cStart),
        fillRule(other.fillRule),
        bounds(other.bounds),
        controlBounds(other.controlBounds),
        require_moveTo(false),
        dirtyBounds(other.dirtyBounds),
        dirtyControlBounds(other.dirtyControlBounds),
        convex(other.convex),
        pathConverter(nullptr)
    {
    }

    QPainterPathData &operator=(const QPainterPathData &) = delete;
    ~QPainterPathData() = default;

    inline bool isClosed() const;
    inline void close();
    inline void maybeMoveTo();
    inline void clear();

    const QVectorPath &vectorPath() {
        if (!pathConverter)
            pathConverter.reset(new QVectorPathConverter(elements, fillRule, convex));
        return pathConverter->path;
    }

    int cStart;
    Qt::FillRule fillRule;

    QRectF bounds;
    QRectF controlBounds;

    uint require_moveTo : 1;
    uint dirtyBounds : 1;
    uint dirtyControlBounds : 1;
    uint convex : 1;

    std::unique_ptr<QVectorPathConverter> pathConverter;
};


inline const QPainterPath QVectorPath::convertToPainterPath() const
{
        QPainterPath path;
        path.ensureData();
        QPainterPathData *data = path.d_func();
        data->elements.reserve(m_count);
        int index = 0;
        data->elements[0].x = m_points[index++];
        data->elements[0].y = m_points[index++];

        if (m_elements) {
            data->elements[0].type = m_elements[0];
            for (int i=1; i<m_count; ++i) {
                QPainterPath::Element element;
                element.x = m_points[index++];
                element.y = m_points[index++];
                element.type = m_elements[i];
                data->elements << element;
            }
        } else {
            data->elements[0].type = QPainterPath::MoveToElement;
            for (int i=1; i<m_count; ++i) {
                QPainterPath::Element element;
                element.x = m_points[index++];
                element.y = m_points[index++];
                element.type = QPainterPath::LineToElement;
                data->elements << element;
            }
        }

        if (m_hints & OddEvenFill)
            data->fillRule = Qt::OddEvenFill;
        else
            data->fillRule = Qt::WindingFill;
        return path;
}

void Q_GUI_EXPORT qt_find_ellipse_coords(const QRectF &r, qreal angle, qreal length,
                                         QPointF* startPoint, QPointF *endPoint);

inline bool QPainterPathData::isClosed() const
{
    const QPainterPath::Element &first = elements.at(cStart);
    const QPainterPath::Element &last = elements.last();
    return first.x == last.x && first.y == last.y;
}

inline void QPainterPathData::close()
{
    Q_ASSERT(ref.loadRelaxed() == 1);
    require_moveTo = true;
    const QPainterPath::Element &first = elements.at(cStart);
    QPainterPath::Element &last = elements.last();
    if (first.x != last.x || first.y != last.y) {
        if (qFuzzyCompare(first.x, last.x) && qFuzzyCompare(first.y, last.y)) {
            last.x = first.x;
            last.y = first.y;
        } else {
            QPainterPath::Element e = { first.x, first.y, QPainterPath::LineToElement };
            elements << e;
        }
    }
}

inline void QPainterPathData::maybeMoveTo()
{
    if (require_moveTo) {
        QPainterPath::Element e = elements.last();
        e.type = QPainterPath::MoveToElement;
        elements.append(e);
        require_moveTo = false;
    }
}

inline void QPainterPathData::clear()
{
    Q_ASSERT(ref.loadRelaxed() == 1);

    elements.clear();

    cStart = 0;
    fillRule = Qt::OddEvenFill;
    bounds = {};
    controlBounds = {};

    require_moveTo = false;
    dirtyBounds = false;
    dirtyControlBounds = false;
    convex = false;

    pathConverter.reset();
}
#define KAPPA qreal(0.5522847498)


QT_END_NAMESPACE

#endif // QPAINTERPATH_P_H
