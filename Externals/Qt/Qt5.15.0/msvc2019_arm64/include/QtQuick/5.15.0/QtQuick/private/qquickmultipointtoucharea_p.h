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

#ifndef QQUICKMULTIPOINTTOUCHAREA_H
#define QQUICKMULTIPOINTTOUCHAREA_H

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

#include "qquickitem.h"
#include "qevent.h"

#include <QPointer>
#include <QMap>
#include <QList>
#include <QtGui/qguiapplication.h>
#include <QtGui/qstylehints.h>

QT_BEGIN_NAMESPACE

class QQuickMultiPointTouchArea;
class Q_AUTOTEST_EXPORT QQuickTouchPoint : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int pointId READ pointId NOTIFY pointIdChanged)
    Q_PROPERTY(QPointingDeviceUniqueId uniqueId READ uniqueId NOTIFY uniqueIdChanged REVISION 9)
    Q_PROPERTY(bool pressed READ pressed NOTIFY pressedChanged)
    Q_PROPERTY(qreal x READ x NOTIFY xChanged)
    Q_PROPERTY(qreal y READ y NOTIFY yChanged)
    Q_PROPERTY(QSizeF ellipseDiameters READ ellipseDiameters NOTIFY ellipseDiametersChanged REVISION 9)
    Q_PROPERTY(qreal pressure READ pressure NOTIFY pressureChanged)
    Q_PROPERTY(qreal rotation READ rotation NOTIFY rotationChanged REVISION 9)
    Q_PROPERTY(QVector2D velocity READ velocity NOTIFY velocityChanged)
    Q_PROPERTY(QRectF area READ area NOTIFY areaChanged)

    Q_PROPERTY(qreal startX READ startX NOTIFY startXChanged)
    Q_PROPERTY(qreal startY READ startY NOTIFY startYChanged)
    Q_PROPERTY(qreal previousX READ previousX NOTIFY previousXChanged)
    Q_PROPERTY(qreal previousY READ previousY NOTIFY previousYChanged)
    Q_PROPERTY(qreal sceneX READ sceneX NOTIFY sceneXChanged)
    Q_PROPERTY(qreal sceneY READ sceneY NOTIFY sceneYChanged)
    QML_NAMED_ELEMENT(TouchPoint)

public:
    QQuickTouchPoint(bool qmlDefined = true)
        : _qmlDefined(qmlDefined)
    {}

    int pointId() const { return _id; }
    void setPointId(int id);

    QPointingDeviceUniqueId uniqueId() const { return _uniqueId; }
    void setUniqueId(const QPointingDeviceUniqueId &id);

    qreal x() const { return _x; }
    qreal y() const { return _y; }
    void setPosition(QPointF pos);

    QSizeF ellipseDiameters() const { return _ellipseDiameters; }
    void setEllipseDiameters(const QSizeF &d);

    qreal pressure() const { return _pressure; }
    void setPressure(qreal pressure);

    qreal rotation() const { return _rotation; }
    void setRotation(qreal r);

    QVector2D velocity() const { return _velocity; }
    void setVelocity(const QVector2D &velocity);

    QRectF area() const { return _area; }
    void setArea(const QRectF &area);

    bool isQmlDefined() const { return _qmlDefined; }

    bool inUse() const { return _inUse; }
    void setInUse(bool inUse) { _inUse = inUse; }

    bool pressed() const { return _pressed; }
    void setPressed(bool pressed);

    qreal startX() const { return _startX; }
    void setStartX(qreal startX);

    qreal startY() const { return _startY; }
    void setStartY(qreal startY);

    qreal previousX() const { return _previousX; }
    void setPreviousX(qreal previousX);

    qreal previousY() const { return _previousY; }
    void setPreviousY(qreal previousY);

    qreal sceneX() const { return _sceneX; }
    void setSceneX(qreal sceneX);

    qreal sceneY() const { return _sceneY; }
    void setSceneY(qreal sceneY);

Q_SIGNALS:
    void pressedChanged();
    void pointIdChanged();
    Q_REVISION(9) void uniqueIdChanged();
    void xChanged();
    void yChanged();
    Q_REVISION(9) void ellipseDiametersChanged();
    void pressureChanged();
    Q_REVISION(9) void rotationChanged();
    void velocityChanged();
    void areaChanged();
    void startXChanged();
    void startYChanged();
    void previousXChanged();
    void previousYChanged();
    void sceneXChanged();
    void sceneYChanged();

private:
    friend class QQuickMultiPointTouchArea;
    int _id = 0;
    qreal _x = 0.0;
    qreal _y = 0.0;
    qreal _pressure = 0.0;
    qreal _rotation = 0;
    QSizeF _ellipseDiameters;
    QVector2D _velocity;
    QRectF _area;
    bool _qmlDefined;
    bool _inUse = false;    //whether the point is currently in use (only valid when _qmlDefined == true)
    bool _pressed = false;
    qreal _startX = 0.0;
    qreal _startY = 0.0;
    qreal _previousX = 0.0;
    qreal _previousY = 0.0;
    qreal _sceneX = 0.0;
    qreal _sceneY = 0.0;
    QPointingDeviceUniqueId _uniqueId;
};

class QQuickGrabGestureEvent : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<QObject> touchPoints READ touchPoints CONSTANT)
    Q_PROPERTY(qreal dragThreshold READ dragThreshold CONSTANT)
    QML_NAMED_ELEMENT(GestureEvent)
    QML_UNCREATABLE("GestureEvent is only available in the context of handling the gestureStarted signal from MultiPointTouchArea.")

