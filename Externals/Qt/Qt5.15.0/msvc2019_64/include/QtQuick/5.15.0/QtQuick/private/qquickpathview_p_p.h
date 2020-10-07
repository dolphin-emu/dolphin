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

#ifndef QQUICKPATHVIEW_P_P_H
#define QQUICKPATHVIEW_P_P_H

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

#include <private/qtquickglobal_p.h>

QT_REQUIRE_CONFIG(quick_pathview);

#include "qquickpathview_p.h"
#include "qquickitem_p.h"

#include <QtQml/qqml.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qcoreapplication.h>

#include <private/qquickanimation_p_p.h>
#include <private/qqmldelegatemodel_p.h>
#include <private/qquicktimeline_p_p.h>
#include <private/qpodvector_p.h>

QT_BEGIN_NAMESPACE

class QQmlOpenMetaObjectType;
class QQuickPathViewAttached;
class QQuickPathViewPrivate : public QQuickItemPrivate, public QQuickItemChangeListener
{
    Q_DECLARE_PUBLIC(QQuickPathView)

public:
    QQuickPathViewPrivate();

    void init();

    void itemGeometryChanged(QQuickItem *item, QQuickGeometryChange change, const QRectF &) override {
        if (change.sizeChange() && (!highlightItem || item != highlightItem)) {
            if (QQuickPathViewAttached *att = attached(item))
                att->m_percent = -1;
            scheduleLayout();
        }
    }

    void scheduleLayout() {
        Q_Q(QQuickPathView);
        if (!layoutScheduled) {
            layoutScheduled = true;
            q->polish();
        }
    }

    QQuickItem *getItem(int modelIndex, qreal z = 0, bool async=false);
    void releaseItem(QQuickItem *item);
    QQuickPathViewAttached *attached(QQuickItem *item);
    QQmlOpenMetaObjectType *attachedType();
    void clear();
    void updateMappedRange();
    qreal positionOfIndex(qreal index) const;
    bool isInBound(qreal position, qreal lower, qreal upper) const;
    void createHighlight();
    void updateHighlight();
    void setHighlightPosition(qreal pos);
    bool isValid() const {
        return model && model->count() > 0 && model->isValid() && path;
    }

    void handleMousePressEvent(QMouseEvent *event);
    void handleMouseMoveEvent(QMouseEvent *event);
    void handleMouseReleaseEvent(QMouseEvent *);

    int calcCurrentIndex();
    void createCurrentItem();
    void updateCurrent();
    static void fixOffsetCallback(void*);
    void fixOffset();
    void setOffset(qreal offset);
    void setAdjustedOffset(qreal offset);
    void regenerate();
    void updateItem(QQuickItem *, qreal);
    enum MovementReason { Other, SetIndex, Mouse };
    void snapToIndex(int index, MovementReason reason);
    QPointF pointNear(const QPointF &point, qreal *nearPercent=0) const;
    void addVelocitySample(qreal v);
    qreal calcVelocity() const;
    qint64 computeCurrentTime(QInputEvent *event) const;
    void setDragging(bool d);

    QQuickPath *path;
    int currentIndex;
    QPointer<QQuickItem> currentItem;
    qreal currentItemOffset;
    qreal startPc;
    QPointF startPoint;
    QPointF startPos;
    qreal offset;
    qreal offsetAdj;
    qreal mappedRange;
    qreal mappedCache;
    bool stealMouse : 1;
    bool ownModel : 1;
    bool interactive : 1;
    bool haveHighlightRange : 1;
    bool autoHighlight : 1;
    bool highlightUp : 1;
    bool layoutScheduled : 1;
    bool moving : 1;
    bool flicking : 1;
    bool dragging : 1;
    bool inRequest : 1;
    bool delegateValidated : 1;
    bool inRefill : 1;
    QElapsedTimer timer;
    qint64 lastPosTime;
    QPointF lastPos;
    qreal dragMargin;
    qreal deceleration;
    qreal maximumFlickVelocity;
    QQuickTimeLine tl;
    QQuickTimeLineValueProxy<QQuickPathViewPrivate> moveOffset;
    int flickDuration;
    int pathItems;
    int requestedIndex;
    int cacheSize;
    qreal requestedZ;
    QList<QQuickItem *> items;
    QList<QQuickItem *> itemCache;
    QPointer<QQmlInstanceModel> model;
    QVariant modelVariant;
    MovementReason moveReason;
    QQuickPathView::MovementDirection movementDirection; // default
    QQuickPathView::MovementDirection moveDirection; // next movement
    QQmlOpenMetaObjectType *attType;
    QQmlComponent *highlightComponent;
    QQuickItem *highlightItem;
    QQuickTimeLineValueProxy<QQuickPathViewPrivate> moveHighlight;
    qreal highlightPosition;
    qreal highlightRangeStart;
    qreal highlightRangeEnd;
    QQuickPathView::HighlightRangeMode highlightRangeMode;
    int highlightMoveDuration;
    int modelCount;
    QPODVector<qreal,10> velocityBuffer;
    QQuickPathView::SnapMode snapMode;
};

QT_END_NAMESPACE

#endif
