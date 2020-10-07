/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWidgets module of the Qt Toolkit.
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

#ifndef QGESTURE_P_H
#define QGESTURE_P_H

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

#include <QtWidgets/private/qtwidgetsglobal_p.h>
#include "qrect.h"
#include "qpoint.h"
#include "qgesture.h"
#include "qelapsedtimer.h"
#include "private/qobject_p.h"

#ifndef QT_NO_GESTURES

QT_BEGIN_NAMESPACE

class QGesturePrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QGesture)

public:
    QGesturePrivate()
        : gestureType(Qt::CustomGesture), state(Qt::NoGesture),
          isHotSpotSet(false), gestureCancelPolicy(0)
    {
    }

    Qt::GestureType gestureType;
    Qt::GestureState state;
    QPointF hotSpot;
    QPointF sceneHotSpot;
    uint isHotSpotSet : 1;
    uint gestureCancelPolicy : 2;
};

class QPanGesturePrivate : public QGesturePrivate
{
    Q_DECLARE_PUBLIC(QPanGesture)

public:
    QPanGesturePrivate()
        : acceleration(0), xVelocity(0), yVelocity(0), pointCount(2)
    {
    }

    qreal horizontalVelocity() const { return xVelocity; }
    void setHorizontalVelocity(qreal value) { xVelocity = value; }
    qreal verticalVelocity() const { return yVelocity; }
    void setVerticalVelocity(qreal value) { yVelocity = value; }

    QPointF lastOffset;
    QPointF offset;
    QPoint startPosition;
    qreal acceleration;
    qreal xVelocity;
    qreal yVelocity;
    int pointCount; // ### fixme Qt 5.5: Add accessor to QPanGesture.
};

class QPinchGesturePrivate : public QGesturePrivate
{
    Q_DECLARE_PUBLIC(QPinchGesture)

public:
    QPinchGesturePrivate()
        : totalScaleFactor(1), lastScaleFactor(1), scaleFactor(1),
          totalRotationAngle(0), lastRotationAngle(0), rotationAngle(0),
          isNewSequence(true)
    {
    }

    QPinchGesture::ChangeFlags totalChangeFlags;
    QPinchGesture::ChangeFlags changeFlags;

    QPointF startCenterPoint;
    QPointF lastCenterPoint;
    QPointF centerPoint;

    qreal totalScaleFactor;
    qreal lastScaleFactor;
    qreal scaleFactor;

    qreal totalRotationAngle;
    qreal lastRotationAngle;
    qreal rotationAngle;

    bool isNewSequence;
    QPointF startPosition[2];
};

class QSwipeGesturePrivate : public QGesturePrivate
{
    Q_DECLARE_PUBLIC(QSwipeGesture)

public:
    enum State {
        NoGesture,
        Started,
        ThreePointsReached
    };

    QSwipeGesturePrivate()
        : horizontalDirection(QSwipeGesture::NoDirection),
          verticalDirection(QSwipeGesture::NoDirection),
          swipeAngle(0),
          state(NoGesture), velocityValue(0)
    {
    }

    qreal velocity() const { return velocityValue; }
    void setVelocity(qreal value) { velocityValue = value; }

    QSwipeGesture::SwipeDirection horizontalDirection;
    QSwipeGesture::SwipeDirection verticalDirection;
    qreal swipeAngle;

    QPoint lastPositions[3];
    State state;
    qreal velocityValue;
    QElapsedTimer time;
};

class QTapGesturePrivate : public QGesturePrivate
{
    Q_DECLARE_PUBLIC(QTapGesture)

public:
    QTapGesturePrivate()
    {
    }

    QPointF position;
};

class QTapAndHoldGesturePrivate : public QGesturePrivate
{
    Q_DECLARE_PUBLIC(QTapAndHoldGesture)

public:
    QTapAndHoldGesturePrivate()
        : timerId(0)
    {
    }

    QPointF position;
    int timerId;
    static int Timeout;
};

QT_END_NAMESPACE

#endif // QT_NO_GESTURES

#endif // QGESTURE_P_H