public:
    QQuickGrabGestureEvent() : _dragThreshold(QGuiApplication::styleHints()->startDragDistance()) {}

    Q_INVOKABLE void grab() { _grab = true; }
    bool wantsGrab() const { return _grab; }

    QQmlListProperty<QObject> touchPoints() {
        return QQmlListProperty<QObject>(this, &_touchPoints);
    }
    qreal dragThreshold() const { return _dragThreshold; }

private:
    friend class QQuickMultiPointTouchArea;
    bool _grab = false;
    qreal _dragThreshold;
    QList<QObject*> _touchPoints;
};

class Q_AUTOTEST_EXPORT QQuickMultiPointTouchArea : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(QQmlListProperty<QQuickTouchPoint> touchPoints READ touchPoints)
    Q_PROPERTY(int minimumTouchPoints READ minimumTouchPoints WRITE setMinimumTouchPoints NOTIFY minimumTouchPointsChanged)
    Q_PROPERTY(int maximumTouchPoints READ maximumTouchPoints WRITE setMaximumTouchPoints NOTIFY maximumTouchPointsChanged)
    Q_PROPERTY(bool mouseEnabled READ mouseEnabled WRITE setMouseEnabled NOTIFY mouseEnabledChanged)
    QML_NAMED_ELEMENT(MultiPointTouchArea)

public:
    QQuickMultiPointTouchArea(QQuickItem *parent=nullptr);
    ~QQuickMultiPointTouchArea();

    int minimumTouchPoints() const;
    void setMinimumTouchPoints(int num);
    int maximumTouchPoints() const;
    void setMaximumTouchPoints(int num);
    bool mouseEnabled() const { return _mouseEnabled; }
    void setMouseEnabled(bool arg);

    QQmlListProperty<QQuickTouchPoint> touchPoints() {
        return QQmlListProperty<QQuickTouchPoint>(this, nullptr, QQuickMultiPointTouchArea::touchPoint_append, QQuickMultiPointTouchArea::touchPoint_count, QQuickMultiPointTouchArea::touchPoint_at, nullptr);
    }

    static void touchPoint_append(QQmlListProperty<QQuickTouchPoint> *list, QQuickTouchPoint* touch) {
        QQuickMultiPointTouchArea *q = static_cast<QQuickMultiPointTouchArea*>(list->object);
        q->addTouchPrototype(touch);
    }

    static int touchPoint_count(QQmlListProperty<QQuickTouchPoint> *list) {
        QQuickMultiPointTouchArea *q = static_cast<QQuickMultiPointTouchArea*>(list->object);
        return q->_touchPrototypes.count();
    }

    static QQuickTouchPoint* touchPoint_at(QQmlListProperty<QQuickTouchPoint> *list, int index) {
        QQuickMultiPointTouchArea *q = static_cast<QQuickMultiPointTouchArea*>(list->object);
        return q->_touchPrototypes.value(index);
    }

Q_SIGNALS:
    void pressed(const QList<QObject*> &touchPoints);
    void updated(const QList<QObject*> &touchPoints);
    void released(const QList<QObject*> &touchPoints);
    void canceled(const QList<QObject*> &touchPoints);
    void gestureStarted(QQuickGrabGestureEvent *gesture);
    void touchUpdated(const QList<QObject*> &touchPoints);
    void minimumTouchPointsChanged();
    void maximumTouchPointsChanged();
    void mouseEnabledChanged();

protected:
    void touchEvent(QTouchEvent *) override;
    bool childMouseEventFilter(QQuickItem *receiver, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseUngrabEvent() override;
    void touchUngrabEvent() override;

    void addTouchPrototype(QQuickTouchPoint* prototype);
    void addTouchPoint(const QTouchEvent::TouchPoint *p);
    void addTouchPoint(const QMouseEvent *e);
    void clearTouchLists();

    void updateTouchPoint(QQuickTouchPoint*, const QTouchEvent::TouchPoint*);
    void updateTouchPoint(QQuickTouchPoint *dtp, const QMouseEvent *e);
    void updateTouchData(QEvent*);

    bool sendMouseEvent(QMouseEvent *event);
    bool shouldFilter(QEvent *event);
    void grabGesture();
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;
#ifdef Q_OS_OSX
    void hoverEnterEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;
    void setTouchEventsEnabled(bool enable);
#endif

private:
    void ungrab(bool normalRelease = false);
    QMap<int,QQuickTouchPoint*> _touchPrototypes;  //TouchPoints defined in QML
    QMap<int,QObject*> _touchPoints;            //All current touch points
    QList<QObject*> _releasedTouchPoints;
    QList<QObject*> _pressedTouchPoints;
    QList<QObject*> _movedTouchPoints;
    int _minimumTouchPoints;
    int _maximumTouchPoints;
    QVector<int> _lastFilterableTouchPointIds;
    QPointer<QQuickTouchPoint> _mouseTouchPoint; // exists when mouse button is down and _mouseEnabled is true; null otherwise
    QTouchEvent::TouchPoint _mouseQpaTouchPoint; // synthetic QPA touch point to hold state and position of the mouse
    const QTouchDevice *_touchMouseDevice;
    QPointF _mousePos;
    bool _stealMouse;
    bool _mouseEnabled;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickTouchPoint)
QML_DECLARE_TYPE(QQuickGrabGestureEvent)
QML_DECLARE_TYPE(QQuickMultiPointTouchArea)

#endif // QQUICKMULTIPOINTTOUCHAREA_H
