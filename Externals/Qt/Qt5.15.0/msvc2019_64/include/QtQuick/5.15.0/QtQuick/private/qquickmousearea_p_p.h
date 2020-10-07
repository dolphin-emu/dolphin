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

#ifndef QQUICKMOUSEAREA_P_P_H
#define QQUICKMOUSEAREA_P_P_H

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

#include "qquickitem_p.h"
#include "qquickevents_p_p.h"

#include <QtGui/qevent.h>
#include <QtCore/qbasictimer.h>

QT_BEGIN_NAMESPACE

class QQuickMouseEvent;
class QQuickMouseArea;
class QQuickPointerMask;
class QQuickMouseAreaPrivate : public QQuickItemPrivate
{
    Q_DECLARE_PUBLIC(QQuickMouseArea)

public:
    QQuickMouseAreaPrivate();
    ~QQuickMouseAreaPrivate();
    void init();

    void saveEvent(QMouseEvent *event);
    enum PropagateType{
        Click,
        DoubleClick,
        PressAndHold
    };
    void propagate(QQuickMouseEvent* event, PropagateType);
    bool propagateHelper(QQuickMouseEvent*, QQuickItem*,const QPointF &, PropagateType);

    bool isPressAndHoldConnected();
    bool isDoubleClickConnected();
    bool isClickConnected();
    bool isWheelConnected();

    bool enabled : 1;
    bool scrollGestureEnabled : 1;
    bool hovered : 1;
    bool longPress : 1;
    bool moved : 1;
    bool stealMouse : 1;
    bool doubleClick : 1;
    bool preventStealing : 1;
    bool propagateComposedEvents : 1;
    bool overThreshold : 1;
    Qt::MouseButtons pressed;
    int pressAndHoldInterval;
#if QT_CONFIG(quick_draganddrop)
    QQuickDrag *drag;
#endif
    QPointer<QQuickPointerMask> mask;
    QPointF startScene;
    QPointF targetStartPos;
    QPointF lastPos;
    QQmlNullableValue<QPointF> lastScenePos;
    Qt::MouseButton lastButton;
    Qt::MouseButtons lastButtons;
    Qt::KeyboardModifiers lastModifiers;
    QBasicTimer pressAndHoldTimer;
#if QT_CONFIG(cursor)
    QCursor *cursor;
#endif
    QQuickMouseEvent quickMouseEvent;
    QQuickWheelEvent quickWheelEvent;
    Qt::MouseEventFlags lastFlags;
};

QT_END_NAMESPACE

#endif // QQUICKMOUSEAREA_P_P_H
