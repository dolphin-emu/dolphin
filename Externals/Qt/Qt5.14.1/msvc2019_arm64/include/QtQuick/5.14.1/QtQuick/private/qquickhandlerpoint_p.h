/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#ifndef QQUICKHANDLERPOINT_H
#define QQUICKHANDLERPOINT_H

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

#include "qquickpointerdevicehandler_p.h"

QT_BEGIN_NAMESPACE

class QQuickMultiPointHandler;
class QQuickSinglePointHandler;

class Q_QUICK_PRIVATE_EXPORT QQuickHandlerPoint {
    Q_GADGET
    Q_PROPERTY(int id READ id)
    Q_PROPERTY(QPointingDeviceUniqueId uniqueId READ uniqueId)
    Q_PROPERTY(QPointF position READ position)
    Q_PROPERTY(QPointF scenePosition READ scenePosition)
    Q_PROPERTY(QPointF pressPosition READ pressPosition)
    Q_PROPERTY(QPointF scenePressPosition READ scenePressPosition)
    Q_PROPERTY(QPointF sceneGrabPosition READ sceneGrabPosition)
    Q_PROPERTY(Qt::MouseButtons pressedButtons READ pressedButtons)
    Q_PROPERTY(Qt::KeyboardModifiers modifiers READ modifiers)
    Q_PROPERTY(QVector2D velocity READ velocity)
    Q_PROPERTY(qreal rotation READ rotation)
    Q_PROPERTY(qreal pressure READ pressure)
    Q_PROPERTY(QSizeF ellipseDiameters READ ellipseDiameters)

public:
    QQuickHandlerPoint();

    int id() const { return m_id; }
    Qt::MouseButtons pressedButtons() const { return m_pressedButtons; }
    Qt::KeyboardModifiers modifiers() const { return m_pressedModifiers; }
    QPointF pressPosition() const { return m_pressPosition; }
    QPointF scenePressPosition() const { return m_scenePressPosition; }
    QPointF sceneGrabPosition() const { return m_sceneGrabPosition; }
    QPointF position() const { return m_position; }
    QPointF scenePosition() const { return m_scenePosition; }
    QVector2D velocity() const { return m_velocity; }
    qreal rotation() const { return m_rotation; }
    qreal pressure() const { return m_pressure; }
    QSizeF ellipseDiameters() const { return m_ellipseDiameters; }
    QPointingDeviceUniqueId uniqueId() const { return m_uniqueId; }
    void localize(QQuickItem *item);

    void reset();
    void reset(const QQuickEventPoint *point);
    void reset(const QVector<QQuickHandlerPoint> &points);

private:
    int m_id = 0;
    QPointingDeviceUniqueId m_uniqueId;
    Qt::MouseButtons m_pressedButtons = Qt::NoButton;
    Qt::KeyboardModifiers m_pressedModifiers = Qt::NoModifier;
    QPointF m_position;
    QPointF m_scenePosition;
    QPointF m_pressPosition;
    QPointF m_scenePressPosition;
    QPointF m_sceneGrabPosition;
    QVector2D m_velocity;
    qreal m_rotation = 0;
    qreal m_pressure = 0;
    QSizeF m_ellipseDiameters;
    friend class QQuickMultiPointHandler;
    friend class QQuickSinglePointHandler;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickHandlerPoint)

#endif // QQUICKHANDLERPOINT_H
